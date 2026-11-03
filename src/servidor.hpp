#ifndef SERVIDOR_HPP
#define SERVIDOR_HPP

#include <semaphore.h>
#include <cerrno>
#include <vector>
#include "mesa.hpp"

struct ArgMesa { // datos que recibe el hilo de cada mesa
    int id;
    int jugPuma, jugPerros;
    const Config* cfg;
    sem_t* cupos;
    const volatile sig_atomic_t* parar;
};

inline void* hiloMesa(void* p) { // el hilo de la mesa hace de Juez
    ArgMesa* a = static_cast<ArgMesa*>(p);
    std::cout << "[MESA " << a->id << "] INICIADA: JUGADOR " << a->jugPuma
              << " (PUMA) VS JUGADOR " << a->jugPerros << " (PERROS)" << std::endl;
    jugarPartida(*a->cfg, a->id, a->parar);
    sem_post(a->cupos); // la mesa se libera y entra el siguiente par
    return nullptr;
}

inline void servir(int N, int M, const Config& cfg, const volatile sig_atomic_t* parar = nullptr) {
    int partidas = N / 2; // cada par de jugadores forma una partida
    sem_t cupos;
    sem_init(&cupos, 0, M); // M mesas simultaneas como maximo
    std::vector<ArgMesa> args(partidas); // reservado de una vez para que los punteros no cambien
    std::vector<pthread_t> th;
    for (int i = 0; i < partidas; i++) {
        if (parar && *parar) break;
        if (sem_trywait(&cupos) != 0) { // no hay mesa libre
            std::cout << "[ESPERA] JUGADORES " << 2 * i + 1 << " y " << 2 * i + 2
                      << " esperan una mesa libre" << std::endl;
            bool ok = false;
            while (!ok) { // espera pasiva en el semaforo
                if (sem_wait(&cupos) == 0) ok = true;
                else if (errno != EINTR || (parar && *parar)) break;
            }
            if (!ok) break;
        }
        args[i] = ArgMesa{i + 1, 2 * i + 1, 2 * i + 2, &cfg, &cupos, parar};
        pthread_t h;
        if (pthread_create(&h, nullptr, hiloMesa, &args[i]) != 0) {
            sem_post(&cupos); // no se pudo crear, devuelve el cupo
            break;
        }
        th.push_back(h);
    }
    if (N % 2 == 1) std::cout << "[ESPERA] JUGADOR " << N << " sin pareja" << std::endl;
    for (size_t i = 0; i < th.size(); i++) pthread_join(th[i], nullptr);
    sem_destroy(&cupos);
}

#endif
