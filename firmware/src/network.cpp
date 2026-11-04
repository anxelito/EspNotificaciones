// firmware/src/network.cpp
#include "network.h"
#include "secrets.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// Every network operation must give up within ~3 s so the worker on core 0
// stays responsive (DNS lookup is the only step not bounded here).
#define HTTP_TIMEOUT_S  ((HTTP_TIMEOUT_MS + 999) / 1000)

static void prepClient(WiFiClientSecure& client) {
    client.setInsecure(); // accepts any cert — sufficient for private OCI endpoint
    client.setHandshakeTimeout(HTTP_TIMEOUT_S);
    client.setTimeout(HTTP_TIMEOUT_S);
}

static void prepHttp(HTTPClient& http) {
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setTimeout(HTTP_TIMEOUT_MS);
}

bool connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
    }
    return WiFi.status() == WL_CONNECTED;
}

void clockBegin() {
    // ESP-IDF SNTP runs in the lwIP thread and re-syncs on its own (default 1 h).
    configTime(TZ_OFFSET * 3600L, 0, NTP_SERVER);
}

bool clockGetHM(int* hour, int* minute) {
    // Equivalent to getLocalTime(&tm, 0) but without its millis() edge case
    // (with ms=0 that loop can run zero times and return false spuriously).
    time_t now;
    struct tm tm;
    time(&now);
    localtime_r(&now, &tm);
    if (tm.tm_year <= (2016 - 1900)) return false; // never synced
    if (hour)   *hour   = tm.tm_hour;
    if (minute) *minute = tm.tm_min;
    return true;
}

bool clockGetDate(int* wday, int* mday, int* mon) {
    time_t now;
    struct tm tm;
    time(&now);
    localtime_r(&now, &tm);
    if (tm.tm_year <= (2016 - 1900)) return false;
    if (wday) *wday = tm.tm_wday;
    if (mday) *mday = tm.tm_mday;
    if (mon)  *mon  = tm.tm_mon;
    return true;
}

// API_URL ends in "/tasks"; the weather endpoint sits next to it.
static String weatherUrl() {
    String u = API_URL;
    int i = u.lastIndexOf('/');
    return (i > 8 ? u.substring(0, i) : u) + "/weather";
}

bool fetchWeather(Weather* w) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    prepClient(client);
    HTTPClient http;
    prepHttp(http);
    http.begin(client, weatherUrl());
    http.addHeader("X-API-Key", API_KEY);

    int code = http.GET();
    if (code != 200) {
        Serial.printf("[net] weather GET failed: %d\n", code);
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;
    if (!(doc["ok"] | false)) return false;           // backend has no location set
    w->ok   = true;
    w->code = doc["code"] | 0;
    w->temp = doc["temp"] | 0.0f;
    return true;
}

int fetchTasks(TaskItem* tasks) {
    if (WiFi.status() != WL_CONNECTED) return -1;

    WiFiClientSecure client;
    prepClient(client);

    HTTPClient http;
    prepHttp(http);
    http.begin(client, API_URL);
    http.addHeader("X-API-Key", API_KEY);

    int code = http.GET();
    if (code != 200) {
        Serial.printf("[net] tasks GET failed: %d (%s)\n", code,
                      http.errorToString(code).c_str());
        http.end();
        return -1;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err != DeserializationError::Ok) {
        Serial.printf("[net] tasks JSON error: %s (%u B)\n", err.c_str(),
                      (unsigned)payload.length());
        return -1;
    }

    JsonArray arr = doc.as<JsonArray>();
    int count = 0;
    for (JsonObject obj : arr) {
        if (count >= MAX_TASKS) break;
        tasks[count].id = obj["id"] | 0;
        strlcpy(tasks[count].text,         obj["text"]         | "", sizeof(tasks[count].text));
        strlcpy(tasks[count].priority,     obj["priority"]     | "normal", sizeof(tasks[count].priority));
        strlcpy(tasks[count].status,       obj["status"]       | "pending", sizeof(tasks[count].status));
        strlcpy(tasks[count].time_display, obj["time_display"] | "", sizeof(tasks[count].time_display));
        count++;
    }
    return count;
}

bool fetchNowPlaying(NowPlaying* np) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    prepClient(client);

    HTTPClient http;
    prepHttp(http);
    http.begin(client, NOWPLAYING_URL);
    http.addHeader("X-API-Key", API_KEY);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;

    np->is_playing   = doc["is_playing"] | false;
    np->progress_ms  = doc["progress_ms"] | 0;
    np->duration_ms  = doc["duration_ms"] | 0;
    strlcpy(np->title,       doc["title"]       | "", sizeof(np->title));
    strlcpy(np->artist,      doc["artist"]      | "", sizeof(np->artist));
    strlcpy(np->album,       doc["album"]       | "", sizeof(np->album));
    strlcpy(np->artwork_url, doc["artwork_url"] | "", sizeof(np->artwork_url));
    const char* acc = doc["accent"] | "";            // "#rrggbb"
    np->hasAccent = strlen(acc) == 7 && acc[0] == '#';
    if (np->hasAccent) {
        uint32_t rgb = strtoul(acc + 1, nullptr, 16);
        np->accent = (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
    }

    return true;
}

int fetchArtwork(const char* url, uint8_t* buf, int maxLen) {
    if (WiFi.status() != WL_CONNECTED) return -1;
    if (strlen(url) == 0) return -1;

    WiFiClientSecure client;
    prepClient(client);

    HTTPClient http;
    prepHttp(http);
    http.begin(client, url);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return -1;
    }

    int contentLen = http.getSize();
    if (contentLen <= 0 || contentLen > maxLen) {
        http.end();
        return -1;
    }

    WiFiClient* stream = http.getStreamPtr();
    int totalRead = 0;
    unsigned long lastData = millis();
    while (totalRead < contentLen) {
        int avail = stream->available();
        if (avail > 0) {
            int toRead = min(avail, contentLen - totalRead);
            int bytesRead = stream->readBytes(buf + totalRead, toRead);
            if (bytesRead > 0) {
                totalRead += bytesRead;
                lastData = millis();
            }
        } else if (!stream->connected() || (millis() - lastData) >= HTTP_TIMEOUT_MS) {
            break; // stalled or dropped: give up instead of spinning forever
        } else {
            delay(1);
        }
    }

    http.end();
    return (totalRead == contentLen) ? totalRead : -1;
}
