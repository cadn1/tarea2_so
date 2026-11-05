#include <algorithm>
#include <charconv>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <iostream>
#include <pthread.h>
#include <string>
#include <vector>
#include "mesa.hpp"
#include "config.hpp"

struct Pareja {
    int puma = 0;
    int perros = 0; // ambos IDs se asignan juntos al completar la pareja
    bool jugando = false;
    bool terminada = false;
    Ganador resultado = NADIE;
    CierrePartida cierre = CIERRE_TIEMPO;
    Tablero* tablero = nullptr; // protegido por Torneo::mtx; no usar fuera de el
};

struct Jugador {
    int pareja = -1;
    Celda rol = VACIA;
};

struct Torneo {
    int jugadores;
    const Config* config; // vive en main hasta esperar todos los trabajadores
    std::vector<Pareja> parejas;
    std::vector<Jugador> participantes; // indexado por ID - 1; protegido por mtx
    int esperandoPareja = 0; // a lo sumo un jugador pendiente de companero
    int llegados = 0;
    std::size_t parejasFormadas = 0;
    std::size_t terminadas = 0;
    bool fallo = false;
    bool detener = false;
    bool sigintRecibido = false;
    bool cerrarReceptor = false;
    bool receptorTerminado = false;
    int jugadorSimulado = 0; // 0 desactiva la demostracion
    bool simularFallo = false;
    pthread_mutex_t mtx;
    pthread_cond_t cond;
};

struct ArgJugador {
    Torneo* torneo;
    int id;
};

struct ArgMesa {
    Torneo* torneo;
    int id;
};

struct ArgSenales {
    Torneo* torneo;
    sigset_t conjunto;
};

void* hiloSenales(void* p) {
    const ArgSenales& a = *static_cast<ArgSenales*>(p);
    Torneo& t = *a.torneo;
    for (;;) {
        int senal = 0;
        const int error = sigwait(&a.conjunto, &senal);
        pthread_mutex_lock(&t.mtx);
        if (t.cerrarReceptor) {
            t.receptorTerminado = true;
            pthread_mutex_unlock(&t.mtx);
            return nullptr;
        }
        if (error != 0) {
            t.fallo = true;
            t.detener = true;
            t.receptorTerminado = true;
            pthread_mutex_lock(&mtxSalida);
            std::cerr << "ERROR: sigwait: " << std::strerror(error) << '\n';
            pthread_mutex_unlock(&mtxSalida);
            pthread_cond_broadcast(&t.cond);
            pthread_mutex_unlock(&t.mtx);
            return nullptr;
        }
        if (!t.sigintRecibido) {
            t.sigintRecibido = true;
            t.detener = true;
            pthread_mutex_lock(&mtxSalida);
            std::cout << "[TORNEO] SIGINT recibido: parada registrada." << std::endl;
            pthread_mutex_unlock(&mtxSalida);
        }
        pthread_cond_broadcast(&t.cond);
        pthread_mutex_unlock(&t.mtx);
    }
}

// Al salir del proceso, ignorar SIGINT descarta senales pendientes y evita que
// una nueva interrupcion mate la limpieza al restaurar la mascara anterior.
int restaurarMascara(const sigset_t& anterior) {
    struct sigaction ignorar{};
    ignorar.sa_handler = SIG_IGN;
    sigemptyset(&ignorar.sa_mask);
    if (sigaction(SIGINT, &ignorar, nullptr) != 0) return errno;
    return pthread_sigmask(SIG_SETMASK, &anterior, nullptr);
}

// El propio jugador llama esto ante una renuncia o un error local recuperable.
// No llamar con t.mtx adquirido. Nunca cambia la parada global del torneo.
bool notificarAbandono(Torneo& t, int id, bool falloControlado = false) {
    if (id <= 0 || id > t.jugadores) return false;
    pthread_mutex_lock(&t.mtx);
    const Jugador& jugador = t.participantes[id - 1];
    const int indice = jugador.pareja;
    bool aceptado = false;
    if (indice >= 0 && indice < static_cast<int>(t.parejas.size()) &&
        !t.detener && !t.fallo) {
        Pareja& pareja = t.parejas[indice];
        // Orden: torneo -> tablero. aceptarAbandono libera tablero antes
        // de adquirir salida, y la mesa retira el puntero antes de destruirlo.
        if (pareja.jugando && !pareja.terminada && pareja.tablero &&
            aceptarAbandono(*pareja.tablero, jugador.rol)) {
            aceptado = true;
            pthread_mutex_lock(&mtxSalida);
            std::cout << "[JUGADOR " << id << "] notifica abandono"
                      << (falloControlado ? " por fallo controlado" : " voluntario")
                      << ". PAREJA " << indice + 1 << '\n';
            pthread_mutex_unlock(&mtxSalida);
            pthread_cond_broadcast(&t.cond);
        }
    }
    pthread_mutex_unlock(&t.mtx);
    return aceptado;
}

void* hiloJugador(void* p) {
    const ArgJugador& a = *static_cast<ArgJugador*>(p);
    Torneo& t = *a.torneo;
    pthread_mutex_lock(&t.mtx);
    Jugador& jugador = t.participantes[a.id - 1];
    if (!t.detener && !t.fallo) {
        // Este registro bajo mtx define el orden real, no pthread_create ni el ID.
        ++t.llegados;
        pthread_mutex_lock(&mtxSalida);
        std::cout << "[JUGADOR " << a.id << "] llegada registrada. ORDEN " << t.llegados << '\n';
        pthread_mutex_unlock(&mtxSalida);
        if (t.esperandoPareja == 0) t.esperandoPareja = a.id;
        else {
            const int primero = t.esperandoPareja;
            const int indice = static_cast<int>(t.parejasFormadas++);
            t.parejas[indice].puma = primero;
            t.parejas[indice].perros = a.id;
            t.participantes[primero - 1] = Jugador{indice, PUMA};
            jugador = Jugador{indice, PERRO};
            t.esperandoPareja = 0;
            pthread_mutex_lock(&mtxSalida);
            std::cout << "[PAREJA " << indice + 1 << "] EMPAREJADOS: JUGADOR "
                      << primero << " (PUMA) VS JUGADOR " << a.id << " (PERROS)\n";
            pthread_mutex_unlock(&mtxSalida);
        }
        pthread_cond_broadcast(&t.cond);
        while (!t.detener && !t.fallo && jugador.pareja < 0 &&
               !(t.llegados == t.jugadores && t.terminadas == t.parejas.size()))
            pthread_cond_wait(&t.cond, &t.mtx);
        if (jugador.pareja < 0) {
            pthread_mutex_lock(&mtxSalida);
            std::cout << "[JUGADOR " << a.id << "] "
                      << (t.detener || t.fallo ? "cierre del torneo; sin pareja.\n" : "sin pareja.\n");
            pthread_mutex_unlock(&mtxSalida);
            pthread_mutex_unlock(&t.mtx);
            return nullptr;
        }
        const int pareja = jugador.pareja;
        // La demostracion ocurre desde el jugador, una vez asignada su mesa.
        if (a.id == t.jugadorSimulado) {
            while (!t.detener && !t.fallo && !t.parejas[pareja].terminada &&
                   !t.parejas[pareja].tablero)
                pthread_cond_wait(&t.cond, &t.mtx);
            timespec limite{};
            clock_gettime(CLOCK_REALTIME, &limite);
            limite.tv_nsec += 200000000; // 200 ms de partida, sin bloquear otras mesas
            if (limite.tv_nsec >= 1000000000) {
                ++limite.tv_sec;
                limite.tv_nsec -= 1000000000;
            }
            int espera = 0;
            while (!t.detener && !t.fallo && !t.parejas[pareja].terminada && espera == 0)
                espera = pthread_cond_timedwait(&t.cond, &t.mtx, &limite);
            const bool notificar = !t.detener && !t.fallo && !t.parejas[pareja].terminada;
            pthread_mutex_unlock(&t.mtx);
            if (notificar) notificarAbandono(t, a.id, t.simularFallo || espera != ETIMEDOUT);
            pthread_mutex_lock(&t.mtx);
        }
        while (!t.detener && !t.fallo && !t.parejas[pareja].terminada)
            pthread_cond_wait(&t.cond, &t.mtx);
        if (t.parejas[pareja].terminada) {
            pthread_mutex_lock(&mtxSalida);
            std::cout << "[JUGADOR " << a.id << "] PARTIDA FINALIZADA [+] RESULTADO: "
                      << t.parejas[pareja].resultado << " | cierre: "
                      << nombreCierre(t.parejas[pareja].cierre) << '\n';
            pthread_mutex_unlock(&mtxSalida);
        }
    }
    if ((t.detener || t.fallo) &&
        (jugador.pareja < 0 || !t.parejas[jugador.pareja].terminada)) {
        pthread_mutex_lock(&mtxSalida);
        std::cout << "[JUGADOR " << a.id << "] cierre del torneo; sin resultado de partida.\n";
        pthread_mutex_unlock(&mtxSalida);
    }
    pthread_mutex_unlock(&t.mtx);
    return nullptr;
}

void* hiloMesa(void* p) {
    const ArgMesa& a = *static_cast<ArgMesa*>(p);
    Torneo& t = *a.torneo;
    const Config& cfg = *t.config; // compartida como solo lectura
    const ControlParada control{&t.mtx, &t.cond, &t.detener, &t.fallo};
    pthread_mutex_lock(&t.mtx);
    while (!t.detener && !t.fallo && t.terminadas < t.parejas.size()) {
        int elegida = -1;
        for (std::size_t i = 0; i < t.parejas.size(); ++i) {
            const Pareja& pareja = t.parejas[i];
            if (pareja.perros != 0 && !pareja.jugando && !pareja.terminada) {
                elegida = static_cast<int>(i);
                break;
            }
        }
        if (elegida < 0) {
            pthread_cond_wait(&t.cond, &t.mtx);
            continue;
        }
        t.parejas[elegida].jugando = true;
        pthread_cond_broadcast(&t.cond);
        pthread_mutex_lock(&mtxSalida);
        std::cout << "[MESA " << a.id << "] INICIO PAREJA " << elegida + 1 << '\n';
        pthread_mutex_unlock(&mtxSalida);
        pthread_mutex_unlock(&t.mtx);

        const ControlAbandono abandono{&t.mtx, &t.cond, &t.parejas[elegida].tablero};
        CierrePartida cierre;
        const Ganador resultado = jugarPartida(cfg, a.id, &control, &abandono, &cierre);

        pthread_mutex_lock(&t.mtx);
        Pareja& pareja = t.parejas[elegida];
        pareja.resultado = resultado;
        pareja.cierre = cierre;
        pareja.terminada = true;
        pareja.jugando = false;
        ++t.terminadas;
        pthread_mutex_lock(&mtxSalida);
        std::cout << "[MESA " << a.id << "] LIBERADA. PAREJA " << elegida + 1
                  << " terminada. Resultado: " << resultado << " | cierre: "
                  << nombreCierre(cierre) << '\n';
        pthread_mutex_unlock(&mtxSalida);
        pthread_cond_broadcast(&t.cond);
    }
    pthread_mutex_unlock(&t.mtx);
    return nullptr;
}

int main(int argc, char* argv[]) { // VALIDAR CANTIDAD DE ARGUMENTOS 
    if (argc < 3 || argc > 4) {
        pthread_mutex_lock(&mtxSalida);
        std::cerr << "ERROR --> USO CORRECTO  --> " << argv[0] << " N M [config.txt]\n";
        pthread_mutex_unlock(&mtxSalida);
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
        pthread_mutex_lock(&mtxSalida);
        std::cerr << "ERROR: N y M deben ser enteros positivos dentro del rango de int, sin texto adicional.\n";
        pthread_mutex_unlock(&mtxSalida);
        return 1;
    }

    if (argc == 4) {
        config_file = argv[3];
    }
    Config cfg;
    if (argc == 4) {
        std::string errorConfig;
        if (!leerConfig(config_file, cfg, errorConfig)) {
            pthread_mutex_lock(&mtxSalida);
            std::cerr << "ERROR: " << errorConfig << '\n';
            pthread_mutex_unlock(&mtxSalida);
            return 1;
        }
    }

    pthread_mutex_lock(&mtxSalida);
    std::cout << "--- SERVIDOR INICIADO ---\n";
    pthread_mutex_unlock(&mtxSalida);
    pthread_mutex_lock(&mtxSalida);
    std::cout << "JUGADORES: " << N << " [-] MESAS DISPONIBLES: " << M << "\n";
    pthread_mutex_unlock(&mtxSalida);

    Torneo torneo;
    torneo.jugadores = N;
    torneo.config = &cfg;
    const char* abandonoSimulado = std::getenv("KOMIKAN_ABANDONA");
    const char* falloSimulado = std::getenv("KOMIKAN_FALLO_JUGADOR");
    const char* simulacion = abandonoSimulado ? abandonoSimulado : falloSimulado;
    if ((abandonoSimulado && falloSimulado) ||
        (simulacion && (!enteroPositivo(simulacion, torneo.jugadorSimulado) ||
                        torneo.jugadorSimulado > N))) {
        std::cerr << "ERROR: use solo KOMIKAN_ABANDONA o KOMIKAN_FALLO_JUGADOR con un ID entre 1 y N.\n";
        return 1;
    }
    torneo.simularFallo = falloSimulado != nullptr;
    const int cantidadMesas = std::min(M, N / 2);
    std::vector<ArgJugador> args;
    std::vector<pthread_t> hilos;
    std::vector<ArgMesa> argsMesas;
    std::vector<pthread_t> mesas;
    try { // RESERVAR ANTES DE CREAR HILOS MANTIENE ESTABLES SUS ARGUMENTOS
        torneo.parejas.resize(N / 2);
        torneo.participantes.resize(N);
        args.resize(N);
        hilos.resize(N);
        argsMesas.resize(cantidadMesas);
        mesas.resize(cantidadMesas);
    } catch (const std::exception& e) {
        pthread_mutex_lock(&mtxSalida);
        std::cerr << "ERROR: no se pudieron reservar los jugadores: " << e.what() << '\n';
        pthread_mutex_unlock(&mtxSalida);
        return 1;
    }
    int error = pthread_mutex_init(&torneo.mtx, nullptr);
    if (error != 0) {
        pthread_mutex_lock(&mtxSalida);
        std::cerr << "ERROR: pthread_mutex_init: " << std::strerror(error) << '\n';
        pthread_mutex_unlock(&mtxSalida);
        return 1;
    }
    error = pthread_cond_init(&torneo.cond, nullptr);
    if (error != 0) {
        pthread_mutex_lock(&mtxSalida);
        std::cerr << "ERROR: pthread_cond_init: " << std::strerror(error) << '\n';
        pthread_mutex_unlock(&mtxSalida);
        pthread_mutex_destroy(&torneo.mtx);
        return 1;
    }
    sigset_t conjunto, mascaraAnterior;
    sigemptyset(&conjunto);
    sigaddset(&conjunto, SIGINT);
    error = pthread_sigmask(SIG_BLOCK, &conjunto, &mascaraAnterior);
    if (error != 0) {
        std::cerr << "ERROR: pthread_sigmask: " << std::strerror(error) << '\n';
        pthread_cond_destroy(&torneo.cond);
        pthread_mutex_destroy(&torneo.mtx);
        return 1;
    }
    ArgSenales argSenales{&torneo, conjunto};
    pthread_t receptor;
    error = pthread_create(&receptor, nullptr, hiloSenales, &argSenales);
    if (error != 0) {
        std::cerr << "ERROR: pthread_create del receptor: " << std::strerror(error) << '\n';
        pthread_cond_destroy(&torneo.cond);
        pthread_mutex_destroy(&torneo.mtx);
        const int restauracion = restaurarMascara(mascaraAnterior);
        if (restauracion != 0)
            std::cerr << "ERROR: restaurar mascara: " << std::strerror(restauracion) << '\n';
        return 1;
    }
    int mesasCreadas = 0;
    for (int i = 0; i < cantidadMesas; ++i) {
        argsMesas[i] = ArgMesa{&torneo, i + 1};
        pthread_mutex_lock(&torneo.mtx);
        if (torneo.detener || torneo.fallo) {
            pthread_mutex_unlock(&torneo.mtx);
            break;
        }
        error = pthread_create(&mesas[i], nullptr, hiloMesa, &argsMesas[i]);
        if (error != 0) {
            torneo.fallo = true;
            torneo.detener = true;
            pthread_mutex_lock(&mtxSalida);
            std::cerr << "ERROR: pthread_create de la mesa " << i + 1 << ": "
                      << std::strerror(error) << '\n';
            pthread_mutex_unlock(&mtxSalida);
            pthread_cond_broadcast(&torneo.cond);
            pthread_mutex_unlock(&torneo.mtx);
            break;
        }
        pthread_mutex_unlock(&torneo.mtx);
        ++mesasCreadas;
    }
    int creados = 0;
    for (int i = 0; i < N; ++i) {
        args[i] = ArgJugador{&torneo, i + 1};
        pthread_mutex_lock(&torneo.mtx);
        if (torneo.detener || torneo.fallo) {
            pthread_mutex_unlock(&torneo.mtx);
            break;
        }
        error = pthread_create(&hilos[i], nullptr, hiloJugador, &args[i]);
        if (error != 0) {
            torneo.fallo = true;
            torneo.detener = true;
            pthread_mutex_lock(&mtxSalida);
            std::cerr << "ERROR: pthread_create del jugador " << i + 1 << ": "
                      << std::strerror(error) << '\n';
            pthread_mutex_unlock(&mtxSalida);
            pthread_cond_broadcast(&torneo.cond);
            pthread_mutex_unlock(&torneo.mtx);
            break;
        }
        pthread_mutex_unlock(&torneo.mtx);
        ++creados;
    }
    bool falloJoin = false;
    for (int i = 0; i < creados; ++i) {
        error = pthread_join(hilos[i], nullptr);
        if (error != 0) {
            pthread_mutex_lock(&mtxSalida);
            std::cerr << "ERROR: pthread_join del jugador " << i + 1 << ": "
                      << std::strerror(error) << '\n';
            pthread_mutex_unlock(&mtxSalida);
            falloJoin = true;
            const ControlParada control{&torneo.mtx, &torneo.cond, &torneo.detener, &torneo.fallo};
            notificarFallo(&control);
        }
    }
    for (int i = 0; i < mesasCreadas; ++i) {
        error = pthread_join(mesas[i], nullptr);
        if (error != 0) {
            pthread_mutex_lock(&mtxSalida);
            std::cerr << "ERROR: pthread_join de la mesa " << i + 1 << ": "
                      << std::strerror(error) << '\n';
            pthread_mutex_unlock(&mtxSalida);
            falloJoin = true;
            const ControlParada control{&torneo.mtx, &torneo.cond, &torneo.detener, &torneo.fallo};
            notificarFallo(&control);
        }
    }
    pthread_mutex_lock(&torneo.mtx);
    torneo.cerrarReceptor = true;
    const bool receptorTerminado = torneo.receptorTerminado;
    pthread_mutex_unlock(&torneo.mtx);
    if (!receptorTerminado) {
        // Solo despierta sigwait: cerrarReceptor impide registrar una falsa interrupcion.
        error = pthread_kill(receptor, SIGINT);
        if (error != 0 && error != ESRCH) {
            pthread_mutex_lock(&mtxSalida);
            std::cerr << "ERROR: despertar receptor: " << std::strerror(error) << std::endl;
            pthread_mutex_unlock(&mtxSalida);
            std::_Exit(EXIT_FAILURE); // no liberar memoria que el receptor podria seguir usando
        }
    }
    error = pthread_join(receptor, nullptr);
    if (error != 0) {
        pthread_mutex_lock(&mtxSalida);
        std::cerr << "ERROR: pthread_join del receptor: " << std::strerror(error) << std::endl;
        pthread_mutex_unlock(&mtxSalida);
        falloJoin = true;
    }
    if (falloJoin) std::_Exit(EXIT_FAILURE);
    pthread_mutex_lock(&torneo.mtx);
    const bool fallo = torneo.fallo;
    const bool interrumpido = torneo.sigintRecibido;
    const std::size_t terminadas = torneo.terminadas;
    pthread_mutex_unlock(&torneo.mtx);
    pthread_cond_destroy(&torneo.cond);
    pthread_mutex_destroy(&torneo.mtx);
    const int restauracion = restaurarMascara(mascaraAnterior);
    pthread_mutex_lock(&mtxSalida);
    if (restauracion != 0)
        std::cerr << "ERROR: restaurar mascara: " << std::strerror(restauracion) << '\n';
    std::cout << "[TORNEO] cierre " << (fallo ? "por fallo interno" :
                  (interrumpido ? "por SIGINT" : "normal"))
              << ". Partidas cerradas: " << terminadas << " / " << N / 2 << std::endl;
    pthread_mutex_unlock(&mtxSalida);
    pthread_mutex_destroy(&mtxSalida); // ya no queda ningun hilo de trabajo ni receptor
    return fallo || restauracion != 0 ? 1 : (interrumpido ? 130 : 0);
}
