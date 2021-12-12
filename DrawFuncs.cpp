#include "DrawFuncs.h"

namespace DrawFuncs
{
  // Good for frame extracted with no processing
  // Or very dark frames extracted with curves applied
  void drawDark(uint8_t *framebuffer)
  {
    Serial.println("Draw Dark");

    epd_clear_area_cycles(epd_full_screen(), 2, 80);
    delay(100);
    
    epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_WHITE);
    epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_WHITE);
    epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_BLACK);
    
    //delay(50);
    epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
    epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
    //epd_push_pixels(epd_full_screen(), 1, 1);
    epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_BLACK);

    epd_push_pixels(epd_full_screen(), 3, 0);
  }

  // Good for frames extracted with curves applied
  void drawLight(uint8_t *framebuffer)
  {
    Serial.println("Draw Light");
    epd_clear_area_cycles(epd_full_screen(), 2, 80);
    delay(100);
    
    epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_WHITE);
    epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_WHITE);
    epd_draw_image(epd_full_screen(), framebuffer, WHITE_ON_BLACK);
    
    //delay(50);
    epd_draw_image(epd_full_screen(), framebuffer, BLACK_ON_WHITE);
    //epd_push_pixels(epd_full_screen(), 1, 1);
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
