#include <cstdlib>
#include <iostream>
#include "mesa.hpp"

int main() {
    int fallos = 0;
    auto comprobar = [&](bool ok, const char* nombre) {
        if (ok) std::cout << "OK: " << nombre << '\n';
        else { ++fallos; std::cerr << "FALLO: " << nombre << '\n'; }
    };
    Config cfg;
    cfg.puma = 12;
    cfg.perros = {{27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 13}};
    Tablero a, b;
    iniciarTablero(a);
    iniciarTablero(b);
    colocarFichasIniciales(a, cfg);
    colocarFichasIniciales(b, cfg);
    bool colocacion = true;
    int perros = 0, pumas = 0;
    for (int i = 0; i < NODOS; ++i) {
        Celda esperada = i == cfg.puma ? PUMA : VACIA;
        int idEsperado = -1;
        for (std::size_t id = 0; id < cfg.perros.size(); ++id) {
            if (cfg.perros[id] == i) { esperada = PERRO; idEsperado = static_cast<int>(id); }
        }
        colocacion = colocacion && a.celdas[i] == esperada && a.idPerro[i] == idEsperado &&
                     b.celdas[i] == esperada && b.idPerro[i] == idEsperado;
        perros += a.celdas[i] == PERRO;
        pumas += a.celdas[i] == PUMA;
    }
    comprobar(colocacion && perros == 12 && pumas == 1,
              "POSICIONES CONFIGURADAS E IDENTIDADES SEGUN EL ORDEN DEL ARREGLO");
    comprobar(moverFicha(a, 12, 7, PUMA, -1) && a.celdas[7] == PUMA &&
              b.celdas[12] == PUMA && b.celdas[7] == VACIA,
              "TABLEROS INDEPENDIENTES CON LA MISMA CONFIGURACION");
    destruirTablero(a);
    destruirTablero(b);

    Config bloqueada;
    bloqueada.perros = {{1, 2, 5, 3, 7, 10, 13, 17, 18, 19, 20, 21}};
    comprobar(jugarPartida(bloqueada, 1) == GANAN_PERROS,
              "BLOQUEO INICIAL -> VICTORIA PERROS");
    std::cout << "RESULTADO: " << fallos << " FALLOS.\n";
    return fallos == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
