#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM !!!"
#endif

#include <Arduino.h>
#include <esp_task_wdt.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
#include "epd_driver.h"
#include "firasans.h"
#include "esp_adc_cal.h"
//#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <JPEGDEC.h>

#define VOLTAGE_THRESHOLD   3.0
#define BATT_PIN            36
#define SD_MISO             12
#define SD_MOSI             13
#define SD_SCLK             14
#define SD_CS               15

#define SHOW_LOG

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  20        /* Time ESP32 will go to sleep (in seconds) */

// Error codes returned by getLastError()
enum {
  SUCCESS = 0,

  SD_CARD_INIT_FAIL, // 1
  FILE_OPEN_ERROR,   // 2
  FILE_READ_ERROR,   // 3
  JPEG_OPEN_ERROR,   // 4
  JPEG_DEC_ERROR,    // 5
  MEM_ALOC_ERROR,    // 6
};
int lastError = SUCCESS;

// Track which file to draw now
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int fileNumber = 0;

// frame buffers
uint8_t *framebuffer;
uint8_t *ditherbuffer;

// For voltage reading
int vref = 1100;

// =========================================
// Jpeg decoder related
// =========================================
// For jpeg decoding
JPEGDEC jpeg;

// Functions to access a file on the SD card
File myfile;

void * myOpen(const char *filename, int32_t *size) {
  Serial.print("Open: ");
  Serial.println(filename);
  myfile = SD.open(filename);
  if (myfile) {
    *size = myfile.size();
  } else {
    lastError = FILE_OPEN_ERROR;
  }

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
    lastError = FILE_READ_ERROR;
    return 0;
  }
  return myfile.read(buffer, length);
}
int32_t mySeek(JPEGFILE *handle, int32_t position) {
  //  Serial.printf("Seek %d\n", position);
  if (!myfile) {
    Serial.println("No file in seek");
    lastError = FILE_READ_ERROR;
    return 0;
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

// =======================================

/**
 * Load a file by name, decode it and draw into a framebuffer
 *
 * @param name: File name to draw
 * @param framebuffer: The framebuffer to draw to
 *
 * @return 0 if failed to open the file, 1 otherwise
 */
int drawFile(const char *name, uint8_t *framebuffer)
{
  int res = jpeg.open((const char *)name, myOpen, myClose, myRead, mySeek, JPEGDraw);
  if (res == 0) {
    Serial.print("Failed to open file: ");
    Serial.println(name);
    lastError = JPEG_OPEN_ERROR;
    return 0;
  }
  jpeg.setPixelType(FOUR_BIT_DITHERED);
  res = jpeg.decodeDither(ditherbuffer, 0);
  jpeg.close();

  if (res == 0) {
    Serial.print("Failed to decode file: ");
    Serial.println(name);
    lastError = JPEG_DECODE_ERROR;
    return 0;
  }

  return 1;
}

void setup()
{
  // Measure wake time
  volatile uint32_t t1 = millis();

  Serial.begin(115200);

  //--------------------------
  // Init SD card
  //--------------------------

  /*
    SD Card test
    Only as a test SdCard hardware, use example reference
    https://github.com/espressif/arduino-esp32/tree/master/libraries/SD/examples
  * * */
  char logBuf[128];

  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI);
  bool rlst = SD.begin(SD_CS);
  if (!rlst) {
    Serial.println("SD init failed");
    snprintf(logBuf, 128, "➸ No detected SdCard");
    lastError = SD_CARD_INIT_FAIL;
  } else {
    snprintf(logBuf, 128, "➸ Detected SdCard insert:%.2f GB", SD.cardSize() / 1024.0 / 1024.0 / 1024.0);
  }

  //--------------------------
  // Read voltage
  //--------------------------

  // Correct the ADC reference voltage
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
    vref = adc_chars.vref;
  }

  // When reading the battery voltage, POWER_EN must be turned on
  epd_init();
  epd_poweron();

  uint16_t vRaw = analogRead(BATT_PIN);
  float battery_voltage = ((float)vRaw / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
  String voltage = "➸ Voltage :" + String(battery_voltage) + "V";
  Serial.println(voltage);

  //--------------------------
  // Init frame buffers
  //--------------------------
  framebuffer = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
  if (!framebuffer) {
    Serial.println("alloc memory failed !!!");
    lastError = MEM_ALOC_ERROR;
    while (1);
  }
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  ditherbuffer = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
  memset(ditherbuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  // Track reboots
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  // Calculate file name
  char fileName[18];
  ++fileNumber;
  sprintf(fileName, "/frames/%06d.jpg", fileNumber);

  // Draw to buffer
  int drawRes = drawFile(fileName, framebuffer);

  if (lastError != SUCCESS) {
    // Failed to draw
    // Assume it's because we tried to open a file that does not exist
    // Start from beginning
    Serial.println("Reseting boot counter");
    fileNumber = 0;
  }

  #ifdef SHOW_LOG
  epd_fill_rect(100, 460, 760, 50, 0xff, framebuffer);
  sprintf(logBuf, "b# %d, Err: %d, v: %d, sd: %d|%d, jpe: %d", bootCount, lastError, vRaw, rlst, SD.cardType(), jpeg.getLastError());
  int cursor_x = 110;
  int cursor_y = 500;

  writeln((GFXfont *)&FiraSans, logBuf, &cursor_x, &cursor_y, framebuffer);
  #endif

  // Display low battery indicator if needed
  if (battery_voltage < VOLTAGE_THRESHOLD) {
    epd_fill_rect(15, 10, 10, 5, 0xff, framebuffer);
    epd_draw_rect(15, 10, 10, 5, 0x00, framebuffer);
    epd_fill_rect(10, 15, 20, 30, 0xff, framebuffer);
    epd_draw_rect(10, 15, 20, 30, 0x00, framebuffer);
  }

  // Draw from frame buffer to display
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  // Draw again for deeper blacks
  delay(700);
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
  delay(700);
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);


  epd_poweroff_all();

  // Report awake time
  volatile uint32_t t2 = millis();
  Serial.printf("Was awake for %dms.\n", t2 - t1);

  // Go to sleep
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Going to sleep now for " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop()
{
  return;
}
