#include <iostream>
#include "tablero.hpp"

int main() {
    Tablero t;
    
    // CASO 1 : PUMA ACORRALADO
    iniciarTablero(t);
    t.celdas[TRONO] = PUMA; 
    t.celdas[TECHO_IZQ] = PERRO;
    t.celdas[TECHO_DER] = PERRO;
    t.celdas[nodo(0, 2)] = PERRO; // Nodo vertical central
    std::cout << "\n[FOTO 1] ACORRALADO EN EL TRONO:";
    imprimirTablero(t);
    std::cout << "Resultado: " << ganador(t) << " (esperado 2)\n";

    // CASO 2 : PUMA CON ESCAPE
    iniciarTablero(t);
    t.celdas[nodo(2, 2)] = PUMA; 
    t.celdas[nodo(2, 1)] = PERRO; 
    t.celdas[nodo(2, 3)] = PERRO; 
    std::cout << "\n[FOTO 2] PUMA EN EL CENTRO CON ESPACIO:";
    imprimirTablero(t);
    std::cout << "Resultado: " << ganador(t) << " (esperado 0)\n";

    // CASO 3 : PUMA GANADOR
    iniciarTablero(t);
    t.celdas[nodo(4, 0)] = PUMA;
    t.capturados = 6;
    std::cout << "\n[FOTO 3] VICTORIA CON 6 CAPTURAS:";
    imprimirTablero(t);
    std::cout << "Resultado: " << ganador(t) << " (esperado 1)\n";

    destruirTablero(t);
    return 0;
}