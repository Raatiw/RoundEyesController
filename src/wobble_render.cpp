#include "wobble_render.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <pgmspace.h>

#include "config.h"

#if defined(ENABLE_WOBBLE_SCREEN)

extern Arduino_GFX *gfx;

namespace
{
static_assert(BITMAP_ANIM_FRAME_COUNT > 0, "Bitmap animation requires at least one frame");

constexpr uint16_t CANVAS_WIDTH = DISPLAY_WIDTH;
constexpr uint16_t CANVAS_HEIGHT = DISPLAY_HEIGHT;

#ifndef BITMAP_ANIM_BACKGROUND
#define BITMAP_ANIM_BACKGROUND 0x0000
#endif

#ifndef BITMAP_ANIM_DEFAULT_DELAY
#define BITMAP_ANIM_DEFAULT_DELAY 67
#endif

int16_t computeOffset(uint16_t canvas, uint16_t image)
{
  if (canvas <= image)
  {
    return 0;
  }
  return static_cast<int16_t>((canvas - image) / 2);
}

const int16_t offsetX = computeOffset(CANVAS_WIDTH, BITMAP_ANIM_WIDTH);
const int16_t offsetY = computeOffset(CANVAS_HEIGHT, BITMAP_ANIM_HEIGHT);
uint16_t currentFrame = 0;
uint32_t lastFrameMillis = 0;

constexpr uint16_t frameCount = BITMAP_ANIM_FRAME_COUNT;

const uint16_t *framePtr(uint16_t index)
{
  return &BITMAP_ANIM_FRAME_DATA[index][0];
}

uint16_t frameDelay(uint16_t index)
{
#if defined(BITMAP_ANIM_DELAY_DATA)
  uint16_t delayValue = pgm_read_word(&BITMAP_ANIM_DELAY_DATA[index]);
  if (delayValue == 0)
  {
    delayValue = BITMAP_ANIM_DEFAULT_DELAY;
  }
  return delayValue;
#else
  (void)index;
  return BITMAP_ANIM_DEFAULT_DELAY;
#endif
}

void drawFrame(uint16_t index)
{
  gfx->draw16bitRGBBitmap(offsetX, offsetY, framePtr(index), BITMAP_ANIM_WIDTH, BITMAP_ANIM_HEIGHT);
}
} // namespace

void wobbleSetup()
{
  gfx->fillScreen(BITMAP_ANIM_BACKGROUND);
  currentFrame = 0;
  lastFrameMillis = millis();
  drawFrame(currentFrame);
}

void wobbleLoop()
{
  const uint32_t now = millis();
  if (now - lastFrameMillis < frameDelay(currentFrame))
  {
    return;
  }

  currentFrame = static_cast<uint16_t>((currentFrame + 1) % frameCount);
  drawFrame(currentFrame);
  lastFrameMillis = now;
}

#endif // ENABLE_WOBBLE_SCREEN
