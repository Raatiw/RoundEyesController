#include "hypno_spiral.h"

#include <Arduino_GFX_Library.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

#include "config.h"

extern Arduino_GFX *gfx;

namespace
{
constexpr uint16_t WIDTH = DISPLAY_WIDTH;
constexpr uint16_t HEIGHT = DISPLAY_HEIGHT;
constexpr size_t PIXEL_COUNT = static_cast<size_t>(WIDTH) * HEIGHT;

uint16_t *spiralBuffer = nullptr;
uint16_t *phaseMap = nullptr;
uint8_t *maskMap = nullptr;
uint16_t phaseOffset = 0;
} // namespace

void hypnoSetup()
{
#if defined(ESP32)
  if (!spiralBuffer)
  {
    spiralBuffer = static_cast<uint16_t *>(
        heap_caps_malloc(PIXEL_COUNT * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  if (!phaseMap)
  {
    phaseMap = static_cast<uint16_t *>(
        heap_caps_malloc(PIXEL_COUNT * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  if (!maskMap)
  {
    maskMap = static_cast<uint8_t *>(
        heap_caps_malloc(PIXEL_COUNT * sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
#else
  if (!spiralBuffer)
  {
    spiralBuffer = static_cast<uint16_t *>(malloc(PIXEL_COUNT * sizeof(uint16_t)));
  }
  if (!phaseMap)
  {
    phaseMap = static_cast<uint16_t *>(malloc(PIXEL_COUNT * sizeof(uint16_t)));
  }
  if (!maskMap)
  {
    maskMap = static_cast<uint8_t *>(malloc(PIXEL_COUNT * sizeof(uint8_t)));
  }
#endif

  if (!spiralBuffer || !phaseMap || !maskMap)
  {
    Serial.println("Hypno spiral: buffer allocation failed");
    return;
  }

  const float cx = (static_cast<float>(WIDTH) - 1.0f) * 0.5f;
  const float cy = (static_cast<float>(HEIGHT) - 1.0f) * 0.5f;
  const float maxRadius = 0.5f * static_cast<float>(WIDTH < HEIGHT ? WIDTH : HEIGHT);
  const float invMaxRadius = (maxRadius > 0.0f) ? (1.0f / maxRadius) : 0.0f;

  for (uint16_t y = 0; y < HEIGHT; ++y)
  {
    const float fy = static_cast<float>(y) - cy;
    for (uint16_t x = 0; x < WIDTH; ++x)
    {
      const size_t index = static_cast<size_t>(y) * WIDTH + x;
      const float fx = static_cast<float>(x) - cx;
      const float radius = sqrtf(fx * fx + fy * fy);
      const bool inside = radius <= maxRadius;
      maskMap[index] = inside ? 1 : 0;

      if (!inside)
      {
        phaseMap[index] = 0;
        continue;
      }

      const float normalizedRadius = radius * invMaxRadius; // 0..1
      float angle = atan2f(fy, fx);                         // -pi..pi
      angle = (angle + PI) / (2.0f * PI);                   // 0..1

      float combined = angle * HYPNO_STRIPE_COUNT + normalizedRadius * HYPNO_TWIST_FACTOR;
      combined = combined - floorf(combined); // keep fractional part in [0,1)
      phaseMap[index] = static_cast<uint16_t>(combined * 65535.0f);
    }
  }
}

void hypnoStep()
{
  if (!spiralBuffer || !phaseMap || !maskMap)
  {
    return;
  }

  phaseOffset += static_cast<uint16_t>(HYPNO_PHASE_INCREMENT);

  for (size_t i = 0; i < PIXEL_COUNT; ++i)
  {
    if (!maskMap[i])
    {
      spiralBuffer[i] = HYPNO_BACKGROUND_COLOR;
      continue;
    }

    const uint16_t value = phaseMap[i] + phaseOffset;
    const uint8_t stripe = (value >> HYPNO_STRIPE_SHIFT) & 0x1;
    spiralBuffer[i] = stripe ? HYPNO_PRIMARY_COLOR : HYPNO_SECONDARY_COLOR;
  }

  gfx->draw16bitRGBBitmap(0, 0, spiralBuffer, WIDTH, HEIGHT);
}
