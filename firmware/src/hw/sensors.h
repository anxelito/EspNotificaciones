#pragma once
#include <cstdint>
#include "../logic/room.h"

void sensorsInit();
void sensorsPoll(RoomReading& r, uint32_t nowMs);
