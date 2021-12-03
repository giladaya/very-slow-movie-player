#pragma once
#include <Arduino.h>
#include "epd_driver.h"

namespace DrawFuncs {
  void drawDark(uint8_t *framebuffer);
  void drawSimple(uint8_t *framebuffer);
  void draw01(uint8_t *framebuffer);
  void draw02(uint8_t *framebuffer);
  void draw04(uint8_t *framebuffer);
  void drawNegative(uint8_t *frameBuffer);
}