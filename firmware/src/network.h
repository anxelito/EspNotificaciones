// firmware/src/network.h
#pragma once
#include <Arduino.h>
#include "tasks.h"
#include "weather.h"

// Returns true if connected
bool connectWiFi();

// Syncs NTP. Returns true on success.
bool syncNTP();

// Returns current time as "HH:MM". Call after syncNTP().
String getCurrentTime();

// Fetches tasks from API. Returns task count, -1 on error.
int fetchTasks(TaskItem* tasks);

// Fetches calendar events from API. Returns event count, -1 on error.
int fetchCalendar(CalEvent* events);

// Fetches current weather from Open-Meteo. Returns true on success.
bool fetchWeather(WeatherData* weather);
