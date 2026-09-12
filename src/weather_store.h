#pragma once

#include "weather_logic.h"

class WeatherStore {
public:
    bool load(WeatherSnapshot &snapshot) const;
    bool save(const WeatherSnapshot &snapshot) const;
};
