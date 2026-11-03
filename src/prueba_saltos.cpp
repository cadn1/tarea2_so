#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include "tablero.hpp"

int main() {
    using Salto = std::array<int, 3>;
    Tablero t;
    iniciarTablero(t);
    int errores = 0;
    auto comprobar = [&](bool condicion, const std::string& mensaje) {
        if (!condicion) {
            std::cerr << "FALLO: " << mensaje << '\n';
            ++errores;
        }
    };
    auto nombre = [](const Salto& s) {
        return std::to_string(s[0]) + "-" + std::to_string(s[1]) + "-" + std::to_string(s[2]);
    };
    auto conexion = [&](int a, int b) {
        return a >= 0 && a < NODOS && b >= 0 && b < NODOS &&
               std::find(t.ady[a].begin(), t.ady[a].end(), b) != t.ady[a].end();
    };

    // VENTANAS CONSECUTIVAS DE TRES NODOS POR FILA Y COLUMNA
    std::set<Salto> esperados;
    for (int linea = 0; linea < 5; ++linea) {
        for (int inicio = 0; inicio < 3; ++inicio) {
            const Salto horizontal = {{3 + 5 * linea + inicio,
                                       4 + 5 * linea + inicio,
                                       5 + 5 * linea + inicio}};
            const Salto vertical = {{3 + 5 * inicio + linea,
                                     8 + 5 * inicio + linea,
                                     13 + 5 * inicio + linea}};
            for (const auto& s : {horizontal, vertical}) {
                esperados.insert(s);
                esperados.insert(Salto{{s[2], s[1], s[0]}});
            }
        }
    }

    comprobar(esperados.size() == 60, "SE ESPERAN 60 SALTOS DE CUADRICULA");
    const Salto especiales[] = {
        {{0, 1, 3}}, {{0, 2, 7}}, {{1, 4, 9}}, {{2, 6, 11}}, {{0, 5, 10}}
    };
    for (const auto& s : especiales) {
        esperados.insert(s);
        esperados.insert(Salto{{s[2], s[1], s[0]}});
    }
    comprobar(esperados.size() == 70, "SE ESPERABAN 70 SALTOS");

    const auto generados = todosLosSaltos();
    std::set<Salto> actuales;
    comprobar(generados.size() == 70, "SE ESPERABAN EXACTAMENTE 70 SALTOS");
    for (const auto& s : generados) {
        comprobar(actuales.insert(s).second, "SALTO DUPLICADO " + nombre(s));
        comprobar(esperados.count(s) == 1,
                  "SALTO NO PERMITIDO: " + nombre(s));
        comprobar(conexion(s[0], s[1]) && conexion(s[1], s[2]),
                  "SALTO SIN DOS CONEXIONES CONSECUTIVAS: " + nombre(s));
    }
    for (const auto& s : esperados) {
        comprobar(actuales.count(s) == 1, "FALTA SALTO PERMITIDO " + nombre(s));
    }

    // CASOS NO VALIDOS
    const Salto prohibidos[] = {
        {{3, 9, 15}},   // DIAGONAL
        {{13, 14, 19}}, // GIRO
        {{3, 5, 7}},    // OMITIR NODOS
        {{6, 7, 8}},   // CRUZA DE UNA FILA CON OTRA
        {{0, 1, 4}},   // GIRO EN EL TECHO
        {{1, 0, 2}}    // GIRO EN EL TRONO
    };
    for (const auto& s : prohibidos) {
        comprobar(actuales.count(s) == 0, "SALTO ILEGAL GENERADO " + nombre(s));
    }

    if (errores == 0) {
        std::cout << "OK -> los 60 saltos dirigidos de cuadricula coinciden exactamente.\n"
                  << "OK -> las 10 trayectorias especiales dirigidas confirmadas estan presentes.\n"
                  << "OK -> exactamente 70 saltos, sin duplicados ni recorridos ilegales.\n"
                  << "OK -> sin diagonales en la cuadricula, giros ni omisiones de nodos.\n"
                  << "OK -> cada salto generado recorre dos conexiones consecutivas.\n";
    }

    destruirTablero(t);
    std::cout << "RESULTADO: " << errores << " FALLOS.\n";
    return errores == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
