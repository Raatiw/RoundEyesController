#pragma once

#include "animated_gif_resource.h"
#include "fractal.h"

inline constexpr AnimatedGifResource kAnimatedGifResource = {
    fractal,
    sizeof(fractal),
    static_cast<uint16_t>(0x0000),
    static_cast<uint16_t>(67)};
