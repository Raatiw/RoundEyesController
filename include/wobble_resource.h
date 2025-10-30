#pragma once

#include "animated_gif_resource.h"
#include "wobble.h"

inline constexpr AnimatedGifResource kAnimatedGifResource = {
    wobble,
    sizeof(wobble),
    static_cast<uint16_t>(0x0000),
    static_cast<uint16_t>(67)};
