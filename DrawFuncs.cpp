#include "DrawFuncs.h"

namespace DrawFuncs
{
  void drawDark(uint8_t *framebuffer)
  {
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

  void drawSimple(uint8_t *framebuffer)
  {
    Serial.println("Draw Simple");
    epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_WHITE);
    delay(50);
    epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
    delay(50);
  }

  void draw01(uint8_t *framebuffer)
  {
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

  void draw02(uint8_t *framebuffer)
  {
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

  void draw04(uint8_t *framebuffer)
  {
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
  void drawNegative(uint8_t *frameBuffer)
  {
    Serial.println("Draw Negative");

    epd_draw_image(epd_full_screen(), frameBuffer, WHITE_ON_WHITE);
    return;
    delay(50);
    epd_draw_image(epd_full_screen(), frameBuffer, WHITE_ON_WHITE);
    delay(50);
    epd_draw_image(epd_full_screen(), frameBuffer, WHITE_ON_WHITE);
  }
}