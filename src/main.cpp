#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <SPI.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_random.h>
#include <esp_attr.h>
#endif

#include "eye_types.h"

// SPI wiring between the Adafruit Feather ESP32 V2 and the GC9A01 display.
static constexpr uint8_t TFT_SCK = SCK;               // GPIO5
static constexpr uint8_t TFT_MOSI = MOSI;             // GPIO19
static constexpr uint8_t TFT_MISO = MISO;             // Shared SPI MISO (used by SD)
static constexpr uint8_t TFT_CS = 33;                 // GPIO33 (D10 / SS)
static constexpr uint8_t TFT_DC = 26;                 // GPIO26 (A0)
static constexpr uint8_t TFT_RST = 25;                // GPIO25 (A1)

Arduino_DataBus *bus =
    new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, TFT_RST, /*rotation=*/0, /*IPS=*/true);

#include "config.h"

#if defined(ENABLE_EYE_PROGRAM) || (!defined(ENABLE_ANIMATED_GIF) && !defined(ENABLE_HYPNO_SPIRAL))
#define ENABLE_EYE_ANIMATION
#endif

#if defined(ENABLE_ANIMATED_GIF) && defined(ANIMATED_GIF_USE_SD)
static constexpr uint8_t SD_CS = SD_CS_PIN;
#endif

#if defined(ENABLE_EYE_ANIMATION)
void user_setup(void);
void user_loop(void);
#endif

#if defined(ENABLE_ANIMATED_GIF)
#include "animated_gif_player.h"
#endif

#if defined(ENABLE_HYPNO_SPIRAL)
#include "hypno_spiral.h"
#endif

#if defined(ENABLE_EYE_ANIMATION)
#include "eye_assets.h"
#include "eye_functions.h"
uint16_t eyeFrameBuffer[EYE_FRAMEBUFFER_PIXELS];
EyeState eye[NUM_EYES];
#endif

uint32_t startTime = 0;

#if defined(ENABLE_EYE_ANIMATION)
void user_setup(void) {}
void user_loop(void) {}
#endif

#if defined(ENABLE_ANIMATED_GIF) && defined(ENABLE_EYE_PROGRAM)
namespace
{
enum class ProgramMode
{
  Gif,
  Eye,
  Hypno
};

ProgramMode currentProgram = ProgramMode::Gif;
size_t gifProgramCount = 0;
size_t eyeProgramCount = 0;
size_t programIndex = 0;
uint32_t programStartMs = 0;
uint8_t eyeBaseRotation = 0;
bool hypnoInitialized = false;
int16_t activeMappedIndex = -1;

#if defined(ENABLE_SWIRL_TRANSITION)
struct SwirlTransitionState
{
  bool active = false;
  int16_t targetMappedIndex = -1;
  uint32_t startMs = 0;
  uint32_t lastFrameMs = 0;
  bool targetPrepared = false;
};

SwirlTransitionState g_swirlTransition;
#endif

void setProgramRotation(ProgramMode mode)
{
  if (mode == ProgramMode::Eye)
  {
    gfx->setRotation(static_cast<uint8_t>((eyeBaseRotation + DISPLAY_ROTATION) & 0x03));
  }
  else if (mode == ProgramMode::Hypno)
  {
    gfx->setRotation(DISPLAY_ROTATION);
  }
  else
  {
    gfx->setRotation(DISPLAY_ROTATION);
  }
}

void fallbackToDefaultEye()
{
  currentProgram = ProgramMode::Eye;
  activeMappedIndex = -1;
  setProgramRotation(currentProgram);
  gfx->fillScreen(EYE_BACKGROUND_COLOR);
  const EyeAsset *asset = getEyeAsset(0);
  if (asset)
  {
    setActiveEye(asset);
    Serial.printf("Eye asset (fallback): %s\n", asset->name ? asset->name : "unknown");
  }
  startTime = millis();
}

void enterHypno()
{
#if defined(ENABLE_HYPNO_SPIRAL)
  if (!hypnoInitialized)
  {
    hypnoSetup();
    hypnoInitialized = true;
  }
  currentProgram = ProgramMode::Hypno;
  activeMappedIndex = -2;
  setProgramRotation(currentProgram);
#else
  fallbackToDefaultEye();
#endif
}

void enterProgram(size_t index)
{
  const size_t programCount = gifProgramCount + eyeProgramCount;
  if (programCount == 0)
  {
    return;
  }
  programIndex = index % programCount;
  programStartMs = millis();

  if (gifProgramCount > 0 && programIndex < gifProgramCount)
  {
    currentProgram = ProgramMode::Gif;
    activeMappedIndex = static_cast<int16_t>(programIndex);
    setProgramRotation(currentProgram);
    gfx->fillScreen(ANIMATED_GIF_BACKGROUND);
    if (!animatedGifOpenAtIndex(programIndex))
    {
      fallbackToDefaultEye();
    }
  }
  else
  {
    currentProgram = ProgramMode::Eye;
    activeMappedIndex = -1;
    setProgramRotation(currentProgram);
    gfx->fillScreen(EYE_BACKGROUND_COLOR);
    const size_t eyeIndex = programIndex - gifProgramCount;
    const EyeAsset *asset = getEyeAsset(eyeIndex);
    if (asset)
    {
      setActiveEye(asset);
      Serial.printf("Eye asset: %s\n", asset->name ? asset->name : "unknown");
    }
    startTime = millis();
  }
}

void renderFirstFrame()
{
  if (currentProgram == ProgramMode::Eye)
  {
    updateEye();
    return;
  }

  if (currentProgram == ProgramMode::Hypno)
  {
#if defined(ENABLE_HYPNO_SPIRAL)
    hypnoStep();
#endif
    return;
  }

  animatedGifLoop();
  if (!animatedGifIsReady())
  {
    fallbackToDefaultEye();
  }
}

void applyMappedProgramImmediate(int16_t mappedIndex)
{
  if (mappedIndex == -2)
  {
    enterHypno();
    return;
  }

  if (mappedIndex < 0)
  {
    fallbackToDefaultEye();
    return;
  }

  if (static_cast<size_t>(mappedIndex) >= gifProgramCount)
  {
    fallbackToDefaultEye();
    return;
  }

  enterProgram(static_cast<size_t>(mappedIndex));
}

void prepareMappedProgram(int16_t mappedIndex)
{
  if (mappedIndex == -2)
  {
#if defined(ENABLE_HYPNO_SPIRAL)
    if (!hypnoInitialized)
    {
      hypnoSetup();
      hypnoInitialized = true;
    }
#endif
    return;
  }

  if (mappedIndex < 0)
  {
    return;
  }

  if (static_cast<size_t>(mappedIndex) >= gifProgramCount)
  {
    return;
  }

  (void)animatedGifOpenAtIndex(static_cast<size_t>(mappedIndex));
}

void requestMappedProgram(int16_t mappedIndex)
{
  if (mappedIndex == -2)
  {
#if !defined(ENABLE_HYPNO_SPIRAL)
    mappedIndex = -1;
#endif
  }

  if (mappedIndex >= 0 && static_cast<size_t>(mappedIndex) >= gifProgramCount)
  {
    mappedIndex = -1;
  }

#if defined(ENABLE_SWIRL_TRANSITION)
  if (SWIRL_TRANSITION_DURATION_MS > 0)
  {
    if (!g_swirlTransition.active && mappedIndex == activeMappedIndex)
    {
      return;
    }

    if (g_swirlTransition.active && mappedIndex == g_swirlTransition.targetMappedIndex)
    {
      return;
    }

    g_swirlTransition.active = true;
    g_swirlTransition.targetMappedIndex = mappedIndex;
    g_swirlTransition.startMs = millis();
    g_swirlTransition.lastFrameMs = 0;
    g_swirlTransition.targetPrepared = false;

#if defined(ENABLE_HYPNO_SPIRAL)
    if (!hypnoInitialized)
    {
      hypnoSetup();
      hypnoInitialized = true;
    }
    setProgramRotation(ProgramMode::Hypno);
    hypnoStep();
    g_swirlTransition.lastFrameMs = g_swirlTransition.startMs;
#endif

    prepareMappedProgram(mappedIndex);
    g_swirlTransition.targetPrepared = true;
    return;
  }
#endif

  if (mappedIndex == activeMappedIndex)
  {
    return;
  }

  applyMappedProgramImmediate(mappedIndex);
  renderFirstFrame();
}

bool swirlTransitionActive(uint32_t now)
{
#if !defined(ENABLE_SWIRL_TRANSITION)
  (void)now;
  return false;
#else
  if (!g_swirlTransition.active)
  {
    return false;
  }

  if (SWIRL_TRANSITION_FRAME_MS > 0 &&
      (g_swirlTransition.lastFrameMs == 0 ||
       (now - g_swirlTransition.lastFrameMs) >= SWIRL_TRANSITION_FRAME_MS))
  {
#if defined(ENABLE_HYPNO_SPIRAL)
    setProgramRotation(ProgramMode::Hypno);
    hypnoStep();
#endif
    g_swirlTransition.lastFrameMs = now;
  }

  const uint32_t elapsed = now - g_swirlTransition.startMs;
  uint32_t minDuration = SWIRL_TRANSITION_MIN_MS;
  uint32_t maxDuration = SWIRL_TRANSITION_DURATION_MS;
  if (maxDuration < minDuration)
  {
    maxDuration = minDuration;
  }

  if ((g_swirlTransition.targetPrepared && elapsed >= minDuration) || (elapsed >= maxDuration))
  {
    g_swirlTransition.active = false;
    applyMappedProgramImmediate(g_swirlTransition.targetMappedIndex);
    renderFirstFrame();
  }

  return true;
#endif
}
} // namespace
#endif

namespace
{
struct UiLine
{
  const char *text;
  uint16_t color;
};

void drawCenteredLines(const UiLine *lines, size_t count)
{
  if (!lines || count == 0)
  {
    return;
  }

  gfx->setTextWrap(false);

  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t w = 0;
  uint16_t h = 0;
  gfx->getTextBounds("A", 0, 0, &x1, &y1, &w, &h);
  const int16_t lineHeight = static_cast<int16_t>(h > 0 ? h : 16);
  const int16_t linePitch = static_cast<int16_t>(lineHeight + 2);
  const int16_t blockHeight = static_cast<int16_t>(count * linePitch - 2);

  const int16_t screenWidth = gfx->width();
  const int16_t screenHeight = gfx->height();
  int16_t y = static_cast<int16_t>((screenHeight - blockHeight) / 2);
  if (y < 0)
  {
    y = 0;
  }

  for (size_t i = 0; i < count; ++i)
  {
    if (!lines[i].text)
    {
      continue;
    }

    gfx->setTextColor(lines[i].color);
    gfx->getTextBounds(lines[i].text, 0, 0, &x1, &y1, &w, &h);
    int16_t x = static_cast<int16_t>((screenWidth - static_cast<int16_t>(w)) / 2);
    if (x < 0)
    {
      x = 0;
    }

    gfx->setCursor(x, y);
    gfx->print(lines[i].text);
    y = static_cast<int16_t>(y + linePitch);
  }
}

// Map incoming WLED effect numbers to programs.
// - `-1` = default eye
// - `-2` = hypno spiral
// - `>=0` = GIF index (must match `ANIMATED_GIF_FILES` ordering in `include/config.h`)
enum class GifProgram : int16_t
{
  Beer = 0,
  Fish,
  Wobble,
  Hearth,
  Fractal,
  Phenakistiscope,
  Tunnel,
  Optical1,
  Optical2,
  Optical3,
  Optical4,
  Optical5,
  Optical6,
  Optical7,
  Drop1,
  Drop2
};

constexpr int16_t kProgramDefaultEye = -1;
constexpr int16_t kProgramHypno = -2;

constexpr int16_t gifProgram(GifProgram program)
{
  return static_cast<int16_t>(program);
}
constexpr bool kEnableAutoSwitch = false;

int16_t mapEffectToProgram(uint8_t effect)
{
  switch (effect)
  {
  case 1:
    return gifProgram(GifProgram::Beer);
  case 2:
    return gifProgram(GifProgram::Fish);
  case 3:
    return gifProgram(GifProgram::Wobble);
  case 4:
    return gifProgram(GifProgram::Hearth);
  case 5:
    return gifProgram(GifProgram::Fractal);
  case 6:
    return gifProgram(GifProgram::Phenakistiscope);
  case 7:
    return gifProgram(GifProgram::Tunnel);
  case 8:
    return gifProgram(GifProgram::Optical1);
  case 9:
    return gifProgram(GifProgram::Optical2);
  case 10:
    return gifProgram(GifProgram::Optical3);
  case 11:
    return gifProgram(GifProgram::Optical4);
  case 12:
    return gifProgram(GifProgram::Optical5);
  case 13:
    return kProgramHypno;
  case 14:
    return gifProgram(GifProgram::Optical6);
  case 15:
    return gifProgram(GifProgram::Optical7);
  case 16:
    return gifProgram(GifProgram::Drop2);
  case 17:
    return gifProgram(GifProgram::Drop1);
  default:
    return kProgramDefaultEye;
  }
}

void applyMappedProgram(uint8_t effect)
{
#if defined(ENABLE_ANIMATED_GIF) && defined(ENABLE_EYE_PROGRAM)
  const int16_t index = mapEffectToProgram(effect);
  requestMappedProgram(index);
#elif defined(ENABLE_ANIMATED_GIF)
  const int16_t index = mapEffectToProgram(effect);
  if (index >= 0)
  {
    if (animatedGifOpenAtIndex(static_cast<size_t>(index)))
    {
      startTime = millis();
    }
  }
#elif defined(ENABLE_HYPNO_SPIRAL)
  (void)effect;
#else
  const EyeAsset *asset = getEyeAsset(0);
  if (asset)
  {
    setActiveEye(asset);
    startTime = millis();
    Serial.printf("Eye asset (mapped): %s\n", asset->name ? asset->name : "unknown");
  }
#endif
}

#if defined(ARDUINO_ARCH_ESP32)
constexpr uint16_t kBleCompanyId = 0xFFFF;
constexpr uint8_t kBleAppId0 = 'V';
constexpr uint8_t kBleAppId1 = 'R';
constexpr uint8_t kBleLegacyTypeSync = 1;
constexpr uint8_t kBleLegacyTypeEye = 2;

constexpr uint8_t kBleMsgMask = 0x0F;
constexpr uint8_t kBleMsgPreset = 1;
constexpr uint8_t kBleFlagGlobal = 0x80;
constexpr size_t kBlePayloadPresetLen = 18;

volatile bool g_blePending = false;
volatile uint8_t g_bleEffect = 0;
volatile uint32_t g_bleTimebase = 0;
uint16_t g_bleLastSeq = 0xFFFF;
uint64_t g_bleLastGroup = 0;
uint8_t g_bleLastKind = 0;
volatile uint32_t g_bleLastMs = 0;
bool g_bleHasSync = false;

uint64_t packGroup48(const uint8_t *group)
{
  uint64_t value = 0;
  for (uint8_t i = 0; i < 6; ++i)
  {
    value = (value << 8) | uint64_t(group[i]);
  }
  return value;
}

void unpackGroup48(uint64_t value, uint8_t out[6])
{
  for (int8_t i = 5; i >= 0; --i)
  {
    out[i] = static_cast<uint8_t>(value & 0xFF);
    value >>= 8;
  }
}

void formatGroup48(uint64_t value, char out[13])
{
  uint8_t group[6] = {0};
  unpackGroup48(value, group);
  snprintf(out, 13, "%02x%02x%02x%02x%02x%02x", group[0], group[1], group[2],
           group[3], group[4], group[5]);
}

	volatile bool g_bleLearnMode = false;
	volatile uint32_t g_bleLearnModeUntilMs = 0;
	volatile bool g_bleGroupSavePending = false;

// "Double reset" pairing trigger support (RESET button).
#if VISUALREMOTE_PAIR_DOUBLE_RESET_MS > 0
RTC_DATA_ATTR uint32_t g_bleResetTapMagic = 0;
constexpr uint32_t kBleResetTapMagic = 0x50414952; // "PAIR"
uint32_t g_bleResetTapClearAtMs = 0;
#endif

	enum class PairUiState : uint8_t
	{
	  None,
	  Hold,
	  Pairing,
	  Paired,
	  Timeout
	};

	PairUiState g_blePairUiState = PairUiState::None;
	uint32_t g_blePairUiUntilMs = 0;
	uint32_t g_blePairUiNextRefreshMs = 0;
	uint64_t g_blePairUiGroupKey = 0;
	bool g_blePairUiDirty = false;

	bool g_blePairHoldActive = false;
	uint32_t g_blePairHoldStartMs = 0;
	int g_blePairHoldRaw = -1;

	bool g_bleGroupFilterEnabled = false;
	uint64_t g_bleGroupFilterKey = 0;

#if VISUALREMOTE_STORE_GROUP_FILTER
constexpr const char *kBlePrefsNamespace = "roundeyes";
constexpr const char *kBlePrefsKeyGroup = "blegrp";
#endif

int8_t hexNibble(char c)
{
  if (c >= '0' && c <= '9')
  {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f')
  {
    return (c - 'a') + 10;
  }
  if (c >= 'A' && c <= 'F')
  {
    return (c - 'A') + 10;
  }
  return -1;
}

bool mac12ToBytes(const char *mac12, uint8_t out[6])
{
  if (!mac12)
  {
    return false;
  }
  if (strlen(mac12) != 12)
  {
    return false;
  }
  for (uint8_t i = 0; i < 6; ++i)
  {
    const int8_t hi = hexNibble(mac12[i * 2]);
    const int8_t lo = hexNibble(mac12[i * 2 + 1]);
    if (hi < 0 || lo < 0)
    {
      return false;
    }
    out[i] = (uint8_t(hi) << 4) | uint8_t(lo);
  }
  return true;
}

bool loadStoredBleGroupFilter(uint64_t &outGroupKey)
{
#if !VISUALREMOTE_STORE_GROUP_FILTER
  (void)outGroupKey;
  return false;
#else
  Preferences prefs;
  if (!prefs.begin(kBlePrefsNamespace, /*readOnly=*/true))
  {
    return false;
  }

  uint8_t group[6] = {0};
  constexpr size_t expected = sizeof(group);
  if (prefs.getBytesLength(kBlePrefsKeyGroup) != expected)
  {
    prefs.end();
    return false;
  }

  const size_t read = prefs.getBytes(kBlePrefsKeyGroup, group, expected);
  prefs.end();
  if (read != expected)
  {
    return false;
  }

  outGroupKey = packGroup48(group);
  return true;
#endif
}

bool saveStoredBleGroupFilter(uint64_t groupKey)
{
#if !VISUALREMOTE_STORE_GROUP_FILTER
  (void)groupKey;
  return false;
#else
  uint8_t group[6] = {0};
  unpackGroup48(groupKey, group);

  Preferences prefs;
  if (!prefs.begin(kBlePrefsNamespace, /*readOnly=*/false))
  {
    return false;
  }

  constexpr size_t expected = sizeof(group);
  const size_t wrote = prefs.putBytes(kBlePrefsKeyGroup, group, expected);
  prefs.end();
  return wrote == expected;
#endif
}

void setBleGroupFilter(uint64_t groupKey, bool persist)
{
  if (groupKey == 0)
  {
    return;
  }

  g_bleGroupFilterKey = groupKey;
  g_bleGroupFilterEnabled = true;

  char groupText[13] = {0};
  formatGroup48(groupKey, groupText);
  Serial.printf("BLE group filter set: %s\n", groupText);

#if VISUALREMOTE_STORE_GROUP_FILTER
  if (persist)
  {
    if (!saveStoredBleGroupFilter(groupKey))
    {
      Serial.println("BLE group filter save failed");
    }
  }
#else
  (void)persist;
#endif
}

	void startBleLearnMode()
	{
	  if (!VISUALREMOTE_PAIR_PERSISTENT && VISUALREMOTE_PAIR_WINDOW_MS == 0)
	  {
	    return;
	  }

	  g_bleLearnMode = true;
	  g_bleLearnModeUntilMs =
	      (VISUALREMOTE_PAIR_PERSISTENT || VISUALREMOTE_PAIR_WINDOW_MS == 0)
	          ? 0
	          : (millis() + VISUALREMOTE_PAIR_WINDOW_MS);
	  g_blePairUiState = PairUiState::Pairing;
	  g_blePairUiDirty = true;
	  g_blePairUiNextRefreshMs = 0;
	  g_blePairUiUntilMs = 0;
#if VISUALREMOTE_PAIR_DEBUG_LOG
	  Serial.printf("BLE pair: learn mode start, window=%lu ms\n",
	                static_cast<unsigned long>(VISUALREMOTE_PAIR_WINDOW_MS));
#endif
	  Serial.printf("BLE pairing window: %lu ms (press preset now)\n",
	                static_cast<unsigned long>(VISUALREMOTE_PAIR_WINDOW_MS));
	}

	bool bleWasDoubleReset()
	{
#if VISUALREMOTE_PAIR_DOUBLE_RESET_MS == 0
	  return false;
#else
	  if (g_bleResetTapMagic == kBleResetTapMagic)
	  {
	    g_bleResetTapMagic = 0;
	    g_bleResetTapClearAtMs = 0;
	    return true;
	  }

	  g_bleResetTapMagic = kBleResetTapMagic;
	  g_bleResetTapClearAtMs = millis() + VISUALREMOTE_PAIR_DOUBLE_RESET_MS;
	  return false;
#endif
	}

	void bleResetTapLoop()
	{
#if VISUALREMOTE_PAIR_DOUBLE_RESET_MS > 0
	  if (g_bleResetTapClearAtMs > 0 &&
	      (int32_t)(millis() - g_bleResetTapClearAtMs) >= 0)
	  {
	    g_bleResetTapMagic = 0;
	    g_bleResetTapClearAtMs = 0;
	  }
#endif
	}

	void blePairButtonLoop()
	{
	  if (VISUALREMOTE_PAIR_BUTTON_PIN < 0 ||
	      (!VISUALREMOTE_PAIR_PERSISTENT && VISUALREMOTE_PAIR_WINDOW_MS == 0) ||
	      VISUALREMOTE_PAIR_HOLD_MS == 0)
	  {
	    return;
	  }

	  static bool wasPressed = false;
	  static bool triggered = false;
	  static uint32_t pressedSinceMs = 0;
	  static uint32_t lastLogMs = 0;

	  const uint32_t now = millis();
	  const int raw = digitalRead(VISUALREMOTE_PAIR_BUTTON_PIN);
	  g_blePairHoldRaw = raw;
	  const bool pressed = VISUALREMOTE_PAIR_BUTTON_ACTIVE_LOW ? (raw == LOW) : (raw == HIGH);
	  g_blePairHoldActive = pressed;

	  if (pressed != wasPressed)
	  {
	    wasPressed = pressed;
	    triggered = false;
	    pressedSinceMs = now;
	    if (pressed)
	    {
	      g_blePairHoldStartMs = now;
	    }
#if VISUALREMOTE_PAIR_DEBUG_LOG
	    Serial.printf("BLE pair btn: gpio=%d raw=%d pressed=%d\n",
	                  VISUALREMOTE_PAIR_BUTTON_PIN, raw, pressed ? 1 : 0);
#endif

	    if (!pressed && g_blePairUiState == PairUiState::Hold)
	    {
	      g_blePairUiState = PairUiState::None;
	      g_blePairUiDirty = true;
	      g_blePairUiNextRefreshMs = 0;
	    }
	  }

	  if (pressed && !triggered && g_blePairUiState == PairUiState::None)
	  {
	    g_blePairUiState = PairUiState::Hold;
	    g_blePairUiDirty = true;
	    g_blePairUiNextRefreshMs = 0;
	    g_blePairUiUntilMs = 0;
	  }

	  if (pressed && !triggered && VISUALREMOTE_PAIR_DEBUG_LOG && (now - lastLogMs) >= 250)
	  {
	    lastLogMs = now;
	    Serial.printf("BLE pair btn: holding %lu/%lu ms raw=%d\n",
	                  static_cast<unsigned long>(now - pressedSinceMs),
	                  static_cast<unsigned long>(VISUALREMOTE_PAIR_HOLD_MS), raw);
	  }

	  if (pressed && !triggered && (now - pressedSinceMs) >= VISUALREMOTE_PAIR_HOLD_MS)
	  {
	    triggered = true;
#if VISUALREMOTE_PAIR_DEBUG_LOG
	    Serial.printf("BLE pair: hold met (%lu ms), entering learn mode\n",
	                  static_cast<unsigned long>(now - pressedSinceMs));
#endif
	    startBleLearnMode();
	  }
	}

	bool blePairingUiLoop()
	{
	  if (!VISUALREMOTE_PAIR_PERSISTENT && VISUALREMOTE_PAIR_WINDOW_MS == 0)
	  {
	    return false;
	  }

	  const uint32_t now = millis();
	  const bool learnMode = g_bleLearnMode;

	  if (learnMode)
	  {
	    if (g_blePairUiState != PairUiState::Pairing)
    {
      g_blePairUiState = PairUiState::Pairing;
      g_blePairUiDirty = true;
      g_blePairUiNextRefreshMs = 0;
      g_blePairUiUntilMs = 0;
    }
  }
	  else if (g_blePairUiState == PairUiState::Pairing)
	  {
	    g_blePairUiState = PairUiState::None;
	    g_blePairUiDirty = false;
	    return false;
	  }

  if (g_blePairUiState == PairUiState::None)
  {
    return false;
  }

	  if ((g_blePairUiState == PairUiState::Paired || g_blePairUiState == PairUiState::Timeout) &&
	      !VISUALREMOTE_PAIR_PERSISTENT && VISUALREMOTE_PAIR_RESULT_DISPLAY_MS > 0 &&
	      g_blePairUiUntilMs > 0 &&
	      (int32_t)(now - g_blePairUiUntilMs) >= 0)
	  {
	    g_blePairUiState = PairUiState::None;
	    g_blePairUiDirty = false;
    return false;
  }

  const uint32_t refreshMs = VISUALREMOTE_PAIR_STATUS_REFRESH_MS;
  if (!g_blePairUiDirty && refreshMs > 0 && g_blePairUiNextRefreshMs > 0 &&
      (int32_t)(now - g_blePairUiNextRefreshMs) < 0)
  {
    return true;
  }

  g_blePairUiDirty = false;
  g_blePairUiNextRefreshMs = (refreshMs > 0) ? (now + refreshMs) : 0;

  const uint8_t prevRotation = gfx->getRotation();
  gfx->setRotation(static_cast<uint8_t>((prevRotation + 2) & 0x03));
  gfx->fillScreen(BLACK);
  gfx->setTextSize(2);

  UiLine lines[6] = {};
  size_t lineCount = 0;

  char lineTime[24] = {0};
  char lineGroup[32] = {0};
  char groupText[13] = {0};

	  switch (g_blePairUiState)
	  {
	  case PairUiState::Hold:
	  {
	    lines[lineCount++] = UiLine{"HOLD TO PAIR", WHITE};
	    lines[lineCount++] = UiLine{"Keep button down", WHITE};

	    const int rawNow = g_blePairHoldRaw;
	    const bool pressedNow = g_blePairHoldActive;

	    char lineBtn[28] = {0};
	    snprintf(lineBtn, sizeof(lineBtn), "BTN raw: %d", rawNow);
	    lines[lineCount++] = UiLine{lineBtn, static_cast<uint16_t>(pressedNow ? GREEN : RED)};

	    const uint32_t heldMs = (pressedNow && g_blePairHoldStartMs > 0) ? (now - g_blePairHoldStartMs) : 0;
	    const uint32_t targetMs = VISUALREMOTE_PAIR_HOLD_MS;
	    char lineHold[28] = {0};
	    snprintf(lineHold, sizeof(lineHold), "Hold: %lu/%lu ms",
	             static_cast<unsigned long>(heldMs),
	             static_cast<unsigned long>(targetMs));
	    lines[lineCount++] = UiLine{lineHold, YELLOW};
	    break;
	  }
	  case PairUiState::Pairing:
	  {
	    lines[lineCount++] = UiLine{"PAIRING", WHITE};
	    lines[lineCount++] = UiLine{"Send preset now", WHITE};

	    if (VISUALREMOTE_PAIR_PERSISTENT || VISUALREMOTE_PAIR_WINDOW_MS == 0 || g_bleLearnModeUntilMs == 0)
	    {
	      snprintf(lineTime, sizeof(lineTime), "Time: INF");
	    }
	    else
	    {
	      const uint32_t untilMs = g_bleLearnModeUntilMs;
	      int32_t remaining = static_cast<int32_t>(untilMs - now);
	      if (remaining < 0)
	      {
	        remaining = 0;
	      }
	      const uint32_t remainingMs = static_cast<uint32_t>(remaining);
	      const uint32_t remainingTenths = (remainingMs + 99) / 100;
	      snprintf(lineTime, sizeof(lineTime), "Time: %lu.%lus",
	               static_cast<unsigned long>(remainingTenths / 10),
	               static_cast<unsigned long>(remainingTenths % 10));
	    }
	    lines[lineCount++] = UiLine{lineTime, YELLOW};

    if (g_bleGroupFilterEnabled)
    {
      formatGroup48(g_bleGroupFilterKey, groupText);
      snprintf(lineGroup, sizeof(lineGroup), "Current: %s", groupText);
    }
    else
    {
      snprintf(lineGroup, sizeof(lineGroup), "Current: ANY");
    }
    lines[lineCount++] = UiLine{lineGroup, GREEN};
	    break;
	  }
  case PairUiState::Paired:
    lines[lineCount++] = UiLine{"PAIRED", GREEN};
    formatGroup48(g_blePairUiGroupKey, groupText);
    snprintf(lineGroup, sizeof(lineGroup), "Group: %s", groupText);
    lines[lineCount++] = UiLine{lineGroup, WHITE};
    lines[lineCount++] = UiLine{"Saved", WHITE};
    break;
  case PairUiState::Timeout:
    lines[lineCount++] = UiLine{"PAIR TIMEOUT", RED};
    lines[lineCount++] = UiLine{"No preset received", YELLOW};
    break;
  default:
    break;
  }

  drawCenteredLines(lines, lineCount);
  gfx->setRotation(prevRotation);
  return true;
}

void initBleGroupFilter()
{
#if VISUALREMOTE_STORE_GROUP_FILTER
  uint64_t storedGroupKey = 0;
  if (loadStoredBleGroupFilter(storedGroupKey))
  {
    g_bleGroupFilterKey = storedGroupKey;
    g_bleGroupFilterEnabled = true;

    char groupText[13] = {0};
    formatGroup48(storedGroupKey, groupText);
    Serial.printf("BLE group filter loaded: %s\n", groupText);
    return;
  }
#endif

  const char *filter = VISUALREMOTE_GROUP_FILTER;
  if (!filter || strlen(filter) != 12)
  {
    g_bleGroupFilterEnabled = false;
    return;
  }

  uint8_t group[6] = {0};
  if (!mac12ToBytes(filter, group))
  {
    Serial.printf("BLE group filter invalid: %s\n", filter);
    g_bleGroupFilterEnabled = false;
    return;
  }

  g_bleGroupFilterKey = packGroup48(group);
  g_bleGroupFilterEnabled = true;
  Serial.printf("BLE group filter enabled: %s\n", filter);
}

class BleSyncCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) override
  {
    if (!advertisedDevice->haveManufacturerData())
    {
      return;
    }
    const std::string &data = advertisedDevice->getManufacturerData();
    if (data.size() < 12)
    {
      return;
    }
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(data.data());
    const uint16_t company = bytes[0] | (uint16_t(bytes[1]) << 8);
    if (company != kBleCompanyId)
    {
      return;
    }
    if (bytes[2] != kBleAppId0 || bytes[3] != kBleAppId1)
    {
      return;
    }

    if (data.size() >= kBlePayloadPresetLen)
    {
      const uint8_t flags = bytes[4];
      const bool learnMode = g_bleLearnMode;
      if ((flags & kBleMsgMask) != kBleMsgPreset)
      {
        return;
      }
      const uint16_t seq = bytes[5] | (uint16_t(bytes[6]) << 8);
      const uint64_t groupKey = packGroup48(bytes + 12);
      if (!learnMode && g_bleGroupFilterEnabled && groupKey != g_bleGroupFilterKey)
      {
        return;
      }
      if (seq == g_bleLastSeq && groupKey == g_bleLastGroup && flags == g_bleLastKind)
      {
        return;
      }

      g_bleLastSeq = seq;
      g_bleLastGroup = groupKey;
      g_bleLastKind = flags;

      if (learnMode && groupKey != 0)
      {
        g_bleLearnMode = false;
        g_bleGroupFilterEnabled = true;
        g_bleGroupFilterKey = groupKey;
        g_bleGroupSavePending = true;
      }

#if !VISUALREMOTE_ACCEPT_GLOBAL_PRESETS
      if (!learnMode && (flags & kBleFlagGlobal))
      {
        return;
      }
#endif

      const uint32_t timebase = uint32_t(bytes[8]) |
                                (uint32_t(bytes[9]) << 8) |
                                (uint32_t(bytes[10]) << 16) |
                                (uint32_t(bytes[11]) << 24);
      g_bleEffect = bytes[7];
      g_bleTimebase = timebase;
      g_blePending = true;
      g_bleLastMs = millis();
      g_bleHasSync = true;
      return;
    }

    const uint8_t type = bytes[4];
    if (type != kBleLegacyTypeSync && type != kBleLegacyTypeEye)
    {
      return;
    }
    const uint16_t seq = bytes[5] | (uint16_t(bytes[6]) << 8);
    if (seq == g_bleLastSeq && type == g_bleLastKind)
    {
      return;
    }

    g_bleLastSeq = seq;
    g_bleLastGroup = 0;
    g_bleLastKind = type;
    g_bleEffect = bytes[7];
    g_bleTimebase = uint32_t(bytes[8]) |
                    (uint32_t(bytes[9]) << 8) |
                    (uint32_t(bytes[10]) << 16) |
                    (uint32_t(bytes[11]) << 24);
    g_blePending = true;
    g_bleLastMs = millis();
    g_bleHasSync = true;
  }
};

	void bleSyncSetup()
	{
	  NimBLEDevice::init("RoundEyes");
	  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
	  initBleGroupFilter();

	  if (VISUALREMOTE_PAIR_BUTTON_PIN >= 0)
	  {
	    // ESP32 GPIO34-39 are input-only and don't support internal pullups/pulldowns.
	    const uint8_t pin = static_cast<uint8_t>(VISUALREMOTE_PAIR_BUTTON_PIN);
	    if (pin >= 34 && pin <= 39)
	    {
	      pinMode(pin, INPUT);
	    }
	    else
	    {
	      pinMode(pin, INPUT_PULLUP);
	    }
#if VISUALREMOTE_PAIR_DEBUG_LOG
	    const int raw = digitalRead(VISUALREMOTE_PAIR_BUTTON_PIN);
	    const bool pressed = VISUALREMOTE_PAIR_BUTTON_ACTIVE_LOW ? (raw == LOW) : (raw == HIGH);
	    Serial.printf("BLE pair btn init: gpio=%d mode=%s raw=%d pressed=%d\n",
	                  VISUALREMOTE_PAIR_BUTTON_PIN,
	                  (pin >= 34 && pin <= 39) ? "INPUT" : "INPUT_PULLUP",
	                  raw, pressed ? 1 : 0);
#endif
	  }

	  NimBLEScan *scan = NimBLEDevice::getScan();
	  scan->setAdvertisedDeviceCallbacks(new BleSyncCallbacks(), true);
	  scan->setActiveScan(false);
	  scan->setInterval(45);
	  scan->setWindow(15);
	  scan->start(0, nullptr, false);

	  if (bleWasDoubleReset())
	  {
	    startBleLearnMode();
	  }
	}

	void bleSyncLoop()
	{
	  bleResetTapLoop();
	  blePairButtonLoop();

	  const bool pairingOverlayActive = (g_blePairUiState != PairUiState::None);

	  if (!VISUALREMOTE_PAIR_PERSISTENT && g_bleLearnMode && VISUALREMOTE_PAIR_WINDOW_MS > 0 &&
	      (int32_t)(millis() - g_bleLearnModeUntilMs) >= 0)
	  {
	    g_bleLearnMode = false;
	    Serial.println("BLE pairing window expired");
	    if (VISUALREMOTE_PAIR_RESULT_DISPLAY_MS > 0)
    {
      g_blePairUiState = PairUiState::Timeout;
      g_blePairUiUntilMs = millis() + VISUALREMOTE_PAIR_RESULT_DISPLAY_MS;
      g_blePairUiDirty = true;
      g_blePairUiNextRefreshMs = 0;
    }
    else
    {
      g_blePairUiState = PairUiState::None;
    }
  }

  if (!g_blePending)
  {
    return;
  }
  const uint8_t effect = g_bleEffect;
  const uint32_t timebase = g_bleTimebase;
  const uint64_t groupKey = g_bleLastGroup;
  g_blePending = false;

	  if (g_bleGroupSavePending)
	  {
	    g_bleGroupSavePending = false;
	    if (groupKey != 0)
	    {
	      setBleGroupFilter(groupKey, /*persist=*/true);
	      if (!VISUALREMOTE_PAIR_PERSISTENT && VISUALREMOTE_PAIR_RESULT_DISPLAY_MS > 0)
	      {
	        g_blePairUiState = PairUiState::Paired;
	        g_blePairUiGroupKey = groupKey;
	        g_blePairUiUntilMs = millis() + VISUALREMOTE_PAIR_RESULT_DISPLAY_MS;
	        g_blePairUiDirty = true;
	        g_blePairUiNextRefreshMs = 0;
	      }
	      else
	      {
	        g_blePairUiState = PairUiState::Paired;
	        g_blePairUiGroupKey = groupKey;
	        g_blePairUiUntilMs = 0;
	        g_blePairUiDirty = true;
	        g_blePairUiNextRefreshMs = 0;
	      }
	    }
	  }

	  const int16_t mapped = mapEffectToProgram(effect);
	  Serial.printf("BLE preset: %u -> map %d timebase %lu\n", effect, mapped,
	                static_cast<unsigned long>(timebase));
	  if (pairingOverlayActive || g_bleLearnMode)
	  {
#if VISUALREMOTE_PAIR_DEBUG_LOG
	    Serial.println("BLE preset ignored (pairing overlay active)");
#endif
	    return;
	  }
	  applyMappedProgram(effect);
	}

bool bleSyncHasLock()
{
  return g_bleHasSync;
}
#else
	void bleSyncSetup() {}
	void bleSyncLoop() {}
	bool bleSyncHasLock() { return false; }
	bool blePairingUiLoop() { return false; }
#endif
	} // namespace

static void waitForSerial()
{
  const uint32_t serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart) < 3000)
  {
    delay(10);
  }
}

#if defined(ENABLE_ANIMATED_GIF) && defined(ANIMATED_GIF_USE_SD)
const char *sdCardTypeName(uint8_t type)
{
  switch (type)
  {
  case CARD_MMC:
    return "MMC";
  case CARD_SD:
    return "SDSC";
  case CARD_SDHC:
    return "SDHC";
  default:
    return "NONE";
  }
}

bool sdFindReadableFile(char *outPath, size_t outPathLen)
{
  if (!outPath || outPathLen == 0)
  {
    return false;
  }
  outPath[0] = '\0';

  const char *const candidates[] = ANIMATED_GIF_FILES;
  const size_t candidateCount = sizeof(candidates) / sizeof(candidates[0]);
  for (size_t i = 0; i < candidateCount; ++i)
  {
    File probe = SD.open(candidates[i], FILE_READ);
    if (probe && !probe.isDirectory())
    {
      strncpy(outPath, candidates[i], outPathLen - 1);
      outPath[outPathLen - 1] = '\0';
      probe.close();
      return true;
    }
    if (probe)
    {
      probe.close();
    }
  }

  File root = SD.open("/");
  if (!root || !root.isDirectory())
  {
    if (root)
    {
      root.close();
    }
    return false;
  }

  while (true)
  {
    File entry = root.openNextFile();
    if (!entry)
    {
      break;
    }
    if (!entry.isDirectory())
    {
      const char *name = entry.name();
      if (name && name[0] != '\0')
      {
        strncpy(outPath, name, outPathLen - 1);
        outPath[outPathLen - 1] = '\0';
      }
      entry.close();
      root.close();
      return outPath[0] != '\0';
    }
    entry.close();
  }

  root.close();
  return false;
}

bool sdBenchmarkRead(const char *path, uint32_t &outBytesPerSecond)
{
  outBytesPerSecond = 0;
  if (!path || path[0] == '\0')
  {
    return false;
  }

  File file = SD.open(path, FILE_READ);
  if (!file || file.isDirectory())
  {
    if (file)
    {
      file.close();
    }
    return false;
  }

  const uint32_t fileSize = static_cast<uint32_t>(file.size());
  const uint32_t targetBytes = fileSize < BOOT_SD_TEST_BYTES ? fileSize : static_cast<uint32_t>(BOOT_SD_TEST_BYTES);
  if (targetBytes == 0)
  {
    file.close();
    return false;
  }

  static uint8_t buffer[BOOT_SD_TEST_BUFFER_BYTES];
  uint32_t totalRead = 0;
  const uint64_t startUs = micros();
  while (totalRead < targetBytes)
  {
    const uint32_t remaining = targetBytes - totalRead;
    const uint32_t chunk = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
    const int bytesRead = file.read(buffer, chunk);
    if (bytesRead <= 0)
    {
      break;
    }
    totalRead += static_cast<uint32_t>(bytesRead);
  }
  file.close();

  const uint64_t elapsedUs = micros() - startUs;
  if (totalRead == 0)
  {
    return false;
  }
  const uint64_t denom = elapsedUs > 0 ? elapsedUs : 1;
  outBytesPerSecond = static_cast<uint32_t>((uint64_t(totalRead) * 1000000ULL) / denom);
  return true;
}

void showSdBootTest()
{
  if (BOOT_SD_TEST_DISPLAY_MS == 0)
  {
    return;
  }

  const uint8_t prevRotation = gfx->getRotation();
  gfx->setRotation(static_cast<uint8_t>((prevRotation + 2) & 0x03));

  gfx->fillScreen(BLACK);
  gfx->setTextSize(2);
  gfx->setTextColor(WHITE);

  const uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    const UiLine lines[] = {
        {"SD TEST", WHITE},
        {"SD FAIL", RED},
        {"No card / init", RED},
    };
    drawCenteredLines(lines, sizeof(lines) / sizeof(lines[0]));
    delay(BOOT_SD_TEST_DISPLAY_MS);
    gfx->setRotation(prevRotation);
    return;
  }

  char lineType[24] = {0};
  snprintf(lineType, sizeof(lineType), "Type: %s", sdCardTypeName(cardType));

  const uint64_t sizeBytes = SD.cardSize();
  char lineSize[24] = {0};
  if (sizeBytes > 0)
  {
    const uint32_t sizeMb = static_cast<uint32_t>(sizeBytes / (1024ULL * 1024ULL));
    snprintf(lineSize, sizeof(lineSize), "Size: %lu MB", static_cast<unsigned long>(sizeMb));
  }

  char lineSpi[24] = {0};
  snprintf(lineSpi, sizeof(lineSpi), "SPI: %lu MHz",
           static_cast<unsigned long>(ANIMATED_GIF_SD_FREQ / 1000000UL));

  char testPath[64] = {0};
  uint32_t bytesPerSec = 0;
  const bool hasReadSpeed = sdFindReadableFile(testPath, sizeof(testPath)) && sdBenchmarkRead(testPath, bytesPerSec);
  char lineRead[28] = {0};
  if (hasReadSpeed)
  {
    snprintf(lineRead, sizeof(lineRead), "Read: %lu KB/s",
	             static_cast<unsigned long>(bytesPerSec / 1024UL));
  }

  UiLine lines[8] = {};
  size_t lineCount = 0;
  lines[lineCount++] = UiLine{"SD TEST", WHITE};
  lines[lineCount++] = UiLine{"SD OK", GREEN};
  lines[lineCount++] = UiLine{lineType, WHITE};
  if (lineSize[0] != '\0')
  {
    lines[lineCount++] = UiLine{lineSize, WHITE};
  }
  lines[lineCount++] = UiLine{lineSpi, WHITE};
  if (hasReadSpeed)
  {
    lines[lineCount++] = UiLine{lineRead, WHITE};
    lines[lineCount++] = UiLine{testPath, WHITE};
  }
  else
  {
    lines[lineCount++] = UiLine{"Read test failed", YELLOW};
  }

  drawCenteredLines(lines, lineCount);

  delay(BOOT_SD_TEST_DISPLAY_MS);
  gfx->setRotation(prevRotation);
}
#endif

void setup()
{
  Serial.begin(115200);
  waitForSerial();

  Serial.println();
  Serial.println("RoundEyesController booting...");
  Serial.printf("SPI pins -> CS:%d DC:%d RST:%d\n", TFT_CS, TFT_DC, TFT_RST);

  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH); // keep deselected until init
#if defined(ENABLE_ANIMATED_GIF) && defined(ANIMATED_GIF_USE_SD)
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); // keep SD deselected until init
#endif
  pinMode(TFT_DC, OUTPUT);
  digitalWrite(TFT_DC, HIGH);
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(10);
  digitalWrite(TFT_RST, LOW);
  delay(10);
  digitalWrite(TFT_RST, HIGH);
  delay(120);

#if (DISPLAY_BACKLIGHT >= 0)
  pinMode(DISPLAY_BACKLIGHT, OUTPUT);
  digitalWrite(DISPLAY_BACKLIGHT, LOW);
#endif

#if defined(ENABLE_ANIMATED_GIF) && defined(ANIMATED_GIF_USE_SD)
  SPI.begin(SCK, MISO, MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI, ANIMATED_GIF_SD_FREQ))
  {
    Serial.println("SD init failed");
  }
  else
  {
    Serial.println("SD init ok");
  }
#endif

#if defined(ENABLE_EYE_ANIMATION)
  user_setup();
  initEyes();
  setActiveEye(activeEye);
#if defined(ENABLE_ANIMATED_GIF) && defined(ENABLE_EYE_PROGRAM)
  eyeBaseRotation = (NUM_EYES > 0) ? eye[0].rotation : 0;
#endif
#endif

#if defined(ARDUINO_ARCH_ESP32)
  randomSeed(esp_random());
#else
  randomSeed(micros());
#endif

  bleSyncSetup();

  if (!gfx->begin())
  {
    Serial.println("GC9A01 init failed");
    while (true)
    {
      delay(1000);
    }
  }

#if defined(ENABLE_ANIMATED_GIF) && defined(ENABLE_EYE_PROGRAM)
  gfx->setRotation(DISPLAY_ROTATION);
#elif defined(ENABLE_ANIMATED_GIF) || defined(ENABLE_HYPNO_SPIRAL)
  gfx->setRotation(DISPLAY_ROTATION);
#else
  const uint8_t baseRotation = (NUM_EYES > 0) ? eye[0].rotation : 0;
  gfx->setRotation(static_cast<uint8_t>((baseRotation + DISPLAY_ROTATION) & 0x03));
#endif
  Serial.println("GC9A01 init ok");

#if (DISPLAY_BACKLIGHT >= 0)
  digitalWrite(DISPLAY_BACKLIGHT, HIGH);
  Serial.printf("Backlight GPIO %d set HIGH\n", DISPLAY_BACKLIGHT);
#endif

  const uint16_t testColors[] = {RED, GREEN, BLUE, WHITE};
  for (uint8_t i = 0; i < (sizeof(testColors) / sizeof(testColors[0])); ++i)
  {
    gfx->fillScreen(testColors[i]);
    Serial.printf("Filled test color index %u\n", i);
    delay(500);
  }
  gfx->fillScreen(BLACK);

#if defined(ENABLE_ANIMATED_GIF) && defined(ANIMATED_GIF_USE_SD)
  showSdBootTest();
  gfx->fillScreen(BLACK);
#endif

#if defined(ENABLE_ANIMATED_GIF)
  animatedGifSetup();
  Serial.println("Animated GIF initialized");
#if defined(ENABLE_ANIMATED_GIF) && defined(ENABLE_EYE_PROGRAM)
  gifProgramCount = animatedGifFileCount();
  eyeProgramCount = eyeAssetCount();
  if (eyeProgramCount > 0)
  {
    enterProgram(gifProgramCount); // start on the default eye (eye index 0)
  }
  else
  {
    enterProgram(0);
  }
#endif
#elif defined(ENABLE_HYPNO_SPIRAL)
  hypnoSetup();
  Serial.println("Hypno spiral initialized");
#else
  startTime = millis();
  Serial.println("Eye animation initialized");
#endif
}

void loop()
{
  bleSyncLoop();
  if (blePairingUiLoop())
  {
    return;
  }
#if defined(ENABLE_ANIMATED_GIF) && defined(ENABLE_EYE_PROGRAM)
  const uint32_t now = millis();
  if (swirlTransitionActive(now))
  {
    return;
  }
  if (kEnableAutoSwitch && !bleSyncHasLock() && ANIMATED_GIF_SWITCH_INTERVAL_MS > 0 &&
      (now - programStartMs) >= ANIMATED_GIF_SWITCH_INTERVAL_MS)
  {
    enterProgram(programIndex + 1);
  }

  if (currentProgram == ProgramMode::Eye)
  {
    updateEye();
  }
  else if (currentProgram == ProgramMode::Hypno)
  {
#if defined(ENABLE_HYPNO_SPIRAL)
    hypnoStep();
#endif
  }
  else
  {
    animatedGifLoop();
    if (!animatedGifIsReady())
    {
      fallbackToDefaultEye();
    }
  }
#elif defined(ENABLE_ANIMATED_GIF)
  animatedGifLoop();
#elif defined(ENABLE_HYPNO_SPIRAL)
  hypnoStep();
#else
  updateEye();
#endif
}
