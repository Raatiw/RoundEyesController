#pragma once

#include "eye_types.h"

// Select which animation to run (enable at most one).
// #define ENABLE_HYPNO_SPIRAL
//#define ENABLE_ANIMATED_GIF



#if defined(ENABLE_HYPNO_SPIRAL) && defined(ENABLE_ANIMATED_GIF)
#error "Select only one animation mode"
#endif

// Common display hardware settings -----------------------------------
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240

// Tie to -1 when the backlight is permanently on (e.g. wired to 3V3).
#define DISPLAY_BACKLIGHT  -1
#define BACKLIGHT_MAX    255

#if defined(ENABLE_HYPNO_SPIRAL)

// Spiral hypnosis configuration --------------------------------------
#define HYPNO_PRIMARY_COLOR     0xFFFF // bright stripe colour (white)
#define HYPNO_SECONDARY_COLOR   0x0000 // dark stripe colour (black)
#define HYPNO_BACKGROUND_COLOR  0x0000 // outside the circle

#define HYPNO_STRIPE_COUNT      3.0f   // number of bright arms around the centre
#define HYPNO_TWIST_FACTOR      5.0f  // amount of angular twist from centre to rim
#define HYPNO_RADIUS_EXPONENT   0.85f  // radial easing (<1 tightens the centre)
#define HYPNO_STRIPE_DUTY       0.58f  // bright stripe proportion (0-1 range)
#define HYPNO_PHASE_INCREMENT   6512    // rotation speed per frame (larger = faster)

#elif defined(ENABLE_ANIMATED_GIF)

#ifndef ANIMATED_GIF_HEADER
//#define ANIMATED_GIF_HEADER "wobble.h"
//#define ANIMATED_GIF_SYMBOL wobble
// #define ANIMATED_GIF_HEADER "fractal.h"
// #define ANIMATED_GIF_SYMBOL fractal
#define ANIMATED_GIF_HEADER "phenakistiscope.h"
#define ANIMATED_GIF_SYMBOL phenakistiscope
#endif

#ifndef ANIMATED_GIF_BACKGROUND
#define ANIMATED_GIF_BACKGROUND 0x0000
#endif

#ifndef ANIMATED_GIF_DEFAULT_DELAY
#define ANIMATED_GIF_DEFAULT_DELAY 67
#endif

#include ANIMATED_GIF_HEADER

struct AnimatedGifResource
{
  const uint8_t *data;
  size_t size;
};

static constexpr AnimatedGifResource kAnimatedGifResource = {
    ANIMATED_GIF_SYMBOL,
    sizeof(ANIMATED_GIF_SYMBOL)};

#undef ANIMATED_GIF_SYMBOL
#undef ANIMATED_GIF_HEADER

#else

// GRAPHICS SETTINGS (appearance of eye) -----------------------------------

// Uncomment to use a symmetrical eyelid without a caruncle.
// #define SYMMETRICAL_EYELID

// Enable ONE of these includes -- HUGE graphics tables for various eyes:
#include "defaultEye.h"      // Standard human-ish hazel eye -OR-
// #include "bigEye.h"       // Custom big eye -OR-
// #include "dragonEye.h"    // Slit pupil fiery dragon/demon eye -OR-
// #include "noScleraEye.h"  // Large iris, no sclera -OR-
// #include "goatEye.h"      // Horizontal pupil goat/Krampus eye -OR-
// #include "newtEye.h"      // Eye of newt -OR-
// #include "terminatorEye.h"// Git to da choppah!
// #include "catEye.h"       // Cartoonish cat (flat "2D" colors)
// #include "owlEye.h"       // Minerva the owl (DISABLE TRACKING)
// #include "naugaEye.h"     // Nauga googly eye (DISABLE TRACKING)
// #include "doeEye.h"       // Cartoon deer eye (DISABLE TRACKING)

// DISPLAY HARDWARE SETTINGS (screen type & connections) -------------------
#define TFT_COUNT 1        // Number of screens (1 or 2)
#if defined(ARDUINO_ADAFRUIT_FEATHER_ESP32_V2)
  #define TFT1_CS -1       // Managed by Arduino_GFX bus
  #define TFT2_CS -1
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  // Default to the Seeed XIAO ESP32S3 wiring
  #define TFT1_CS 4
  #define TFT2_CS 4
#else
  #define TFT1_CS -1
  #define TFT2_CS -1
#endif
#define TFT_1_ROT 0        // TFT 1 rotation
#define TFT_2_ROT 0        // TFT 2 rotation
#define EYE_1_XPOSITION  ((DISPLAY_WIDTH - SCREEN_WIDTH) / 2)
#define EYE_2_XPOSITION  ((DISPLAY_WIDTH - SCREEN_WIDTH) / 2)
#define EYE_1_YPOSITION  ((DISPLAY_HEIGHT - SCREEN_HEIGHT) / 2)
#define EYE_2_YPOSITION  ((DISPLAY_HEIGHT - SCREEN_HEIGHT) / 2)

// EYE LIST ----------------------------------------------------------------
#define NUM_EYES 1 // Number of eyes to display (1 or 2)

#define BLINK_PIN   -1 // Pin for manual blink button (BOTH eyes)
#define LH_WINK_PIN -1 // Left wink pin (set to -1 for no pin)
#define RH_WINK_PIN -1 // Right wink pin (set to -1 for no pin)

#if (NUM_EYES == 2)
  static eyeInfo_t eyeInfo[] = {
    { TFT1_CS, LH_WINK_PIN, TFT_1_ROT, EYE_1_XPOSITION, EYE_1_YPOSITION }, // LEFT EYE
    { TFT2_CS, RH_WINK_PIN, TFT_2_ROT, EYE_2_XPOSITION, EYE_2_YPOSITION }, // RIGHT EYE
  };
#else
  static eyeInfo_t eyeInfo[] = {
    { TFT1_CS, LH_WINK_PIN, TFT_1_ROT, EYE_1_XPOSITION, EYE_1_YPOSITION }, // SINGLE EYE
  };
#endif

// INPUT SETTINGS (for controlling eye motion) -----------------------------

// #define JOYSTICK_X_PIN A0 // Analog pin for eye horiz pos (else auto)
// #define JOYSTICK_Y_PIN A1 // Analog pin for eye vert position (")
// #define JOYSTICK_X_FLIP   // If defined, reverse stick X axis
// #define JOYSTICK_Y_FLIP   // If defined, reverse stick Y axis
//#define ENABLE_EYELIDS     // If defined, render eyelids and blinking
#define TRACKING            // If defined, eyelid tracks pupil
#define AUTOBLINK           // If defined, eyes also blink autonomously

// #define LIGHT_PIN      -1 // Light sensor pin
#define LIGHT_CURVE  0.33 // Light sensor adjustment curve
#define LIGHT_MIN       0 // Minimum useful reading from light sensor
#define LIGHT_MAX    1023 // Maximum useful reading from sensor

#define IRIS_SMOOTH         // If enabled, filter input from IRIS_PIN
#if !defined(IRIS_MIN)      // Each eye might have its own MIN/MAX
  #define IRIS_MIN       90 // Iris size (0-1023) in brightest light
#endif
#if !defined(IRIS_MAX)
  #define IRIS_MAX      130 // Iris size (0-1023) in darkest light
#endif

#endif
