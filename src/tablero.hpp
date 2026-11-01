#ifndef TABLERO_HPP
#define TABLERO_HPP

#include <vector>
#include <array>
#include <iostream>
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
    bool fin;                     // la partida termino, los hilos deben parar
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
    t.fin = false;
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


inline bool moverFicha(Tablero& t, int desde, int hacia, Celda tipo) { // intenta mover, true si lo logro
    pthread_mutex_lock(&t.mtx);
    bool ok = false;
    if (t.celdas[desde] == tipo && t.celdas[hacia] == VACIA) { // sigo en mi casilla y el destino esta libre
        for (int v : t.ady[desde]) {
            if (v == hacia) { // el destino es adyacente
                t.celdas[desde] = VACIA;
                t.celdas[hacia] = tipo;
                ok = true;
                break;
            }
        }
    }
    pthread_mutex_unlock(&t.mtx);
    return ok;
}


inline bool capturarPerro(Tablero& t, int desde, int sobre, int hacia) { // salto del puma, true si captura
    pthread_mutex_lock(&t.mtx);
    bool ok = false;
    if (t.celdas[desde] == PUMA && t.celdas[sobre] == PERRO && t.celdas[hacia] == VACIA) {
        t.celdas[desde] = VACIA;  // el puma sale de su casilla
        t.celdas[sobre] = VACIA;  // el perro desaparece
        t.celdas[hacia] = PUMA;   // el puma cae al otro lado
        t.capturados++;
        ok = true;
    }
    pthread_mutex_unlock(&t.mtx);
    return ok;
}

inline int saltoCuadricula(int desde, int sobre) { // nodo de aterrizaje en linea recta, o -1 si no existe
    if (desde < 3 || sobre < 3) return -1; // solo la cuadricula, el techo va aparte
    int f1 = (desde - 3) / COLS, c1 = (desde - 3) % COLS;
    int f2 = (sobre - 3) / COLS, c2 = (sobre - 3) % COLS;
    int f3 = 2 * f2 - f1, c3 = 2 * c2 - c1; // un paso mas en la misma direccion
    if (f3 < 0 || f3 >= FILAS || c3 < 0 || c3 >= COLS) return -1; // se sale del tablero
    return nodo(f3, c3);
}


const bool SALTOS_TECHO = false; // saltos que cruzan el techo

inline std::vector<std::array<int,3>> todosLosSaltos() { // triples (desde, sobre, hacia) en linea recta
    std::vector<std::array<int,3>> s;
    auto agregar = [&](int a, int b, int c) { s.push_back(std::array<int,3>{{a, b, c}}); };
    const int dir[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    for (int f = 0; f < FILAS; f++)
        for (int c = 0; c < COLS; c++)
            for (const auto& d : dir) {
                int f3 = f + 2 * d[0], c3 = c + 2 * d[1];
                if (f3 < 0 || f3 >= FILAS || c3 < 0 || c3 >= COLS) continue; // el aterrizaje se sale
                agregar(nodo(f, c), nodo(f + d[0], c + d[1]), nodo(f3, c3));
            }
    if (SALTOS_TECHO) { // lineas rectas que cruzan el techo, ida y vuelta
        agregar(nodo(0, 0), TECHO_IZQ, TRONO);
        agregar(TRONO, TECHO_IZQ, nodo(0, 0));
        agregar(nodo(0, 4), TECHO_DER, TRONO);
        agregar(TRONO, TECHO_DER, nodo(0, 4));
        agregar(nodo(1, 1), nodo(0, 1), TECHO_IZQ);
        agregar(TECHO_IZQ, nodo(0, 1), nodo(1, 1));
        agregar(nodo(1, 3), nodo(0, 3), TECHO_DER);
        agregar(TECHO_DER, nodo(0, 3), nodo(1, 3));
        agregar(nodo(1, 2), nodo(0, 2), TRONO);
        agregar(TRONO, nodo(0, 2), nodo(1, 2));
    }
    return s;
}

enum Ganador { NADIE, GANA_PUMA, GANAN_PERROS }; // resultado de la partida

inline bool pumaSinMovimientos(const Tablero& t) { // el que llama ya tiene el mutex
    int p = -1;
    for (int i = 0; i < NODOS; i++)
        if (t.celdas[i] == PUMA) p = i;
    if (p < 0) return false; // no hay puma en el tablero
    for (int v : t.ady[p]) // movimiento simple
        if (t.celdas[v] == VACIA) return false;
    for (const auto& s : todosLosSaltos()) // captura legal
        if (s[0] == p && t.celdas[s[1]] == PERRO && t.celdas[s[2]] == VACIA) return false;
    return true;
}

inline Ganador ganador(Tablero& t) { // revisa la victoria bajo el candado
    pthread_mutex_lock(&t.mtx);
    Ganador g = NADIE;
    if (t.capturados >= 6) g = GANA_PUMA;
    else if (pumaSinMovimientos(t)) g = GANAN_PERROS;
    pthread_mutex_unlock(&t.mtx);
    return g;
}

inline const char* obtenerNombre(Celda c) {
    if (c == PUMA) return "Puma "; 
    if (c == PERRO) return "Perro";
    return "  .  "; 
}

inline void imprimirTablero(Tablero& t) {
    pthread_mutex_lock(&t.mtx);
    std::cout << "\nTABLERO INICIAL:\n";
    
    // TRONO
    std::cout << "                        " << obtenerNombre(t.celdas[TRONO]) << "\n";
    std::cout << "                       /  |  \\ \n";
    std::cout << "                    /     |     \\ \n";
    std::cout << "                 /        |        \\ \n";
    
    // FILA DEL TECHO NODOS 1 y 2
    std::cout << "            " << obtenerNombre(t.celdas[TECHO_IZQ]) 
              << "---------+---------" 
              << obtenerNombre(t.celdas[TECHO_DER]) << "\n";
    
    // DIAGONALES
    std::cout << "           /  |           |           |  \\ \n";
    std::cout << "        /     |           |           |     \\ \n";
    std::cout << "     /        |           |           |        \\ \n";
    
    // CUADRICULA 5x5
    for (int f = 0; f < FILAS; f++) {
        // Fila de Nodos
        std::cout << obtenerNombre(t.celdas[nodo(f, 0)]) << "-------" 
                  << obtenerNombre(t.celdas[nodo(f, 1)]) << "-------"
                  << obtenerNombre(t.celdas[nodo(f, 2)]) << "-------"
                  << obtenerNombre(t.celdas[nodo(f, 3)]) << "-------"
                  << obtenerNombre(t.celdas[nodo(f, 4)]) << "\n";
        
        // FILAS DE CONEXIONES
        if (f < FILAS - 1) {
            std::cout << "  |           |           |           |           |\n";
            std::cout << "  |           |           |           |           |\n";
            std::cout << "  |           |           |           |           |\n";
        }
    }
    pthread_mutex_unlock(&t.mtx);
}


inline bool partidaTerminada(Tablero& t) { // los hilos preguntan esto en cada ciclo
    pthread_mutex_lock(&t.mtx);
    bool f = t.fin;
    pthread_mutex_unlock(&t.mtx);
    return f;
}

inline void terminarPartida(Tablero& t) { // el juez avisa que se acabo
    pthread_mutex_lock(&t.mtx);
    t.fin = true;
    pthread_mutex_unlock(&t.mtx);
}

#endif
