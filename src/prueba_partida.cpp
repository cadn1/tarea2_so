#include <iostream>
#include "mesa.hpp"

int main() {
    Config cfg;
    cfg.maxSegundos = 20;
    cfg.logCadaMs = 2000;
    Ganador g = jugarPartida(cfg, 1);
    std::cout << "resultado: " << g << " (0 = nadie o tiempo, 1 = puma, 2 = perros)" << std::endl;
    return 0;
}
