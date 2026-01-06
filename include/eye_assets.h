#pragma once

#include <stddef.h>
#include <stdint.h>

struct EyeAsset
{
  const char *name;
  const uint16_t *sclera;
  const uint16_t *iris;
  const uint16_t *polar;
  const uint8_t *upper;
  const uint8_t *lower;
  uint16_t scleraWidth;
  uint16_t scleraHeight;
  uint16_t irisMapWidth;
  uint16_t irisMapHeight;
  uint16_t irisWidth;
  uint16_t irisHeight;
  uint16_t screenWidth;
  uint16_t screenHeight;
  uint16_t irisMin;
  uint16_t irisMax;
};

extern const EyeAsset *activeEye;

size_t eyeAssetCount();
const EyeAsset *getEyeAsset(size_t index);
