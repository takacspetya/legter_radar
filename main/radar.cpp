#include "radar.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

namespace {

constexpr uint32_t kSweepPeriodMs = 4000;
constexpr int kSweepTrailDeg = 30;
constexpr float kMaxExtrapolationS = 60.0f;
constexpr uint32_t kStaleAfterMs = 30000;
constexpr int kMaxLabels = 8;

// Egy gép aktuális (becsült) helyzete
struct Track {
  const Aircraft *ac;
  float east, north, distNm;
};
Track tracks[kMaxAircraft];

// Elrendezés – radarBegin() számolja ki
int cx, cy, R;
int ts;    // alap betűméret-szorzó
float px;  // pixel-szorzó egy 240 px-es kijelzőhöz képest

inline float rad(float d) { return d * (float)M_PI / 180.0f; }
inline int sx(float v) { return (int)lroundf(v * px); }

uint16_t hsv(float h, float s, float v) {
  float c = v * s;
  float hp = fmodf(h / 60.0f, 6.0f);
  float x = c * (1 - fabsf(fmodf(hp, 2.0f) - 1));
  float r = 0, g = 0, b = 0;
  if (hp < 1)      { r = c; g = x; }
  else if (hp < 2) { r = x; g = c; }
  else if (hp < 3) { g = c; b = x; }
  else if (hp < 4) { g = x; b = c; }
  else if (hp < 5) { r = x; b = c; }
  else             { r = c; b = x; }
  float m = v - c;
  return rgb((uint8_t)((r + m) * 255), (uint8_t)((g + m) * 255),
             (uint8_t)((b + m) * 255));
}

// Magasság szerinti szín: alacsonyan narancs, középen zöld, magasan lila.
uint16_t altColor(const Aircraft &a) {
  if (a.onGround) return rgb(140, 140, 140);
  if (!a.hasAlt) return rgb(220, 220, 220);
  float t = std::clamp(a.altFt / 40000.0f, 0.0f, 1.0f);
  return hsv(20 + t * 260, 0.85f, 1.0f);
}

const char *label(const Aircraft &a) { return a.callsign[0] ? a.callsign : a.hex; }

bool moving(const Aircraft &a) {
  return !a.onGround && !isnan(a.gsKt) && !isnan(a.trackDeg);
}

// Holtszámítással továbbvezeti a pozíciókat, és távolság szerint rendez.
int buildTracks(const Snapshot &s) {
  float since = (millis() - s.fetchedAtMs) / 1000.0f;
  for (int i = 0; i < s.count; ++i) {
    const Aircraft &a = s.ac[i];
    Track &t = tracks[i];
    t.ac = &a;
    t.east = a.east;
    t.north = a.north;
    if (moving(a)) {
      float age = fminf(a.posAgeS + since, kMaxExtrapolationS);
      float d = a.gsKt * age / 3600.0f;
      t.east += d * sinf(rad(a.trackDeg));
      t.north += d * cosf(rad(a.trackDeg));
    }
    t.distNm = sqrtf(t.east * t.east + t.north * t.north);
  }
  std::sort(tracks, tracks + s.count,
            [](const Track &a, const Track &b) { return a.distNm < b.distNm; });
  return s.count;
}

int findSelected(UiState &ui, int n) {
  for (int i = 0; i < n; ++i) {
    if (strcmp(tracks[i].ac->hex, ui.selectedHex) == 0) return i;
  }
  strlcpy(ui.selectedHex, tracks[0].ac->hex, sizeof ui.selectedHex);
  return 0;
}

void textCentered(Canvas &g, const char *s, int y, int size, uint16_t color) {
  g.text(cx - Canvas::textWidth(s, size) / 2, y, s, size, color);
}

void fmtAlt(char *buf, size_t n, const Aircraft &a) {
  if (a.onGround) {
    snprintf(buf, n, "GND");
  } else if (!a.hasAlt) {
    snprintf(buf, n, "---");
  } else {
    const char *trend = a.vRateFpm > 300 ? " ^" : a.vRateFpm < -300 ? " v" : "";
    snprintf(buf, n, "%ld %s%s", lroundf(a.altFt * units::kAltPerFt),
             units::kAlt, trend);
  }
}

void fmtDist(char *buf, size_t n, float d) {
  snprintf(buf, n, d < 10 ? "%.1f" : "%.0f", d);
}

void drawPlane(Canvas &g, int x, int y, float trackDeg, uint16_t color) {
  if (isnan(trackDeg)) {
    g.fillCircle(x, y, sx(2), color);
    return;
  }
  float r = rad(trackDeg);
  float s = sinf(r), c = cosf(r);
  float len = 6 * px, half = 4 * px;
  // képernyőn az irányvektor (s, -c), a rá merőleges (c, s)
  float bx = x - s * len * 0.6f, by = y + c * len * 0.6f;
  g.fillTriangle(lroundf(x + s * len), lroundf(y - c * len),
                 lroundf(bx + c * half), lroundf(by + s * half),
                 lroundf(bx - c * half), lroundf(by - s * half), color);
}

void drawRadar(Canvas &g, const Snapshot &s, const UiState &ui, int n, bool wifiOk) {
  const float rangeNm = units::rangeNm(ui.rangeIdx);
  const float scale = R / rangeNm;

  // Pásztázó sugár elhalványuló csóvával
  float sweep = (millis() % kSweepPeriodMs) * 360.0f / kSweepPeriodMs;
  for (int i = kSweepTrailDeg; i >= 0; --i) {
    float a = rad(sweep - i);
    uint8_t v = i == 0 ? 220 : 90 * (kSweepTrailDeg - i) / kSweepTrailDeg;
    g.line(cx, cy, cx + lroundf(R * sinf(a)), cy - lroundf(R * cosf(a)), rgb(0, v, 0));
  }

  // Távolsággyűrűk és tengelyek
  const uint16_t ringCol = rgb(0, 90, 0);
  for (int i = 1; i <= 3; ++i) g.circle(cx, cy, R * i / 3, ringCol);
  g.hline(cx - R, cy, 2 * R, ringCol);
  g.vline(cx, cy - R, 2 * R, ringCol);

  const uint16_t scaleCol = rgb(0, 160, 0);
  char buf[24];
  for (int i = 1; i <= 3; ++i) {
    fmtDist(buf, sizeof buf, units::kRangeSteps[ui.rangeIdx] * i / 3);
    if (i == 3) strlcat(buf, units::kDist, sizeof buf);
    g.text(cx + R * i / 3 - Canvas::textWidth(buf, ts) - sx(2), cy + sx(3), buf, ts, scaleCol);
  }
  g.text(cx + sx(3), cy - R + sx(4), "N", ts, scaleCol);

  // Gépek – a lista távolság szerint rendezett
  int inRange = 0;
  for (int i = 0; i < n; ++i) {
    const Track &t = tracks[i];
    if (t.distNm > rangeNm) break;
    ++inRange;
    const Aircraft &a = *t.ac;
    int x = cx + lroundf(t.east * scale);
    int y = cy - lroundf(t.north * scale);
    uint16_t col = altColor(a);

    // Vezetővonal: hol lesz egy perc múlva
    if (moving(a)) {
      float d = a.gsKt / 60.0f * scale;
      float r = rad(a.trackDeg);
      g.line(x, y, x + lroundf(d * sinf(r)), y - lroundf(d * cosf(r)), dim(col));
    }
    drawPlane(g, x, y, a.trackDeg, col);

    if (i < kMaxLabels) {
      const char *l = label(a);
      int lx = x > cx + R / 2 ? x - sx(6) - Canvas::textWidth(l, ts) : x + sx(6);
      g.text(lx, y - sx(3), l, ts, rgb(200, 200, 200));
    }
  }

  // Saját pozíció
  g.fillCircle(cx, cy, sx(2), rgb(255, 255, 255));

  // Állapotsor
  const uint16_t errCol = rgb(255, 80, 60);
  uint16_t statusCol = rgb(0, 200, 0);
  if (!wifiOk) {
    snprintf(buf, sizeof buf, "NO WIFI");
    statusCol = errCol;
  } else if (s.status == FeedStatus::Connecting) {
    snprintf(buf, sizeof buf, "CONNECTING");
  } else if (s.status == FeedStatus::HttpError) {
    snprintf(buf, sizeof buf, "HTTP %d", s.lastHttpCode);
    statusCol = errCol;
  } else if (s.status == FeedStatus::ParseError) {
    snprintf(buf, sizeof buf, "DATA ERR");
    statusCol = errCol;
  } else {
    snprintf(buf, sizeof buf, "%d AC", inRange);
  }
  uint32_t age = millis() - s.fetchedAtMs;
  if (s.fetchedAtMs && age > kStaleAfterMs) {
    size_t len = strlen(buf);
    snprintf(buf + len, sizeof buf - len, " %lus", (unsigned long)(age / 1000));
    if (s.status == FeedStatus::Ok) statusCol = rgb(255, 180, 0);
  }
  textCentered(g, buf, cy + R - sx(16), ts, statusCol);
}

void drawDetail(Canvas &g, UiState &ui, int n) {
  g.circle(cx, cy, R, rgb(0, 90, 0));
  if (n == 0) {
    textCentered(g, "NO AIRCRAFT", cy - 4 * ts, ts, rgb(160, 160, 160));
    return;
  }
  int sel = findSelected(ui, n);
  const Track &t = tracks[sel];
  const Aircraft &a = *t.ac;
  uint16_t col = altColor(a);

  // Irányjelző pötty a peremen: merre van a gép tőlünk
  float brg = atan2f(t.east, t.north) * 180.0f / (float)M_PI;
  if (brg < 0) brg += 360;
  g.fillCircle(cx + lroundf((R - sx(7)) * sinf(rad(brg))),
               cy - lroundf((R - sx(7)) * cosf(rad(brg))), sx(4), col);

  const uint16_t grey = rgb(170, 170, 170);
  const uint16_t white = rgb(235, 235, 235);
  char buf[32], val[24];

  snprintf(buf, sizeof buf, "%d/%d", sel + 1, n);
  textCentered(g, buf, cy + sx(-80), ts, grey);
  textCentered(g, label(a), cy + sx(-64), 3 * ts, col);
  snprintf(buf, sizeof buf, "%s  %s", a.type[0] ? a.type : "?", a.reg[0] ? a.reg : a.hex);
  textCentered(g, buf, cy + sx(-34), ts, grey);

  fmtAlt(val, sizeof val, a);
  snprintf(buf, sizeof buf, "ALT %s", val);
  textCentered(g, buf, cy + sx(-16), 2 * ts, white);

  if (isnan(a.gsKt)) snprintf(buf, sizeof buf, "GS ---");
  else snprintf(buf, sizeof buf, "GS %ld %s", lroundf(a.gsKt * units::kSpdPerKt), units::kSpd);
  textCentered(g, buf, cy + sx(4), 2 * ts, white);

  if (isnan(a.trackDeg)) snprintf(buf, sizeof buf, "TRK ---");
  else snprintf(buf, sizeof buf, "TRK %03ld", lroundf(a.trackDeg) % 360);
  textCentered(g, buf, cy + sx(24), 2 * ts, white);

  fmtDist(val, sizeof val, t.distNm * units::kDistPerNm);
  snprintf(buf, sizeof buf, "DST %s %s", val, units::kDist);
  textCentered(g, buf, cy + sx(44), 2 * ts, white);

  snprintf(buf, sizeof buf, "BRG %03ld", lroundf(brg) % 360);
  textCentered(g, buf, cy + sx(64), 2 * ts, white);
}

}  // namespace

void radarBegin(const Canvas &g) {
  int size = std::min(g.width(), g.height());
  cx = g.width() / 2;
  cy = g.height() / 2;
  R = size / 2 - 2;
  px = size / 240.0f;
  ts = std::max(1, (int)lroundf(px));
}

void radarSplash(Canvas &g, const char *msg) {
  g.fill(0);
  g.circle(cx, cy, R, rgb(0, 90, 0));
  textCentered(g, "LEGTER RADAR", cy - sx(14), 2 * ts, rgb(0, 220, 0));
  textCentered(g, msg, cy + sx(10), ts, rgb(170, 170, 170));
}

void radarDraw(Canvas &g, const Snapshot &s, UiState &ui, bool wifiOk) {
  g.fill(0);
  int n = buildTracks(s);
  if (ui.mode == ViewMode::Radar) {
    drawRadar(g, s, ui, n, wifiOk);
  } else {
    drawDetail(g, ui, n);
  }
}

void radarSelectNext(const Snapshot &s, UiState &ui) {
  int n = buildTracks(s);
  if (n == 0) return;
  int next = (findSelected(ui, n) + 1) % n;
  strlcpy(ui.selectedHex, tracks[next].ac->hex, sizeof ui.selectedHex);
}
