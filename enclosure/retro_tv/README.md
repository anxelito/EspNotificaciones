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
| `PCB_W`, `PCB_H` | pantalla entera (cristal + placa), en horizontal | 63 × 38 |
| `SCREEN_T` | grosor de la pantalla: del cristal a la parte de atrás de la placa | 5 |
| `WIN_W`, `WIN_H` | zona que se ve | 49.5 × 37.2 |
| `WIN_OFF_X` | cuánto se desplaza la zona visible respecto al centro del PCB | 0 |
| `TTP_W`, `TTP_H` | placa del TTP223 | 15 × 11 |
| `DHTM_L`, `DHTM_W` | plaquita del DHT11: largo con los pines incluidos y ancho | 30 × 17 |
| `DHTM_STACK` | de la cara del sensor a la parte de atrás de la plaquita | 10 |
| `ESP_L` | largo del ESP32, con el conector micro-USB incluido | 55 |
| `RAIL_OFF` | dónde van los raíles del ESP32: entre sus dos filas de pines | 8 |

Muchas placas tienen el cristal desplazado unos milímetros hacia el lado contrario a los pines. Mídelo y ponlo en `WIN_OFF_X`. Es positivo si el cristal está a la derecha.

## Comprobar que todo cabe
`fit_check.py` reconstruye en 3D la caja con las mismas medidas del script. Mete dentro la pantalla, el ESP32, el DHT11, el TTP223, las clavijas dupont y el cable USB, y avisa de cualquier choque:

```bash
pip install manifold3d
python enclosure/retro_tv/fit_check.py                  # cables dupont, pines de la pantalla a la derecha
python enclosure/retro_tv/fit_check.py --header left    # pines de la pantalla a la izquierda
python enclosure/retro_tv/fit_check.py --solder         # cables soldados
python enclosure/retro_tv/fit_check.py --narrow         # ESP32 estrecho (filas a 22.86 mm)
python enclosure/retro_tv/fit_check.py --dht-pins angle # DHT11 con pines en ángulo
```

Si cambias alguna medida, vuelve a ejecutarlo antes de imprimir.

## Exportar e imprimir (sin soportes)
El script deja todas las piezas **tumbadas en la cama (Z = 0) ya en su posición de impresión**. Ocupan unos 195 × 165 mm. Para ver la tele montada, suprime los últimos pasos **Mover** de la línea de tiempo, o pon `PRINT_LAYOUT = False`.

Para exportar todo de una vez: clic derecho en el componente raíz → **Guardar como malla** → STL o 3MF, y ábrelo en tu laminador. También puedes exportar cada cuerpo por separado.

| Pieza | Orientación | Notas |
|---|---|---|
| Cuerpo TV | con la **cara delantera en la cama** | 0.2 mm, 3 perímetros, 15 % de relleno |
| Tapa trasera | con la cara exterior en la cama | |
| Antena | plana | pega la bola en otro color si quieres |
| Patas ×4 | boca abajo (pivote en la cama) | las delanteras son más largas: inclinan la tele `TILT_DEG` grados |

PLA o PETG. Tamaño del cuerpo: 112 × 78 × 72 mm.

## Montaje
1. **Pantalla:** métela boca abajo entre las 4 guías de las esquinas, con el cristal contra el frente. Comprueba que la imagen queda centrada en la ventana y fíjala con una gota de cola caliente (o cinta de doble cara) en cada esquina.
2. **Botón táctil (TTP223):** mételo en el hueco rectangular que hay justo detrás de la rueda de arriba, con la cara del pad (la que no lleva componentes) contra la pared. La pared ahí mide 1.2 mm, así que el plástico no se nota: **tocar la rueda cambia de pantalla**. Sujétalo con cinta de doble cara fina, sin aire entre el pad y la pared. Si no responde bien, baja `TOUCH_WALL` a 1.0.
3. **Sensor de temperatura (DHT11 en plaquita):** va en la guía de la cara interior de la tapa trasera, arriba a la izquierda, detrás de las rejillas.
   - Mételo con la rejilla del sensor contra la tapa, entrando por el lado de los pines, hasta que toque el tope.
   - Las pestañas lo sujetan sin pegamento.
   - Vale tanto si tus pines son rectos como si van en ángulo.
   - Queda lejos del ESP32, así mide el aire de la habitación y no el calor de la placa.
   - Conecta sus cables antes de cerrar la tapa.
4. **ESP32:** apóyalo sobre los dos raíles del suelo, con los pines hacia abajo y el micro-USB hacia atrás. Los raíles quedan entre las dos filas de pines y miden 21 mm de alto, así que caben las clavijas dupont por debajo. Empújalo hacia delante hasta el tope del raíl derecho. Así el conector queda justo detrás del agujero de la tapa. Una gota de cola caliente basta.
5. **Antena y patas:** encájalas arriba y abajo; una gota de pegamento si quedan sueltas.
6. **Tapa:** 4 tornillos **M3 × 8** autorroscantes en los postes de las esquinas.
7. **Alimentación:** enchufa el cable micro-USB por el hueco de la tapa trasera, con la tapa ya cerrada. El ESP32 alimenta la pantalla, el TTP223 y el DHT11, así que no hace falta otro cable.

El cableado entre piezas está en [`firmware/WIRING.md`](../../firmware/WIRING.md).
