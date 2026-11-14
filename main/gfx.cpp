#include "gfx.h"

#include <math.h>
#include <stdlib.h>

#include <algorithm>
#include <utility>

#include "font5x7.h"

void Canvas::fill(uint16_t c) {
  if (c == 0) {
    memset(buf_, 0, (size_t)w_ * h_ * sizeof(uint16_t));
    return;
  }
  std::fill(buf_, buf_ + w_ * h_, swap(c));
}

void Canvas::hline(int x, int y, int w, uint16_t c) {
  if (y < 0 || y >= h_) return;
  int x1 = std::min(x + w, w_);
  x = std::max(x, 0);
  if (x >= x1) return;
  std::fill(buf_ + y * w_ + x, buf_ + y * w_ + x1, swap(c));
}

void Canvas::vline(int x, int y, int h, uint16_t c) {
  for (int i = 0; i < h; ++i) pixel(x, y + i, c);
}

void Canvas::fillRect(int x, int y, int w, int h, uint16_t c) {
  for (int i = 0; i < h; ++i) hline(x, y + i, w, c);
}

void Canvas::line(int x0, int y0, int x1, int y1, uint16_t c) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    pixel(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void Canvas::circle(int cx, int cy, int r, uint16_t c) {
  int x = r, y = 0, err = 1 - r;
  while (x >= y) {
    pixel(cx + x, cy + y, c); pixel(cx - x, cy + y, c);
    pixel(cx + x, cy - y, c); pixel(cx - x, cy - y, c);
    pixel(cx + y, cy + x, c); pixel(cx - y, cy + x, c);
    pixel(cx + y, cy - x, c); pixel(cx - y, cy - x, c);
    ++y;
    if (err < 0) {
      err += 2 * y + 1;
    } else {
      --x;
      err += 2 * (y - x) + 1;
    }
  }
}

void Canvas::fillCircle(int cx, int cy, int r, uint16_t c) {
  for (int dy = -r; dy <= r; ++dy) {
    int dx = (int)sqrtf((float)(r * r - dy * dy));
    hline(cx - dx, cy + dy, 2 * dx + 1, c);
  }
}

void Canvas::fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c) {
  // y szerint rendezés: y0 <= y1 <= y2
  if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
  if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }
  if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
  if (y0 == y2) {
    int lo = std::min({x0, x1, x2}), hi = std::max({x0, x1, x2});
    hline(lo, y0, hi - lo + 1, c);
    return;
  }
  for (int y = y0; y <= y2; ++y) {
    int xa = x0 + (x2 - x0) * (y - y0) / (y2 - y0);  // hosszú él
    int xb = y < y1 ? x0 + (x1 - x0) * (y - y0) / (y1 - y0)
                    : (y2 == y1 ? x1 : x1 + (x2 - x1) * (y - y1) / (y2 - y1));
    if (xa > xb) std::swap(xa, xb);
    hline(xa, y, xb - xa + 1, c);
  }
}

void Canvas::text(int x, int y, const char *s, int size, uint16_t c) {
  for (; *s; ++s, x += 6 * size) {
    unsigned char ch = (unsigned char)*s;
    if (ch < 32 || ch > 126) ch = '?';
    const uint8_t *glyph = kFont5x7[ch - 32];
    for (int col = 0; col < 5; ++col) {
      for (int row = 0; row < 7; ++row) {
        if (!(glyph[col] & (1 << row))) continue;
        if (size == 1) pixel(x + col, y + row, c);
        else fillRect(x + col * size, y + row * size, size, size, c);
      }
    }
  }
}
