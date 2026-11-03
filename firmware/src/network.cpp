// firmware/src/network.cpp
#include "network.h"
#include "secrets.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

static WiFiUDP    _ntpUDP;
static NTPClient  _timeClient(_ntpUDP, NTP_SERVER, TZ_OFFSET * 3600, 60000);

bool connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
    }
    return WiFi.status() == WL_CONNECTED;
}

bool syncNTP() {
    static bool _begun = false;
    if (!_begun) {
        _timeClient.begin();
        _begun = true;
    }
    return _timeClient.forceUpdate();
}

String getCurrentTime() {
    _timeClient.update();
    // getFormattedTime() returns "HH:MM:SS"
    return _timeClient.getFormattedTime().substring(0, 5);
}

int fetchTasks(TaskItem* tasks) {
    if (WiFi.status() != WL_CONNECTED) return -1;

    WiFiClientSecure client;
    client.setInsecure(); // accepts any cert — sufficient for private OCI endpoint

    HTTPClient http;
    http.begin(client, API_URL);
    http.addHeader("X-API-Key", API_KEY);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return -1;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return -1;

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
    client.setInsecure();

    HTTPClient http;
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

    return true;
}

int fetchArtwork(const char* url, uint8_t* buf, int maxLen) {
    if (WiFi.status() != WL_CONNECTED) return -1;
    if (strlen(url) == 0) return -1;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
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
    while (totalRead < contentLen) {
        int avail = stream->available();
        if (avail > 0) {
            int toRead = min(avail, contentLen - totalRead);
            int bytesRead = stream->readBytes(buf + totalRead, toRead);
            totalRead += bytesRead;
        } else {
            delay(1);
        }
    }

    http.end();
    return totalRead;
}
