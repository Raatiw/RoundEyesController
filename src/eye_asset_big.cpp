#include <Arduino.h>

#include "eye_assets.h"

#include "config.h"

#ifndef EYE_IRIS_MIN_DEFAULT
#define EYE_IRIS_MIN_DEFAULT 90
#endif
#ifndef EYE_IRIS_MAX_DEFAULT
#define EYE_IRIS_MAX_DEFAULT 130
#endif

#ifdef IRIS_MIN
#undef IRIS_MIN
#endif
#ifdef IRIS_MAX
#undef IRIS_MAX
#endif

#include "bigEye.h"

#ifndef IRIS_MIN
#define IRIS_MIN EYE_IRIS_MIN_DEFAULT
#endif
#ifndef IRIS_MAX
#define IRIS_MAX EYE_IRIS_MAX_DEFAULT
#endif

extern const EyeAsset kEyeBig = {
    "big",
    reinterpret_cast<const uint16_t *>(sclera),
    reinterpret_cast<const uint16_t *>(iris),
    reinterpret_cast<const uint16_t *>(polar),
    reinterpret_cast<const uint8_t *>(upper),
    reinterpret_cast<const uint8_t *>(lower),
    SCLERA_WIDTH,
    SCLERA_HEIGHT,
    IRIS_MAP_WIDTH,
    IRIS_MAP_HEIGHT,
    IRIS_WIDTH,
    IRIS_HEIGHT,
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    IRIS_MIN,
    IRIS_MAX};

#undef SCLERA_WIDTH
#undef SCLERA_HEIGHT
#undef IRIS_MAP_WIDTH
#undef IRIS_MAP_HEIGHT
#undef IRIS_WIDTH
#undef IRIS_HEIGHT
#undef SCREEN_WIDTH
#undef SCREEN_HEIGHT
#undef IRIS_MIN
#undef IRIS_MAX
