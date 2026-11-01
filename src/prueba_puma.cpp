#include <iostream>
#include "mesa.hpp"

int main() {
    Tablero t;
    iniciarTablero(t);
    t.celdas[nodo(2, 2)] = PUMA;
    int posPerros[3] = {nodo(2, 3), nodo(3, 2), nodo(1, 1)};
    ArgFicha puma{&t, nodo(2, 2), PUMA, 1, 50, 300};
    ArgFicha perros[3];
    pthread_t th[4];
    for (int i = 0; i < 3; i++) {
        t.celdas[posPerros[i]] = PERRO;
        perros[i] = ArgFicha{&t, posPerros[i], PERRO, (unsigned)(10 + i), 50, 300};
    }
    pthread_create(&th[0], nullptr, hiloPuma, &puma);
    for (int i = 0; i < 3; i++) pthread_create(&th[i + 1], nullptr, hiloPerro, &perros[i]);
    sleep(3);
    terminarPartida(t);
    for (int i = 0; i < 4; i++) pthread_join(th[i], nullptr);
    int pumas = 0, vivos = 0;
    for (int i = 0; i < NODOS; i++) {
        if (t.celdas[i] == PUMA) pumas++;
        if (t.celdas[i] == PERRO) vivos++;
    }
    std::cout << "pumas: " << pumas << " (esperado 1)" << std::endl;
    std::cout << "perros + capturados: " << vivos + t.capturados << " (esperado 3)" << std::endl;
    destruirTablero(t);
    return 0;
}
