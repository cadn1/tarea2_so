#include <iostream>
#include "tablero.hpp"

int main() {
    Tablero t;
    iniciarTablero(t);
    std::cout << "saltos: " << todosLosSaltos().size() << " (esperado 60)" << std::endl;

    t.celdas[nodo(4, 4)] = PUMA; // puma en una esquina, rodeado
    t.celdas[nodo(3, 4)] = PERRO;
    t.celdas[nodo(4, 3)] = PERRO;
    t.celdas[nodo(2, 4)] = PERRO; // bloquea el aterrizaje
    t.celdas[nodo(4, 2)] = PERRO;
    std::cout << "acorralado: " << ganador(t) << " (esperado 2)" << std::endl;

    t.celdas[nodo(2, 4)] = VACIA; // ahora puede capturar saltando
    std::cout << "con salida por captura: " << ganador(t) << " (esperado 0)" << std::endl;

    t.capturados = 6;
    std::cout << "6 capturas: " << ganador(t) << " (esperado 1)" << std::endl;

    destruirTablero(t);
    return 0;
}
