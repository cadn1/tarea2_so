#include <iostream>
#include "tablero.hpp"

int main() {
    Tablero t;
    iniciarTablero(t);
    t.celdas[nodo(2, 2)] = PUMA;
    t.celdas[nodo(2, 3)] = PERRO;
    int aterriza = saltoCuadricula(nodo(2, 2), nodo(2, 3));
    std::cout << "aterriza en: " << aterriza << " (esperado " << nodo(2, 4) << ")" << std::endl;
    std::cout << "captura: " << capturarPerro(t, nodo(2, 2), nodo(2, 3), aterriza) << std::endl;
    std::cout << "capturados: " << t.capturados << std::endl;
    std::cout << "salto fuera del tablero: " << saltoCuadricula(nodo(2, 3), nodo(2, 4)) << std::endl;
    destruirTablero(t);
    return 0;
}
