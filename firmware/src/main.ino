// firmware/src/main.ino
#include "config.h"
#include "secrets.h"
#include "display.h"
#include "tasks.h"
#include "network.h"
#include <SPIFFS.h>

static TaskItem tasks[MAX_TASKS];
static int      taskCount  = 0;
static bool     online     = false;
static unsigned long lastPoll = 0;
static unsigned long lastNTP  = 0;

void setup() {
    Serial.begin(115200);

    displayInit();
    drawSplash("Iniciando...");

    SPIFFS.begin(true);  // format on first run if needed
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
    }

    String t = online ? getCurrentTime() : "--:--";
    redrawAll(tasks, taskCount, t, online);
}

void loop() {
    unsigned long now = millis();

    if (online && (now - lastNTP >= NTP_SYNC_INTERVAL)) {
        syncNTP();
        lastNTP = now;
    }

    if (now - lastPoll >= POLL_INTERVAL_MS) {
        int result = fetchTasks(tasks);
        if (result >= 0) {
            taskCount = result;
            online    = true;
            sortTasks(tasks, taskCount);
            saveTasks(tasks, taskCount);
        } else {
            online = false;
        }
        String t = online ? getCurrentTime() : "--:--";
        redrawAll(tasks, taskCount, t, online);
        lastPoll = now;
    }
}
