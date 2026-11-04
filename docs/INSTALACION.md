# Guía de instalación

Esta guía va paso a paso, desde las piezas sueltas hasta el panel funcionando en tu mesa. Calcula una tarde.

**Índice**

1. [Piezas y cableado](#1-piezas-y-cableado)
2. [Bot de Telegram](#2-bot-de-telegram)
3. [Backend](#3-backend)
4. [Firmware](#4-firmware)
5. [Música (opcional)](#5-música-opcional)
6. [Elegir mascota](#6-elegir-mascota)
7. [Caja impresa (opcional)](#7-caja-impresa-opcional)
8. [Problemas comunes](#8-problemas-comunes)

```
Tu móvil ──Telegram──▶ backend (FastAPI, HTTPS) ◀──cada 5 s── ESP32 ──▶ pantalla
                         │  /tasks  /weather  /nowplaying
                         └─ Open-Meteo, Spotify o Last.fm
```

El ESP32 no habla con Telegram. Pregunta a tu backend cada pocos segundos, así que el backend tiene que estar accesible por **HTTPS** desde tu WiFi. Telegram también exige HTTPS para su webhook.

---

## 1. Piezas y cableado

| Pieza | Notas |
|---|---|
| ESP32 DevKit (38 pines) | cualquier ESP32-WROOM |
| Pantalla ST7789 de 2.4" 240×320, SPI | la de 8 pines (VCC, GND, SCL, SDA, RES, DC, CS, BL) |
| DHT11 | opcional: temperatura y humedad del cuarto |
| TTP223 | opcional: botón táctil. Sin él, se usa el botón BOOT de la placa |

| Pantalla | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCL / SCK | GPIO18 |
| SDA / MOSI | GPIO23 |
| CS | GPIO15 |
| DC | GPIO2 |
| RES / RST | GPIO4 |
| BL / LED | 3V3 |

| Sensor | ESP32 |
|---|---|
| DHT11 DATA | GPIO27 (el módulo de 3 pines ya lleva la resistencia) |
| TTP223 I/O | GPIO32 |
| VCC y GND de los dos | 3V3 y GND |

Los pines de la pantalla están en [`firmware/src/User_Setup.h`](../firmware/src/User_Setup.h) y los de los sensores en [`firmware/src/config.h`](../firmware/src/config.h). Más detalles en [`firmware/WIRING.md`](../firmware/WIRING.md).

---

## 2. Bot de Telegram

1. En Telegram, abre [@BotFather](https://t.me/BotFather) y envía `/newbot`.
2. Elige un nombre y un usuario que acabe en `bot`.
3. Guarda el **token**; tiene esta forma: `123456789:AA...`.
4. Busca tu **id de usuario** con [@userinfobot](https://t.me/userinfobot).

   Lo usarás para que el bot solo te haga caso a ti.

---

## 3. Backend

Hace falta Python 3.11 o superior.

### 3a. Prepararlo

```bash
git clone https://github.com/anxelito/EspNotificaciones.git
cd EspNotificaciones/backend
python -m venv .venv
source .venv/bin/activate          # Windows: .venv\Scripts\activate
pip install -r requirements.txt
cp .env.example .env
```

Rellena el `.env`:

| Variable | Qué poner |
|---|---|
| `TELEGRAM_BOT_TOKEN` | el token de BotFather |
| `TELEGRAM_SECRET_TOKEN` | una cadena aleatoria inventada (protege el webhook) |
| `TELEGRAM_ALLOWED_USERS` | tu id de usuario. Si lo dejas vacío, cualquiera que encuentre el bot puede añadirte tareas |
| `API_KEY` | otra cadena aleatoria; el ESP32 la manda en cada petición |
| `BASE_URL` | la URL HTTPS pública del backend, sin barra al final |
| `DATABASE_URL` | `./tasks.db` está bien |
| `WEATHER_LAT`, `WEATHER_LON` | las coordenadas de tu ciudad, por ejemplo `40.4168` y `-3.7038` para Madrid |
| `SPOTIFY_*`, `LASTFM_*` | opcional, ver [Música](#5-música-opcional) |

Para generar cadenas aleatorias: `python -c "import secrets; print(secrets.token_urlsafe(32))"`.

Al arrancar, el backend registra el webhook en Telegram usando `BASE_URL`. No tienes que hacer nada más.

### 3b. Opción rápida: en tu PC con un túnel de Cloudflare

Sirve para probarlo hoy mismo, sin servidor ni dominio.

1. Instala [`cloudflared`](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/downloads/).
2. En una terminal, abre el túnel y copia la URL `https://xxxx.trycloudflare.com` que muestra:

   ```bash
   cloudflared tunnel --url http://localhost:8000
   ```

3. Pon esa URL en `BASE_URL` del `.env`.
4. En otra terminal, desde `backend/` y con el venv activado, arranca el backend:

   ```bash
   uvicorn main:app --port 8000
   ```

5. Comprueba que responde:

   ```bash
   curl https://xxxx.trycloudflare.com/health      # {"status":"ok"}
   ```

> La URL de `trycloudflare.com` cambia cada vez que reinicias el túnel. Para uso diario, usa la opción 3c.

### 3c. Opción para siempre: un servidor

Vale cualquier VPS Linux, por ejemplo el nivel gratuito de Oracle Cloud o una Raspberry Pi en casa. Hace falta un dominio con HTTPS delante. Lo más sencillo es un [túnel con nombre de Cloudflare](https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/get-started/) (gratis, no abre puertos) o [Caddy](https://caddyserver.com/) como proxy inverso.

Para que arranque solo, usa systemd:

1. Copia [`backend/esp32-tasks.service`](../backend/esp32-tasks.service) a `/etc/systemd/system/`.
2. Ajusta `User`, `WorkingDirectory`, `EnvironmentFile` y la ruta del venv.
3. Actívalo:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now esp32-tasks
journalctl -u esp32-tasks -f        # ver los logs
```

### 3d. Probarlo

1. Escribe al bot `Hola!` y debería contestarte con `✅ #1 [URGENTE] Hola`.
2. Comprueba que el ESP32 podrá leer las tareas:

```bash
curl -H "X-API-Key: TU_API_KEY" https://tu-dominio/tasks
```

---

## 4. Firmware

1. Instala [PlatformIO](https://platformio.org/install), como extensión de VS Code o desde la línea de comandos (`pip install platformio`).
2. Crea tu configuración:

   ```bash
   cd firmware/src
   cp secrets.h.example secrets.h
   ```

   | Ajuste | Qué poner |
   |---|---|
   | `WIFI_SSID`, `WIFI_PASS` | tu WiFi, que tiene que ser de **2.4 GHz** |
   | `API_URL` | `https://tu-dominio/tasks`; tiene que acabar en `/tasks` |
   | `API_KEY` | la misma que en el `.env` |
   | `NOWPLAYING_URL` | `https://tu-dominio/nowplaying` |
   | `TZ_POSIX` | tu zona horaria con horario de verano. Para España peninsular: `CET-1CEST,M3.5.0,M10.5.0/3`. [Lista de zonas](https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv) |

   `secrets.h` está en `.gitignore`, así que nunca se sube.

3. Conecta la placa por USB y flashea:

   ```bash
   cd firmware
   pio run -e esp32dev -t upload
   pio device monitor              # opcional: ver qué hace
   ```

   Si se queda en `Connecting...` o sale `Wrong boot mode`, mantén pulsado el botón **BOOT** de la placa hasta que empiece a escribir.

4. Al arrancar sale la pantalla **Habitación**:
   - Un toque cambia a **Tareas**.
   - Mantener pulsado más de 1 s acaricia a la mascota.
   - La pantalla **Música** aparece sola cuando suena algo.

Para comprobar los cambios sin hardware:
- `pio test -e native` ejecuta los tests de la lógica en el PC;
- `python firmware/tools/snapshots/snap.py` genera capturas de la pantalla Habitación.

---

## 5. Música (opcional)

**Con Spotify Premium.** La API de Spotify solo funciona si el dueño de la app es Premium.

1. Crea una app en el [panel de desarrolladores de Spotify](https://developer.spotify.com/dashboard).
2. Añade la *Redirect URI* `http://127.0.0.1:8888/callback`.
3. En tu PC, ejecuta:

   ```bash
   python backend/get_spotify_token.py <CLIENT_ID> <CLIENT_SECRET>
   ```

4. Se abre el navegador. Acepta los permisos y pega en el `.env` las tres líneas `SPOTIFY_*` que imprime el script.

**Sin Premium, con Last.fm:**

1. Crea una cuenta en [Last.fm](https://www.last.fm).
2. Conecta Spotify en *Ajustes → Aplicaciones* para activar el scrobbling.
3. Crea una [API key](https://www.last.fm/api/account/create).
4. Pon `LASTFM_API_KEY` y `LASTFM_USER` en el `.env`.

El panel usa Spotify si está configurado y responde, y si no, Last.fm. La portada se busca en Last.fm o en iTunes.

---

## 6. Elegir mascota

Cada diseño de la pantalla Habitación está en su rama:

| Rama | Mascota |
|---|---|
| `design/robot` | robot en una estación espacial |
| `design/gato` | gatito en su cuarto |
| `design/perro` | perrito en el jardín |
| `design/pinguino` | pingüino en el polo |
| `design/pez` | pececito en la pecera |
| `design/rana` | rana en el estanque |

```bash
git checkout design/pinguino
cd firmware && pio run -e esp32dev -t upload
```

Tu `secrets.h` se conserva al cambiar de rama. Puedes ver todas las mascotas en la [galería del README](../README.md#diseños-de-la-habitación).

---

## 7. Caja impresa (opcional)

La caja **Mini TV retro** es un script paramétrico de Fusion 360:
- deja las piezas listas para imprimir sin soportes;
- tiene sitio para el TTP223 detrás de una pared de 1.2 mm, una guía para el DHT11 y la salida del cable USB;
- está inclinada para que veas bien la pantalla.

Instrucciones en [`enclosure/retro_tv/README.md`](../enclosure/retro_tv/README.md). El [visor 3D](../enclosure/viewer) tiene más ideas de caja.

---

## 8. Problemas comunes

| Síntoma | Solución |
|---|---|
| Pone **OFFLINE** | Comprueba que `API_URL` acaba en `/tasks`, que es HTTPS y que la `API_KEY` coincide. El monitor serie muestra el código HTTP |
| El bot no contesta | Comprueba `BASE_URL` en el `.env` y que `curl https://tu-dominio/health` responde. Mira los logs del backend. Si pusiste `TELEGRAM_ALLOWED_USERS`, comprueba tu id |
| Hora incorrecta | Ajusta `TZ_POSIX` en `secrets.h` |
| Colores invertidos o imagen desplazada | En `User_Setup.h`, prueba `TFT_INVERSION_ON` / `TFT_INVERSION_OFF` o `TFT_RGB_ORDER TFT_BGR` |
| La pantalla se queda en blanco | Revisa CS, DC y RST. BL tiene que ir a 3V3 |
| Temperatura `--` | El DHT11 no responde. Revisa DATA en GPIO27 y su alimentación |
| El toque no hace nada | Revisa el TTP223 en GPIO32. Mientras tanto, el botón BOOT hace lo mismo |
| Spotify da error 403 | Spotify exige Premium al dueño de la app. Usa Last.fm |
