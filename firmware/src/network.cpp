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

int fetchCalendar(CalEvent* events) {
    if (WiFi.status() != WL_CONNECTED) return -1;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(API_URL);
    url.replace("/tasks", "/calendar");
    http.begin(client, url);
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
        if (count >= MAX_EVENTS) break;
        strlcpy(events[count].summary,      obj["summary"]      | "", sizeof(events[count].summary));
        strlcpy(events[count].time_display, obj["time_display"] | "", sizeof(events[count].time_display));
        events[count].day = obj["day"] | 0;
        count++;
    }
    return count;
}

static const char* _weatherDesc(int code) {
    if (code == 0)        return "Despejado";
    if (code <= 3)        return "Nubes";
    if (code <= 48)       return "Niebla";
    if (code <= 55)       return "Llovizna";
    if (code <= 65)       return "Lluvia";
    if (code <= 75)       return "Nieve";
    if (code <= 82)       return "Chubascos";
    if (code <= 99)       return "Tormenta";
    return "Desconocido";
}

bool fetchWeather(WeatherData* weather) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast"
                 "?latitude=" WEATHER_LAT
                 "&longitude=" WEATHER_LON
                 "&current=temperature_2m,weathercode"
                 "&timezone=auto";
    http.begin(client, url);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;

    JsonObject current = doc["current"];
    weather->temp        = current["temperature_2m"] | 0.0f;
    weather->weathercode = current["weathercode"]    | 0;
    strlcpy(weather->description, _weatherDesc(weather->weathercode), sizeof(weather->description));
    return true;
}
