#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <SPI.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_random.h>
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
  if (VISUALREMOTE_PAIR_WINDOW_MS == 0)
  {
    return;
  }

  g_bleLearnMode = true;
  g_bleLearnModeUntilMs = millis() + VISUALREMOTE_PAIR_WINDOW_MS;
  Serial.printf("BLE pairing window: %lu ms (press preset now)\n",
                static_cast<unsigned long>(VISUALREMOTE_PAIR_WINDOW_MS));
}

void blePairButtonLoop()
{
  if (VISUALREMOTE_PAIR_BUTTON_PIN < 0 || VISUALREMOTE_PAIR_WINDOW_MS == 0 ||
      VISUALREMOTE_PAIR_HOLD_MS == 0)
  {
    return;
  }

  static bool wasPressed = false;
  static bool triggered = false;
  static uint32_t pressedSinceMs = 0;

  const bool pressed = (digitalRead(VISUALREMOTE_PAIR_BUTTON_PIN) == LOW);
  const uint32_t now = millis();

  if (pressed != wasPressed)
  {
    wasPressed = pressed;
    triggered = false;
    pressedSinceMs = now;
  }

  if (pressed && !triggered && (now - pressedSinceMs) >= VISUALREMOTE_PAIR_HOLD_MS)
  {
    triggered = true;
    startBleLearnMode();
  }
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
    pinMode(VISUALREMOTE_PAIR_BUTTON_PIN, INPUT_PULLUP);
  }

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new BleSyncCallbacks(), true);
  scan->setActiveScan(false);
  scan->setInterval(45);
  scan->setWindow(15);
  scan->start(0, nullptr, false);
}

void bleSyncLoop()
{
  blePairButtonLoop();

  if (g_bleLearnMode && VISUALREMOTE_PAIR_WINDOW_MS > 0 &&
      (int32_t)(millis() - g_bleLearnModeUntilMs) >= 0)
  {
    g_bleLearnMode = false;
    Serial.println("BLE pairing window expired");
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
    }
  }

  const int16_t mapped = mapEffectToProgram(effect);
  Serial.printf("BLE preset: %u -> map %d timebase %lu\n", effect, mapped,
                static_cast<unsigned long>(timebase));
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
