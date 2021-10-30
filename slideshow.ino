#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM !!!"
#endif

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "epd_driver.h"
#include "firasans.h"
#include "esp_adc_cal.h"
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <JPEGDEC.h>

#define VOLTAGE_THRESHOLD   3.0
#define BATT_PIN            36
#define SD_MISO             12
#define SD_MOSI             13
#define SD_SCLK             14
#define SD_CS               15

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  20        /* Time ESP32 will go to sleep (in seconds) */

// Track which file to draw now
RTC_DATA_ATTR int bootCount = 0;

uint8_t *framebuffer;
uint8_t *ditherbuffer;
int vref = 1100;

JPEGDEC jpeg;

// Functions to access a file on the SD card
File myfile;

void * myOpen(const char *filename, int32_t *size) {
  Serial.print("Open: ");
  Serial.println(filename);
  myfile = SD.open(filename);
  *size = myfile.size();
  return &myfile;
}
void myClose(void *handle) {
  //  Serial.println("Close");
  if (myfile) myfile.close();
  else Serial.println("No file in close");
}
int32_t myRead(JPEGFILE *handle, uint8_t *buffer, int32_t length) {
  //  Serial.printf("Read %d\n", length);
  if (!myfile) {
    Serial.println("No file in read");
    return 0;
  }
  return myfile.read(buffer, length);
}
int32_t mySeek(JPEGFILE *handle, int32_t position) {
  //  Serial.printf("Seek %d\n", position);
  if (!myfile) {
    return 0;
    Serial.println("No file in seek");
  }
  return myfile.seek(position);
}

// Function to draw pixels to the display
int JPEGDraw(JPEGDRAW *pDraw) {
  Rect_t area = {
    .x = pDraw->x,
    .y = pDraw->y,
    .width = pDraw->iWidth,
    .height =  pDraw->iHeight
  };

  epd_copy_to_framebuffer(area, (uint8_t *) pDraw->pPixels, framebuffer);

  //Serial.printf("jpeg draw: x,y=%d,%d, cx,cy = %d,%d\n",
  //              pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  return 1;
}

int drawFile(const char *name, uint8_t *framebuffer)
{
  int res = jpeg.open((const char *)name, myOpen, myClose, myRead, mySeek, JPEGDraw);
  if (res == 0) {
    Serial.print("Failed to open file: ");
    Serial.println(name);
    return 0;
  }
  jpeg.setPixelType(FOUR_BIT_DITHERED);
  jpeg.decodeDither(ditherbuffer, 0);
  jpeg.close();

  return 1;
}

void setup()
{
  volatile uint32_t t1 = millis();

  char buf[128];

  Serial.begin(115200);

  /*
    SD Card test
    Only as a test SdCard hardware, use example reference
    https://github.com/espressif/arduino-esp32/tree/master/libraries/SD/examples
  * * */
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI);
  bool rlst = SD.begin(SD_CS);
  if (!rlst) {
    Serial.println("SD init failed");
    snprintf(buf, 128, "➸ No detected SdCard");
  } else {
    snprintf(buf, 128, "➸ Detected SdCard insert:%.2f GB", SD.cardSize() / 1024.0 / 1024.0 / 1024.0);
  }

  // Correct the ADC reference voltage
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
    vref = adc_chars.vref;
  }

  // When reading the battery voltage, POWER_EN must be turned on
  epd_poweron();

  uint16_t v = analogRead(BATT_PIN);
  float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
  String voltage = "➸ Voltage :" + String(battery_voltage) + "V";
  Serial.println(voltage);

  epd_init();

  // Init frame buffers
  framebuffer = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
  if (!framebuffer) {
    Serial.println("alloc memory failed !!!");
    while (1);
  }
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  ditherbuffer = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
  memset(ditherbuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  char fileName[18];
  sprintf(fileName, "/frames/%06d.jpg", bootCount);

  File entry = SD.open(fileName);

  int drawRes = drawFile(fileName, framebuffer);
  if (drawRes == 0) {
    Serial.println("Reseting boot counter");
    bootCount = 0;
  } else {
    if (battery_voltage < VOLTAGE_THRESHOLD) {
      epd_fill_rect(15, 10, 10, 5, 0xff, framebuffer);
      epd_draw_rect(15, 10, 10, 5, 0x00, framebuffer);
      epd_fill_rect(10, 15, 20, 30, 0xff, framebuffer);
      epd_draw_rect(10, 15, 20, 30, 0x00, framebuffer);
    }

    epd_clear();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    // Draw again for deeper blacks
    delay(700);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    delay(700);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  }



  // === log
  //  Rect_t logArea = {
  //    .x = 100,
  //    .y = 460,
  //    .width = 760,
  //    .height = 50,
  //  };
  //
  //  int cursor_x = 200;
  //  int cursor_y = 500;
  //
  //  epd_clear_area(logArea);
  //  writeln((GFXfont *)&FiraSans, fileName, &cursor_x, &cursor_y, NULL);
  //
  //  cursor_x += 40;
  //  sprintf(fileName, "%d", jpeg.getLastError());
  //
  //  writeln((GFXfont *)&FiraSans, fileName, &cursor_x, &cursor_y, NULL);
  // === log end


  // There are two ways to close

  // It will turn off the power of the ink screen, but cannot turn off the blue LED light.
  //   epd_poweroff();

  //It will turn off the power of the entire
  // POWER_EN control and also turn off the blue LED light
  epd_poweroff_all();

  delay(1000);

  volatile uint32_t t2 = millis();
  Serial.printf("Was awake for %dms.\n", t2 - t1);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                 " Seconds");
  Serial.println("Going to sleep now");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop()
{
  return;
}
