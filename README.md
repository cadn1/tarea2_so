# Torneo de Komikan Concurrente — Tarea 2 de Sistemas Operativos

Implementación en **C++17 con POSIX Threads (`pthreads`)** de un torneo automático de Komikan. Cada partida enfrenta a un **Puma** contra **12 Perros** en un tablero de **28 nodos**. Las fichas se mueven concurrentemente, sin turnos estrictos; un servidor forma parejas de jugadores y administra hasta `M` mesas de juego simultáneas.

> **Modalidad:** el juego se simula automáticamente. No se escriben movimientos en la terminal. `N` y `M` son parámetros del torneo, no coordenadas del tablero.

## 1. Requisitos y compilación

- Sistema Unix/Linux con `g++` y soporte para C++17 y pthreads (proyecto utilizado en Linux/CachyOS).
- No requiere librerías externas.

Desde la raíz del repositorio:

```bash
g++ -Wall -Wextra -std=c++17 -pthread -I src src/main.cpp -o torneo
```

También se ha comprobado la compilación con `-Werror` añadido a esos parámetros.

## 2. Cómo ejecutar el torneo

```bash
./torneo N M [config.txt]
```

| Parámetro | Significado |
|---|---|
| `N` | Número de jugadores (hilos de jugadores). Debe ser un entero positivo. |
| `M` | Número máximo de mesas activas simultáneamente. Debe ser un entero positivo. |
| `config.txt` | Archivo opcional de configuración. Si se omite, se usan los valores predeterminados. |

Ejemplos:

```bash
./torneo 2 1
./torneo 4 2
./torneo 6 2 config.txt
```

Cada jugador registra su llegada. **Los dos primeros que llegan forman la primera pareja, los dos siguientes la segunda**, y así sucesivamente; no se emparejan por ID fijo. El primero de cada pareja representa al **Puma** y el segundo a los **Perros**. Si `N` es impar, el último en llegar queda sin pareja y termina limpiamente al finalizar el torneo.

Existen como máximo `min(M, N/2)` trabajadores de mesa. Si hay parejas pendientes, esperan pasivamente; una mesa liberada puede atender otra pareja. El orden de llegada puede variar entre ejecuciones por la planificación de los hilos.

## 3. Reglas implementadas

- El tablero posee **28 nodos**: `0` es el trono; `1` y `2` son nodos superiores; los nodos `3` a `27` forman una cuadrícula de 5×5.
- **Puma:** ocupa una casilla; se mueve a posiciones adyacentes vacías y puede capturar saltando sobre un Perro a una posición vacía válida. Existen trayectorias especiales en el sector superior del tablero.
- **Perros:** son 12 fichas con identidad individual; avanzan a casillas adyacentes vacías, **sin capturar**. No realizan movimientos diagonales no permitidos por el tablero.
- **Victoria del Puma:** captura **6 Perros**.
- **Victoria de los Perros:** el Puma queda sin movimientos simples ni capturas posibles.
- **Sin turnos:** cada ficha intenta actuar mediante su propio hilo, con retardos aleatorios configurables.

No se garantiza que haya un ganador antes del límite de tiempo: una partida puede cerrar por `TIEMPO`.

## 4. Cómo leer los tableros y los resultados

El tablero se imprime usando texto ASCII:

| Texto/símbolo | Significado |
|---|---|
| `Puma` | Ubicación actual del Puma. |
| `Perro` | Ubicación de un Perro. La identidad de cada perro se conserva internamente. |
| `.` | Posición vacía. |
| Líneas (`-`, `|`, `/`, `\`) | Dibujo de las conexiones del tablero. |

**Se muestran tres tipos de tablero:**

1. **`[MESA X] tablero inicial:`** muestra las posiciones antes de comenzar a mover las fichas.
2. **`[MESA X] estado:`** es una fotografía periódica de la partida. Su frecuencia aproximada depende de `logCadaMs`; **no representa cada movimiento individual**.
3. **`[MESA X] tablero final:`** muestra las posiciones al terminar esa partida.

Las mesas imprimen sus tableros independientemente. `X` identifica la mesa, no la pareja; una misma mesa puede atender varias parejas.

Los eventos más importantes tienen este formato:

```text
[JUGADOR 4] llegada registrada. ORDEN 1
[PAREJA 1] EMPAREJADOS: JUGADOR 4 (PUMA) VS JUGADOR 1 (PERROS)
[MESA 2] INICIO PAREJA 1
[MESA 2] tablero inicial:
...
[MESA 2] estado:
...
[MESA 2] tablero final:
...
[MESA 2] fin. Ganador: PUMA | vivos: 6 | capturados: 6 | total: 12 | cierre: VICTORIA
[MESA 2] LIBERADA. PAREJA 1 terminada. Resultado: 1 | cierre: VICTORIA
[TORNEO] cierre normal. Partidas cerradas: 2 / 2
```

Este bloque es **ilustrativo**: los IDs, mesa, posiciones, ganador y cantidad de capturas cambian entre ejecuciones.

En la línea de resultado:

- **`vivos`**: Perros que quedan en el tablero; **`capturados`**: Perros retirados; la suma normalmente es 12.
- **`Ganador`**: `PUMA`, `PERROS` o `NADIE`.
- **`cierre`**: `VICTORIA` (regla del juego), `TIEMPO`, `ABANDONO`, `SIGINT` o `FALLO INTERNO`.
- **`Resultado`**: `1` = Puma, `2` = Perros, `0` = sin ganador.
- **`LIBERADA`**: la mesa terminó la partida y puede atender otra pareja.
- **`Partidas cerradas: X / Y`**: resumen del torneo; puede ser menor que `Y` si se interrumpe.

### Guardar el registro sin saturar la terminal

```bash
./torneo 6 2 config.txt > torneo.log 2>&1
grep -E 'EMPAREJADOS|INICIO PAREJA|fin\. Ganador|LIBERADA|cierre normal|cierre por SIGINT' torneo.log
less torneo.log       # presionar q para salir
```

Se recomienda **redirigir a un archivo regular** cuando hay muchos tableros. Una salida dirigida a un pipe lleno sin consumidor puede bloquear al escritor y retrasar el cierre por SIGINT: es una limitación conocida.

## 5. Archivo `config.txt`

El repositorio incluye un ejemplo. Cada línea utiliza `clave=valor`, permite espacios alrededor del signo `=` y admite comentarios con `#`.

```ini
minMs=50
maxMs=300
semilla=42
maxSegundos=60
logCadaMs=2000
puma=0
perros=13,17,18,23,19,24,20,25,21,26,22,27
```

| Clave | Valor predeterminado | Uso |
|---|---|---|
| `minMs` | `50` | Retardo mínimo de una ficha, en milisegundos. |
| `maxMs` | `300` | Retardo máximo; debe ser mayor o igual a `minMs`. |
| `semilla` | `1` | Semilla inicial de la selección aleatoria. |
| `maxSegundos` | `60` | Límite **aproximado** de duración de cada partida. |
| `logCadaMs` | `2000` | Intervalo **aproximado** de impresión del estado. |
| `puma` | `0` | Nodo inicial del Puma. |
| `perros` | `13,17,18,23,19,24,20,25,21,26,22,27` | Exactamente 12 posiciones de perros. |

Las posiciones deben estar entre `0` y `27`, sin superposiciones. Una configuración inválida o un archivo inexistente se rechazan **antes de crear los hilos del torneo**. Si se omite una clave, se usa su valor predeterminado.

La semilla fija las secuencias aleatorias propias de los hilos; **no garantiza tableros finales idénticos**, porque el sistema operativo puede ejecutar los hilos en diferente orden.

### Configuración breve para ensayar

Guardar, por ejemplo, como `config_corta.txt`:

```ini
minMs=50
maxMs=100
maxSegundos=2
logCadaMs=1000
```

Y ejecutar:

```bash
./torneo 4 2 config_corta.txt > prueba.log 2>&1
```

## 6. Abandono, fallos controlados y SIGINT

El abandono se notifica desde el hilo del jugador y el juez finaliza solo **su partida**. Si abandona el Puma, ganan los Perros; si abandona el jugador de los Perros, gana el Puma. Las demás mesas continúan jugando.

Para **simular** esos eventos se aceptan variables de entorno (indican el **ID real** del jugador, no su rol):

```bash
KOMIKAN_ABANDONA=1 ./torneo 2 1 config_corta.txt
KOMIKAN_FALLO_JUGADOR=2 ./torneo 2 1 config_corta.txt
```

Usar solo una variable a la vez. **El rol depende del orden de llegada**: para saber quién abandona como Puma o Perros, revisar el mensaje `EMPAREJADOS`. Una simulación de fallo controlado no equivale a poder recuperarse de una caída arbitraria del proceso o de un hilo.

**Para interrumpir todo el torneo:** presionar `Ctrl+C`. El receptor de `SIGINT` solicita parada global, despierta las esperas y finaliza ordenadamente cuando los hilos pueden continuar; se devuelve código **130**. SIGINT **no equivale** a abandono de un jugador. Un error interno global devuelve normalmente código **1** y una ejecución normal código **0**.

## 7. Arquitectura y sincronización

- **N hilos de jugadores:** registran llegadas, se emparejan y esperan resultados.
- **Hasta `min(M, N/2)` hilos de mesa:** cada uno hace de juez, coordina una partida y reutiliza la mesa.
- **13 hilos de fichas por partida activa:** un Puma y doce Perros, salvo terminación inmediata o fallos durante su creación.
- **Un receptor de señales:** espera SIGINT mediante `sigwait()`.
- **Mutex y condición del torneo:** protegen parejas, espera pasiva, paradas y resultados.
- **Mutex y condición por tablero:** serializan cada movimiento/captura y permiten despertar las fichas al cerrar.
- **Mutex global de consola:** evita que se intercalen las impresiones de distintas mesas.
- **`pthread_join()`:** se espera la finalización de los hilos creados antes de liberar los recursos compartidos utilizados.

La decisión de usar **un mutex por tablero** simplifica la integridad de los movimientos y capturas, aunque impide realizar dos modificaciones simultáneas dentro de la misma partida. **Mesas diferentes sí pueden jugar concurrentemente.**

## 8. Archivos y pruebas

```text
src/main.cpp       Servidor, emparejamiento por llegada, mesas, SIGINT y jugadores
src/mesa.hpp       Partida, juez, hilos de fichas, cierre y resultados
src/tablero.hpp    Geometría, movimientos, capturas y condiciones de victoria
src/config.hpp     Lectura y validación de configuración
tests/prueba_*.cpp Pruebas C++ de geometría, saltos, capturas, concurrencia y cierres
config.txt         Ejemplo de configuración
README.md          Esta guía
informe.pdf        Informe técnico de entrega (máximo 2 páginas)
```

Ejemplo para compilar y ejecutar una prueba individual:

```bash
g++ -Wall -Wextra -Werror -std=c++17 -pthread -I src tests/prueba_cierre.cpp -o /tmp/prueba_cierre
/tmp/prueba_cierre
```

La prueba de **orden controlado de llegadas** necesita una opción de enlace especial porque intercepta `pthread_create` solo en el ejecutable de pruebas:

```bash
g++ -Wall -Wextra -Werror -std=c++17 -pthread -I src tests/prueba_llegadas.cpp -Wl,--wrap=pthread_create -o /tmp/prueba_llegadas
KOMIKAN_ORDEN_PRUEBA=4,1,3,2 /tmp/prueba_llegadas 4 2 config_corta.txt
```

Se esperan las parejas **4–1** y **3–2**. La variable `KOMIKAN_ORDEN_PRUEBA` pertenece a ese ejecutable de pruebas, **no al servidor normal**.

## 9. Limitaciones y alcance

- La simulación es automática, no un juego interactivo por teclado.
- La planificación concurrente puede cambiar el orden de movimientos aunque se repita la semilla.
- `maxSegundos` y `logCadaMs` son tiempos aproximados, calculados por ciclos del juez.
- La capacidad para N y M elevados depende de los recursos disponibles; no se garantiza ejecutar cualquier cantidad de hilos.
- Los abandonos y fallos locales detectados pueden notificarse. No hay recuperación general frente a corrupción de memoria, bloqueos irreversibles o caídas del proceso.
- Si la salida estándar se bloquea por falta de consumidor, el cierre puede demorarse. Para conservar registros en torneos grandes, usar un archivo regular.

## Autores

- Juan Pablo Manríquez
- Cristóbal Deck
