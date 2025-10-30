#include "wobble_render.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#include "config.h"

#if defined(ENABLE_WOBBLE_SCREEN)

#include "wobble.h"

extern Arduino_GFX *gfx;

namespace
{
constexpr uint16_t CANVAS_WIDTH = DISPLAY_WIDTH;
constexpr uint16_t CANVAS_HEIGHT = DISPLAY_HEIGHT;
constexpr uint16_t BACKGROUND_COLOR = 0x0000; // black

int16_t computeOffset(uint16_t canvas, uint16_t image)
{
  if (canvas <= image)
  {
    return 0;
  }
  return static_cast<int16_t>((canvas - image) / 2);
}

const int16_t offsetX = computeOffset(CANVAS_WIDTH, WOBBLE_WIDTH);
const int16_t offsetY = computeOffset(CANVAS_HEIGHT, WOBBLE_HEIGHT);
} // namespace

void wobbleSetup()
{
  gfx->fillScreen(BACKGROUND_COLOR);
  gfx->draw16bitRGBBitmap(offsetX, offsetY, wobble, WOBBLE_WIDTH, WOBBLE_HEIGHT);
}

void wobbleLoop()
{
  delay(16); // Yield periodically; nothing else to animate yet.
}

#endif // ENABLE_WOBBLE_SCREEN
