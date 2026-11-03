#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include "tablero.hpp"

int main() {
    int errores = 0;
    int casos = 0;
    auto comprobar = [&](bool condicion, const std::string& mensaje) {
        if (!condicion) {
            std::cerr << "FALLO: " << mensaje << '\n';
            ++errores;
        }
    };
    auto probar = [&](const std::string& nombre, int desde, int sobre, int hacia,
                      Celda origen, Celda intermedio, Celda destino, bool esperado) {
        Tablero t;
        iniciarTablero(t);
        if (desde >= 0 && desde < NODOS) t.celdas[desde] = origen;
        if (sobre >= 0 && sobre < NODOS) t.celdas[sobre] = intermedio;
        if (hacia >= 0 && hacia < NODOS) t.celdas[hacia] = destino;
        for (int i = 0, id = 0; i < NODOS; ++i)
            if (t.celdas[i] == PERRO) t.idPerro[i] = id++;
        std::array<int, NODOS> idsEsperados;
        for (int i = 0; i < NODOS; ++i) idsEsperados[i] = t.idPerro[i];
        if (esperado) idsEsperados[sobre] = -1;
        t.capturados = 2;
        std::array<Celda, NODOS> esperadas;
        for (int i = 0; i < NODOS; ++i) esperadas[i] = t.celdas[i];
        if (esperado) {
            esperadas[desde] = VACIA;
            esperadas[sobre] = VACIA;
            esperadas[hacia] = PUMA;
        }
        const int erroresAntes = errores;
        comprobar(capturarPerro(t, desde, sobre, hacia) == esperado, nombre + ": RETORNO INCORRECTO");
        comprobar(t.capturados == 2 + (esperado ? 1 : 0), nombre + ": CONTADOR INCORRECTO");
        for (int i = 0; i < NODOS; ++i) {
            comprobar(t.idPerro[i] == idsEsperados[i], nombre + ": identidad incorrecta");
            comprobar(t.celdas[i] == esperadas[i],
                      nombre + ": CELDA INCORRECTA " + std::to_string(i));
        }
        comprobar(!t.fin, nombre + ": SE MODIFICO FIN");
        destruirTablero(t);
        ++casos;
        if (errores == erroresAntes) std::cout << "OK: " << nombre << '\n';
    };

    probar("CAPTURA HORIZONTAL", 13, 14, 15, PUMA, PERRO, VACIA, true);
    probar("CAPTURA VERTICAL", 5, 10, 15, PUMA, PERRO, VACIA, true);
    const std::array<int, 3> trono[] = {
        {{0, 1, 3}}, {{0, 2, 7}}, {{1, 4, 9}}, {{2, 6, 11}}, {{0, 5, 10}}
    };
    for (const auto& s : trono) {
        probar("trono " + std::to_string(s[0]) + "-" + std::to_string(s[1]) + "-" + std::to_string(s[2]),
               s[0], s[1], s[2], PUMA, PERRO, VACIA, true);
        probar("trono inverso " + std::to_string(s[2]) + "-" + std::to_string(s[1]) + "-" + std::to_string(s[0]),
               s[2], s[1], s[0], PUMA, PERRO, VACIA, true);
    }
    probar("DIAGONAL ILEGAL", 3, 9, 15, PUMA, PERRO, VACIA, false);
    probar("GIRO ILEGAL", 13, 14, 19, PUMA, PERRO, VACIA, false);
    probar("SALTO DEMASIADO LARGO", 3, 5, 7, PUMA, PERRO, VACIA, false);
    probar("DESTINO OCUPADO", 13, 14, 15, PUMA, PERRO, PERRO, false);
    probar("SIN PERRO INTERMEDIO", 13, 14, 15, PUMA, VACIA, VACIA, false);
    probar("SIN PUMA EN EL ORIGEN", 13, 14, 15, PERRO, PERRO, VACIA, false);
    for (int invalido : {-1, NODOS}) {
        probar("ORIGEN FUERA DE RANGO " + std::to_string(invalido),
               invalido, 14, 15, PUMA, PERRO, VACIA, false);
        probar("INTERMEDIO FUERA DE RANGO " + std::to_string(invalido),
               13, invalido, 15, PUMA, PERRO, VACIA, false);
        probar("DESTINO FUERA DE RANGO " + std::to_string(invalido),
               13, 14, invalido, PUMA, PERRO, VACIA, false);
    }
    std::cout << "RESULTADO: " << casos << " CASOS, " << errores << " FALLOS.\n";
    return errores == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
