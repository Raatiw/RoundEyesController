#pragma once

#include <stdint.h>
#include <stddef.h>

struct AnimatedGifResource
{
  const uint8_t *data;
  size_t size;
  uint16_t background;
  uint16_t defaultDelay;
};
