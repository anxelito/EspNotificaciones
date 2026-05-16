// firmware/src/weather.h
#pragma once

struct WeatherData {
    float temp;
    int   weathercode;
    char  description[20];
};
