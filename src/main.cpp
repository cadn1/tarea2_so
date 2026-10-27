#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    //  
    if (argc < 3 || argc > 4) { // PERMITIR CANTIDAD VALIDA DE ARGUMENTOS
        std::cerr << "ERROR --> USO CORRECTO  --> " << argv[0] << " N M [config.txt]\n";
        return 1;
    }

    int N = 0;
    int M = 0;
    std::string config_file = "";

    // ATRAPAR ERRORES
    try {
        N = std::stoi(argv[1]);
        M = std::stoi(argv[2]);
    } catch (const std::exception& e) {
        std::cerr << "ERROR: N y M deben ser numeros enteros validos.\n";
        return 1;
    }

    if (argc == 4) {
        config_file = argv[3];
    }

    std::cout << "SERVIDOR INICIADO\n"; // SALIDA
    std::cout << "JUGADORES --> " << N <<" <-- " <<"[-]" << " MESAS --> " << M <<" <--" << "\n";


    return 0;
}