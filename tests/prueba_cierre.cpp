#include <cstdlib>
#include <iostream>
#include "mesa.hpp"

struct Carrera {
    Tablero* tablero;
    bool victoriaPrimero;
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    bool primeroTermino = false;
    bool captura = false;
    bool abandono = false;
};

struct IntentoCierre { Carrera* carrera; bool victoria; };

void* intentarCierre(void* p) {
    const auto& intento = *static_cast<IntentoCierre*>(p);
    Carrera& c = *intento.carrera;
    const bool primero = intento.victoria == c.victoriaPrimero;
    pthread_mutex_lock(&c.mtx);
    while (!primero && !c.primeroTermino) pthread_cond_wait(&c.cond, &c.mtx);
    pthread_mutex_unlock(&c.mtx);
    if (intento.victoria) c.captura = capturarPerro(*c.tablero, 13, 14, 15);
    else c.abandono = aceptarAbandono(*c.tablero, PUMA);
    if (primero) {
        pthread_mutex_lock(&c.mtx);
        c.primeroTermino = true;
        pthread_cond_broadcast(&c.cond);
        pthread_mutex_unlock(&c.mtx);
    }
    return nullptr;
}

int main() {
    int fallos = 0;
    auto comprobar = [&](bool ok, const char* mensaje) {
        std::cout << (ok ? "OK: " : "FALLO: ") << mensaje << '\n';
        if (!ok) ++fallos;
    };
    //DOS HILOS Y ORDEN IMPUESTO X CONDICION
    for (bool victoriaPrimero : {false, true}) {
        Tablero t;
        if (iniciarTablero(t) != 0) std::exit(EXIT_FAILURE);
        t.celdas[13] = PUMA;
        t.celdas[14] = PERRO;
        t.idPerro[14] = 0;
        t.capturados = 5; // LA CAPTURA SGTE CONFIRMA LA VICTORIA
        Carrera carrera{&t, victoriaPrimero};
        IntentoCierre intentos[] = {{&carrera, true}, {&carrera, false}};
        pthread_t hilos[2];
        for (int i = 0; i < 2; ++i)
            if (pthread_create(&hilos[i], nullptr, intentarCierre, &intentos[i]) != 0)
                std::exit(EXIT_FAILURE);
        for (pthread_t hilo : hilos)
            if (pthread_join(hilo, nullptr) != 0) std::exit(EXIT_FAILURE);
        comprobar(carrera.captura == victoriaPrimero && carrera.abandono != victoriaPrimero,
                  "SOLO LA PRIMERA OPERACION PUEDE CERRAR LA PARTIDA CORRECTAMENTE");
        comprobar(t.fin && t.capturados == (victoriaPrimero ? 6 : 5) &&
                  t.abandona == (victoriaPrimero ? VACIA : PUMA) &&
                  ganador(t) == (victoriaPrimero ? GANA_PUMA : NADIE),
                  "PARTIDA TERMINO( GANA CAPTURA = 6 PERROS CAPTURADOS) (GANA ABANDONO = SIGUEN SIENDO 5 CAPTURADOS) ESTADO DE ABANDONO SEA CORRECTO");
        comprobar(!aceptarAbandono(t, PERRO) && !moverFicha(t, 13, 8, PUMA, -1) &&
                  !moverFicha(t, 14, 9, PERRO, 0), "CIERRE RECHAZA NUEVOS ABANDONOS Y MOVIMIENTOS");
        comprobar(!esperarRetardo(t, 2000), "SI TABLERO TERMINA, LA FICHA NO QUEDA ESPERANDO");
        pthread_cond_destroy(&carrera.cond);
        pthread_mutex_destroy(&carrera.mtx);
        destruirTablero(t);
    }
    for (Celda jugador : {PUMA, PERRO}) {
        Tablero t;
        if (iniciarTablero(t) != 0) std::exit(EXIT_FAILURE);
        colocarFichasIniciales(t);
        comprobar(aceptarAbandono(t, jugador) && t.fin && t.abandona == jugador,
                  "AMBOS JUGADORES PUEDEN CERRAR EL TABLERO INMEDIATAMENTE");
        destruirTablero(t);
    }
    {
        Tablero t;
        if (iniciarTablero(t) != 0) std::exit(EXIT_FAILURE);
        colocarFichasIniciales(t);
        terminarPartida(t); // CIERRE POR TIEMPO, SIGINT o FALLO: NO ES ABANDONO
        comprobar(!aceptarAbandono(t, PUMA) && t.abandona == VACIA,
                  "PARTIDA CERRANDOSE NO SE ACEPTA ABANDONO");
        destruirTablero(t);
    }
    std::cout << "RESULTADO: " << fallos << " FALLOS.\n";
    return fallos == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
