# Cableado

## DHT11
Modulo de 3 pines: VCC -> 3V3, GND -> GND, DATA -> GPIO27.

DHT11 suelto de 4 pines:
- Pin 1: VCC (3V3)
- Pin 2: DATA -> GPIO27, con resistencia de 10 kOhm a 3V3
- Pin 3: sin conexion
- Pin 4: GND

## TTP223 (tactil)
VCC -> 3V3, GND -> GND, I/O -> GPIO32.

## Brillo de pantalla (opcional)
TFT BL -> un GPIO libre (por ejemplo 25) y poner `#define BACKLIGHT_PIN 25` en `src/config.h`.
Por defecto es -1 (sin regulacion de brillo).

## Prueba final
Antes de flashear: copiar `src/secrets.h.example` a `src/secrets.h` y rellenar
los valores reales (`WIFI_SSID`, `WIFI_PASS`, `API_URL`, `API_KEY`, `TZ_OFFSET`, `NOWPLAYING_URL`). `secrets.h` no se sube a git.

Flashear con `pio run -e esp32dev -t upload` y abrir el monitor serie
(`pio device monitor`). En el arranque debe salir `[boot] cat sprite OK` y la
memoria libre (se espera >= 40 KB).

Comprobar en la placa:
- [ ] Al arrancar aparece la pantalla Habitacion con el gato.
- [ ] Un toque corto cambia de pantalla: Habitacion -> Tareas -> (Spotify, solo si suena musica) -> Habitacion. Los puntos de abajo marcan la pantalla actual.
- [ ] Mantener pulsado mas de 1 s en Habitacion: el gato pone cara feliz y da un saltito.
- [ ] Dejar la pantalla en Tareas 30 s sin tocar: vuelve sola a Habitacion.
- [ ] Calentar el sensor con la mano: al pasar de 25 grados el gato y la temperatura se ponen naranjas ("calor").
- [ ] Poner `SLEEP_START_H` en `src/config.h` a la hora actual y volver a flashear: el gato se duerme (bola, Zzz, mas oscuro). El primer toque solo lo despierta unos 10 s (no cambia de pantalla) y luego se vuelve a dormir. Despues, devolver `SLEEP_START_H` a 23.
- [ ] Al empezar una cancion en Spotify aparece la nota musical en la cabecera, pero la pantalla NO cambia sola.
- [ ] Con el WiFi caido (apagar el router): la cabecera muestra `OFFLINE` en rojo y los toques siguen respondiendo al instante.
