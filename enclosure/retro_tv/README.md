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

Muchas placas tienen el cristal desplazado unos milímetros hacia el lado contrario a los pines. Mídelo y ponlo en `WIN_OFF_X`. Es positivo si el cristal está a la derecha.

## Exportar e imprimir (sin soportes)
Clic derecho en cada cuerpo → **Guardar como malla** → STL.

| Pieza | Orientación | Notas |
|---|---|---|
| Cuerpo TV | con la **cara delantera en la cama** | 0.2 mm, 3 perímetros, 15 % de relleno |
| Tapa trasera | con la cara exterior en la cama | |
| Antena | plana | pega la bola en otro color si quieres |
| Patas ×4 | de pie | |

PLA o PETG. Tamaño del cuerpo: 112 × 78 × 72 mm.

## Montaje
1. **Pantalla:** 4 tornillos **M2 × 6** en los postes de delante. Comprueba que la imagen queda centrada en la ventana.
2. **TTP223:** pégalo con cinta de doble cara justo detrás de la rueda de arriba (la pared ahí mide 1.2 mm). Así, **tocar esa rueda cambia de pantalla**.
3. **DHT11:** pégalo en la pared izquierda, detrás de la rejilla lateral. Así lee el aire de la habitación y no el calor del ESP32.
4. **ESP32:** apóyalo sobre los dos raíles del suelo, con los pines hacia abajo y el micro-USB hacia atrás. Una gota de cola caliente basta.
5. **Antena y patas:** encájalas arriba y abajo; una gota de pegamento si quedan sueltas.
6. **Tapa:** 4 tornillos **M3 × 8** autorroscantes en los postes de las esquinas. El cable USB sale por el hueco de la tapa.

El cableado entre piezas está en [`firmware/WIRING.md`](../../firmware/WIRING.md).
