// firmware/src/weather.h -- outside weather from the backend (/weather)
#pragma once

struct Weather {
    bool  ok;       // a reading has been received
    int   code;     // WMO weather code (Open-Meteo)
    float temp;     // °C
};
