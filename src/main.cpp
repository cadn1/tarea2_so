#include <iostream>
#include <string>

int main(int argc, char* argv[]) { // VALIDACION PARA QUE SOLO ADMITA 3 o 4 PARAMETROS ./torneo n m config (opcional)
    if (argc < 3 || argc > 4) {
        std::cerr << "ERROR --> USO CORRECTO  --> " << argv[0] << " N M [config.txt]\n";
        return 1;
    }

    // CONVERSION
    int N = std::stoi(argv[1]);
    int M = std::stoi(argv[2]);
    
    std::string config_file = "";
    if (argc == 4) {
        config_file = argv[3];
    }

    std::cout << "SERVIDOR INICIADO\n";
    std::cout << "JUGADORES --> " << N <<" <-- " <<"[-]" << " MESAS --> " << M <<" <--" << "\n";

    return 0;
}