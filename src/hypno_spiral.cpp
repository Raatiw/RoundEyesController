#include "hypno_spiral.h"

#include <Arduino_GFX_Library.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#if defined(ESP32)
#include <esp_heap_caps.h>
#endif

#include "config.h"

#if !defined(ENABLE_HYPNO_SPIRAL)

void hypnoSetup() {}
void hypnoStep() {}

#else

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

#if defined(HYPNO_RAINBOW_PRIMARY)
constexpr uint16_t rgb565FromRgb888(uint8_t r, uint8_t g, uint8_t b)
{
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

uint16_t wheelRgb565(uint8_t wheelPos)
{
  wheelPos = static_cast<uint8_t>(255 - wheelPos);

  if (wheelPos < 85)
  {
    const uint8_t ramp = static_cast<uint8_t>(wheelPos * 3);
    return rgb565FromRgb888(static_cast<uint8_t>(255 - ramp), 0, ramp);
  }

  if (wheelPos < 170)
  {
    wheelPos = static_cast<uint8_t>(wheelPos - 85);
    const uint8_t ramp = static_cast<uint8_t>(wheelPos * 3);
    return rgb565FromRgb888(0, ramp, static_cast<uint8_t>(255 - ramp));
  }

  wheelPos = static_cast<uint8_t>(wheelPos - 170);
  const uint8_t ramp = static_cast<uint8_t>(wheelPos * 3);
  return rgb565FromRgb888(ramp, static_cast<uint8_t>(255 - ramp), 0);
}
#endif
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
  const float radiusExponent = HYPNO_RADIUS_EXPONENT;
  const bool applyRadiusExponent = fabsf(radiusExponent - 1.0f) > 1e-5f;

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
      const float easedRadius =
          applyRadiusExponent ? powf(normalizedRadius, radiusExponent) : normalizedRadius;

      float angle = atan2f(fy, fx); // -pi..pi
      angle = (angle + PI) / (2.0f * PI); // 0..1

      float combined = angle * HYPNO_STRIPE_COUNT + easedRadius * HYPNO_TWIST_FACTOR;
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

  phaseOffset = static_cast<uint16_t>(phaseOffset + HYPNO_PHASE_INCREMENT);

  float duty = HYPNO_STRIPE_DUTY;
  if (duty < 0.0f)
  {
    duty = 0.0f;
  }
  else if (duty > 1.0f)
  {
    duty = 1.0f;
  }
  const uint16_t dutyThreshold = static_cast<uint16_t>(duty * 65535.0f);

  for (size_t i = 0; i < PIXEL_COUNT; ++i)
  {
    if (!maskMap[i])
    {
      spiralBuffer[i] = HYPNO_BACKGROUND_COLOR;
      continue;
    }

    const uint16_t value = static_cast<uint16_t>(phaseMap[i] + phaseOffset);
    const bool stripeOn = value < dutyThreshold;
    if (stripeOn)
    {
#if defined(HYPNO_RAINBOW_PRIMARY)
      spiralBuffer[i] = wheelRgb565(static_cast<uint8_t>(value >> 8));
#else
      spiralBuffer[i] = HYPNO_PRIMARY_COLOR;
#endif
    }
    else
    {
      spiralBuffer[i] = HYPNO_SECONDARY_COLOR;
    }
  }

  gfx->draw16bitRGBBitmap(0, 0, spiralBuffer, WIDTH, HEIGHT);
}

#endif // ENABLE_HYPNO_SPIRAL
