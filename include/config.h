#pragma once

#include "eye_types.h"

// Enable the animation families you want to use.
#define ENABLE_HYPNO_SPIRAL
#define ENABLE_ANIMATED_GIF
#define ENABLE_EYE_PROGRAM // Include the eye animation in the program loop.

// Optional visual transition when switching effects via BLE mapping.
// Uses the hypno spiral renderer as a short "swirl" interstitial.
#define ENABLE_SWIRL_TRANSITION
// Swirl transition shows the hypno spiral while the next program is prepared.
// It ends early as soon as the target is ready, with the values below acting
// as minimum/maximum bounds.
#define SWIRL_TRANSITION_DURATION_MS 500  // max
#define SWIRL_TRANSITION_MIN_MS 33        // min
#define SWIRL_TRANSITION_FRAME_MS 33

// BLE control (WLED VisualRemote usermod) ------------------------------
// VisualRemote broadcasts a manufacturer-data payload containing a preset ID.
// This firmware maps that preset number onto eye/GIF programs.
// - Set `VISUALREMOTE_ACCEPT_GLOBAL_PRESETS` to 1 to also react to the "global"
//   (WLED-to-WLED) preset broadcasts.
// - Set `VISUALREMOTE_GROUP_FILTER` to a 12-hex-digit string (e.g.
//   "aabbccddeeff") to only accept presets for that group; leave empty to
//   accept any group.
#ifndef VISUALREMOTE_ACCEPT_GLOBAL_PRESETS
#define VISUALREMOTE_ACCEPT_GLOBAL_PRESETS 0
#endif

#ifndef VISUALREMOTE_GROUP_FILTER
#define VISUALREMOTE_GROUP_FILTER ""
#endif

// Hold the BOOT button for `VISUALREMOTE_PAIR_HOLD_MS` to open a short
// "pairing" window where the next received VisualRemote packet will be used to
// learn + persist the group filter.
#ifndef VISUALREMOTE_PAIR_BUTTON_PIN
  #if defined(BUTTON)
    #define VISUALREMOTE_PAIR_BUTTON_PIN BUTTON
  #else
    #define VISUALREMOTE_PAIR_BUTTON_PIN -1
  #endif
#endif

#ifndef VISUALREMOTE_PAIR_HOLD_MS
#define VISUALREMOTE_PAIR_HOLD_MS 1500
#endif

#ifndef VISUALREMOTE_PAIR_WINDOW_MS
#define VISUALREMOTE_PAIR_WINDOW_MS 10000
#endif

// Pairing UI feedback.
#ifndef VISUALREMOTE_PAIR_STATUS_REFRESH_MS
#define VISUALREMOTE_PAIR_STATUS_REFRESH_MS 250
#endif
#ifndef VISUALREMOTE_PAIR_RESULT_DISPLAY_MS
#define VISUALREMOTE_PAIR_RESULT_DISPLAY_MS 2000
#endif

// Persist learned group filters in ESP32 NVS (Preferences).
#ifndef VISUALREMOTE_STORE_GROUP_FILTER
#define VISUALREMOTE_STORE_GROUP_FILTER 1
#endif

// Common display hardware settings -----------------------------------
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_ROTATION 2 // 0=default, 1=90°, 2=180°, 3=270°

// Tie to -1 when the backlight is permanently on (e.g. wired to 3V3).
#define DISPLAY_BACKLIGHT  -1
#define BACKLIGHT_MAX    255

#if defined(ENABLE_HYPNO_SPIRAL)

// Spiral hypnosis configuration --------------------------------------
#define HYPNO_PRIMARY_COLOR     0xFFFF // bright stripe colour (white)
#define HYPNO_SECONDARY_COLOR   0x0000 // dark stripe colour (black)
#define HYPNO_BACKGROUND_COLOR  0x0000 // outside the circle

// If enabled, the bright stripes render as a rainbow wheel instead of
// `HYPNO_PRIMARY_COLOR` (the dark stripes still use `HYPNO_SECONDARY_COLOR`).
#define HYPNO_RAINBOW_PRIMARY

#define HYPNO_STRIPE_COUNT      3.0f   // number of bright arms around the centre
#define HYPNO_TWIST_FACTOR      5.0f  // amount of angular twist from centre to rim
#define HYPNO_RADIUS_EXPONENT   0.85f  // radial easing (<1 tightens the centre)
#define HYPNO_STRIPE_DUTY       0.58f  // bright stripe proportion (0-1 range)
#define HYPNO_PHASE_INCREMENT   6512    // rotation speed per frame (larger = faster)

#endif

#if defined(ENABLE_ANIMATED_GIF)

// Enable SD-backed GIF playback instead of PROGMEM assets.
#define ANIMATED_GIF_USE_SD

// SD card configuration.
#ifndef SD_CS_PIN
#define SD_CS_PIN 32
#endif
#ifndef ANIMATED_GIF_SD_FREQ
#define ANIMATED_GIF_SD_FREQ 40000000
#endif

// Boot-time SD benchmark overlay (shown after the color test).
#ifndef BOOT_SD_TEST_DISPLAY_MS
#define BOOT_SD_TEST_DISPLAY_MS 2000
#endif
#ifndef BOOT_SD_TEST_BYTES
#define BOOT_SD_TEST_BYTES (256UL * 1024UL)
#endif
#ifndef BOOT_SD_TEST_BUFFER_BYTES
#define BOOT_SD_TEST_BUFFER_BYTES 4096
#endif

// GIF files to cycle through on the SD card (root directory by default).
#ifndef ANIMATED_GIF_FILES
#define ANIMATED_GIF_FILES                                                                                      \
  {                                                                                                              \
    "/beer.gif", "/fish.gif", "/wobble.gif", "/hearth.gif", "/fractal.gif", "/phenakistiscope.gif", "/tunnel.gif", \
        "/optical1.gif", "/optical2.gif", "/optical3.gif", "/optical4.gif", "/optical5.gif", "/optical6.gif",       \
        "/optical7.gif", "/drop1.gif", "/drop2.gif"                                                              \
  }
#endif

// Switch to the next program every N milliseconds.
#ifndef ANIMATED_GIF_SWITCH_INTERVAL_MS
#define ANIMATED_GIF_SWITCH_INTERVAL_MS 10000
#endif

#if defined(ENABLE_EYE_PROGRAM)
#define ANIMATED_GIF_DISABLE_AUTO_SWITCH
#endif

#if !defined(ANIMATED_GIF_USE_SD)
  #ifndef ANIMATED_GIF_HEADER
  #define ANIMATED_GIF_HEADER "wobble.h"
  #define ANIMATED_GIF_SYMBOL wobble
  //#define ANIMATED_GIF_HEADER "fractal.h"
  //#define ANIMATED_GIF_SYMBOL fractal
  //#define ANIMATED_GIF_HEADER "phenakistiscope.h"
  // #define ANIMATED_GIF_SYMBOL phenakistiscope
  // #define ANIMATED_GIF_HEADER "tunnel.h"
  // #define ANIMATED_GIF_SYMBOL tunnel
  #endif
#endif

#ifndef ANIMATED_GIF_BACKGROUND
#define ANIMATED_GIF_BACKGROUND 0x0000
#endif

#ifndef ANIMATED_GIF_DEFAULT_DELAY
#define ANIMATED_GIF_DEFAULT_DELAY 33
#endif

#ifndef ANIMATED_GIF_MAX_DELAY
#define ANIMATED_GIF_MAX_DELAY 33
#endif

#if !defined(ANIMATED_GIF_USE_SD)
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
#endif

#endif

#if defined(ENABLE_EYE_PROGRAM) || (!defined(ENABLE_ANIMATED_GIF) && !defined(ENABLE_HYPNO_SPIRAL))

// GRAPHICS SETTINGS (appearance of eye) -----------------------------------

// Uncomment to use a symmetrical eyelid without a caruncle.
// #define SYMMETRICAL_EYELID

// Eye sprite headers are compiled via src/eye_asset_*.cpp.
// Keep this list as a reference for available styles.
// #include "defaultEye.h"      // Standard human-ish hazel eye -OR-
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

// Framebuffer size for eye rendering (must fit the largest eye SCREEN_*).
#ifndef EYE_FRAMEBUFFER_WIDTH
#define EYE_FRAMEBUFFER_WIDTH 128
#endif
#ifndef EYE_FRAMEBUFFER_HEIGHT
#define EYE_FRAMEBUFFER_HEIGHT 128
#endif
#define EYE_FRAMEBUFFER_PIXELS (EYE_FRAMEBUFFER_WIDTH * EYE_FRAMEBUFFER_HEIGHT)

// Optional: when the eye assets are smaller than the physical display
// (e.g. 128×128 assets on a 240×240 round TFT), scale the rendered eye to fill
// the panel. Leave this disabled if you want the original eye sizing.
// #define EYE_SCALE_TO_DISPLAY
#ifndef EYE_SCALE_CHUNK_LINES
#define EYE_SCALE_CHUNK_LINES 16
#endif

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
#define EYE_1_XPOSITION  ((DISPLAY_WIDTH - EYE_FRAMEBUFFER_WIDTH) / 2)
#define EYE_2_XPOSITION  ((DISPLAY_WIDTH - EYE_FRAMEBUFFER_WIDTH) / 2)
#define EYE_1_YPOSITION  ((DISPLAY_HEIGHT - EYE_FRAMEBUFFER_HEIGHT) / 2)
#define EYE_2_YPOSITION  ((DISPLAY_HEIGHT - EYE_FRAMEBUFFER_HEIGHT) / 2)

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
#define ENABLE_EYELIDS     // If defined, render eyelids and blinking
#define TRACKING            // If defined, eyelid tracks pupil
#define AUTOBLINK           // If defined, eyes also blink autonomously

// #define LIGHT_PIN      -1 // Light sensor pin
#define LIGHT_CURVE  0.33 // Light sensor adjustment curve
#define LIGHT_MIN       0 // Minimum useful reading from light sensor
#define LIGHT_MAX    1023 // Maximum useful reading from sensor

// Eyelid rendering: the stock Uncanny Eyes assets provide `upper[]`/`lower[]`
// as 8-bit threshold maps (not colored textures). When enabled, masked pixels
// are filled with a shaded lid color instead of solid black.
#define ENABLE_EYELID_SHADING
#ifndef EYELID_COLOR_SHADOW
#define EYELID_COLOR_SHADOW 0xB3CC // darker (RGB565)
#endif
#ifndef EYELID_COLOR_HIGHLIGHT
#define EYELID_COLOR_HIGHLIGHT 0xFF17 // lighter (RGB565)
#endif

//#define ENABLE_EYELASHES
#ifndef EYELASH_COLOR
#define EYELASH_COLOR 0x0000 // lash/liner colour (RGB565)
#endif
// Lash length is expressed in eyelid-map units (0-255). Bigger = longer lashes.
#ifndef EYELASH_LENGTH
#define EYELASH_LENGTH 20
#endif
// Thickness of the continuous "lash line" right at the lid edge.
#ifndef EYELASH_BASE_THICKNESS
#define EYELASH_BASE_THICKNESS 2
#endif
// 0..255 (lower = fewer lashes drawn along the lid).
#ifndef EYELASH_DENSITY
#define EYELASH_DENSITY 180
#endif
#ifndef EYELASH_LENGTH_VARIATION
#define EYELASH_LENGTH_VARIATION 6
#endif

// Lower lashes are typically shorter and sparser than the upper lash line.
#ifndef EYELASH_LOWER_LENGTH
#define EYELASH_LOWER_LENGTH 10
#endif
#ifndef EYELASH_LOWER_BASE_THICKNESS
#define EYELASH_LOWER_BASE_THICKNESS 1
#endif
#ifndef EYELASH_LOWER_DENSITY
#define EYELASH_LOWER_DENSITY 120
#endif
#ifndef EYELASH_LOWER_LENGTH_VARIATION
#define EYELASH_LOWER_LENGTH_VARIATION 4
#endif

#ifndef EYE_BACKGROUND_COLOR
#define EYE_BACKGROUND_COLOR EYELID_COLOR_SHADOW
#endif

#define IRIS_SMOOTH         // If enabled, filter input from IRIS_PIN
#if !defined(IRIS_MIN)      // Each eye might have its own MIN/MAX
  #define IRIS_MIN       90 // Iris size (0-1023) in brightest light
#endif
#if !defined(IRIS_MAX)
  #define IRIS_MAX      130 // Iris size (0-1023) in darkest light
#endif
#ifndef EYE_IRIS_MIN_DEFAULT
#define EYE_IRIS_MIN_DEFAULT 90
#endif
#ifndef EYE_IRIS_MAX_DEFAULT
#define EYE_IRIS_MAX_DEFAULT 130
#endif

#endif
