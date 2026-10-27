#include <iostream>
#include <string>

int main(int argc, char* argv[]) { // VALIDAR CANTIDAD DE ARGUMENTOS 
    if (argc < 3 || argc > 4) {
        std::cerr << "ERROR --> USO CORRECTO  --> " << argv[0] << " N M [config.txt]\n";
        return 1;
    }

    int N = 0;
    int M = 0;
    std::string config_file = "";

    try { // CONVERSION QUE NO PERMITE ERRORES
        N = std::stoi(argv[1]);
        M = std::stoi(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: N y M deben ser numeros enteros validos.\n";
        return 1;
    }

    if (argc == 4) {
        config_file = argv[3];
    }

    std::cout << "--- SERVIDOR INICIADO ---\n";
    std::cout << "JUGADORES: " << N << " [-] MESAS DISPONIBLES: " << M << "\n";

    int partidas_posibles = N / 2; // JUGADORES PAR
    int mesas_activas = std::min(partidas_posibles, M); // MÁS CHICO
    int jugadores_sobrantes = N - (mesas_activas * 2); // LOS QUE SOBRAN

    std::cout << "\n--- EMPAREJAMIENTO ---\n";
    for (int i = 1; i <= mesas_activas; i++) {
        int id_puma = (i * 2) - 1;
        int id_perros = i * 2;
        std::cout << "[MESA " << i << "] INICIADA: JUGADOR " << id_puma 
                  << " (PUMA) VS JUGADOR " << id_perros << " (PERROS)\n";
    }

    if (mesas_activas < partidas_posibles) {
        std::cout << "LO SIENTO, ACTUALMENTE NO EXISTEN SUFICIENTES MESAS DISPONIBLES\n";
        std::cout << "FALTAN: " << (partidas_posibles - mesas_activas) << " MESA(S)\n";
    }

    if (jugadores_sobrantes > 0) {
        std::cout << jugadores_sobrantes << " JUGADOR(ES) EN ESPERA.\n";
    } else {
        std::cout << jugadores_sobrantes << " JUGADORES EN ESPERA.\n";
    }

    return 0;
}