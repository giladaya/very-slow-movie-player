#pragma once
#include <Arduino.h>
#include "epd_driver.h"

namespace DrawFuncs {
  void drawDark(uint8_t *framebuffer);
  void drawLight(uint8_t *framebuffer);
  void drawNegative(uint8_t *frameBuffer);
}
