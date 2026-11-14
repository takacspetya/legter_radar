#pragma once

#include <stdint.h>

#include "common.h"

struct Aircraft {
  char hex[8];        // ICAO 24 bites cím, nagybetűvel
  char callsign[10];
  char type[6];       // pl. A320
  char reg[12];       // lajstromjel, pl. HA-LYK
  bool onGround;
  bool hasAlt;
  int32_t altFt;
  int16_t vRateFpm;
  float gsKt;         // NAN, ha ismeretlen
  float trackDeg;     // NAN, ha ismeretlen
  float east, north;  // tengeri mérföld a saját pozíciótól, a lekérdezés idején
  float posAgeS;      // a pozíció kora a lekérdezés idején
};

enum class FeedStatus : uint8_t { Connecting, Ok, HttpError, ParseError };

struct Snapshot {
  Aircraft ac[kMaxAircraft];
  uint16_t count;
  uint32_t fetchedAtMs;  // utolsó sikeres lekérdezés (millis)
  FeedStatus status;
  int lastHttpCode;
};

// Elindítja a háttérben futó lekérdező taskot.
void feedBegin();

// Szálbiztos másolat a legfrissebb adatokról.
void feedCopy(Snapshot &out);
