#ifndef MESA_HPP
#define MESA_HPP

#include <unistd.h>
#include <cstdlib>
#include <vector>
#include <array>
#include <csignal>
#include "tablero.hpp"

struct ArgFicha { // datos que recibe cada hilo de ficha
    Tablero* t;
    int pos;          // casilla actual de esta ficha
    Celda tipo;
    unsigned semilla; // cada hilo tiene la suya
    int minMs, maxMs; // rango del retardo entre movimientos
};

inline void* hiloPerro(void* p) { // ciclo de decision de un perro
    ArgFicha* a = static_cast<ArgFicha*>(p);
    Tablero& t = *a->t;
    while (!partidaTerminada(t)) {
        usleep((a->minMs + rand_r(&a->semilla) % (a->maxMs - a->minMs + 1)) * 1000);
        if (partidaTerminada(t)) break; // no mover despues de que termino
        const std::vector<int>& vecinos = t.ady[a->pos]; // solo lectura, no cambia tras iniciar
        int destino = vecinos[rand_r(&a->semilla) % vecinos.size()];
        if (moverFicha(t, a->pos, destino, a->tipo)) a->pos = destino; // si pierde la carrera, reintenta
    }
    return nullptr;
}


inline void* hiloPuma(void* p) { // ciclo de decision del puma: captura si puede, si no se mueve
    ArgFicha* a = static_cast<ArgFicha*>(p);
    Tablero& t = *a->t;
    std::vector<std::array<int,3>> saltos = todosLosSaltos(); // se calcula una sola vez
    while (!partidaTerminada(t)) {
        usleep((a->minMs + rand_r(&a->semilla) % (a->maxMs - a->minMs + 1)) * 1000);
        if (partidaTerminada(t)) break;
        bool hizo = false;
        for (const auto& s : saltos) { // primero intenta capturar
            if (s[0] != a->pos) continue;
            if (capturarPerro(t, s[0], s[1], s[2])) { a->pos = s[2]; hizo = true; break; }
        }
        if (!hizo) { // si no pudo capturar, se mueve a un vecino
            const std::vector<int>& vecinos = t.ady[a->pos];
            int destino = vecinos[rand_r(&a->semilla) % vecinos.size()];
            if (moverFicha(t, a->pos, destino, PUMA)) a->pos = destino;
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
};

inline void colocarFichasIniciales(Tablero& t) { // posicion por defecto, el config puede cambiarla
    t.celdas[TRONO] = PUMA;
    t.celdas[nodo(2, 0)] = PERRO;
    t.celdas[nodo(2, 4)] = PERRO;
    for (int c = 0; c < COLS; c++) {
        t.celdas[nodo(3, c)] = PERRO;
        t.celdas[nodo(4, c)] = PERRO;
    }
}

inline Ganador jugarPartida(const Config& cfg, int idMesa, const volatile sig_atomic_t* parar = nullptr) { // quien la llama hace de Juez
    Tablero t;
    iniciarTablero(t);
    colocarFichasIniciales(t);

    ArgFicha args[13]; // 0 = puma, 1 a 12 = perros
    pthread_t th[13];
    int k = 0;         // cuantos perros hay
    args[0] = ArgFicha{&t, TRONO, PUMA, cfg.semilla, cfg.minMs, cfg.maxMs};
    for (int i = 0; i < NODOS; i++) {
        if (t.celdas[i] != PERRO) continue;
        k++;
        args[k] = ArgFicha{&t, i, PERRO, cfg.semilla + (unsigned)k, cfg.minMs, cfg.maxMs};
    }

    int creados = 0;
    bool fallo = false;
    for (int i = 0; i <= k && !fallo; i++) { // un hilo por ficha
        void* (*f)(void*) = (i == 0) ? hiloPuma : hiloPerro;
        if (pthread_create(&th[i], nullptr, f, &args[i]) != 0) fallo = true;
        else creados++;
    }

    int cadaCiclos = (cfg.logCadaMs >= 100) ? cfg.logCadaMs / 100 : 1; // el juez revisa cada 100 ms
    Ganador g = NADIE;
    int ciclos = 0;
    while (!fallo && g == NADIE && ciclos < cfg.maxSegundos * 10) {
        usleep(100 * 1000);
        ciclos++;
        if (parar && *parar) break; // llego Ctrl+C
        g = ganador(t);
        if (ciclos % cadaCiclos == 0) { // log del estado
            std::cout << "[MESA " << idMesa << "] estado:";
            imprimirTablero(t);
        }
    }

    terminarPartida(t); // avisa a todos los hilos que paren
    for (int i = 0; i < creados; i++) pthread_join(th[i], nullptr);
    std::cout << "[MESA " << idMesa << "] fin. Ganador: "
              << (g == GANA_PUMA ? "PUMA" : (g == GANAN_PERROS ? "PERROS" : "NADIE"))
              << " | capturados: " << t.capturados << std::endl;
    destruirTablero(t);
    return g;
}

#endif
