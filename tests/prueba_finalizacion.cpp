#include <array>
#include <cstdlib>
#include <iostream>
#include "tablero.hpp"

struct Estado {
    std::array<Celda, NODOS> celdas;
    std::array<int, NODOS> ids;
    int capturados;
    bool fin;
};

// SIN HILOS ACTIVOS O DESPUES DE PHTREAD_JOIN
Estado guardar(const Tablero& t) {
    Estado e;
    for (int i = 0; i < NODOS; ++i) {
        e.celdas[i] = t.celdas[i];
        e.ids[i] = t.idPerro[i];
    }
    e.capturados = t.capturados;
    e.fin = t.fin;
    return e;
}

bool igual(const Tablero& t, const Estado& e) {
    const Estado actual = guardar(t);
    return actual.celdas == e.celdas && actual.ids == e.ids &&
           actual.capturados == e.capturados && actual.fin == e.fin;
}

void preparar(Tablero& t) {
    iniciarTablero(t);
    t.celdas[13] = PUMA;
    t.celdas[14] = PERRO;
    t.idPerro[14] = 0;
    t.capturados = 2;
}

struct Intentos {
    Tablero* t;
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    bool preparado = false;
    bool continuar = false;
    bool vioActiva = false;
    bool rechazados = true;
};

void* intentarDespues(void* p) {
    auto& a = *static_cast<Intentos*>(p);
    // NINGUN HILO PUEDE MOVER O CAPTURAR FICHAS DSP DE QUE TERMINE LA PARTIDA
    a.vioActiva = !partidaTerminada(*a.t);
    pthread_mutex_lock(&a.mtx);
    a.preparado = true;
    pthread_cond_broadcast(&a.cond);
    while (!a.continuar) pthread_cond_wait(&a.cond, &a.mtx);
    pthread_mutex_unlock(&a.mtx);
    for (int i = 0; i < 100; ++i) {
        if (moverFicha(*a.t, 13, 8, PUMA, -1)) a.rechazados = false;
        if (moverFicha(*a.t, 14, 9, PERRO, 0)) a.rechazados = false;
        if (capturarPerro(*a.t, 13, 14, 15)) a.rechazados = false;
    }
    return nullptr;
}

int main() {
    int errores = 0;
    auto comprobar = [&](bool ok, const char* nombre) {
        if (ok) std::cout << "OK: " << nombre << '\n';
        else { ++errores; std::cerr << "FALLO: " << nombre << '\n'; }
    };
    {
        Tablero t;
        preparar(t);
        comprobar(moverFicha(t, 13, 8, PUMA, -1), "PUMA PUEDE MOVERSE EN PARTIDA ACTIVA");
        comprobar(moverFicha(t, 8, 13, PUMA, -1), "PUMA PUEDE REGRESAR");
        comprobar(moverFicha(t, 14, 9, PERRO, 0), "PERRO PUEDE MOVERSE EN PARTIDA ACTIVA");
        comprobar(moverFicha(t, 9, 14, PERRO, 0), "PERRO PUEDE REGRESAR CON SU IDENTIDAD");
        comprobar(capturarPerro(t, 13, 14, 15) && t.capturados == 3 &&
                  t.celdas[15] == PUMA && t.celdas[14] == VACIA && t.idPerro[14] == -1,
                  "CAPTURA LEGAL PERMITIDA");
        destruirTablero(t);
    }
    {
        Tablero t;
        preparar(t);
        terminarPartida(t);
        comprobar(partidaTerminada(t), "PARTIDA FINALIZADA");
        const Estado e = guardar(t);
        comprobar(!moverFicha(t, 13, 8, PUMA, -1) && igual(t, e),
                  "PUMA NO PUEDE MOVERSE ( PARRIDA TERMINO)");
        comprobar(!moverFicha(t, 14, 9, PERRO, 0) && igual(t, e),
                  "PERRO NO PUEDE MOVERSE ( PARTIDA TERMINO)");
        comprobar(!capturarPerro(t, 13, 14, 15) && igual(t, e),
                  "PUMA NO PUEDE CAPTURAR");
        destruirTablero(t);
    }
    {
        Tablero t;
        preparar(t); // SI UN HILO TERMINA LA PARTIDA OTRO HILO NO PUEDE MOVER LAS FICHAS
        Intentos a{&t};
        pthread_t th;
        if (pthread_create(&th, nullptr, intentarDespues, &a) != 0) {
            std::cerr << "FALLO: pthread_create\n";
            return EXIT_FAILURE;
        }
        pthread_mutex_lock(&a.mtx);
        while (!a.preparado) pthread_cond_wait(&a.cond, &a.mtx);
        terminarPartida(t);
        a.continuar = true;
        pthread_cond_broadcast(&a.cond);
        pthread_mutex_unlock(&a.mtx);
        if (pthread_join(th, nullptr) != 0) std::exit(EXIT_FAILURE);
        comprobar(a.vioActiva && a.rechazados, "COMPROBAR 300 INTENTOS");
        Estado esperado;
        esperado.celdas.fill(VACIA);
        esperado.ids.fill(-1);
        esperado.celdas[13] = PUMA;
        esperado.celdas[14] = PERRO;
        esperado.ids[14] = 0;
        esperado.capturados = 2;
        esperado.fin = true;
        comprobar(igual(t, esperado), "TABLERO IGUAL AL ESPERADO");
        pthread_cond_destroy(&a.cond);
        pthread_mutex_destroy(&a.mtx);
        destruirTablero(t);
    }
    {
        Tablero t;
        preparar(t);
        t.capturados = 5;
        t.celdas[16] = PERRO; t.idPerro[16] = 1;
        t.celdas[19] = PERRO; t.idPerro[19] = 2;
        comprobar(capturarPerro(t, 13, 14, 15) && partidaTerminada(t) && t.capturados == 6,
                  "SEXTA CAPTURA -> SE TERMINA LA PARTIDA SIN JUEZ");
        comprobar(ganador(t) == GANA_PUMA, "GANADOR -> DEBE DECIR PUMA");
        const Estado e = guardar(t);
        comprobar(!moverFicha(t, 15, 10, PUMA, -1) && igual(t, e),
                  "PUMA NO SE MUEVE DSP DE LA SEXTA CAPTURA");
        comprobar(!moverFicha(t, 19, 14, PERRO, 2) && igual(t, e),
                  "PERRO NO SE MUEVE DSP DE LA SEXTA CAPTURA");
        comprobar(!capturarPerro(t, 15, 16, 17) && igual(t, e),
                  "NO SE ACEPTA UNA SEPTIMA CAPTURA");
        destruirTablero(t);
    }
    {
        Tablero t; // PERROS BLOQUEAN AL PUMA
        iniciarTablero(t);
        t.celdas[TRONO] = PUMA;
        const int perros[] = {1, 2, 5, 3, 7, 11};
        for (int id = 0; id < 6; ++id) {
            t.celdas[perros[id]] = PERRO;
            t.idPerro[perros[id]] = id;
        }
        comprobar(ganador(t) == NADIE && !partidaTerminada(t),
                  "ANTES DEL BLOQUEO TODAVIA NO HAY GANADOR");
        comprobar(moverFicha(t, 11, 10, PERRO, 5) && partidaTerminada(t),
                  "PUMA BLOQUEADO SE CIERRA LA PARTIDA");
        comprobar(ganador(t) == GANAN_PERROS, "GANADOR DEBE SER PERROS");
        const Estado e = guardar(t);
        comprobar(!moverFicha(t, 3, 8, PERRO, 3) && igual(t, e),
                  "PERRO NO PUEDE MOVERSE YA QUE LA PARTIDA TERMINO");
        comprobar(!moverFicha(t, 0, 1, PUMA, -1) && !capturarPerro(t, 0, 1, 3) && igual(t, e),
                  "PUMA NO PUEDE MOVERSE YA QUE LA PARTIDA TERMINO");
        destruirTablero(t);
    }
    // LA PARTIDA TERMINO PERO FIN == FALSE
    for (bool captura : {false, true}) {
        Tablero t;
        preparar(t);
        t.capturados = 6;
        Estado e = guardar(t);
        e.fin = true;
        const bool ok = captura ? capturarPerro(t, 13, 14, 15)
                                : moverFicha(t, 13, 8, PUMA, -1);
        comprobar(!ok && igual(t, e) && ganador(t) == GANA_PUMA,
                  "PUMA YA GANO");
        destruirTablero(t);
    }
    {
        Tablero t;
        iniciarTablero(t);
        t.celdas[0] = PUMA;
        const int perros[] = {1, 2, 5, 3, 7, 10};
        for (int id = 0; id < 6; ++id) {
            t.celdas[perros[id]] = PERRO;
            t.idPerro[perros[id]] = id;
        }
        Estado e = guardar(t);
        e.fin = true;
        comprobar(!moverFicha(t, 3, 8, PERRO, 3) && igual(t, e) && ganador(t) == GANAN_PERROS,
                  "PERROS YA GANARON PUMA NO PUEDE NI DEBERIA MOVERSE");
        destruirTablero(t);
    }
    std::cout << "RESULTADO: " << errores << " FALLOS.\n";
    return errores == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
