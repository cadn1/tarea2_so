#include <iostream>
#include "tablero.hpp"

int main() {
    Tablero t;
    iniciarTablero(t);
    t.celdas[nodo(2, 2)] = PERRO;
    std::cout << "mover a casilla adyacente: " << moverFicha(t, nodo(2, 2), nodo(2, 3), PERRO) << std::endl;
    std::cout << "mover lejos (no adyacente): " << moverFicha(t, nodo(2, 3), nodo(4, 4), PERRO) << std::endl;
    destruirTablero(t);
    return 0;
}
