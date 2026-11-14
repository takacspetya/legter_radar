#pragma once

#include "feed.h"
#include "gfx.h"

enum class ViewMode : uint8_t { Radar, Detail };

struct UiState {
  ViewMode mode;
  int rangeIdx;
  char selectedHex[8];  // üres = a legközelebbi gép
};

void radarBegin(const Canvas &g);
void radarSplash(Canvas &g, const char *msg);
void radarDraw(Canvas &g, const Snapshot &s, UiState &ui, bool wifiOk);

// Részletes nézetben a következő (távolabbi) gépre lép.
void radarSelectNext(const Snapshot &s, UiState &ui);
