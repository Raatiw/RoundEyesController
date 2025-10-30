#include "animated_gif_player.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <AnimatedGIF.h>

#include "config.h"

#if defined(ENABLE_ANIMATED_GIF)

extern Arduino_GFX *gfx;

namespace
{
AnimatedGIF gif;
bool gifReady = false;
int16_t offsetX = 0;
int16_t offsetY = 0;
uint16_t lineBuffer[DISPLAY_WIDTH];
uint32_t lastFrameMillis = 0;
uint16_t lastFrameDelay = ANIMATED_GIF_DEFAULT_DELAY;

constexpr uint16_t CANVAS_WIDTH = DISPLAY_WIDTH;
constexpr uint16_t CANVAS_HEIGHT = DISPLAY_HEIGHT;

#ifndef ANIMATED_GIF_BACKGROUND
#define ANIMATED_GIF_BACKGROUND 0x0000
#endif

#ifndef ANIMATED_GIF_DEFAULT_DELAY
#define ANIMATED_GIF_DEFAULT_DELAY 67
#endif

static_assert(ANIMATED_GIF_SIZE > 0, "Animated GIF resource must not be empty");

void blitRun(int16_t x, int16_t y, int16_t length)
{
  if (length <= 0)
  {
    return;
  }
  gfx->draw16bitRGBBitmap(x, y, lineBuffer, length, 1);
}

void GIFDraw(GIFDRAW *pDraw)
{
  int16_t x = offsetX + pDraw->iX;
  int16_t y = offsetY + pDraw->iY + pDraw->y;
  int16_t width = static_cast<int16_t>(pDraw->iWidth);

  if (x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT)
  {
    return;
  }

  if (x < 0)
  {
    const int16_t delta = static_cast<int16_t>(-x);
    x = 0;
    width -= delta;
    pDraw->pPixels += delta;
  }

  if (width <= 0)
  {
    return;
  }

  if (x + width > CANVAS_WIDTH)
  {
    width = CANVAS_WIDTH - x;
  }

  if (width <= 0)
  {
    return;
  }

  uint8_t *src = pDraw->pPixels;
  uint16_t *palette = pDraw->pPalette;

  if (pDraw->ucDisposalMethod == 2)
  {
    for (int16_t i = 0; i < width; ++i)
    {
      if (src[i] == pDraw->ucTransparent)
      {
        src[i] = pDraw->ucBackground;
      }
    }
    pDraw->ucHasTransparency = 0;
  }

  if (pDraw->ucHasTransparency)
  {
    int16_t runLen = 0;
    int16_t runStart = 0;

    for (int16_t i = 0; i < width; ++i)
    {
      uint8_t index = src[i];
      if (index == pDraw->ucTransparent)
      {
        if (runLen > 0)
        {
          blitRun(x + runStart, y, runLen);
          runLen = 0;
        }
        runStart = i + 1;
      }
      else
      {
        if (runLen == 0)
        {
          runStart = i;
        }
        if (runLen < static_cast<int16_t>(sizeof(lineBuffer) / sizeof(lineBuffer[0])))
        {
          lineBuffer[runLen++] = palette[index];
        }
      }
    }

    if (runLen > 0)
    {
      blitRun(x + runStart, y, runLen);
    }
  }
  else
  {
    const int16_t maxChunk = static_cast<int16_t>(sizeof(lineBuffer) / sizeof(lineBuffer[0]));
    int16_t processed = 0;
    while (processed < width)
    {
      int16_t chunk = width - processed;
      if (chunk > maxChunk)
      {
        chunk = maxChunk;
      }
      for (int16_t i = 0; i < chunk; ++i)
      {
        lineBuffer[i] = palette[src[processed + i]];
      }
      blitRun(x + processed, y, chunk);
      processed += chunk;
    }
  }
}
} // namespace

void animatedGifSetup()
{
  gif.begin(LITTLE_ENDIAN_PIXELS);

  gfx->fillScreen(ANIMATED_GIF_BACKGROUND);

  const uint8_t *gifData = reinterpret_cast<const uint8_t *>(ANIMATED_GIF_DATA);
  if (!gif.openFLASH(const_cast<uint8_t *>(gifData), static_cast<int>(ANIMATED_GIF_SIZE), GIFDraw))
  {
    Serial.println("Animated GIF: failed to open memory resource");
    gifReady = false;
    return;
  }

  const int canvasWidth = gif.getCanvasWidth();
  const int canvasHeight = gif.getCanvasHeight();

  offsetX = (CANVAS_WIDTH > canvasWidth) ? static_cast<int16_t>((CANVAS_WIDTH - canvasWidth) / 2) : 0;
  offsetY = (CANVAS_HEIGHT > canvasHeight) ? static_cast<int16_t>((CANVAS_HEIGHT - canvasHeight) / 2) : 0;

  lastFrameMillis = millis();
  lastFrameDelay = 0;
  gifReady = true;
}

void animatedGifLoop()
{
  if (!gifReady)
  {
    return;
  }

  const uint32_t now = millis();
  if (now - lastFrameMillis < lastFrameDelay)
  {
    return;
  }

  int delayMs = 0;
  const int result = gif.playFrame(false, &delayMs);
  if (result < 0)
  {
    Serial.printf("Animated GIF: playback error %d\n", gif.getLastError());
    gifReady = false;
    gif.close();
    return;
  }

  if (delayMs <= 0)
  {
    delayMs = ANIMATED_GIF_DEFAULT_DELAY;
  }

  lastFrameDelay = static_cast<uint16_t>(delayMs);
  lastFrameMillis = now;

  if (result == 0)
  {
    gif.reset();
    lastFrameDelay = 0;
  }
}

#endif // ENABLE_ANIMATED_GIF
