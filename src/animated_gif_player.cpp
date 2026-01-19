#include "animated_gif_player.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <AnimatedGIF.h>

#include "config.h"

#if defined(ANIMATED_GIF_USE_SD)
#include <SD.h>
#endif

#if defined(ENABLE_ANIMATED_GIF)

extern Arduino_GFX *gfx;

namespace
{
AnimatedGIF gif;
bool gifReady = false;
int16_t offsetX = 0;
int16_t offsetY = 0;
uint16_t gifSourceWidth = 0;
uint16_t gifSourceHeight = 0;
bool gifScaleEnabled = false;
uint8_t gifScale = 1;
uint16_t lineBuffer[DISPLAY_WIDTH];
uint32_t lastFrameMillis = 0;
uint16_t lastFrameDelay = ANIMATED_GIF_DEFAULT_DELAY;

constexpr uint16_t CANVAS_WIDTH = DISPLAY_WIDTH;
constexpr uint16_t CANVAS_HEIGHT = DISPLAY_HEIGHT;

#if !defined(ANIMATED_GIF_USE_SD)
static_assert(kAnimatedGifResource.size > 0, "Animated GIF resource must not be empty");
#endif

#if defined(ANIMATED_GIF_USE_SD)
constexpr uint32_t kGifSwitchIntervalMs = ANIMATED_GIF_SWITCH_INTERVAL_MS;
const char *const kGifFiles[] = ANIMATED_GIF_FILES;
constexpr size_t kGifFileCount = sizeof(kGifFiles) / sizeof(kGifFiles[0]);
size_t currentGifIndex = 0;
uint32_t lastSwitchMillis = 0;
File gifFile;
#endif

void resetGifTiming()
{
  lastFrameMillis = millis();
  lastFrameDelay = 0;
#if defined(ANIMATED_GIF_USE_SD)
  lastSwitchMillis = lastFrameMillis;
#endif
}

void blitRun(int16_t x, int16_t y, int16_t length)
{
  if (length <= 0)
  {
    return;
  }
  gfx->draw16bitRGBBitmap(x, y, lineBuffer, length, 1);
}

void blitScaledRun(int16_t x, int16_t y, int16_t length, uint8_t scale)
{
  if (length <= 0 || scale == 0)
  {
    return;
  }
  for (uint8_t row = 0; row < scale; ++row)
  {
    gfx->draw16bitRGBBitmap(x, static_cast<int16_t>(y + row), lineBuffer, length, 1);
  }
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

  const uint8_t scale = gifScaleEnabled ? gifScale : 1;
  if (gifScaleEnabled)
  {
    x = static_cast<int16_t>(x * scale);
    y = static_cast<int16_t>(y * scale);
    width = static_cast<int16_t>(width * scale);
    if (x >= CANVAS_WIDTH || y >= CANVAS_HEIGHT)
    {
      return;
    }
    if (x + width > CANVAS_WIDTH)
    {
      width = static_cast<int16_t>(CANVAS_WIDTH - x);
    }
    if (width <= 0)
    {
      return;
    }
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
          const uint16_t color = palette[index];
          for (uint8_t sx = 0; sx < scale; ++sx)
          {
            if (runLen < static_cast<int16_t>(sizeof(lineBuffer) / sizeof(lineBuffer[0])))
            {
              lineBuffer[runLen++] = color;
            }
          }
        }
      }
    }

    if (runLen > 0)
    {
      if (gifScaleEnabled)
      {
        blitScaledRun(static_cast<int16_t>(x + runStart * scale), y, runLen, scale);
      }
      else
      {
        blitRun(x + runStart, y, runLen);
      }
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
        const uint16_t color = palette[src[processed + i]];
        if (gifScaleEnabled)
        {
          for (uint8_t sx = 0; sx < scale; ++sx)
          {
            const int16_t idx = static_cast<int16_t>(i * scale + sx);
            if (idx < maxChunk)
            {
              lineBuffer[idx] = color;
            }
          }
        }
        else
        {
          lineBuffer[i] = color;
        }
      }
      if (gifScaleEnabled)
      {
        const int16_t scaledLen = static_cast<int16_t>(chunk * scale);
        blitScaledRun(static_cast<int16_t>(x + processed * scale), y, scaledLen, scale);
        processed += chunk;
      }
      else
      {
        blitRun(x + processed, y, chunk);
        processed += chunk;
      }
    }
  }
}

#if defined(ANIMATED_GIF_USE_SD)
void *GIFOpenFile(const char *szFilename, int32_t *pFileSize)
{
  gifFile.close();
  gifFile = SD.open(szFilename, FILE_READ);
  if (!gifFile)
  {
    return nullptr;
  }
  *pFileSize = static_cast<int32_t>(gifFile.size());
  return static_cast<void *>(&gifFile);
}

void GIFCloseFile(void *pHandle)
{
  File *file = static_cast<File *>(pHandle);
  if (file)
  {
    file->close();
  }
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
  File *file = static_cast<File *>(pFile->fHandle);
  if (!file)
  {
    return 0;
  }

  int32_t bytesToRead = iLen;
  if ((pFile->iSize - pFile->iPos) < iLen)
  {
    bytesToRead = pFile->iSize - pFile->iPos;
  }
  if (bytesToRead <= 0)
  {
    return 0;
  }

  const int32_t bytesRead = static_cast<int32_t>(file->read(pBuf, bytesToRead));
  pFile->iPos += bytesRead;
  return bytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
  if (iPosition < 0)
  {
    iPosition = 0;
  }
  else if (iPosition >= pFile->iSize)
  {
    iPosition = pFile->iSize - 1;
  }

  File *file = static_cast<File *>(pFile->fHandle);
  if (!file)
  {
    return 0;
  }

  file->seek(iPosition);
  pFile->iPos = iPosition;
  return iPosition;
}

bool openGifAtIndex(size_t index)
{
  gif.close();
  gifReady = false;

  if (index >= kGifFileCount)
  {
    return false;
  }

  const char *filename = kGifFiles[index];
  if (!gif.open(filename, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    Serial.printf("Animated GIF: failed to open %s\n", filename);
    return false;
  }

  const int canvasWidth = gif.getCanvasWidth();
  const int canvasHeight = gif.getCanvasHeight();

  gifSourceWidth = static_cast<uint16_t>(canvasWidth);
  gifSourceHeight = static_cast<uint16_t>(canvasHeight);
  gifScaleEnabled = (canvasWidth <= CANVAS_WIDTH / 2) && (canvasHeight <= CANVAS_HEIGHT / 2);
  gifScale = gifScaleEnabled ? 2 : 1;
  const uint16_t scaledWidth = static_cast<uint16_t>(canvasWidth * gifScale);
  const uint16_t scaledHeight = static_cast<uint16_t>(canvasHeight * gifScale);
  offsetX = (CANVAS_WIDTH > scaledWidth) ? static_cast<int16_t>((CANVAS_WIDTH - scaledWidth) / 2) : 0;
  offsetY = (CANVAS_HEIGHT > scaledHeight) ? static_cast<int16_t>((CANVAS_HEIGHT - scaledHeight) / 2) : 0;

  resetGifTiming();
  gifReady = true;
  return true;
}

bool openNextGif()
{
  if (kGifFileCount == 0)
  {
    return false;
  }

  for (size_t attempt = 0; attempt < kGifFileCount; ++attempt)
  {
    const size_t index = (currentGifIndex + attempt) % kGifFileCount;
    if (openGifAtIndex(index))
    {
      currentGifIndex = index;
      return true;
    }
  }

  return false;
}
#endif
} // namespace

void animatedGifSetup()
{
  gif.begin(LITTLE_ENDIAN_PIXELS);

  gfx->fillScreen(ANIMATED_GIF_BACKGROUND);

#if defined(ANIMATED_GIF_USE_SD)
  currentGifIndex = 0;
  if (!openNextGif())
  {
    Serial.println("Animated GIF: no SD GIF files opened");
    gifReady = false;
    return;
  }
#else
  const uint8_t *gifData = reinterpret_cast<const uint8_t *>(kAnimatedGifResource.data);
  if (!gif.openFLASH(const_cast<uint8_t *>(gifData), static_cast<int>(kAnimatedGifResource.size), GIFDraw))
  {
    Serial.println("Animated GIF: failed to open memory resource");
    gifReady = false;
    return;
  }

  const int canvasWidth = gif.getCanvasWidth();
  const int canvasHeight = gif.getCanvasHeight();

  gifSourceWidth = static_cast<uint16_t>(canvasWidth);
  gifSourceHeight = static_cast<uint16_t>(canvasHeight);
  gifScaleEnabled = (canvasWidth <= CANVAS_WIDTH / 2) && (canvasHeight <= CANVAS_HEIGHT / 2);
  gifScale = gifScaleEnabled ? 2 : 1;
  const uint16_t scaledWidth = static_cast<uint16_t>(canvasWidth * gifScale);
  const uint16_t scaledHeight = static_cast<uint16_t>(canvasHeight * gifScale);
  offsetX = (CANVAS_WIDTH > scaledWidth) ? static_cast<int16_t>((CANVAS_WIDTH - scaledWidth) / 2) : 0;
  offsetY = (CANVAS_HEIGHT > scaledHeight) ? static_cast<int16_t>((CANVAS_HEIGHT - scaledHeight) / 2) : 0;

  resetGifTiming();
  gifReady = true;
#endif
}

void animatedGifLoop()
{
  if (!gifReady)
  {
    return;
  }

  const uint32_t now = millis();

#if defined(ANIMATED_GIF_USE_SD) && !defined(ANIMATED_GIF_DISABLE_AUTO_SWITCH)
  if (kGifFileCount > 1 && kGifSwitchIntervalMs > 0 &&
      (now - lastSwitchMillis) >= kGifSwitchIntervalMs)
  {
    currentGifIndex = (currentGifIndex + 1) % kGifFileCount;
    if (openNextGif())
    {
      return;
    }
  }
#endif

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
  if (delayMs > ANIMATED_GIF_MAX_DELAY)
  {
    delayMs = ANIMATED_GIF_MAX_DELAY;
  }

  lastFrameDelay = static_cast<uint16_t>(delayMs);
  lastFrameMillis = now;

  if (result == 0)
  {
    gif.reset();
    lastFrameDelay = 0;
  }
}

size_t animatedGifFileCount()
{
#if defined(ANIMATED_GIF_USE_SD)
  return kGifFileCount;
#else
  return 1;
#endif
}

bool animatedGifOpenAtIndex(size_t index)
{
#if defined(ANIMATED_GIF_USE_SD)
  return openGifAtIndex(index);
#else
  if (index != 0 || !gifReady)
  {
    return false;
  }

  gif.reset();
  resetGifTiming();
  return true;
#endif
}

bool animatedGifIsReady()
{
  return gifReady;
}

#endif // ENABLE_ANIMATED_GIF
