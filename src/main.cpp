#include <charconv>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <string>
#include <vector>
#include <semaphore.h>
#include <csignal>
#include <ctime>
#include "mesa.hpp"

struct Torneo {
    int jugadores;
    std::vector<int> llegadas; // cantidad de integrantes presentes por pareja
    bool fallo = false;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    sem_t cupos;                          // cuenta las mesas libres (M)
    Config cfg;                           // parametros de las partidas
    const volatile sig_atomic_t* parar = nullptr; // Ctrl+C
};

static volatile sig_atomic_t paraTodo = 0; // lo pone el manejador de Ctrl+C
static void manejador(int) { paraTodo = 1; }

struct ArgJugador {
    Torneo* torneo;
    int id;
};

static bool esperarMesa(sem_t* s, const volatile sig_atomic_t* parar) { // espera pasiva, despierta cada 100 ms por si hay Ctrl+C
    while (!(parar && *parar)) {
        timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 100 * 1000000;
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
        if (sem_timedwait(s, &ts) == 0) return true;
    }
    return false;
}

void* hiloJugador(void* p) {
    const ArgJugador& a = *static_cast<ArgJugador*>(p);
    Torneo& t = *a.torneo;
    pthread_mutex_lock(&t.mtx);
    std::cout << "[JUGADOR " << a.id << "] llegada registrada.\n";
    if (t.jugadores % 2 != 0 && a.id == t.jugadores) {
        std::cout << "[JUGADOR " << a.id << "] sin pareja.\n";
        pthread_mutex_unlock(&t.mtx);
        return nullptr;
    }
    const int pareja = (a.id - 1) / 2;
    bool lanzar = false; // true = este jugador hace de Juez de la mesa
    if (!t.fallo) {
        ++t.llegadas[pareja];
        if (t.llegadas[pareja] == 2) { // solo el segundo integrante anuncia
            const int primero = pareja * 2 + 1;
            std::cout << "[PAREJA " << pareja + 1 << "] EMPAREJADOS: JUGADOR "
                      << primero << " (PUMA) VS JUGADOR " << primero + 1 << " (PERROS)\n";
            lanzar = true;
            pthread_cond_broadcast(&t.cond);
        }
        while (!t.fallo && t.llegadas[pareja] < 2)
            pthread_cond_wait(&t.cond, &t.mtx);
    }
    pthread_mutex_unlock(&t.mtx);
    if (lanzar) { // el segundo en llegar espera mesa y juega la partida
        if (sem_trywait(&t.cupos) != 0) {
            std::cout << "[PAREJA " << pareja + 1 << "] espera una mesa libre" << std::endl;
            if (!esperarMesa(&t.cupos, t.parar)) return nullptr;
        }
        jugarPartida(t.cfg, pareja + 1, t.parar);
        sem_post(&t.cupos); // la mesa queda libre
    }
    return nullptr;
}

int main(int argc, char* argv[]) { // VALIDAR CANTIDAD DE ARGUMENTOS 
    if (argc < 3 || argc > 4) {
        std::cerr << "ERROR --> USO CORRECTO  --> " << argv[0] << " N M [config.txt]\n";
        return 1;
    }

    int N = 0;
    int M = 0;
    std::string config_file = "";

    auto enteroPositivo = [](const std::string& texto, int& numero) {
        const char* fin = texto.data() + texto.size();
        const auto resultado = std::from_chars(texto.data(), fin, numero);
        return resultado.ec == std::errc{} && resultado.ptr == fin && numero > 0;
    };
    if (!enteroPositivo(argv[1], N) || !enteroPositivo(argv[2], M)) {
        std::cerr << "ERROR: N y M deben ser enteros positivos dentro del rango de int, sin texto adicional.\n";
        return 1;
    }

    if (argc == 4) {
        config_file = argv[3];
    }

    std::cout << "--- SERVIDOR INICIADO ---\n";
    std::cout << "JUGADORES: " << N << " [-] MESAS DISPONIBLES: " << M << "\n";

    Torneo torneo;
    torneo.jugadores = N;
    std::vector<ArgJugador> args;
    std::vector<pthread_t> hilos;
    try { // RESERVAR ANTES DE CREAR HILOS MANTIENE ESTABLES SUS ARGUMENTOS
        torneo.llegadas.resize(N / 2, 0);
        args.resize(N);
        hilos.resize(N);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: no se pudieron reservar los jugadores: " << e.what() << '\n';
        return 1;
    }
    int error = pthread_mutex_init(&torneo.mtx, nullptr);
    if (error != 0) {
        std::cerr << "ERROR: pthread_mutex_init: " << std::strerror(error) << '\n';
        return 1;
    }
    error = pthread_cond_init(&torneo.cond, nullptr);
    if (error != 0) {
        std::cerr << "ERROR: pthread_cond_init: " << std::strerror(error) << '\n';
        pthread_mutex_destroy(&torneo.mtx);
        return 1;
    }
    if (sem_init(&torneo.cupos, 0, M) != 0) {
        std::cerr << "ERROR: sem_init: " << std::strerror(errno) << '\n';
        pthread_cond_destroy(&torneo.cond);
        pthread_mutex_destroy(&torneo.mtx);
        return 1;
    }
    torneo.parar = &paraTodo;
    struct sigaction sa; // Ctrl+C sin SA_RESTART
    sa.sa_handler = manejador;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    int creados = 0;
    for (int i = 0; i < N; ++i) {
        args[i] = ArgJugador{&torneo, i + 1};
        error = pthread_create(&hilos[i], nullptr, hiloJugador, &args[i]);
        if (error != 0) {
            pthread_mutex_lock(&torneo.mtx);
            torneo.fallo = true;
            std::cerr << "ERROR: pthread_create del jugador " << i + 1 << ": "
                      << std::strerror(error) << '\n';
            pthread_cond_broadcast(&torneo.cond);
            pthread_mutex_unlock(&torneo.mtx);
            break;
        }
        ++creados;
    }
    bool falloJoin = false;
    for (int i = 0; i < creados; ++i) {
        error = pthread_join(hilos[i], nullptr);
        if (error != 0) {
            std::cerr << "ERROR: pthread_join del jugador " << i + 1 << ": "
                      << std::strerror(error) << '\n';
            falloJoin = true;
        }
    }
    if (falloJoin) return 1; // no destruir primitivas si no se confirmo la salida de todos
    sem_destroy(&torneo.cupos);
    pthread_cond_destroy(&torneo.cond);
    pthread_mutex_destroy(&torneo.mtx);
    if (paraTodo) std::cout << "\n[SERVIDOR] Ctrl+C: partidas cerradas y recursos liberados\n";
    return torneo.fallo ? 1 : 0;
}
