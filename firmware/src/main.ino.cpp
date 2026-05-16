# 1 "C:\\Users\\angel\\AppData\\Local\\Temp\\tmpq1ue9bvd"
#include <Arduino.h>
# 1 "D:/EspNotificaciones/firmware/src/main.ino"

#include "config.h"
#include "secrets.h"
#include "display.h"
#include "tasks.h"
#include "network.h"
#include "weather.h"
#include <SPIFFS.h>

static TaskItem tasks[MAX_TASKS];
static int taskCount = 0;
static CalEvent calEvents[MAX_EVENTS];
static int calEventCount = 0;
static WeatherData weather = {0.0f, 0, "---"};

static bool online = false;
static int currentPanel = 0;
static unsigned long lastPoll = 0;
static unsigned long lastNTP = 0;
static unsigned long lastCalPoll = 0;
static unsigned long lastWeatherPoll = 0;
static unsigned long lastPanelSwitch = 0;
static String currentTime();
void setup();
void loop();
#line 24 "D:/EspNotificaciones/firmware/src/main.ino"
static String currentTime() {
    return online ? getCurrentTime() : "--:--";
}

void setup() {
    Serial.begin(115200);

    displayInit();
    drawSplash("Iniciando...");

    SPIFFS.begin(true);
    taskCount = loadTasks(tasks);
    if (taskCount > 0) {
        sortTasks(tasks, taskCount);
        redrawAll(tasks, taskCount, "--:--", false);
    }

    drawSplash("Conectando WiFi...");
    online = connectWiFi();

    if (online) {
        drawSplash("Sincronizando hora...");
        syncNTP();
        lastNTP = millis();

        drawSplash("Cargando datos...");
        calEventCount = fetchCalendar(calEvents);
        if (calEventCount < 0) calEventCount = 0;
        fetchWeather(&weather);
        lastCalPoll = millis();
        lastWeatherPoll = millis();
    }

    drawPanel(currentPanel, tasks, taskCount, calEvents, calEventCount, weather, currentTime(), online);
    lastPanelSwitch = millis();
}

void loop() {
    unsigned long now = millis();


    if (online && (now - lastNTP >= NTP_SYNC_INTERVAL)) {
        syncNTP();
        lastNTP = now;
    }


    if (now - lastPanelSwitch >= PANEL_INTERVAL_MS) {
        currentPanel = (currentPanel + 1) % 3;
        lastPanelSwitch = now;
        drawPanel(currentPanel, tasks, taskCount, calEvents, calEventCount, weather, currentTime(), online);
    }


    if (now - lastPoll >= POLL_INTERVAL_MS) {
        int result = fetchTasks(tasks);
        if (result >= 0) {
            taskCount = result;
            online = true;
            sortTasks(tasks, taskCount);
            saveTasks(tasks, taskCount);
        } else {
            online = false;
        }
        if (currentPanel == 0) {
            drawPanel(0, tasks, taskCount, calEvents, calEventCount, weather, currentTime(), online);
        }
        lastPoll = now;
    }


    if (online && (now - lastCalPoll >= CALENDAR_POLL_MS)) {
        int result = fetchCalendar(calEvents);
        if (result >= 0) calEventCount = result;
        lastCalPoll = now;
    }


    if (online && (now - lastWeatherPoll >= WEATHER_POLL_MS)) {
        fetchWeather(&weather);
        lastWeatherPoll = now;
    }
}