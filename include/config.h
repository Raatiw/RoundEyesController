#pragma once

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
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240

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

#define DISPLAY_BACKLIGHT  32 // Pin for backlight control (-1 for none)
#define BACKLIGHT_MAX    255

// EYE LIST ----------------------------------------------------------------
#define NUM_EYES 1 // Number of eyes to display (1 or 2)

#define BLINK_PIN   -1 // Pin for manual blink button (BOTH eyes)
#define LH_WINK_PIN -1 // Left wink pin (set to -1 for no pin)
#define RH_WINK_PIN -1 // Right wink pin (set to -1 for no pin)

#if (NUM_EYES == 2)
  eyeInfo_t eyeInfo[] = {
    { TFT1_CS, LH_WINK_PIN, TFT_1_ROT, EYE_1_XPOSITION }, // LEFT EYE
    { TFT2_CS, RH_WINK_PIN, TFT_2_ROT, EYE_2_XPOSITION }, // RIGHT EYE
  };
#else
  eyeInfo_t eyeInfo[] = {
    { TFT1_CS, LH_WINK_PIN, TFT_1_ROT, EYE_1_XPOSITION }, // SINGLE EYE
  };
#endif

// INPUT SETTINGS (for controlling eye motion) -----------------------------

// #define JOYSTICK_X_PIN A0 // Analog pin for eye horiz pos (else auto)
// #define JOYSTICK_Y_PIN A1 // Analog pin for eye vert position (")
// #define JOYSTICK_X_FLIP   // If defined, reverse stick X axis
// #define JOYSTICK_Y_FLIP   // If defined, reverse stick Y axis
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
