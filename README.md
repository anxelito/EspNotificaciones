<h1 align="center">Panel de escritorio ESP32</h1>

<p align="center"><b>Una mascota animada en tu mesa que te enseña la hora, el tiempo, la temperatura de tu cuarto, tus tareas de Telegram y lo que suena en Spotify.</b></p>

<p align="center">
  <a href="https://github.com/anxelito/EspNotificaciones/actions/workflows/tests.yml"><img alt="Tests" src="https://github.com/anxelito/EspNotificaciones/actions/workflows/tests.yml/badge.svg"></a>
  <a href="LICENSE"><img alt="Licencia MIT" src="https://img.shields.io/badge/licencia-MIT-4fd18b"></a>
  <img alt="ESP32" src="https://img.shields.io/badge/ESP32-PlatformIO-ff8a3d">
  <img alt="Pantalla" src="https://img.shields.io/badge/pantalla-ST7789%202.4%22-5ab0ff">
  <img alt="Backend" src="https://img.shields.io/badge/backend-FastAPI-b48cff">
  <a href="https://anxelito.github.io/EspNotificaciones/"><img alt="Pruébalo online" src="https://img.shields.io/badge/pru%C3%A9balo-online-ff6b8a"></a>
</p>

<p align="center"><img src="docs/designs/robot/hero.png" width="560" alt="Pantalla Habitación: Robot en la estación"></p>

<p align="center">
  <a href="#pruébalo">Pruébalo</a> ·
  <a href="#diseños-de-la-habitación">Diseños</a> ·
  <a href="#monta-el-tuyo">Monta el tuyo</a> ·
  <a href="docs/INSTALACION.md">Guía completa</a> ·
  <a href="#preguntas-frecuentes">Preguntas frecuentes</a>
</p>

> **Esta rama:** diseño **E · Robot en la estación**. Cada diseño de la pantalla Habitación vive en su propia rama `design/<nombre>`; consulta la [galería](#diseños-de-la-habitación).

## Pruébalo

**Sin hardware, en el navegador:**
- [**Simulador del panel**](https://anxelito.github.io/EspNotificaciones/simulador/): el robot en su estación. Cambia la hora, la temperatura, la humedad y el tiempo; tócalo, acarícialo o mándale una tarea.
- [**Visor 3D de cajas**](https://anxelito.github.io/EspNotificaciones/caja/): seis ideas de carcasa imprimible, para girarlas, cambiarles el color y la inclinación.

**Con hardware:** cuesta unos 15 € en piezas y se monta en una tarde. Ve a [Monta el tuyo](#monta-el-tuyo).

## Qué hace

| Pantalla | Qué muestra |
|---|---|
| **Habitación** | Reloj grande, fecha y tiempo de tu ciudad; la mascota con su escena; temperatura, humedad y confort del DHT11 |
| **Tareas** | Las tareas que mandas al bot de Telegram, con prioridad, progreso y aviso cuando llega una nueva |
| **Música** | La canción de Spotify (o Last.fm en la versión gratuita): portada grande, fondo difuminado, ecualizador y barra de progreso |

- Un **toque** en el botón cambia de pantalla.
- Si **mantienes pulsado** más de 1 s en Habitación, acaricias a la mascota.

### Cómo reacciona la mascota

- **Por la noche** (23:00–8:00) se duerme. Un toque la despierta un momento.
- **Según la temperatura** de la habitación: tiembla con frío y suda con calor. Además, un detalle de su ropa (collar, bufanda, pajarita…) toma el color del confort.
- **Cuando llega una tarea**, la trae en un sobre.

## Diseños de la Habitación

Hay seis mascotas con su propio escenario. Todas comparten el mismo comportamiento (estados de ánimo y acciones). Solo cambia el dibujo: `room_scene.*` y `cat_draw.cpp`.

| | | |
|:-:|:-:|:-:|
| <img src="docs/designs/robot/dia.gif" width="260" alt="Robot en la estación"><br>**E · Robot en la estación** ⭐<br>[`design/robot`](../../tree/design/robot) | <img src="docs/designs/gato/dia.gif" width="260" alt="Gatito en su cuarto"><br>**A · Gatito en su cuarto**<br>[`design/gato`](../../tree/design/gato) | <img src="docs/designs/perro/dia.gif" width="260" alt="Perrito en el jardín"><br>**B · Perrito en el jardín**<br>[`design/perro`](../../tree/design/perro) |
| <img src="docs/designs/pinguino/dia.gif" width="260" alt="Pingüino en el polo"><br>**C · Pingüino en el polo**<br>[`design/pinguino`](../../tree/design/pinguino) | <img src="docs/designs/pez/dia.gif" width="260" alt="Pececito en la pecera"><br>**D · Pececito en la pecera**<br>[`design/pez`](../../tree/design/pez) | <img src="docs/designs/rana/dia.gif" width="260" alt="Rana en el estanque"><br>**F · Rana en el estanque**<br>[`design/rana`](../../tree/design/rana) |

⭐ = el diseño de esta rama. Los GIF son capturas reales del firmware: el código de dibujo se compila en el PC (ver [herramientas](#herramientas)).

### E · Robot en la estación

Un robot que flota en una estación espacial, con un ojo de buey al espacio. El planeta cambia de color con la temperatura de la habitación y el panel parpadea.

**Qué hace:** Vuela de lado a lado, escanea con la pantalla, se queda sin batería, estira los brazos y bota. Por la noche se sienta en su base a cargar, con gorro de dormir.

| Día | Noche | Frío |
|:-:|:-:|:-:|
| <img src="docs/designs/robot/dia.png" width="250" alt="Robot en la estación, día"> | <img src="docs/designs/robot/noche.png" width="250" alt="Robot en la estación, noche"> | <img src="docs/designs/robot/frio.png" width="250" alt="Robot en la estación, frío"> |
| **Calor** | **Caricia** | **Nueva tarea** |
| <img src="docs/designs/robot/calor.png" width="250" alt="Robot en la estación, calor"> | <img src="docs/designs/robot/carino.png" width="250" alt="Robot en la estación, caricia"> | <img src="docs/designs/robot/tarea.png" width="250" alt="Robot en la estación, nueva tarea"> |

<details><summary>De noche (animado)</summary>

<img src="docs/designs/robot/noche.gif" width="400" alt="Robot en la estación durmiendo">
</details>

Pruébalo: `git checkout design/robot`

### A · Gatito en su cuarto

El diseño original: una ventana con el cielo según la hora y el tiempo real, una alfombra y una planta.

**Qué hace:** Pasea, mira alrededor, se lava, bosteza, se estira y salta. Por la noche se duerme en su cojín.

| Día | Noche | Frío |
|:-:|:-:|:-:|
| <img src="docs/designs/gato/dia.png" width="250" alt="Gatito en su cuarto, día"> | <img src="docs/designs/gato/noche.png" width="250" alt="Gatito en su cuarto, noche"> | <img src="docs/designs/gato/frio.png" width="250" alt="Gatito en su cuarto, frío"> |
| **Calor** | **Caricia** | **Nueva tarea** |
| <img src="docs/designs/gato/calor.png" width="250" alt="Gatito en su cuarto, calor"> | <img src="docs/designs/gato/carino.png" width="250" alt="Gatito en su cuarto, caricia"> | <img src="docs/designs/gato/tarea.png" width="250" alt="Gatito en su cuarto, nueva tarea"> |

<details><summary>De noche (animado)</summary>

<img src="docs/designs/gato/noche.gif" width="400" alt="Gatito en su cuarto durmiendo">
</details>

Pruébalo: `git checkout design/gato`

### B · Perrito en el jardín

Exterior a pantalla completa. El cielo, las nubes, la lluvia (con charcos) y la nieve (en el césped y el tejado) siguen al tiempo real. Valla, colinas y caseta.

**Qué hace:** Trota, levanta una oreja, se rasca, bosteza, hace la reverencia de juego y salta. Por la noche se enrosca en su cojín. Si hace frío, bufanda y vaho; si hace calor, lengua fuera.

| Día | Noche | Frío |
|:-:|:-:|:-:|
| <img src="docs/designs/perro/dia.png" width="250" alt="Perrito en el jardín, día"> | <img src="docs/designs/perro/noche.png" width="250" alt="Perrito en el jardín, noche"> | <img src="docs/designs/perro/frio.png" width="250" alt="Perrito en el jardín, frío"> |
| **Calor** | **Caricia** | **Nueva tarea** |
| <img src="docs/designs/perro/calor.png" width="250" alt="Perrito en el jardín, calor"> | <img src="docs/designs/perro/carino.png" width="250" alt="Perrito en el jardín, caricia"> | <img src="docs/designs/perro/tarea.png" width="250" alt="Perrito en el jardín, nueva tarea"> |

<details><summary>De noche (animado)</summary>

<img src="docs/designs/perro/noche.gif" width="400" alt="Perrito en el jardín durmiendo">
</details>

Pruébalo: `git checkout design/perro`

### C · Pingüino en el polo

Montañas de hielo, mar, nieve e iglú. Las noches despejadas tienen aurora boreal animada y, si la habitación está caliente, aparecen charcos de deshielo.

**Qué hace:** Camina como un pato, se acicala, abre el pico, se pone de puntillas y salta. Duerme sentado con gorro. Con frío le salen chispitas de felicidad; con calor suda y se abanica.

| Día | Noche | Frío |
|:-:|:-:|:-:|
| <img src="docs/designs/pinguino/dia.png" width="250" alt="Pingüino en el polo, día"> | <img src="docs/designs/pinguino/noche.png" width="250" alt="Pingüino en el polo, noche"> | <img src="docs/designs/pinguino/frio.png" width="250" alt="Pingüino en el polo, frío"> |
| **Calor** | **Caricia** | **Nueva tarea** |
| <img src="docs/designs/pinguino/calor.png" width="250" alt="Pingüino en el polo, calor"> | <img src="docs/designs/pinguino/carino.png" width="250" alt="Pingüino en el polo, caricia"> | <img src="docs/designs/pinguino/tarea.png" width="250" alt="Pingüino en el polo, nueva tarea"> |

<details><summary>De noche (animado)</summary>

<img src="docs/designs/pinguino/noche.gif" width="400" alt="Pingüino en el polo durmiendo">
</details>

Pruébalo: `git checkout design/pinguino`

### D · Pececito en la pecera

Toda la pantalla es agua. Su color sigue a la temperatura de la habitación (más azul con frío, más verde con calor, más oscura de noche). Hay arena, algas, un castillo y burbujas.

**Qué hace:** Nada de lado a lado, mira, echa burbujas, bosteza, abre las aletas y da un acelerón. Por la noche descansa en la arena.

| Día | Noche | Frío |
|:-:|:-:|:-:|
| <img src="docs/designs/pez/dia.png" width="250" alt="Pececito en la pecera, día"> | <img src="docs/designs/pez/noche.png" width="250" alt="Pececito en la pecera, noche"> | <img src="docs/designs/pez/frio.png" width="250" alt="Pececito en la pecera, frío"> |
| **Calor** | **Caricia** | **Nueva tarea** |
| <img src="docs/designs/pez/calor.png" width="250" alt="Pececito en la pecera, calor"> | <img src="docs/designs/pez/carino.png" width="250" alt="Pececito en la pecera, caricia"> | <img src="docs/designs/pez/tarea.png" width="250" alt="Pececito en la pecera, nueva tarea"> |

<details><summary>De noche (animado)</summary>

<img src="docs/designs/pez/noche.gif" width="400" alt="Pececito en la pecera durmiendo">
</details>

Pruébalo: `git checkout design/pez`

### F · Rana en el estanque

Estanque con nenúfares, juncos, flor de loto y árboles. Cuando llueve salen anillos en el agua. De día vuela una libélula y de noche hay luciérnagas.

**Qué hace:** Rema en su nenúfar, caza moscas con la lengua, croa inflando la papada, estira las patas y pega saltos enormes. Por la noche duerme tapada con una hoja.

| Día | Noche | Frío |
|:-:|:-:|:-:|
| <img src="docs/designs/rana/dia.png" width="250" alt="Rana en el estanque, día"> | <img src="docs/designs/rana/noche.png" width="250" alt="Rana en el estanque, noche"> | <img src="docs/designs/rana/frio.png" width="250" alt="Rana en el estanque, frío"> |
| **Calor** | **Caricia** | **Nueva tarea** |
| <img src="docs/designs/rana/calor.png" width="250" alt="Rana en el estanque, calor"> | <img src="docs/designs/rana/carino.png" width="250" alt="Rana en el estanque, caricia"> | <img src="docs/designs/rana/tarea.png" width="250" alt="Rana en el estanque, nueva tarea"> |

<details><summary>De noche (animado)</summary>

<img src="docs/designs/rana/noche.gif" width="400" alt="Rana en el estanque durmiendo">
</details>

Pruébalo: `git checkout design/rana`

## Monta el tuyo

### Lo que necesitas

| Pieza | Aprox. | Notas |
|---|---|---|
| ESP32 DevKit (38 pines) | 6 € | cualquier ESP32-WROOM vale |
| Pantalla ST7789 2.4" 240×320 (SPI) | 6 € | se usa en horizontal |
| DHT11 | 1.5 € | temperatura y humedad |
| TTP223 | 0.5 € | botón táctil; funciona a través de 1.2 mm de plástico. Opcional: sin él, el botón BOOT de la placa hace lo mismo |
| Cables dupont y un cable micro-USB | | |
| Impresora 3D | | opcional, para la [caja Mini TV retro](enclosure/retro_tv) |

### Cableado

| Pantalla ST7789 | ESP32 | | Sensor | ESP32 |
|---|---|---|---|---|
| VCC | 3V3 | | DHT11 DATA | GPIO27 |
| GND | GND | | TTP223 I/O | GPIO32 |
| SCL / SCK | GPIO18 | | VCC de ambos | 3V3 |
| SDA / MOSI | GPIO23 | | GND de ambos | GND |
| CS | GPIO15 | | | |
| DC | GPIO2 | | | |
| RES / RST | GPIO4 | | | |
| BL / LED | 3V3 | | | |

Más detalle (DHT11 suelto de 4 pines, regular el brillo) en [`firmware/WIRING.md`](firmware/WIRING.md).

### En 4 pasos

1. **Crea un bot de Telegram:** habla con [@BotFather](https://t.me/BotFather), usa `/newbot` y guarda el token.
2. **Arranca el backend** (Python 3.11+). Necesita una URL HTTPS pública:
   - un VPS con dominio,
   - o, para probar desde tu PC, un túnel gratis de Cloudflare.

   ```bash
   cd backend
   python -m venv .venv && source .venv/bin/activate   # Windows: .venv\Scripts\activate
   pip install -r requirements.txt
   cp .env.example .env        # rellena el token, API_KEY, BASE_URL...
   uvicorn main:app --port 8000
   ```
3. **Configura el firmware:**
   - Copia `firmware/src/secrets.h.example` a `secrets.h`.
   - Pon tu WiFi, la URL del backend y la misma `API_KEY`.
4. **Flashea** con [PlatformIO](https://platformio.org/install):

   ```bash
   cd firmware
   pio run -e esp32dev -t upload   # si falla, mantén BOOT pulsado durante "Connecting..."
   ```

Ya está: escribe al bot y la tarea aparece en el panel. 🎉

👉 La **[guía completa](docs/INSTALACION.md)** explica cada paso:
- el túnel de Cloudflare;
- dejar el backend como servicio;
- Spotify o Last.fm;
- el tiempo de tu ciudad;
- la zona horaria;
- la caja impresa.

### Usar el bot

| Envías | Pasa |
|---|---|
| `Comprar leche` | tarea normal |
| `Pagar la luz!` | tarea **urgente** (con `!` al final) |
| `/done 3` | marca la tarea 3 como hecha |
| `/delete 3` | borra la tarea 3 |
| `/list` | lista las tareas |
| `/clear` | borra las tareas hechas |

### Cambiar de diseño

```bash
git checkout design/perro          # o gato, pinguino, pez, robot, rana
cd firmware && pio run -e esp32dev -t upload
```

`firmware/src/secrets.h` no está en git, así que se conserva al cambiar de rama.

## Cómo funciona

```
Telegram ──webhook──▶ FastAPI ──HTTPS──▶ ESP32 ──SPI──▶ ST7789
Spotify / Last.fm ──▶   /nowplaying               ▲
Open-Meteo ─────────▶   /weather              DHT11 + TTP223
```

- **[`backend/`](backend)**: FastAPI con SQLite.
  - Bot de Telegram para las tareas.
  - `/nowplaying`: Spotify, o Last.fm si la cuenta es gratuita. La portada llega ya compuesta a 320×200 con su color de acento.
  - `/weather`: el tiempo de Open-Meteo, con caché de 10 min.
- **[`firmware/`](firmware)**: PlatformIO + TFT_eSPI.
  - La red va en un hilo propio, en el núcleo 0.
  - Las fuentes Nunito son suaves (anti-aliasing).
  - Las animaciones usan sprites, así que no parpadean.
  - La lógica (estados de ánimo, comportamiento, interfaz) tiene tests en el PC: `pio test -e native`.

## Herramientas

| Herramienta | Para qué |
|---|---|
| [`firmware/tools/snapshots`](firmware/tools/snapshots) | `python firmware/tools/snapshots/snap.py` compila el código de dibujo real en el PC, con una imitación de TFT_eSPI, y genera las capturas y los GIF de `docs/` |
| [`firmware/tools/simulator`](firmware/tools/simulator) | Simulador en el navegador (robot): hora, temperatura, tiempo, caricias… |
| [`firmware/tools/make_fonts.py`](firmware/tools/make_fonts.py) | Genera las fuentes suaves |

## Preguntas frecuentes

<details><summary><b>La pantalla pone OFFLINE</b></summary>

- `API_URL` tiene que acabar en `/tasks` y ser **HTTPS**.
- La `API_KEY` de `secrets.h` tiene que ser la misma que la del `.env`.
- El monitor serie (`pio device monitor`) muestra el código HTTP de cada fallo.
</details>

<details><summary><b>El flasheo falla con "Wrong boot mode" o no conecta</b></summary>

Mantén pulsado el botón **BOOT** de la placa mientras sale `Connecting...` y suéltalo cuando empiece a escribir.
</details>

<details><summary><b>No tengo Spotify Premium</b></summary>

La API de Spotify exige Premium al dueño de la app. Sin Premium:
1. Conecta Spotify a [Last.fm](https://www.last.fm/settings/applications) ("scrobbling").
2. Pon `LASTFM_API_KEY` y `LASTFM_USER` en el `.env`.

La pantalla de música funcionará igual, con portada y progreso estimado.
</details>

<details><summary><b>Los colores salen invertidos o la imagen desplazada</b></summary>

Hay pantallas ST7789 de varios fabricantes. Prueba en `firmware/src/User_Setup.h`:
- `TFT_INVERSION_ON` o `TFT_INVERSION_OFF`;
- `TFT_RGB_ORDER TFT_BGR`.
</details>

<details><summary><b>¿Necesito el sensor y el botón táctil?</b></summary>

No.
- Sin DHT11, la tira de abajo muestra `--`.
- Sin TTP223, el botón BOOT de la placa sirve para tocar (corto) y acariciar (largo).
</details>

<details><summary><b>Quiero crear mi propia mascota</b></summary>

Crea una rama y cambia solo dos cosas:
- `firmware/src/ui/room_scene.*`: el escenario;
- `firmware/src/ui/cat_draw.cpp`: la mascota.

Para ver el resultado sin flashear, `python firmware/tools/snapshots/snap.py` genera las capturas en el PC. Si te sale una bonita, ¡manda un PR!
</details>

## Contribuir

Se aceptan issues y PRs: nuevas mascotas, cajas o pantallas.
- Lógica: `pio test -e native`.
- Backend: `cd backend && pytest`.

## Licencia y créditos

- Código bajo licencia [MIT](LICENSE).
- Fuente [Nunito](https://github.com/googlefonts/nunito) (SIL Open Font License 1.1), convertida a fuentes suaves de TFT_eSPI.
- Usa [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI), [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder), [ArduinoJson](https://arduinojson.org), [FastAPI](https://fastapi.tiangolo.com), [python-telegram-bot](https://python-telegram-bot.org) y [Open-Meteo](https://open-meteo.com).
