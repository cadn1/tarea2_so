#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include "tablero.hpp"

int main() {
    Tablero t;
    iniciarTablero(t);
    int errores = 0;
    auto comprobar = [&](bool condicion, const std::string& mensaje) {
        if (!condicion) {
            std::cerr << "FALLO: " << mensaje << '\n';
            ++errores;
        }
    };

    // VALIDAR INDICES: NO ACCEDER SIN COMPROBAR RANGO
    for (int u = 0; u < NODOS; ++u) {
        std::set<int> vistos;
        for (int v : t.ady[u]) {
            const std::string arista = std::to_string(u) + " -> " + std::to_string(v);
            comprobar(v >= 0 && v < NODOS, "INDICE INVALIDO EN " + arista);
            comprobar(vistos.insert(v).second, "CONEXION DUPLICADA EN " + arista);
            comprobar(u != v, "CONEXION CONSIGO MISMO EN " + arista);
            if (v < 0 || v >= NODOS) continue;
            comprobar(std::find(t.ady[v].begin(), t.ady[v].end(), u) != t.ady[v].end(),
                      "FALTA CONEXION INVERSA DE " + arista);
        }
    }

    comprobar(FILAS == 5 && COLS == 5 && NODOS == 28,
              "SE ESPERAN 25 NODOS DE CUADRICULA y 3 ESPECIALES");
    std::set<int> indices;
    for (int f = 0; f < 5; ++f) {
        for (int c = 0; c < 5; ++c) {
            const int u = nodo(f, c);
            comprobar(u == 3 + f * 5 + c, "NUMERACION INCORRECTA DE LA CUADRICULA");
            comprobar(indices.insert(u).second, "NODO DE CUADRICULA REPETIDO");
            if (u < 3 || u >= NODOS) {
                comprobar(false, "INDICE DE CUADRICULA FUERA DE RANGO");
                continue;
            }
            std::set<int> esperados;
            if (f > 0) esperados.insert(3 + (f - 1) * 5 + c);
            if (f < 4) esperados.insert(3 + (f + 1) * 5 + c);
            if (c > 0) esperados.insert(3 + f * 5 + c - 1);
            if (c < 4) esperados.insert(3 + f * 5 + c + 1);
            std::set<int> actuales;
            for (int v : t.ady[u]) if (v >= 3) actuales.insert(v);
            comprobar(actuales == esperados,
                      "VECINOS DE CUADRICULA INCORRECTOS  " + std::to_string(u));
        }
    }

    const std::set<int> especiales[3] = {{1, 2, 5}, {0, 2, 3, 4}, {0, 1, 6, 7}};
    for (int u = 0; u < 3; ++u) {
        const std::set<int> actuales(t.ady[u].begin(), t.ady[u].end());
        comprobar(actuales == especiales[u],
                  "VECINOS ESPECIALES INCORRECTOS " + std::to_string(u));
    }

    destruirTablero(t);
    if (errores != 0) {
        std::cerr << "RESULTADO: " << errores << " COMPROBACIONES FALLIDAS.\n";
        return EXIT_FAILURE;
    }
    std::cout << "OK: NUMERACION DE LOS 28 NODOS.\n"
              << "OK: CUADRICULA 5x5 CORRECTA.\n"
              << "OK: VECINOS ACTUALES DE LOS NODOS 0 1 2.\n"
              << "OK: CONEXIONES BIDIRECCIONALES.\n"
              << "OK: SIN INDICES INVALIDOS, DUPLICADOS NI CONECTADOS CONSIGO MISMO.\n"
              << "RESULTADO EXITOSO.\n";
    return EXIT_SUCCESS;
}
