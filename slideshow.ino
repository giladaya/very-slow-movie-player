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

#define VOLTAGE_THRESHOLD   3.7
#define BATT_PIN            36
#define SD_MISO             12
#define SD_MOSI             13
#define SD_SCLK             14
#define SD_CS               15

// threshold of frame values average to consider a frame as dark
#define BRIGHTNESS_TH 3.5

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

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
RTC_DATA_ATTR int fileNumber = 0;
RTC_DATA_ATTR char curFolder[128];
char frameFilePath[128];

// Path of file to store current folder and frame number
#define FOLDER_FILE "/__cur_folder.txt"

//--------------------
// Config parameters
//--------------------
/*
    Time ESP32 will go to sleep in seconds
    (larger values - longer battery life)
*/
#define TIME_TO_SLEEP  15

/*
    How many frames to advance each update
    Value is a matter of taste.
    Lower means more uniform pictures,
    higher gives more variance but some scenes may be missed
*/
#define FRAMES_DELTA 1

/*
   Whether to use high quality screen clearing.
   False: some ghosting may show.
   True: no ghosting but uses more power
*/
#define HQ_CLEAR true

/*
   Whether to use different draw logic for dark frames
   False: dark frames look closer to the original but
     details may not show as they are too dark
   True: dark frames look better.
*/
#define DARK_FRAME_ENH true

/*
   How many frames to wait between saving current file number.
   Lower values means less frames will repeat in case of reset
   but more writes to the SD card.
*/
#define SAVE_FILE_NUM_EVERY 50   /* How many frames to wait between saving file number */

//#define SHOW_LOG
//#define SHOW_BARS


// frame buffers
uint8_t *_jpegDrawBuffer;
uint8_t *framebuffer;
uint8_t *ditherbuffer;
uint8_t *nframebuffer;

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

  epd_copy_to_framebuffer(area, (uint8_t *) pDraw->pPixels, _jpegDrawBuffer);

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
int drawFile(const char *name, uint8_t *frameBuffer)
{
  _jpegDrawBuffer = frameBuffer;
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
  Serial.println("Draw Dark");
  
  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_WHITE);
  delay(50);
  
  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
  
  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
  epd_push_pixels(epd_full_screen(), 20, 0);
  epd_push_pixels(epd_full_screen(), 20, 0);
  delay(50);
  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_BLACK);
}

void drawSimple(uint8_t *framebuffer) {
  Serial.println("Draw Simple");
  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_WHITE);
  delay(50);
  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
  delay(50);
}

void draw01(uint8_t *framebuffer) {
  Serial.println("Draw 01");
  //epd_clear();
  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_WHITE);
  delay(50);
  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
  delay(50);

  //epd_push_pixels(epd_full_screen(), 5, 0);

  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_BLACK);
  //delay(50);

  epd_push_pixels(epd_full_screen(), 20, 1);
  //delay(30);

  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);

  epd_push_pixels(epd_full_screen(), 2, 1);
}

void draw02(uint8_t *framebuffer) {
  Serial.println("Draw 02");
  
  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_WHITE);
  delay(50);
  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
  delay(50);

  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_BLACK);
  delay(50);

  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);

  epd_push_pixels(epd_full_screen(), 2, 1);
}

void draw04(uint8_t *framebuffer) {
  Serial.println("Draw 04");
  
  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_WHITE);
  delay(50);

  epd_push_pixels(epd_full_screen(), 20, 0);
  delay(20);
  epd_push_pixels(epd_full_screen(), 20, 0);
  delay(20);

  delay(150);
  epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_BLACK);
  //epd_push_pixels(epd_full_screen(), 10, 1);
  epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
}

// Used to erase previous frame before drawing a new frame
void drawNegative(uint8_t *frameBuffer) {
  Serial.println("Draw Negative");
  
  epd_draw_image(epd_full_screen(), frameBuffer, WHITE_ON_WHITE);
  return;
  delay(50);
  epd_draw_image(epd_full_screen(), frameBuffer, WHITE_ON_WHITE);
  delay(50);
  epd_draw_image(epd_full_screen(), frameBuffer, WHITE_ON_WHITE);
}

void drawBattery(uint8_t *framebuffer, float voltage) {
  if (voltage < VOLTAGE_THRESHOLD) {
    epd_fill_rect(15, 10, 10, 5, 0xff, framebuffer);
    epd_draw_rect(15, 10, 10, 5, 0x00, framebuffer);
    epd_fill_rect(10, 15, 20, 30, 0xff, framebuffer);
    epd_draw_rect(10, 15, 20, 30, 0x00, framebuffer);
  }
}

void readFolderFile() {
  myfile = SD.open(FOLDER_FILE);
  if (myfile) {
    int pos = 0;
    uint8_t ch;
    while (myfile.available()) {
      ch = myfile.read();
      if (ch == ',') {
        break;
      }
      curFolder[pos++] = ch;
    }

    //https://stackoverflow.com/questions/42602481/how-do-i-write-integers-to-a-micro-sd-card-on-an-arduino
    myfile.read((byte*)&fileNumber, sizeof(int)); // read 2 bytes

    myfile.close();
    Serial.printf("Read folder from SD %s, %d\n", curFolder, fileNumber);
  } else {
    Serial.printf("Failed to open file %s\n", FOLDER_FILE);
  }
}

void writeFolderFile() {
  myfile = SD.open(FOLDER_FILE, FILE_WRITE);
  if (myfile) {
    // Folder name
    myfile.print(curFolder);

    // Delimiter
    myfile.print(',');

    // Frame number
    myfile.write((byte*)&fileNumber, sizeof(int)); // write 2 bytes

    myfile.close();
    Serial.printf("Saved folder name to SD: %s, %d\n", curFolder, fileNumber);
  }
  else {
    Serial.printf("Failed to open file for write %s\n", FOLDER_FILE);
  }
}

void genFilePath(const char* folder, const int fileNumber, char* frameFilePath) {
  sprintf(frameFilePath, "%s/%06d.jpg", folder, fileNumber);
}

/*
   Update the global file tracking variables:
   curFolder
   fileNumber
   frameFilePath
*/
void moveToNextFolder() {
  Serial.println("Searching new folder");
  // Reset file counter
  fileNumber = 1;

  // Now need to find the next folder
  File dir = SD.open("/");
  dir.rewindDirectory();

  File entry;
  // Try to find File for curFolder
  Serial.printf("Moving to current folder %s\n", curFolder);
  while (true) {
    entry = dir.openNextFile();
    if (!entry || (entry.isDirectory() && strcmp(entry.name(), curFolder) == 0)) {
      break;
    }
    // Serial.printf("Checked %s, is not %s\n", entry.name(), curFolder);
  }

  if (!entry) {
    // current folder was not found, just rollback.
    Serial.println("current folder was not found, just rewind");
    dir.rewindDirectory();
  } else {
    Serial.printf("current folder found %s\n", entry.name());
  }

  // Now look for a folder with a file we can display
  bool looped = false;

  while (true) {
    entry = dir.openNextFile();
    if (!entry) {
      // Reached the end of the folder, try to go back to the start
      if (!looped) {
        Serial.println("Rewind folder");
        dir.rewindDirectory();
        looped = true;
        continue;
      } else {
        // We already looped once, so nothing found - give up
        Serial.println("Suitable folder not found");
        break;
      }
    }

    // We are guaranteed to have entry here
    Serial.printf("Checking %s\n", entry.name());
    if (entry.isDirectory()) {
      genFilePath(entry.name(), fileNumber, frameFilePath);
      if (SD.exists(frameFilePath)) {
        strlcpy(curFolder, entry.name(), 127);
        Serial.printf("Found new folder %s\n", curFolder);
        // Persist new folder name to SD card
        writeFolderFile();
        break;
      } else {
        Serial.printf("File not found %s, moving on\n", frameFilePath);
      }
    }
  }
}
void updateFolderFile(uint8_t delta) {
  fileNumber += delta;
  genFilePath(curFolder, fileNumber, frameFilePath);

  if (!SD.exists(frameFilePath)) {
    moveToNextFolder();
  }
}

float readVoltage() {
  // Correct the ADC reference voltage
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
    vref = adc_chars.vref;
  }

  uint16_t vRaw = analogRead(BATT_PIN);
  float battery_voltage = ((float)vRaw / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);

  return battery_voltage;
}

void updateDisplay() {
  // Measure wake time
  volatile uint32_t t1 = millis();

  char logBuf[128];

  //--------------------------
  // Read voltage
  //--------------------------
  volatile float battery_voltage = readVoltage();
  String voltage = "➸ Voltage :" + String(battery_voltage) + "V";
  Serial.println(voltage);

  //--------------------------
  // Read folder name from SD
  //--------------------------
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.printf("Wakeup reason %d\n", wakeup_reason);
  if (wakeup_reason == 0) {
    Serial.printf("Wakeup after reset\n");
    // read folder from file
    readFolderFile();
  }

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

  if (HQ_CLEAR) {
    nframebuffer = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
    memset(ditherbuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  }

  //--------------------------
  // Draw to frame buffers
  //--------------------------
  // First, draw last frame for erasing if needed
  if (HQ_CLEAR) {
    genFilePath(curFolder, fileNumber, frameFilePath);
    drawFile(frameFilePath, nframebuffer);
  }

  // Now draw new frame
  updateFolderFile(FRAMES_DELTA);
  if (fileNumber > 0) {
    Serial.printf("Draw new %s\n", frameFilePath);
    drawFile(frameFilePath, framebuffer);
  } else {
    int cursor_x = 420;
    int cursor_y = 290;
    writeln((GFXfont *)&FiraSans, "The End", &cursor_x, &cursor_y, framebuffer);
  }

  // We persist file details to SD card every SAVE_FILE_NUM_EVERY frames
  // To continue where we stopped in case of power loss
  if (fileNumber % SAVE_FILE_NUM_EVERY == 0) {
    writeFolderFile();
  }

  sprintf(
    logBuf,
    "%s, b# %d, v: %.2f, Err: %d, %d",
    frameFilePath,
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

  //--------------------------
  // Draw from frame buffers to display
  //--------------------------
  if (HQ_CLEAR) {
    drawNegative(nframebuffer);
  } else {
    epd_clear();
  }

  delay(50);

  if (DARK_FRAME_ENH) {
    float average = calcPartialAvg(framebuffer, 50);
    if (average < BRIGHTNESS_TH) {
      drawDark(framebuffer);
    } else {
      draw02(framebuffer);
    }
  } else {
    draw02(framebuffer);
  }

  // Report awake time
  volatile uint32_t t2 = millis();
  Serial.printf("Update took %dms.\n", t2 - t1);
}

void setup()
{
  Serial.begin(115200);

  // Track reboots
  ++bootCount;
  Serial.printf("****** Boot number %d ******\n", bootCount);

  //--------------------------
  // Init SD card
  //--------------------------

  // Note:
  // The order of the following lines is VERY important!

  // 1. Init SPI
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI);

  // 2. Init EPD
  epd_init();

  // 3. Init SD card
  bool rlst = SD.begin(SD_CS);
  if (rlst) {
    Serial.printf("➸ Detected SdCard insert:%.2f GB\n", SD.cardSize() / 1024.0 / 1024.0 / 1024.0);

    //--------------------
    // Draw new frame
    //--------------------

    // When reading the battery voltage, POWER_EN must be turned on
    epd_poweron();

    updateDisplay();

    // Turn off the power of the entire
    // POWER_EN control and also turn off the blue LED light
    epd_poweroff_all();
  } else {
    Serial.println("SD init failed, restarting");
    ESP.restart();
  }

  //------------------------------
  // Go to sleep
  //------------------------------
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Going to sleep now for " + String(TIME_TO_SLEEP) + " Seconds");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop()
{
  return;
}
