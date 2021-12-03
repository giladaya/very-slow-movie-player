#pragma once

#include <Arduino.h>
#include <SD.h>
#include <JPEGDEC.h>
#include "epd_driver.h"

namespace JpegUtils {
  extern uint8_t *_jpegDrawBuffer;
  extern JPEGDEC jpeg;
  extern File myfile;
  extern int lastError;
  extern uint8_t *ditherbuffer;

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


  void * myOpen(const char *filename, int32_t *size);
  void myClose(void *handle);
  int32_t myRead(JPEGFILE *handle, uint8_t *buffer, int32_t length);
  int32_t mySeek(JPEGFILE *handle, int32_t position);
  int JPEGDraw(JPEGDRAW *pDraw);
  int drawFileToBuffer(const char *name, uint8_t *frameBuffer);
}