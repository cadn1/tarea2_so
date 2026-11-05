# Komikan Concurrente

Torneo concurrente de **Komikan** (Tarea 2 de Sistemas Operativos): un Puma contra 12 Perros en un tablero de 28 nodos, con jugadores, mesas de juego y árbitros que corren simultáneamente usando hilos POSIX, mutex y variables de condición.

## Requisitos

- Sistema operativo tipo Unix (Linux, probado en Ubuntu 24.04)
- Compilador `g++` con soporte para C++17 y pthreads
- No requiere librerías externas adicionales

## Compilación

```bash
g++ -Wall -Wextra -Werror -std=c++17 -pthread -I src src/main.cpp -o torneo
```

## Estructura del Directorio

```text
📁 tarea2_so/
├── 📁 src/
│   ├── config.hpp
│   ├── main.cpp
│   ├── mesa.hpp
│   └── tablero.hpp
├── 📁 tests/
│   └── prueba_*.cpp
├── config.txt
├── informe.pdf
└── README.md
```

## Uso

```bash
./torneo N M [config.txt]
```

| Argumento | Descripción |
|---|---|
| `N` | Cantidad de jugadores que llegan al torneo |
| `M` | Máximo de mesas (partidas) simultáneas |
| `config.txt` | Archivo opcional de configuración (ver más abajo) |

Ejemplos:
```bash
./torneo 4 2                 # 2 parejas, hasta 2 partidas a la vez
./torneo 6 2 config.txt      # 3 parejas, 2 mesas: la tercera pareja espera una mesa libre
```

Los jugadores se emparejan por orden de llegada, de a dos: el primero es el Puma y el segundo los Perros. Si `N` es impar, un jugador queda sin pareja. El programa juega solo; no se ingresan comandos. Con `Ctrl+C` el torneo se detiene de forma ordenada y termina con código `130`.

## Reglas implementadas

- El Puma se mueve a casillas adyacentes libres y captura saltando en línea recta sobre un perro hacia una casilla vacía.
- Los 12 Perros se mueven a casillas adyacentes libres y no capturan.
- **Gana el Puma** al capturar 6 perros. **Ganan los Perros** si el Puma queda sin movimientos ni capturas posibles.
- No hay turnos: cada ficha se mueve por su cuenta, con retardos aleatorios entre `minMs` y `maxMs` (por defecto 50 a 300 ms).
- Si un jugador abandona, gana su oponente.

El tablero es un grafo de 28 nodos: `0` es el trono, `1` y `2` los nodos del techo, y `3` a `27` la cuadrícula 5×5 por filas (el nodo de la fila `f`, columna `c` es `3 + 5*f + c`). Se imprime en ASCII.

## Salida

Eventos principales (ejemplo con `./torneo 6 2 config.txt`):

```text
[PAREJA 1] EMPAREJADOS: JUGADOR 1 (PUMA) VS JUGADOR 2 (PERROS)
[MESA 1] INICIO PAREJA 1
[MESA 1] fin. Ganador: PUMA | vivos: 6 | capturados: 6 | total: 12 | cierre: VICTORIA
[MESA 1] LIBERADA. PAREJA 1 terminada. Resultado: 1 | cierre: VICTORIA
[TORNEO] cierre normal. Partidas cerradas: 3 / 3
```

`cierre` puede ser `VICTORIA`, `ABANDONO`, `TIEMPO`, `SIGINT` o `FALLO INTERNO`. Resultado: `0` sin ganador, `1` gana el Puma, `2` ganan los Perros.

## Archivo de configuración

El archivo `config.txt` es opcional. Sirve para cambiar los ajustes de las partidas sin tocar el código: la velocidad de las fichas, la semilla del azar y la posición inicial del Puma y los Perros. Si no se entrega, se usan los valores por defecto.

Se usa como tercer argumento:

```bash
./torneo 4 2 config.txt
```

Cada línea tiene el formato `clave=valor`, y todo lo que está después de `#` es un comentario. El repositorio incluye un `config.txt` de ejemplo con los valores por defecto (salvo la semilla, que es 42).

| Clave | Por defecto | Qué cambia |
|---|---|---|
| `minMs`, `maxMs` | 50, 300 | Cada ficha espera un tiempo al azar entre estos dos valores (en milisegundos) antes de moverse |
| `semilla` | 1 | Con la misma semilla las decisiones al azar se repiten |
| `maxSegundos` | 60 | Duración máxima de una partida |
| `logCadaMs` | 2000 | Cada cuántos milisegundos se imprime el tablero |
| `puma` | 0 | Nodo donde parte el Puma (0 es el trono) |
| `perros` | 13,17,18,23,19,24,20,25,21,26,22,27 | Nodos donde parten los 12 perros |

Los nodos van de 0 a 27 y no pueden repetirse ni coincidir con el del Puma. Si algún valor es inválido, el programa muestra un error con el número de línea y no inicia el torneo.

## Simulación de abandono o fallo

Para probar el manejo de desconexiones hay dos variables de entorno, solo para pruebas (no usar ambas a la vez). Reciben el ID de un jugador que tenga pareja:

```bash
KOMIKAN_ABANDONA=1 ./torneo 2 1 config.txt        # el jugador 1 abandona
KOMIKAN_FALLO_JUGADOR=2 ./torneo 2 1 config.txt   # el jugador 2 sufre un fallo controlado
```

El oponente gana con `cierre: ABANDONO`. Como el rol depende del orden de llegada, quién gana puede variar entre ejecuciones.

## Pruebas

Los tests están en `tests/` y se compilan uno a uno, por ejemplo:

```bash
g++ -Wall -Wextra -Werror -std=c++17 -pthread -I src tests/prueba_cierre.cpp -o prueba_cierre && ./prueba_cierre
```

`tests/prueba_llegadas.cpp` necesita además `-Wl,--wrap=pthread_create` y se ejecuta con un orden de llegada forzado: `KOMIKAN_ORDEN_PRUEBA=4,1,3,2 ./prueba_llegadas 4 2`.

## Autores

- Juan Pablo Manriquez
- Cristobal Deck
