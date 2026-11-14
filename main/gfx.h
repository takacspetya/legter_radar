#pragma once

#include <stdint.h>
#include <string.h>

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Fél fényerő
inline uint16_t dim(uint16_t c) { return (c >> 1) & 0x7BEF; }

// Egyszerű RGB565 képpuffer rajzolóprimitívekkel.
// A színeket natív sorrendben kapja, és bájtcserélve tárolja,
// mert a kijelző big-endian sorrendben várja a pixeleket.
class Canvas {
 public:
  Canvas(int w, int h, uint16_t *buf) : w_(w), h_(h), buf_(buf) {}

  int width() const { return w_; }
  int height() const { return h_; }
  const uint16_t *buffer() const { return buf_; }

  void fill(uint16_t c);
  void pixel(int x, int y, uint16_t c) {
    if ((unsigned)x < (unsigned)w_ && (unsigned)y < (unsigned)h_) buf_[y * w_ + x] = swap(c);
  }
  void hline(int x, int y, int w, uint16_t c);
  void vline(int x, int y, int h, uint16_t c);
  void fillRect(int x, int y, int w, int h, uint16_t c);
  void line(int x0, int y0, int x1, int y1, uint16_t c);
  void circle(int cx, int cy, int r, uint16_t c);
  void fillCircle(int cx, int cy, int r, uint16_t c);
  void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t c);

  // 5x7-es betűk, 6 px-es lépközzel, `size`-szoros nagyítással
  void text(int x, int y, const char *s, int size, uint16_t c);
  static int textWidth(const char *s, int size) { return (int)strlen(s) * 6 * size; }

 private:
  static uint16_t swap(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

  int w_, h_;
  uint16_t *buf_;
};
