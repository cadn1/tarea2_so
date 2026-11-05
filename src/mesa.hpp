#ifndef MESA_HPP
#define MESA_HPP

#include <unistd.h>
#include <cstdlib>
#include <vector>
#include <array>
#include <cstring>
#include <cerrno>
#include <ctime>
#include "tablero.hpp"

// C++17: una sola instancia compartida por todas las mesas y unidades de traduccion.
inline pthread_mutex_t mtxSalida = PTHREAD_MUTEX_INITIALIZER;

enum CausaParada { SIN_PARADA, PARADA_SIGINT, PARADA_FALLO };
enum CierrePartida { CIERRE_VICTORIA, CIERRE_ABANDONO, CIERRE_SIGINT, CIERRE_TIEMPO, CIERRE_FALLO };

inline const char* nombreCierre(CierrePartida cierre) {
    switch (cierre) {
        case CIERRE_VICTORIA: return "VICTORIA";
        case CIERRE_ABANDONO: return "ABANDONO";
        case CIERRE_SIGINT: return "SIGINT";
        case CIERRE_TIEMPO: return "TIEMPO";
        case CIERRE_FALLO: return "FALLO INTERNO";
    }
    return "FALLO INTERNO";
}

struct ControlAbandono {
    pthread_mutex_t* mtx;
    pthread_cond_t* cond;
    Tablero** tablero; // publicado solo mientras el tablero existe; protegido por mtx
};

// Conserva el mismo plazo ante despertares espurios. La condicion libera mtx
// mientras duerme; fin se comprueba bajo ese mutex para no perder despertares.
inline bool esperarRetardo(Tablero& t, int ms) {
    pthread_mutex_lock(&t.mtx);
    int error = 0;
    if (!t.fin) {
        timespec limite{};
        if (clock_gettime(CLOCK_REALTIME, &limite) != 0) error = errno;
        else {
            limite.tv_sec += ms / 1000;
            limite.tv_nsec += (ms % 1000) * 1000000L;
            if (limite.tv_nsec >= 1000000000L) {
                ++limite.tv_sec;
                limite.tv_nsec -= 1000000000L;
            }
            while (!t.fin && error == 0)
                error = pthread_cond_timedwait(&t.cambio, &t.mtx, &limite);
        }
        if (error != 0 && error != ETIMEDOUT) {
            t.errorEspera = error;
            terminarBajoMutex(t);
        }
    }
    const bool activa = !t.fin;
    pthread_mutex_unlock(&t.mtx);
    return activa;
}

struct ControlParada {
    pthread_mutex_t* mtx;
    pthread_cond_t* cond;
    bool* detener;
    bool* fallo;
};

// Consultar sin tener adquirido el mutex del tablero ni el de salida.
inline CausaParada consultarParada(const ControlParada* control) {
    if (!control) return SIN_PARADA;
    pthread_mutex_lock(control->mtx);
    const CausaParada causa = *control->fallo ? PARADA_FALLO :
                             (*control->detener ? PARADA_SIGINT : SIN_PARADA);
    pthread_mutex_unlock(control->mtx);
    return causa;
}

inline void notificarFallo(const ControlParada* control) {
    if (!control) return;
    pthread_mutex_lock(control->mtx);
    *control->fallo = true;
    *control->detener = true;
    pthread_cond_broadcast(control->cond);
    pthread_mutex_unlock(control->mtx);
}

struct ArgFicha { // datos que recibe cada hilo de ficha
    Tablero* t;
    int pos;          // casilla actual de esta ficha
    Celda tipo;
    unsigned semilla; // cada hilo tiene la suya
    int idPerro;      // fijo durante la partida; -1 para el puma
    int minMs, maxMs; // rango del retardo entre movimientos
};

inline void* hiloPerro(void* p) { // ciclo de decision de un perro
    ArgFicha* a = static_cast<ArgFicha*>(p);
    Tablero& t = *a->t;
    while (!partidaTerminada(t) && perroEnPosicion(t, a->pos, a->idPerro)) {
        if (!esperarRetardo(t, a->minMs + rand_r(&a->semilla) % (a->maxMs - a->minMs + 1))) break;
        if (partidaTerminada(t) || !perroEnPosicion(t, a->pos, a->idPerro)) break; // capturado o fin
        const std::vector<int>& vecinos = t.ady[a->pos]; // solo lectura, no cambia tras iniciar
        int destino = vecinos[rand_r(&a->semilla) % vecinos.size()];
        if (moverFicha(t, a->pos, destino, a->tipo, a->idPerro)) a->pos = destino; // si pierde la carrera, reintenta
    }
    return nullptr;
}


inline void* hiloPuma(void* p) { // ciclo de decision del puma: captura si puede, si no se mueve
    ArgFicha* a = static_cast<ArgFicha*>(p);
    Tablero& t = *a->t;
    std::vector<std::array<int,3>> saltos = todosLosSaltos(); // se calcula una sola vez
    while (!partidaTerminada(t)) {
        if (!esperarRetardo(t, a->minMs + rand_r(&a->semilla) % (a->maxMs - a->minMs + 1))) break;
        if (partidaTerminada(t)) break;
        bool hizo = false;
        for (const auto& s : saltos) { // primero intenta capturar
            if (s[0] != a->pos) continue;
            if (capturarPerro(t, s[0], s[1], s[2])) { a->pos = s[2]; hizo = true; break; }
        }
        if (!hizo) { // si no pudo capturar, se mueve a un vecino
            const std::vector<int>& vecinos = t.ady[a->pos];
            int destino = vecinos[rand_r(&a->semilla) % vecinos.size()];
            if (moverFicha(t, a->pos, destino, PUMA, -1)) a->pos = destino;
        }
    }
    return nullptr;
}


struct Config { // parametros de la partida, los llenara el parseo del config.txt
    int minMs = 50;        // retardo minimo entre movimientos
    int maxMs = 300;       // retardo maximo
    unsigned semilla = 1;  // para repetir una partida
    int maxSegundos = 60;  // tope de duracion, evita partidas eternas
    int logCadaMs = 2000;  // cada cuanto se imprime el tablero
    int puma = TRONO;
    // El indice en este arreglo es el ID: conserva el orden de colocacion actual.
    std::array<int, 12> perros{{13, 17, 18, 23, 19, 24, 20, 25, 21, 26, 22, 27}};
};

inline void colocarFichasIniciales(Tablero& t, const Config& cfg = Config{}) {
    t.celdas[cfg.puma] = PUMA;
    for (std::size_t id = 0; id < cfg.perros.size(); ++id) {
        t.celdas[cfg.perros[id]] = PERRO;
        t.idPerro[cfg.perros[id]] = static_cast<int>(id);
    }
}

inline Ganador jugarPartida(const Config& cfg, int idMesa, const ControlParada* parar = nullptr,
                           const ControlAbandono* abandono = nullptr,
                           CierrePartida* cierreSalida = nullptr) { // quien la llama hace de Juez
    Tablero t;
    const int errorInicio = iniciarTablero(t);
    if (errorInicio != 0) {
        if (cierreSalida) *cierreSalida = CIERRE_FALLO;
        notificarFallo(parar);
        pthread_mutex_lock(&mtxSalida);
        std::cerr << "[MESA " << idMesa << "] ERROR: pthread_mutex_init del tablero: "
                  << std::strerror(errorInicio) << '\n';
        pthread_mutex_unlock(&mtxSalida);
        return NADIE; // sin publicar t, crear fichas ni destruir su mutex fallido
    }
    colocarFichasIniciales(t, cfg);
    pthread_mutex_lock(&mtxSalida);
    std::cout << "[MESA " << idMesa << "] tablero inicial:";
    imprimirTablero(t);
    pthread_mutex_unlock(&mtxSalida);
    Ganador g = ganador(t);
    if (g != NADIE) terminarPartida(t); // victoria inicial, antes de crear fichas

    ArgFicha args[13]; // 0 = puma, 1 a 12 = perros
    pthread_t th[13];
    int k = 0;         // cuantos perros hay
    args[0] = ArgFicha{&t, cfg.puma, PUMA, cfg.semilla, -1, cfg.minMs, cfg.maxMs};
    for (int i = 0; i < NODOS; i++) {
        if (t.celdas[i] != PERRO) continue;
        k++;
        args[k] = ArgFicha{&t, i, PERRO, cfg.semilla + (unsigned)k, t.idPerro[i], cfg.minMs, cfg.maxMs};
    }

    // Publicar despues de preparar los argumentos: desde aqui un jugador
    // puede cerrar el tablero. Nunca se adquiere el torneo desde su mutex.
    if (abandono) {
        pthread_mutex_lock(abandono->mtx);
        *abandono->tablero = &t;
        pthread_cond_broadcast(abandono->cond);
        pthread_mutex_unlock(abandono->mtx);
    }

    int creados = 0;
    bool fallo = false;
    CausaParada causa = consultarParada(parar);
    for (int i = 0; i <= k && !fallo && g == NADIE; i++) { // un hilo por ficha en partida activa
        causa = consultarParada(parar);
        if (causa != SIN_PARADA || partidaTerminada(t)) break;
        void* (*f)(void*) = (i == 0) ? hiloPuma : hiloPerro;
        const int error = pthread_create(&th[i], nullptr, f, &args[i]);
        if (error != 0) {
            fallo = true;
            notificarFallo(parar);
            pthread_mutex_lock(&mtxSalida);
            std::cerr << "[MESA " << idMesa << "] ERROR: pthread_create de ficha " << i
                      << ": " << std::strerror(error) << '\n';
            pthread_mutex_unlock(&mtxSalida);
        } else creados++;
    }

    int cadaCiclos = (cfg.logCadaMs >= 100) ? cfg.logCadaMs / 100 : 1; // el juez revisa cada 100 ms
    int ciclos = 0;
    while (!fallo && causa == SIN_PARADA && g == NADIE && ciclos < cfg.maxSegundos * 10) {
        usleep(100 * 1000);
        ciclos++;
        causa = consultarParada(parar);
        if (causa != SIN_PARADA) break;
        g = ganador(t);
        if (partidaTerminada(t)) break;
        if (ciclos % cadaCiclos == 0) { // log del estado
            pthread_mutex_lock(&mtxSalida); // salida antes del mutex interno del tablero
            std::cout << "[MESA " << idMesa << "] estado:";
            imprimirTablero(t);
            pthread_mutex_unlock(&mtxSalida);
        }
    }

    terminarPartida(t); // avisa a todos los hilos que paren
    if (abandono) {
        pthread_mutex_lock(abandono->mtx);
        *abandono->tablero = nullptr; // rechazar solicitudes antes de joins/destruccion
        pthread_mutex_unlock(abandono->mtx);
    }
    bool falloJoin = false;
    for (int i = 0; i < creados; i++) {
        const int error = pthread_join(th[i], nullptr);
        if (error != 0) {
            falloJoin = true;
            notificarFallo(parar);
            pthread_mutex_lock(&mtxSalida);
            std::cerr << "[MESA " << idMesa << "] ERROR: pthread_join de ficha " << i
                      << ": " << std::strerror(error) << '\n';
            pthread_mutex_unlock(&mtxSalida);
        }
    }
    // Si no se confirmo la salida, no liberar el tablero ni los argumentos de esas fichas.
    if (falloJoin) std::_Exit(EXIT_FAILURE);
    const Celda abandona = t.abandona; // todas las fichas ya estan unidas
    if (t.errorEspera != 0) {
        fallo = true;
        notificarFallo(parar);
        pthread_mutex_lock(&mtxSalida);
        std::cerr << "[MESA " << idMesa << "] ERROR: espera de ficha: "
                  << std::strerror(t.errorEspera) << '\n';
        pthread_mutex_unlock(&mtxSalida);
    }
    g = ganador(t); // conservar una victoria real ocurrida justo antes del cierre
    const CierrePartida cierre = fallo || causa == PARADA_FALLO ? CIERRE_FALLO :
        (abandona != VACIA ? CIERRE_ABANDONO : (g != NADIE ? CIERRE_VICTORIA :
        (causa == PARADA_SIGINT ? CIERRE_SIGINT : CIERRE_TIEMPO)));
    if (cierre == CIERRE_ABANDONO) g = abandona == PUMA ? GANAN_PERROS : GANA_PUMA;
    if (cierreSalida) *cierreSalida = cierre;
    pthread_mutex_lock(&mtxSalida);
    std::cout << "[MESA " << idMesa << "] tablero final:";
    imprimirTablero(t);
    int vivos = 0;
    for (int i = 0; i < NODOS; i++)
        if (t.celdas[i] == PERRO) vivos++;
    std::cout << "[MESA " << idMesa << "] fin. Ganador: "
              << (g == GANA_PUMA ? "PUMA" : (g == GANAN_PERROS ? "PERROS" : "NADIE"))
              << " | vivos: " << vivos << " | capturados: " << t.capturados
              << " | total: " << vivos + t.capturados
              << " | cierre: " << nombreCierre(cierre)
              << std::endl;
    pthread_mutex_unlock(&mtxSalida);
    destruirTablero(t);
    return g;
}

#endif
