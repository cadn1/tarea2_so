#include <iostream>
#include <cstdlib>
#include <string>
#include "tablero.hpp"

int main() {
    int errores = 0;
    int casos = 0;
    auto comprobar = [&](Tablero& t, Ganador esperado, const std::string& nombre) {
        for (int i = 0, id = 0; i < NODOS; ++i)
            t.idPerro[i] = (t.celdas[i] == PERRO) ? id++ : -1;
        const Ganador obtenido = ganador(t);
        ++casos;
        if (obtenido != esperado) {
            std::cerr << "FALLO: " << nombre << " | obtenido: " << obtenido
                      << " | esperado: " << esperado << '\n';
            ++errores;
        } else {
            std::cout << "OK: " << nombre << '\n';
        }
    };

    // Cada escenario inicializa y destruye su propio mutex exactamente una vez.
    {
        Tablero t;
        if (iniciarTablero(t) != 0) std::exit(EXIT_FAILURE);
        t.celdas[TRONO] = PUMA;
        t.celdas[TECHO_IZQ] = PERRO;
        t.celdas[TECHO_DER] = PERRO;
        t.celdas[nodo(0, 2)] = PERRO;
        // Bloquear tambien los tres aterrizajes posibles desde el trono.
        t.celdas[nodo(0, 0)] = PERRO;
        t.celdas[nodo(0, 4)] = PERRO;
        t.celdas[nodo(1, 2)] = PERRO;
        comprobar(t, GANAN_PERROS, "puma sin movimientos ni capturas");

        // Abrir cada aterrizaje por separado: sigue rodeado, pero puede capturar.
        for (int destino : {nodo(0, 0), nodo(0, 4), nodo(1, 2)}) {
            t.celdas[destino] = VACIA;
            comprobar(t, NADIE, "puma rodeado con captura hacia " + std::to_string(destino));
            t.celdas[destino] = PERRO;
        }
        destruirTablero(t);
    }

    {
        Tablero t;
        if (iniciarTablero(t) != 0) std::exit(EXIT_FAILURE);
        t.celdas[nodo(2, 2)] = PUMA;
        t.celdas[nodo(2, 1)] = PERRO;
        t.celdas[nodo(2, 3)] = PERRO;
        // Impedir las capturas horizontales; conserva movimientos verticales.
        t.celdas[nodo(2, 0)] = PERRO;
        t.celdas[nodo(2, 4)] = PERRO;
        comprobar(t, NADIE, "puma con movimiento simple y sin capturas");
        destruirTablero(t);
    }

    {
        Tablero t;
        if (iniciarTablero(t) != 0) std::exit(EXIT_FAILURE);
        t.celdas[nodo(2, 2)] = PUMA;
        t.celdas[nodo(2, 3)] = PERRO;
        t.capturados = 5;
        comprobar(t, NADIE, "cinco capturas no bastan para ganar");
        if (!capturarPerro(t, nodo(2, 2), nodo(2, 3), nodo(2, 4)) || t.capturados != 6) {
            std::cerr << "FALLO: no se pudo realizar la sexta captura\n";
            ++errores;
        }
        comprobar(t, GANA_PUMA, "puma ganador tras realizar la sexta captura");
        destruirTablero(t);
    }

    std::cout << "Resultado: " << casos << " comprobaciones de victoria, " << errores << " fallos.\n";
    return errores == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
