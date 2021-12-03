#include "MyUtils.h"

#define BATT_PIN            36
// Threshold to show low battery indicator
#define VOLTAGE_THRESHOLD   3.7

namespace MyUtils
{
  int vref = 1100;

  /**
   Calculate average of values in framebuffer,
   using pixels from center area
   Pixel data is packed (two pixels per byte)
*/
  float calcPartialAvg(uint8_t *framebuffer, uint8_t margin)
  {
    // calculate frame average
    uint32_t sum = 0;
    uint8_t mini = 255;
    uint8_t maxi = 0;
    uint8_t pix1, pix2;
    for (uint32_t i = margin; i < EPD_HEIGHT - margin; i++)
    {
      for (uint32_t j = margin; j < EPD_WIDTH / 2 - margin; j++)
      {
        pix1 = framebuffer[i * EPD_WIDTH / 2 + j] & 0x0F;
        pix2 = (framebuffer[i * EPD_WIDTH / 2 + j] & 0xF0) >> 4;
        if (pix1 > maxi)
          maxi = pix1;
        if (pix2 > maxi)
          maxi = pix2;
        if (pix1 < mini)
          mini = pix1;
        if (pix2 < mini)
          mini = pix2;
        sum = sum + pix1 + pix2;
      }
    }
    float frameAverage = (float)sum / ((EPD_WIDTH - margin - margin) * (EPD_HEIGHT - margin - margin));
    Serial.printf("margin %d, min: %d, max: %d, avg: %.2f, sum: %d\n", margin, mini, maxi, frameAverage, sum);
    return frameAverage;
  }

  float readVoltage()
  {
    // Correct the ADC reference voltage
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
      Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
      vref = adc_chars.vref;
    }

    uint16_t vRaw = analogRead(BATT_PIN);
    float battery_voltage = ((float)vRaw / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);

    return battery_voltage;
  }


  void renderBattery(uint8_t *framebuffer, float voltage)
  {
    if (voltage < VOLTAGE_THRESHOLD)
    {
      epd_fill_rect(15, 10, 10, 5, 0xff, framebuffer);
      epd_draw_rect(15, 10, 10, 5, 0x00, framebuffer);
      epd_fill_rect(10, 15, 20, 30, 0xff, framebuffer);
      epd_draw_rect(10, 15, 20, 30, 0x00, framebuffer);
    }
  }
}
