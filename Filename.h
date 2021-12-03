#pragma once

#include <SD.h>

// Path of file to store current folder and frame number
#define FOLDER_FILE "/__cur_folder.txt"

namespace Filename
{
  char *getPath();
  void readFromSd();
  void writeToSd();
  void advance(uint8_t delta);
  void moveToNextFolder();
  void genFilePath(const char *folder, const int fileNumber, char *frameFilePath);

  extern RTC_DATA_ATTR int fileNumber;
  extern RTC_DATA_ATTR char curFolder[128];
  extern char frameFilePath[128];
}
