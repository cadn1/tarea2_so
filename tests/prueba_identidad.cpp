#include <cstdlib>
#include <iostream>
#include <set>
#include "mesa.hpp"

// LA COMDICION ORDENA LOS EVENTOS -> NINGUN RETARDO ESTABLECE SU ORDEN
struct Puerta {
    pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    bool abierta = false;
    void esperar() {
        pthread_mutex_lock(&mtx);
        while (!abierta) pthread_cond_wait(&cond, &mtx);
        pthread_mutex_unlock(&mtx);
    }
    void abrir() {
        pthread_mutex_lock(&mtx);
        abierta = true;
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&mtx);
    }
    ~Puerta() {
        pthread_cond_destroy(&cond);
        pthread_mutex_destroy(&mtx);
    }
};

struct Intento {
    Tablero* t;
    Puerta* inicio;
    Puerta* terminado;
    bool captura;
    bool ok = false;
};

void* intentar(void* p) {
    auto& a = *static_cast<Intento*>(p);
    a.inicio->esperar();
    a.ok = a.captura ? capturarPerro(*a.t, 13, 14, 15)
                      : moverFicha(*a.t, 14, 9, PERRO, 0);
    a.terminado->abrir();
    return nullptr;
}

struct HiloCapturado {
    ArgFicha ficha;
    Puerta* inicio;
    Puerta* terminado;
};

void* ejecutarCapturado(void* p) {
    auto& a = *static_cast<HiloCapturado*>(p);
    a.inicio->esperar();
    hiloPerro(&a.ficha);
    a.terminado->abrir();
    return nullptr;
}

bool integridad(const Tablero& t) {  // SOLO ANTES DE CREAR HILOS O DESPUES DEL JOIN
    std::set<int> ids;
    for (int i = 0; i < NODOS; ++i) {
        if (t.celdas[i] == PERRO) {
            if (t.idPerro[i] < 0 || t.idPerro[i] >= 12 || !ids.insert(t.idPerro[i]).second)
                return false;
        } else if (t.idPerro[i] != -1) return false;
    }
    return true;
}

int main() {
    int errores = 0;
    auto comprobar = [&](bool ok, const char* nombre) {
        if (!ok) { ++errores; std::cerr << "FALLO: " << nombre << '\n'; }
        else std::cout << "OK: " << nombre << '\n';
    };
    auto crear = [&](pthread_t& th, void* (*f)(void*), void* arg) {
        if (pthread_create(&th, nullptr, f, arg) != 0) {
            std::cerr << "FALLO: pthread_create\n";
            std::exit(EXIT_FAILURE);
        }
    };
    auto unirHilo = [&](pthread_t th) {
        comprobar(pthread_join(th, nullptr) == 0, "pthread_join");
    };
    {
        Tablero t;
        if (iniciarTablero(t) != 0) std::exit(EXIT_FAILURE);
        colocarFichasIniciales(t);
        std::set<int> ids;
        for (int i = 0; i < NODOS; ++i) if (t.idPerro[i] >= 0) ids.insert(t.idPerro[i]);
        comprobar(integridad(t) && ids.size() == 12, "DOCE IDENTIDADES INICIALES UNICAS");
        destruirTablero(t);
    }
    {
        Tablero t;
        if (iniciarTablero(t) != 0) std::exit(EXIT_FAILURE);
        t.celdas[13] = PUMA;
        t.celdas[14] = PERRO; t.idPerro[14] = 0;
        t.celdas[19] = PERRO; t.idPerro[19] = 1;
        Celda antes[NODOS]; int idsAntes[NODOS];
        for (int i = 0; i < NODOS; ++i) { antes[i] = t.celdas[i]; idsAntes[i] = t.idPerro[i]; }
        comprobar(!moverFicha(t, 14, 9, PERRO, 1), "RECHAZA IDENTIDAD INCORRECTA");
        bool igual = t.capturados == 0 && !t.fin;
        for (int i = 0; i < NODOS; ++i)
            igual = igual && antes[i] == t.celdas[i] && idsAntes[i] == t.idPerro[i];
        comprobar(igual, "RECHAZO CONSERVA TABLERO E IDENTIDADES");

        Puerta inicio, terminado;
        HiloCapturado a{{&t, 14, PERRO, 42, 0, 50, 300}, &inicio, &terminado};
        pthread_t th;
        crear(th, ejecutarCapturado, &a);
        comprobar(capturarPerro(t, 13, 14, 15), "CAPTURA AL PERRO 0");
        comprobar(t.celdas[14] == VACIA && t.idPerro[14] == -1 && t.capturados == 1,
                  "CAPTURA ELIMINA FICHA E IDENTIDAD");
        comprobar(moverFicha(t, 19, 14, PERRO, 1), "SUPERVIVIENTE REOCUPA ANTIGUA CASILLA");
        comprobar(!moverFicha(t, 14, 9, PERRO, 0), "CAPTURADO NO PUEDE MOVER AL SUPERVIVIENTE");
        inicio.abrir();
        terminado.esperar(); // TIMEOUT EXTERNO DETECTA SI EL HILO NO RETORNA
        unirHilo(th);
        comprobar(!partidaTerminada(t) && a.ficha.pos == 14 && t.idPerro[14] == 1,
                  "HILO CAPTURADO TERMINA CON PARTIDA ACTIVA SIN MOVER OTRA FICHA");
        bool movimientos = true;
        for (int i = 0; i < 100; ++i) {
            movimientos = moverFicha(t, 14, 9, PERRO, 1) && movimientos;
            movimientos = integridad(t) && movimientos;
            movimientos = moverFicha(t, 9, 14, PERRO, 1) && movimientos;
            movimientos = integridad(t) && movimientos;
        }
        comprobar(movimientos && t.idPerro[14] == 1 && t.capturados == 1,
                  "SUPERVIVIENTE CONSERVA IDENTIDAD TRAS 200 MOVIMIENTOS");
        destruirTablero(t);
    }
    for (int caso = 0; caso < 12; ++caso) {
        Tablero t;
        if (iniciarTablero(t) != 0) std::exit(EXIT_FAILURE);
        t.celdas[13] = PUMA;
        t.celdas[14] = PERRO; t.idPerro[14] = 0;
        Puerta inicioCaptura, inicioMovimiento, finCaptura, finMovimiento;
        Intento c{&t, &inicioCaptura, &finCaptura, true};
        Intento m{&t, &inicioMovimiento, &finMovimiento, false};
        pthread_t tc, tm;
        crear(tc, intentar, &c); crear(tm, intentar, &m);
        if (caso == 0) {
            inicioCaptura.abrir(); finCaptura.esperar(); inicioMovimiento.abrir();
        } else if (caso == 1) {
            inicioMovimiento.abrir(); finMovimiento.esperar(); inicioCaptura.abrir();
        } else {
            inicioCaptura.abrir(); inicioMovimiento.abrir();
        }
        unirHilo(tc); unirHilo(tm);
        bool valido = c.ok != m.ok;
        if (c.ok) valido = valido && t.celdas[15] == PUMA && t.celdas[14] == VACIA &&
                                      t.celdas[9] == VACIA && t.capturados == 1;
        else valido = valido && t.celdas[13] == PUMA && t.celdas[14] == VACIA &&
                               t.idPerro[9] == 0 && t.capturados == 0;
        if (caso == 0) valido = valido && c.ok;
        if (caso == 1) valido = valido && m.ok;
        comprobar(valido && integridad(t), "CAPTURA Y MOVIMIENTOS CONCURRENTES CONSISTENTES");
        destruirTablero(t);
    }
    std::cout << "RESULTADOS: " << errores << " FALLOS.\n";
    return errores == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
