#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include "mesa.hpp"

namespace detalle_config {
inline std::string_view recortar(std::string_view texto) {
    const auto primero = texto.find_first_not_of(" \t\r\n\f\v");
    if (primero == std::string_view::npos) return {};
    const auto ultimo = texto.find_last_not_of(" \t\r\n\f\v");
    return texto.substr(primero, ultimo - primero + 1);
}

template<class T>
inline bool numero(std::string_view texto, T& valor) {
    if (texto.empty()) return false;
    const auto resultado = std::from_chars(texto.data(), texto.data() + texto.size(), valor);
    return resultado.ec == std::errc{} && resultado.ptr == texto.data() + texto.size();
}
}

// CLAVE = VALOR; PERROS USAN DOCE INDICES SEPARADOS POR COMAS
// SOLO VALIDA DATOS NO SE COLOCAN FICHAS NI SE COMPRUEBA CONDICIONES DE VICTORIA
inline bool leerConfig(const std::string& archivo, Config& salida, std::string& error) {
    auto fallar = [&](int linea, const std::string& motivo) {
        error = archivo + (linea ? ":" + std::to_string(linea) : "") + ": " + motivo;
        return false;
    };
    std::ifstream entrada(archivo);
    if (!entrada.is_open()) return fallar(0, "NO SE PUDO ABRIR EL ARCHIVO");

    Config candidato;
    std::map<std::string, int> lineas;
    std::string linea;
    int numeroLinea = 0;
    while (std::getline(entrada, linea)) {
        ++numeroLinea;
        std::string_view texto(linea);
        texto = detalle_config::recortar(texto.substr(0, texto.find('#')));
        if (texto.empty()) continue;
        const auto igual = texto.find('=');
        if (igual == std::string_view::npos || texto.find('=', igual + 1) != std::string_view::npos)
            return fallar(numeroLinea, "SE ESPERABA UNA CLAVE=VALOR");
        const std::string clave(detalle_config::recortar(texto.substr(0, igual)));
        const auto valor = detalle_config::recortar(texto.substr(igual + 1));
        if (clave.empty() || valor.empty()) return fallar(numeroLinea, "CLAVE O VALOR VACIO");
        if (clave != "minMs" && clave != "maxMs" && clave != "semilla" &&
            clave != "maxSegundos" && clave != "logCadaMs" && clave != "puma" && clave != "perros")
            return fallar(numeroLinea, "clave desconocida: " + clave);
        if (!lineas.emplace(clave, numeroLinea).second)
            return fallar(numeroLinea, "clave repetida: " + clave);

        if (clave == "perros") {
            std::size_t inicio = 0;
            for (std::size_t i = 0; i < candidato.perros.size(); ++i) {
                const auto coma = valor.find(',', inicio);
                const auto parte = detalle_config::recortar(valor.substr(inicio,
                    coma == std::string_view::npos ? coma : coma - inicio));
                if (!detalle_config::numero(parte, candidato.perros[i]))
                    return fallar(numeroLinea, "PERROS: POSICION INVALIDA O FUERA DEL RANGO INT");
                if (i + 1 < candidato.perros.size()) {
                    if (coma == std::string_view::npos)
                        return fallar(numeroLinea, "PERROS DEBE CONTENER EXACTAMENTE 12 POSICIONES");
                    inicio = coma + 1;
                } else if (coma != std::string_view::npos) {
                    return fallar(numeroLinea, "PERROS DEBE CONTENER EXACTAMENTE 12 POSICIONES");
                }
            }
        } else if (clave == "semilla") {
            if (!detalle_config::numero(valor, candidato.semilla))
                return fallar(numeroLinea, "SEMILLA DEBE SER UN ENTERO SIN SIGNO");
        } else {
            int* campo = nullptr;
            if (clave == "minMs") campo = &candidato.minMs;
            else if (clave == "maxMs") campo = &candidato.maxMs;
            else if (clave == "maxSegundos") campo = &candidato.maxSegundos;
            else if (clave == "logCadaMs") campo = &candidato.logCadaMs;
            else campo = &candidato.puma;
            if (!detalle_config::numero(valor, *campo))
                return fallar(numeroLinea, clave + ": ENTERO INVALIDO O FUERA DEL RANGO INT");
        }
    }
    if (entrada.bad() || !entrada.eof()) return fallar(numeroLinea, "ERROR AL LEER EL ARCHIVO");
    auto origen = [&](const std::string& clave) {
        const auto it = lineas.find(clave);
        return it == lineas.end() ? 0 : it->second;
    };
    if (candidato.minMs <= 0) return fallar(origen("minMs"), "minMs DEBE SER POSITIVO");
    if (candidato.maxMs < candidato.minMs)
        return fallar(std::max(origen("minMs"), origen("maxMs")), "maxMs DEBE SER MAYOR O IGUAL QUE minMs");
    // LOS HILOS CALCULAN EL RETARDO EN MS MEDIANTE UNA MULTIPLICACION DE INTS
    if (candidato.maxMs > std::numeric_limits<int>::max() / 1000)
        return fallar(origen("maxMs"), "maxMs PROVOCA DESBORDAMIENTO AL MULTIPLICAR X 1000");
    if (candidato.maxSegundos <= 0 || candidato.maxSegundos > std::numeric_limits<int>::max() / 10)
        return fallar(origen("maxSegundos"), "maxSegundos DEBE SER POSITIVO Y PERMITIR MULTIPLICAR x10 SIN DESBORDAMIENTO");
    if (candidato.logCadaMs <= 0)
        return fallar(origen("logCadaMs"), "logCadaMs DEBE SER POSITIVO");
    if (candidato.puma < 0 || candidato.puma >= NODOS)
        return fallar(origen("puma"), "PUMA DEBE ESTAR ENTRE 0 y 27");
    std::array<bool, NODOS> ocupadas{};
    for (int posicion : candidato.perros) {
        if (posicion < 0 || posicion >= NODOS)
            return fallar(origen("perros"), "CADA POSICION DE PERROS DEBE ESTAR ENTRE 0 y 27");
        if (ocupadas[posicion]) return fallar(origen("perros"), "POSICION DE PERRO REPETIDA");
        if (posicion == candidato.puma)
            return fallar(std::max(origen("puma"), origen("perros")), "PUMA Y PERRO NO PUEDEN OCUPAR LA MISMA POSICION");
        ocupadas[posicion] = true;
    }
    salida = candidato;
    error.clear();
    return true;
}

#endif
