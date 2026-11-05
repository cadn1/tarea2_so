#include <iostream>
#include "mesa.hpp"

int main() {
    Tablero t;
    if (iniciarTablero(t) != 0) std::exit(EXIT_FAILURE);
    t.celdas[nodo(2, 2)] = PERRO;
    t.idPerro[nodo(2, 2)] = 0;
    ArgFicha a{&t, nodo(2, 2), PERRO, 42, 0, 50, 300};
    imprimirTablero(t); // ANTES
    pthread_t th;
    pthread_create(&th, nullptr, hiloPerro, &a);
    sleep(2);
    terminarPartida(t);
    pthread_join(th, nullptr);
    imprimirTablero(t); // DESPUES
    int perros = 0;
    for (int i = 0; i < NODOS; i++) if (t.celdas[i] == PERRO) perros++;
    std::cout << "PERROS EN EL TABLERO: " << perros << " (esperado 1)" << std::endl;
    std::cout << "CASILLA FINAL COINCIDE " << (t.celdas[a.pos] == PERRO) << " (ESPERADO 1)" << std::endl;
    std::cout << "EL PERRO SE MOVIO: " << (a.pos != nodo(2, 2)) << " (ESPERADO 1)" << std::endl;
    destruirTablero(t);
    return 0;
}
