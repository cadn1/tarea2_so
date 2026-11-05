#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <unistd.h>
#include "config.hpp"

bool mismaConfig(const Config& a, const Config& b) {
    return a.minMs == b.minMs && a.maxMs == b.maxMs && a.semilla == b.semilla &&
           a.maxSegundos == b.maxSegundos && a.logCadaMs == b.logCadaMs &&
           a.puma == b.puma && a.perros == b.perros;
}

bool diagnosticoContiene(const std::string& error, const std::string& esperado) {
    const auto inicio = error.find(": ");
    if (inicio == std::string::npos || esperado.empty()) return false;
    auto minusculas = [](std::string texto) {
        for (char& c : texto) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return texto;
    };
    // Solo comparar el motivo sin distinguir mayusculas. Archivo y linea
    // mantienen sus comprobaciones exactas en cada caso de prueba.
    return minusculas(error.substr(inicio + 2)).find(minusculas(esperado)) != std::string::npos;
}

int main() {
    char plantilla[] = "/tmp/komikan_config_XXXXXX";
    const char* directorio = mkdtemp(plantilla);
    if (!directorio) {
        std::cerr << "FALLO: NO SE PUDO CREAR EL DIRECTORIO (TEMPORAL)\n";
        return EXIT_FAILURE;
    }
    int casos = 0, fallos = 0;
    std::vector<std::string> propios;
    auto comprobar = [&](bool ok, const std::string& mensaje) {
        if (!ok) { ++fallos; std::cerr << "FALLO: " << mensaje << '\n'; }
    };
    auto probar = [&](const std::string& nombre, const std::string& contenido,
                      bool esperado, int lineaError = 0, const std::string& detalle = "") {
        ++casos;
        const int antesFallos = fallos;
        const std::string ruta = std::string(directorio) + "/caso_" + std::to_string(casos) + ".txt";
        propios.push_back(ruta);
        {
            std::ofstream archivo(ruta);
            archivo << contenido;
            if (!archivo) {
                std::cerr << "FALLO: NO SE PUDO ESCRIBIR " << ruta << '\n';
                std::exit(EXIT_FAILURE);
            }
        }
        Config cfg;
        cfg.minMs = 7; // !!! el lector debe partir de predeterminados, no de esta salida !!!!
        cfg.semilla = 99;
        const Config antes = cfg;
        std::string error = "ERROR ANTERIOR";
        const bool ok = leerConfig(ruta, cfg, error);
        comprobar(ok == esperado, nombre + ": RETURN INCORRECTO; " + error);
        if (esperado) comprobar(error.empty(), nombre + ": ERROR NO LIMPIADO");
        else {
            comprobar(mismaConfig(cfg, antes), nombre + ": SALIDA MODIFICADA TRAS RECHAZO");
            comprobar(error.find(ruta + ":") == 0, nombre + ": FALTA NOMBRE DEL ARCHIVO");
            if (lineaError)
                comprobar(error.find(ruta + ":" + std::to_string(lineaError) + ":") == 0,
                          nombre + ": NUMERO DE LINEA INCORRECTO; " + error);
            comprobar(diagnosticoContiene(error, detalle), nombre + ": DIAGNOSTICO INCORRECTO; " + error);
        }
        if (fallos == antesFallos) std::cout << "OK: " << nombre << '\n';
        return cfg;
    };

    const Config predeterminada;
    comprobar(mismaConfig(probar("archivo vacio", "", true), predeterminada), "predeterminados completos");
    comprobar(mismaConfig(probar("solo comentarios y espacios", " # comentario\n\n\t \r\n", true), predeterminada),
              "comentarios conservan predeterminados");
    const Config completa = probar("archivo completo, espacios y comentarios",
        " minMs = 10 # intento\nmaxMs=900\nsemilla=42\nmaxSegundos=15\nlogCadaMs=77\n"
        "puma=4\n perros = 1, 2,3,5,6,7,8,9,10,11,12,13 # doce perros\n", true);
    comprobar(completa.minMs == 10 && completa.maxMs == 900 && completa.semilla == 42 &&
              completa.maxSegundos == 15 && completa.logCadaMs == 77 && completa.puma == 4 &&
              completa.perros == std::array<int,12>{{1,2,3,5,6,7,8,9,10,11,12,13}},
              "valores completos y orden de identidades");
    Config parcialEsperada;
    parcialEsperada.semilla = 0;
    comprobar(mismaConfig(probar("clave parcial y semilla cero", "semilla=0\n", true), parcialEsperada),
              "claves ausentes mantienen predeterminados");
    probar("retardos iguales", "minMs=1\nmaxMs=1\n", true);
    probar("orden de claves independiente", "maxMs=700\nminMs=500\n", true);
    const auto maxInt = std::numeric_limits<int>::max();
    const auto maxUnsigned = std::numeric_limits<unsigned>::max();
    comprobar(probar("semilla maxima", "semilla=" + std::to_string(maxUnsigned), true).semilla == maxUnsigned,
              "semilla maxima conservada");
    probar("limites aritmeticos validos", "maxMs=" + std::to_string(maxInt / 1000) +
           "\nmaxSegundos=" + std::to_string(maxInt / 10) + "\nlogCadaMs=" + std::to_string(maxInt), true);
    probar("puma bloqueado permitido", "puma=0\nperros=1,2,5,3,7,10,13,17,18,19,20,21\n", true);

    probar("clave desconocida", "# cabecera\nextra=1\n", false, 2, "desconocida");
    probar("clave repetida", "semilla=1\n semilla = 2\n", false, 2, "repetida");
    probar("perros repetido como clave", "perros=13,17,18,23,19,24,20,25,21,26,22,27\nperros=1\n",
           false, 2, "repetida");
    probar("sin igual", "minMs 50\n", false, 1, "clave=valor");
    probar("varios iguales", "minMs=5=0\n", false, 1, "clave=valor");
    probar("clave vacia", " =50\n", false, 1, "vacio");
    probar("valor vacio", "minMs= # vacio\n", false, 1, "vacio");
    probar("texto no numerico", "minMs=abc\n", false, 1, "entero");
    probar("sufijo numerico", "maxMs=300abc\n", false, 1, "entero");
    probar("numero decimal", "minMs=1.5\n", false, 1, "entero");
    probar("espacio dentro del numero", "minMs=5 0\n", false, 1, "entero");
    probar("desbordamiento int", "minMs=999999999999999999999999\n", false, 1, "rango");
    probar("semilla negativa", "semilla=-1\n", false, 1, "sin signo");
    probar("desbordamiento unsigned", "semilla=184467440737095516160\n", false, 1, "semilla debe ser un entero sin signo");
    probar("semilla con sufijo", "semilla=12x\n", false, 1, "semilla debe ser un entero sin signo");
    probar("minimo cero", "minMs=0\n", false, 1, "positivo");
    probar("minimo negativo", "minMs=-1\n", false, 1, "positivo");
    probar("retardos invertidos", "minMs=80\nmaxMs=70\n", false, 2, "mayor o igual");
    probar("minimo incompatible con maximo predeterminado", "minMs=400\n", false, 1, "mayor o igual");
    probar("maximo cero", "maxMs=0\n", false, 1, "mayor o igual");
    probar("desbordamiento microsegundos", "maxMs=" + std::to_string(maxInt / 1000 + 1), false, 1, "1000");
    probar("duracion cero", "maxSegundos=0\n", false, 1, "positivo");
    probar("duracion negativa", "maxSegundos=-2\n", false, 1, "positivo");
    probar("desbordamiento ciclos", "maxSegundos=" + std::to_string(maxInt / 10 + 1), false, 1, "10");
    probar("registro cero", "logCadaMs=0\n", false, 1, "positivo");
    probar("registro negativo", "logCadaMs=-1\n", false, 1, "positivo");
    probar("registro fuera de int", "logCadaMs=99999999999999999999\n", false, 1, "rango");
    probar("puma negativo", "puma=-1\n", false, 1, "0 y 27");
    probar("puma fuera del tablero", "puma=28\n", false, 1, "0 y 27");
    probar("puma coincide con perros predeterminados", "puma=13\n", false, 1, "misma posicion");
    probar("perro fuera del tablero", "perros=28,17,18,23,19,24,20,25,21,26,22,27", false, 1, "0 y 27");
    probar("perro negativo", "perros=-1,17,18,23,19,24,20,25,21,26,22,27", false, 1, "0 y 27");
    probar("perros duplicados", "perros=13,13,18,23,19,24,20,25,21,26,22,27", false, 1, "repetida");
    probar("perro coincide con puma", "perros=0,17,18,23,19,24,20,25,21,26,22,27", false, 1, "misma posicion");
    probar("ocupacion validada al finalizar lectura", "perros=0,17,18,23,19,24,20,25,21,26,22,27\npuma=4", true);
    probar("once perros", "perros=13,17,18,23,19,24,20,25,21,26,22", false, 1, "perros debe contener exactamente 12 posiciones");
    probar("trece perros", "perros=13,17,18,23,19,24,20,25,21,26,22,27,1", false, 1, "perros debe contener exactamente 12 posiciones");
    probar("coma final", "perros=13,17,18,23,19,24,20,25,21,26,22,27,", false, 1, "perros debe contener exactamente 12 posiciones");
    probar("elemento vacio", "perros=13,,18,23,19,24,20,25,21,26,22,27", false, 1, "perros: posicion invalida o fuera del rango int");
    probar("posicion con sufijo", "perros=13x,17,18,23,19,24,20,25,21,26,22,27", false, 1, "perros: posicion invalida o fuera del rango int");
    probar("desbordamiento posicion", "perros=99999999999999999999,17,18,23,19,24,20,25,21,26,22,27",
           false, 1, "perros: posicion invalida o fuera del rango int");
    {
        ++casos;
        Config cfg;
        cfg.minMs = 7;
        cfg.semilla = 99;
        const Config antes = cfg;
        std::string error;
        const std::string ruta = std::string(directorio) + "/no_existe.txt";
        const int antesFallos = fallos;
        comprobar(!leerConfig(ruta, cfg, error), "archivo inexistente: DEBE SER RECHAZADO");
        comprobar(mismaConfig(cfg, antes), "archivo inexistente: SALIDA MODIFICADA TRAS RECHAZO");
        comprobar(error.find(ruta + ":") == 0, "archivo inexistente: FALTA NOMBRE DEL ARCHIVO");
        comprobar(diagnosticoContiene(error, "no se pudo abrir el archivo"),
                  "archivo inexistente: DIAGNOSTICO INCORRECTO; " + error);
        if (fallos == antesFallos) std::cout << "OK: archivo inexistente y salida intacta\n";
    }
    {
        Tablero t;
        if (iniciarTablero(t) != 0) std::exit(EXIT_FAILURE);
        colocarFichasIniciales(t);
        comprobar(t.celdas[predeterminada.puma] == PUMA, "puma predeterminado coincide con colocacion actual");
        for (std::size_t id = 0; id < predeterminada.perros.size(); ++id)
            comprobar(t.celdas[predeterminada.perros[id]] == PERRO &&
                      t.idPerro[predeterminada.perros[id]] == static_cast<int>(id),
                      "orden predeterminado conserva identidad " + std::to_string(id));
        destruirTablero(t);
    }
    // LIMPIAR ARCHIVOS CREADOS X ESTA PRUEBA
    for (const auto& ruta : propios) comprobar(std::remove(ruta.c_str()) == 0, "LIMPIEZA ARCHIVO");
    comprobar(rmdir(directorio) == 0, "LIMPIEZA DIRECTORIO");
    std::cout << "RESULTADO: " << casos << " CASOS DE LECTURA, " << fallos << " FALLOS.\n";
    return fallos == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
