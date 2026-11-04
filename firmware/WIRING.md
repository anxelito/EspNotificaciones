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
