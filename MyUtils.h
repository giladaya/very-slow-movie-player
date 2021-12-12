#pragma once
#include <Arduino.h>
#include "epd_driver.h"
#include "esp_adc_cal.h"

namespace MyUtils
{
  float calcPartialAvg(uint8_t *framebuffer, uint8_t margin);
  float calcPartialDarkRatio(uint8_t *framebuffer, uint8_t margin);
  float readVoltage();
  void renderBattery(uint8_t *framebuffer, float voltage);

  // For voltage reading
  extern int vref;
}
