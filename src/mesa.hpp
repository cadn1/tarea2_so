#ifndef MESA_HPP
#define MESA_HPP

#include <unistd.h>
#include <cstdlib>
#include <vector>
#include <array>
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

#endif
