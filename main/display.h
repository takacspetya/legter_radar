#pragma once

#include <stdint.h>

#include "sdkconfig.h"

constexpr int kLcdSize = CONFIG_RADAR_LCD_SIZE;

void displayBegin();

// A teljes képpuffert kiküldi, és megvárja, amíg az átvitel befejeződik.
// A puffer DMA-képes memóriában legyen, big-endian RGB565 pixelekkel.
void displayFlush(const uint16_t *fb);
