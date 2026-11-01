#include <iostream>
#include "mesa.hpp"

int main() {
    Tablero t;
    iniciarTablero(t);
    t.celdas[nodo(2, 2)] = PERRO;
    ArgFicha a{&t, nodo(2, 2), PERRO, 42, 50, 300};
    imprimirTablero(t); // antes
    pthread_t th;
    pthread_create(&th, nullptr, hiloPerro, &a);
    sleep(2);
    terminarPartida(t);
    pthread_join(th, nullptr);
    imprimirTablero(t); // despues
    int perros = 0;
    for (int i = 0; i < NODOS; i++) if (t.celdas[i] == PERRO) perros++;
    std::cout << "perros en el tablero: " << perros << " (esperado 1)" << std::endl;
    std::cout << "casilla final coincide: " << (t.celdas[a.pos] == PERRO) << " (esperado 1)" << std::endl;
    std::cout << "el perro se movio: " << (a.pos != nodo(2, 2)) << " (esperado 1)" << std::endl;
    destruirTablero(t);
    return 0;
}
