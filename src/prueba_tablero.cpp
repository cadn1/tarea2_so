#include <iostream>
#include "tablero.hpp"

int main() {
    Tablero t;
    iniciarTablero(t);
    int suma = 0;
    for (int i = 0; i < NODOS; i++) suma += t.ady[i].size();
    std::cout << "Nodos: " << NODOS << ", aristas: " << suma / 2 << std::endl;
    destruirTablero(t);
    return 0;
}
