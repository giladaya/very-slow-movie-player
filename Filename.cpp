#include "Filename.h"

namespace Filename
{
  char *getPath()
  {
    genFilePath(curFolder, fileNumber, frameFilePath);
    return frameFilePath;
  }

  void readFromSd()
  {
    File myfile = SD.open(FOLDER_FILE);
    if (myfile)
    {
      int pos = 0;
      uint8_t ch;
      while (myfile.available())
      {
        ch = myfile.read();
        if (ch == ',')
        {
          break;
        }
        curFolder[pos++] = ch;
      }

      //https://stackoverflow.com/questions/42602481/how-do-i-write-integers-to-a-micro-sd-card-on-an-arduino
      myfile.read((byte *)&fileNumber, sizeof(int)); // read 2 bytes

      myfile.close();
      Serial.printf("Read folder from SD %s, %d\n", curFolder, fileNumber);
    }
    else
    {
      Serial.printf("Failed to open file %s\n", FOLDER_FILE);
    }
  }

  void writeToSd()
  {
    File myfile = SD.open(FOLDER_FILE, FILE_WRITE);
    if (myfile)
    {
      // Folder name
      myfile.print(curFolder);

      // Delimiter
      myfile.print(',');

      // Frame number
      myfile.write((byte *)&fileNumber, sizeof(int)); // write 2 bytes

      myfile.close();
      Serial.printf("Saved folder name to SD: %s, %d\n", curFolder, fileNumber);
    }
    else
    {
      Serial.printf("Failed to open file for write %s\n", FOLDER_FILE);
    }
  }
  void advance(uint8_t delta)
  {
    fileNumber += delta;
    genFilePath(curFolder, fileNumber, frameFilePath);

    if (!SD.exists(frameFilePath))
    {
      moveToNextFolder();
    }
  }

  void moveToNextFolder()
  {
    Serial.println("Searching new folder");
    // Reset file counter
    fileNumber = 1;

    // Now need to find the next folder
    File dir = SD.open("/");
    dir.rewindDirectory();

    File entry;
    // Try to find File for curFolder
    Serial.printf("Moving to current folder %s\n", curFolder);
    while (true)
    {
      entry = dir.openNextFile();
      if (!entry || (entry.isDirectory() && strcmp(entry.name(), curFolder) == 0))
      {
        break;
      }
      // Serial.printf("Checked %s, is not %s\n", entry.name(), curFolder);
    }

    if (!entry)
    {
      // current folder was not found, just rollback.
      Serial.println("current folder was not found, just rewind");
      dir.rewindDirectory();
    }
    else
    {
      Serial.printf("current folder found %s\n", entry.name());
    }

    // Now look for a folder with a file we can display
    bool looped = false;

    while (true)
    {
      entry = dir.openNextFile();
      if (!entry)
      {
        // Reached the end of the folder, try to go back to the start
        if (!looped)
        {
          Serial.println("Rewind folder");
          dir.rewindDirectory();
          looped = true;
          continue;
        }
        else
        {
          // We already looped once, so nothing found - give up
          Serial.println("Suitable folder not found");
          break;
        }
      }

      // We are guaranteed to have entry here
      Serial.printf("Checking %s\n", entry.name());
      if (entry.isDirectory())
      {
        genFilePath(entry.name(), fileNumber, frameFilePath);
        if (SD.exists(frameFilePath))
        {
          strlcpy(curFolder, entry.name(), 127);
          Serial.printf("Found new folder %s\n", curFolder);
          // Persist new folder name to SD card
          writeToSd();
          break;
        }
        else
        {
          Serial.printf("File not found %s, moving on\n", frameFilePath);
        }
      }
    }
  }

  void genFilePath(const char *folder, const int fileNumber, char *frameFilePath)
  {
    sprintf(frameFilePath, "%s/%06d.jpg", folder, fileNumber);
  }

  RTC_DATA_ATTR int fileNumber = 0;
  RTC_DATA_ATTR char curFolder[128];
  char frameFilePath[128];
}
