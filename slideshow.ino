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

#define VOLTAGE_THRESHOLD   3.9
#define BATT_PIN            36
#define SD_MISO             12
#define SD_MOSI             13
#define SD_SCLK             14
#define SD_CS               15

//#define SHOW_LOG
//#define SHOW_BARS

// delay between contrast-enhancing re-draws of buffer
#define REDRAW_DLAY 100
// threshold of frame values average to consider a frame as dark
#define BRIGHTNESS_TH 3.75

#define FOLDER "moonrise"

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  300        /* Time ESP32 will go to sleep (in seconds) */

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

// Track number of wakeups
RTC_DATA_ATTR int bootCount = 0;

// Track which file to draw now
RTC_DATA_ATTR int fileNumber = 1;

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
   Load a file by name, decode it and draw into a framebuffer

   @param name: File name to draw
   @param framebuffer: The framebuffer to draw to

   @return 0 if failed to open the file, 1 otherwise
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

/**
   Calculate average of values in framebuffer,
   using pixels from center area
   Pixel data is packed (two pixels per byte)
*/
float calcPartialAvg(uint8_t *framebuffer, uint8_t margin) {
  // calculate frame average
  uint32_t sum = 0;
  uint8_t mini = 255;
  uint8_t maxi = 0;
  uint8_t pix1, pix2;
  for (uint32_t i = margin; i < EPD_HEIGHT - margin; i++) {
    for (uint32_t j = margin; j < EPD_WIDTH / 2 - margin; j++) {
      pix1 = framebuffer[i * EPD_WIDTH / 2 + j] & 0x0F;
      pix2 = (framebuffer[i * EPD_WIDTH / 2 + j] & 0xF0) >> 4;
      if (pix1 > maxi) maxi = pix1;
      if (pix2 > maxi) maxi = pix2;
      if (pix1 < mini) mini = pix1;
      if (pix2 < mini) mini = pix2;
      sum = sum + pix1 + pix2;
    }
  }
  float frameAverage = (float)sum / ((EPD_WIDTH - margin - margin) * (EPD_HEIGHT - margin - margin));
  Serial.printf("margin %d, min: %d, max: %d, avg: %.2f, sum: %d\n", margin, mini, maxi, frameAverage, sum);
  return frameAverage;
}

void drawDark(uint8_t *framebuffer) {
  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
  //  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
//  delay(REDRAW_DLAY);
  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
  epd_push_pixels(epd_full_screen(), 20, 0);
  epd_push_pixels(epd_full_screen(), 20, 0);
  delay(REDRAW_DLAY);
  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_BLACK);
}

void drawLight(uint8_t *framebuffer) {
  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_BLACK);
  delay(REDRAW_DLAY);

  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_BLACK);

  delay(REDRAW_DLAY);

  epd_push_pixels(epd_full_screen(), 20, 1);
  epd_push_pixels(epd_full_screen(), 20, 1);

  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
}

void drawLightSimple(uint8_t *framebuffer) {
  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_BLACK);
  epd_push_pixels(epd_full_screen(), 20, 1);
  epd_push_pixels(epd_full_screen(), 20, 1);
  epd_push_pixels(epd_full_screen(), 20, 1);
  epd_push_pixels(epd_full_screen(), 20, 1);
  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
}

void drawBattery(uint8_t *framebuffer, float voltage) {
  if (voltage < VOLTAGE_THRESHOLD) {
    epd_fill_rect(15, 10, 10, 5, 0xff, framebuffer);
    epd_draw_rect(15, 10, 10, 5, 0x00, framebuffer);
    epd_fill_rect(10, 15, 20, 30, 0xff, framebuffer);
    epd_draw_rect(10, 15, 20, 30, 0x00, framebuffer);
  }
}

void setup()
{
  // Measure wake time
  volatile uint32_t t1 = millis();

  char logBuf[128];

  Serial.begin(115200);

  //--------------------------
  // Init SD card
  //--------------------------
  /*
    SD Card test
    Only as a test SdCard hardware, use example reference
    https://github.com/espressif/arduino-esp32/tree/master/libraries/SD/examples
  * * */

  // Note:
  // The order of the following lines is VERY important!
  // 1. Init SPI
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI);

  // 2. Init EPD
  epd_init();

  // 3. Init SD card
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

  //--------------------------
  // Calculate file name
  //--------------------------
  char fileName[128];
  sprintf(fileName, "/%s/%06d.jpg", FOLDER, fileNumber);
  fileNumber += 1;

  if (!SD.exists(fileName)) {
    fileNumber = 1;
    int cursor_x = 420;
    int cursor_y = 290;
    writeln((GFXfont *)&FiraSans, "The End", &cursor_x, &cursor_y, framebuffer);
  } else {
    // Draw to buffer
    drawFile(fileName, framebuffer);
  }

  // calculate frame average
  float frameAverage = calcPartialAvg(framebuffer, 50);

  sprintf(
    logBuf,
    "%s, b# %d, v: %.2f, Err: %d, %d",
    fileName,
    bootCount,
    battery_voltage,
    lastError,
    jpeg.getLastError()
  );
  Serial.println(logBuf);

#ifdef SHOW_LOG
  epd_fill_rect(50, 460, 860, 50, 0xff, framebuffer);
  epd_fill_rect(40, 475, 20, 20, 0x00, framebuffer);
  epd_fill_rect(900, 475, 20, 20, 0x00, framebuffer);

  int cursor_x = 60;
  int cursor_y = 500;

  writeln((GFXfont *)&FiraSans, logBuf, &cursor_x, &cursor_y, framebuffer);
#endif

#ifdef SHOW_BARS
  uint8_t c1 = bootCount % 2 ? 0xff : 0x00;
  uint8_t c2 = bootCount % 2 ? 0x00 : 0xff;

  epd_fill_rect(0, 0, 480, 10, c1, framebuffer);
  epd_fill_rect(480, 0, 480, 10, c2, framebuffer);
  epd_fill_rect(0, 530, 480, 10, c2, framebuffer);
  epd_fill_rect(480, 530, 480, 10, c1, framebuffer);
#endif

  // Display low battery indicator if needed
  drawBattery(framebuffer, battery_voltage);

  // Draw from frame buffer to display
//  epd_clear();
  epd_clear_area_cycles(epd_full_screen(), 4, 50);
  epd_clear_area_cycles(epd_full_screen(), 4, 20);

  if (frameAverage < BRIGHTNESS_TH) {
    Serial.println("Drawing for dark frame");
    drawDark(framebuffer);
  } else {
    Serial.println("Drawing for light frame");
    drawLightSimple(framebuffer);
  }

  // Turn off the power of the entire
  // POWER_EN control and also turn off the blue LED light
  epd_poweroff_all();

  // Report awake time
  volatile uint32_t t2 = millis();
  Serial.printf("Was awake for %dms.\n", t2 - t1);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Going to sleep now for " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop()
{
  return;
}
