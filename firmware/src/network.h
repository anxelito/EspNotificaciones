// firmware/src/network.h
#pragma once
#include <Arduino.h>
#include "tasks.h"
#include "nowplaying.h"

// Returns true if connected
bool connectWiFi();

// Syncs NTP. Returns true on success.
bool syncNTP();

// Returns current time as "HH:MM". Call after syncNTP().
String getCurrentTime();

// Fetches tasks from API. Returns task count, -1 on error.
int fetchTasks(TaskItem* tasks);

// Fetches now-playing info from backend. Returns true on success.
bool fetchNowPlaying(NowPlaying* np);

// Downloads JPEG artwork into buffer. Returns bytes written, -1 on error.
int fetchArtwork(const char* url, uint8_t* buf, int maxLen);
