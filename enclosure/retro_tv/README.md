# Mini TV retro: caja del panel

Script de Fusion 360 que genera la caja completa con medidas editables.

## Abrir en Fusion 360
1. Fusion 360 → **Archivo → Nuevo diseño**. Tiene que ser un diseño vacío.
2. **Utilidades → Complementos → Scripts y complementos** (o pulsa `Mayús+S`).
3. Junto a **Mis scripts**, pulsa el **+** verde → **Script o complemento del dispositivo** → elige la carpeta `enclosure/retro_tv`.
4. Selecciona **retro_tv** → **Ejecutar**.

Se crean 7 cuerpos: **Cuerpo TV**, **Tapa trasera**, **Antena** y **Pata 1–4**.

Para cambiar una medida, edita el bloque `PARAMETERS` al principio de `retro_tv.py` (botón *Editar* en el mismo diálogo) y vuelve a ejecutarlo en un diseño nuevo.

## Antes de imprimir: mide tu pantalla
Las ST7789 de 2.4" cambian según el fabricante. Comprueba con un calibre:

| Parámetro | Qué es | Por defecto |
|---|---|---|
| `PCB_W`, `PCB_H` | placa de la pantalla, en horizontal | 71 × 43 |
| `PCB_HOLE_INSET` | centro de los agujeros desde el borde | 2.5 |
| `GLASS_T` | del frente del cristal al frente del PCB | 3.5 |
| `WIN_W`, `WIN_H` | zona que se ve | 49.5 × 37.2 |
| `WIN_OFF_X` | cuánto se desplaza la zona visible respecto al centro del PCB | 0 |
| `TTP_W`, `TTP_H` | placa del TTP223 | 15 × 11 |
| `DHT_W`, `DHT_T` | cuerpo azul del DHT11: ancho y grosor | 12 × 5.5 |
| `ESP_L` | largo del ESP32, con el conector micro-USB incluido | 55 |

Muchas placas tienen el cristal desplazado unos milímetros hacia el lado contrario a los pines. Mídelo y ponlo en `WIN_OFF_X`. Es positivo si el cristal está a la derecha.

## Exportar e imprimir (sin soportes)
Clic derecho en cada cuerpo → **Guardar como malla** → STL.

| Pieza | Orientación | Notas |
|---|---|---|
| Cuerpo TV | con la **cara delantera en la cama** | 0.2 mm, 3 perímetros, 15 % de relleno |
| Tapa trasera | con la cara exterior en la cama | |
| Antena | plana | pega la bola en otro color si quieres |
| Patas ×4 | boca abajo (pivote en la cama) | las delanteras son más largas: inclinan la tele `TILT_DEG` grados |

PLA o PETG. Tamaño del cuerpo: 112 × 78 × 72 mm.

## Montaje
1. **Pantalla:** 4 tornillos **M2 × 6** en los postes de delante. Comprueba que la imagen queda centrada en la ventana.
2. **Botón táctil (TTP223):** mételo en el hueco rectangular que hay justo detrás de la rueda de arriba, con la cara del pad (la que no lleva componentes) contra la pared. La pared ahí mide 1.2 mm, así que el plástico no se nota: **tocar la rueda cambia de pantalla**. Sujétalo con cinta de doble cara fina, sin aire entre el pad y la pared. Si no responde bien, baja `TOUCH_WALL` a 1.0.
3. **Sensor de temperatura (DHT11):** deslízalo desde atrás por la guía de la pared izquierda, con la rejilla del sensor mirando a la rejilla de la caja, hasta que toque el tope. Las pestañas lo sujetan sin pegamento. Queda en el lado contrario al ESP32 para que mida el aire de la habitación y no el calor de la placa.
4. **ESP32:** apóyalo sobre los dos raíles del suelo, con los pines hacia abajo y el micro-USB hacia atrás, y empújalo hacia delante hasta el tope. Así el conector queda justo detrás del agujero de la tapa. Una gota de cola caliente basta.
5. **Antena y patas:** encájalas arriba y abajo; una gota de pegamento si quedan sueltas.
6. **Tapa:** 4 tornillos **M3 × 8** autorroscantes en los postes de las esquinas.
7. **Alimentación:** enchufa el cable micro-USB por el hueco de la tapa trasera, con la tapa ya cerrada. El ESP32 alimenta la pantalla, el TTP223 y el DHT11, así que no hace falta otro cable.

El cableado entre piezas está en [`firmware/WIRING.md`](../../firmware/WIRING.md).
