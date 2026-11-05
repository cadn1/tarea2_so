// EJECUTABLE SOLO DE PRUEBA: INTERCEPTA SOLO EL PTHREAD CREATE
// Compilar con C++17, -pthread, -I src y -Wl,--wrap=pthread_create.
#define main mainTorneo
#include "../src/main.cpp"
#undef main

namespace {
std::vector<int> orden;
int limite;

bool entero(const std::string& texto, int& valor) {
    const auto r = std::from_chars(texto.data(), texto.data() + texto.size(), valor);
    return r.ec == std::errc{} && r.ptr == texto.data() + texto.size();
}

void* llegarEnOrden(void* p) {
    const auto& a = *static_cast<ArgJugador*>(p);
    Torneo& t = *a.torneo;
    const int turno = static_cast<int>(std::find(orden.begin(), orden.end(), a.id) - orden.begin());
    pthread_mutex_lock(&t.mtx);
    // EL SGTE HILO SOLO PASA DESPUES DEL REGISTRO REAL DEL ANTERIOR
    // SIGINT TAMBIEN LIBERA A LOS JUGADORES RETENIDOS POR ESTE TEST!!!
    while (!t.detener && !t.fallo && (t.llegados < turno || turno >= limite))
        pthread_cond_wait(&t.cond, &t.mtx);
    pthread_mutex_unlock(&t.mtx);
    void* resultado = hiloJugador(p);
    pthread_mutex_lock(&t.mtx);
    const bool sinPareja = t.jugadores % 2 != 0 && turno == t.jugadores - 1;
    const int parejaEsperada = sinPareja ? -1 : turno / 2;
    const Celda rolEsperado = sinPareja ? VACIA : (turno % 2 == 0 ? PUMA : PERRO);
    const Jugador& jugador = t.participantes[a.id - 1];
    const bool valido = t.detener || t.fallo ||
        (jugador.pareja == parejaEsperada && jugador.rol == rolEsperado &&
         (sinPareja || (t.parejas[parejaEsperada].puma == orden[turno - turno % 2] &&
                        t.parejas[parejaEsperada].perros == orden[turno - turno % 2 + 1])));
    pthread_mutex_unlock(&t.mtx);
    if (!valido) {
        std::cerr << "FALLO: PAREJA NO COINCIDE CON EL ORDEN CONTROLADO\n";
        std::_Exit(2);
    }
    return resultado;
}
}

extern "C" int __real_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
extern "C" int __wrap_pthread_create(pthread_t* hilo, const pthread_attr_t* attr,
                                     void* (*funcion)(void*), void* arg) {
    return __real_pthread_create(hilo, attr, funcion == hiloJugador ? llegarEnOrden : funcion, arg);
}

int main(int argc, char* argv[]) {
    int n = 0;
    const char* lista = std::getenv("KOMIKAN_ORDEN_PRUEBA");
    if (argc < 3 || argc > 4 || !entero(argv[1], n) || n <= 0 || !lista) {
        std::cerr << "USO: KOMIKAN_ORDEN_PRUEBA=4,1,3,2 " << argv[0] << " N M [config.txt]\n";
        return 1;
    }
    std::vector<bool> vistos(n, false);
    std::string pendientes(lista);
    while (true) {
        const auto coma = pendientes.find(',');
        int id = 0;
        if (!entero(pendientes.substr(0, coma), id) || id < 1 || id > n || vistos[id - 1]) {
            std::cerr << "ERROR: EL ORDEN DEBE CONTENER CADA ID de 1 a N SOLO 1 VEZ.\n";
            return 1;
        }
        vistos[id - 1] = true;
        orden.push_back(id);
        if (coma == std::string::npos) break;
        pendientes.erase(0, coma + 1);
    }
    limite = n;
    const char* pausa = std::getenv("KOMIKAN_LIMITE_PRUEBA");
    if (orden.size() != static_cast<std::size_t>(n) ||
        (pausa && (!entero(pausa, limite) || limite < 0 || limite > n))) {
        std::cerr << "ERROR: orden incompleto o limite fuera de 0..N.\n";
        return 1;
    }
    return mainTorneo(argc, argv);
}
