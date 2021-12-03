#include "JpegUtils.h";

namespace JpegUtils
{
  uint8_t *_jpegDrawBuffer;
  JPEGDEC jpeg;
  File myfile;
  int lastError = SUCCESS;
  uint8_t *ditherbuffer;

  void *myOpen(const char *filename, int32_t *size)
  {
    Serial.print("Open: ");
    Serial.println(filename);
    myfile = SD.open(filename);
    if (myfile)
    {
      *size = myfile.size();
    }
    else
    {
      lastError = FILE_OPEN_ERROR;
    }

    return &myfile;
  }

  void myClose(void *handle)
  {
    //  Serial.println("Close");
    if (myfile)
      myfile.close();
    else
      Serial.println("No file in close");
  }

  int32_t myRead(JPEGFILE *handle, uint8_t *buffer, int32_t length)
  {
    //  Serial.printf("Read %d\n", length);
    if (!myfile)
    {
      Serial.println("No file in read");
      lastError = FILE_READ_ERROR;
      return 0;
    }
    return myfile.read(buffer, length);
  }

  int32_t mySeek(JPEGFILE *handle, int32_t position)
  {
    //  Serial.printf("Seek %d\n", position);
    if (!myfile)
    {
      Serial.println("No file in seek");
      lastError = FILE_READ_ERROR;
      return 0;
    }
    return myfile.seek(position);
  }

  int JPEGDraw(JPEGDRAW *pDraw)
  {
    Rect_t area = {
        .x = pDraw->x,
        .y = pDraw->y,
        .width = pDraw->iWidth,
        .height = pDraw->iHeight};

    epd_copy_to_framebuffer(area, (uint8_t *)pDraw->pPixels, _jpegDrawBuffer);

    //Serial.printf("jpeg draw: x,y=%d,%d, cx,cy = %d,%d\n",
    //              pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
    return 1;
  }

  int drawFileToBuffer(const char *name, uint8_t *frameBuffer)
  {
    _jpegDrawBuffer = frameBuffer;
    int res = jpeg.open((const char *)name, myOpen, myClose, myRead, mySeek, JPEGDraw);
    if (res == 0)
    {
      Serial.print("Failed to open file: ");
      Serial.println(name);
      lastError = JPEG_OPEN_ERROR;
      return 0;
    }

    // Init buffer
    ditherbuffer = (uint8_t *)heap_caps_malloc(EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
    memset(ditherbuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    jpeg.setPixelType(FOUR_BIT_DITHERED);
    res = jpeg.decodeDither(ditherbuffer, 0);
    jpeg.close();

    if (res == 0)
    {
      Serial.print("Failed to decode file: ");
      Serial.println(name);
      lastError = JPEG_DECODE_ERROR;
      return 0;
    }

    return 1;
  }
}