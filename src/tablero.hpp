#ifndef TABLERO_HPP
#define TABLERO_HPP

#include <vector>
#include <pthread.h>

enum Celda { VACIA, PUMA, PERRO }; // que hay en cada casilla

const int FILAS = 5; // cuadricula principal
const int COLS = 5;
const int NODOS = 28; // 25 de la cuadricula + 2 del techo + 1 trono
const int TRONO = 0;  // nodo 0 = punta del triangulo
const int TECHO_IZQ = 1; // nodos 1 y 2 = los dos del medio del triangulo
const int TECHO_DER = 2;

inline int nodo(int f, int c) { return 3 + f * COLS + c; } // fila/columna -> indice de nodo

struct Tablero {
    Celda celdas[NODOS];          // estado de cada nodo
    std::vector<int> ady[NODOS];  // vecinos de cada nodo
    int capturados;               // perros capturados por el puma
    pthread_mutex_t mtx;          // un solo candado para todo el tablero
};

inline void unir(Tablero& t, int a, int b) { // arista en ambos sentidos
    t.ady[a].push_back(b);
    t.ady[b].push_back(a);
}

inline void iniciarTablero(Tablero& t) { // vacia el tablero, arma las aristas y crea el mutex
    for (int i = 0; i < NODOS; i++) {
        t.celdas[i] = VACIA;
        t.ady[i].clear();
    }
    t.capturados = 0;
    for (int f = 0; f < FILAS; f++)
        for (int c = 0; c < COLS; c++) {
            if (c < COLS - 1) unir(t, nodo(f, c), nodo(f, c + 1));  // horizontal
            if (f < FILAS - 1) unir(t, nodo(f, c), nodo(f + 1, c)); // vertical
        }
    unir(t, TRONO, TECHO_IZQ);        // lados del triangulo
    unir(t, TRONO, TECHO_DER);
    unir(t, TECHO_IZQ, nodo(0, 0));   // techo izq: esquina y columna 1
    unir(t, TECHO_IZQ, nodo(0, 1));
    unir(t, TECHO_DER, nodo(0, 3));   // techo der: columna 3 y esquina
    unir(t, TECHO_DER, nodo(0, 4));
    unir(t, TRONO, nodo(0, 2));       // vertical del trono al centro
    unir(t, TECHO_IZQ, TECHO_DER);    // barra entre los dos del techo
    pthread_mutex_init(&t.mtx, nullptr);
}

inline void destruirTablero(Tablero& t) { // libera el mutex al cerrar la mesa
    pthread_mutex_destroy(&t.mtx);
}

#endif
