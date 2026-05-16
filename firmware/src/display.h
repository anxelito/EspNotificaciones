// firmware/src/display.h
#pragma once
#include <TFT_eSPI.h>
#include "tasks.h"
#include "config.h"
#include "weather.h"

extern TFT_eSPI tft;

void displayInit();
void drawSplash(const String& msg);

// Redraws full screen: header + all task cards
void redrawAll(TaskItem* tasks, int count, const String& time_str, bool online);

void drawCalendarPanel(CalEvent* events, int count, const String& time_str, bool online);

void drawWeatherPanel(const WeatherData& weather, const String& time_str, bool online);

void drawPanel(int panel, TaskItem* tasks, int taskCount,
               CalEvent* events, int calCount,
               const WeatherData& weather,
               const String& time_str, bool online);
