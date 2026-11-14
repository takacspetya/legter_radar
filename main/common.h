#pragma once

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_timer.h"
#include "sdkconfig.h"

constexpr int kMaxAircraft = 64;

inline uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }

inline double homeLat() { return strtod(CONFIG_RADAR_HOME_LAT, nullptr); }
inline double homeLon() { return strtod(CONFIG_RADAR_HOME_LON, nullptr); }

namespace units {

#ifdef CONFIG_RADAR_UNITS_METRIC
constexpr float kDistPerNm = 1.852f;
constexpr const char *kDist = "km";
constexpr float kAltPerFt = 0.3048f;
constexpr const char *kAlt = "m";
constexpr float kSpdPerKt = 1.852f;
constexpr const char *kSpd = "km/h";
// Választható hatótávok (rövid gombnyomás léptet)
constexpr float kRangeSteps[] = {10, 25, 50, 100};
#else
constexpr float kDistPerNm = 1.0f;
constexpr const char *kDist = "nm";
constexpr float kAltPerFt = 1.0f;
constexpr const char *kAlt = "ft";
constexpr float kSpdPerKt = 1.0f;
constexpr const char *kSpd = "kt";
constexpr float kRangeSteps[] = {5, 10, 25, 50};
#endif

constexpr int kRangeCount = sizeof(kRangeSteps) / sizeof(kRangeSteps[0]);
constexpr int kDefaultRange = 1;

inline float rangeNm(int idx) { return kRangeSteps[idx] / kDistPerNm; }

// A legnagyobb hatótávot kérdezzük le, így a váltás azonnali.
inline int fetchRadiusNm() {
  int r = (int)ceilf(rangeNm(kRangeCount - 1)) + 1;
  return r > 250 ? 250 : r;
}

}  // namespace units
