// firmware/src/tasks.cpp
#include "tasks.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

static int scoreTask(const TaskItem& t) {
    if (strcmp(t.status, "done") == 0)    return 2;
    if (strcmp(t.priority, "urgent") == 0) return 0;
    return 1;
}

void sortTasks(TaskItem* tasks, int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (scoreTask(tasks[j]) < scoreTask(tasks[i])) {
                TaskItem tmp = tasks[i];
                tasks[i]     = tasks[j];
                tasks[j]     = tmp;
            }
        }
    }
}

bool saveTasks(const TaskItem* tasks, int count) {
    File f = SPIFFS.open("/tasks.json", "w");
    if (!f) return false;

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < count; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"]           = tasks[i].id;
        obj["text"]         = tasks[i].text;
        obj["priority"]     = tasks[i].priority;
        obj["status"]       = tasks[i].status;
        obj["time_display"] = tasks[i].time_display;
    }
    serializeJson(doc, f);
    f.close();
    return true;
}

int loadTasks(TaskItem* tasks) {
    if (!SPIFFS.exists("/tasks.json")) return 0;

    File f = SPIFFS.open("/tasks.json", "r");
    if (!f) return 0;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return 0;

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
