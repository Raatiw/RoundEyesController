#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <SPI.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <NimBLEDevice.h>
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

  if ((now - g_swirlTransition.startMs) >= SWIRL_TRANSITION_DURATION_MS)
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
// Map incoming WLED effect numbers (0-40) to GIF program indices.
// Use -1 to fall back to the default eye animation.
constexpr int16_t kEffectToProgramMap[] = {
    -1,  // 0
    0,   // 1 -> beer.gif
    1,   // 2 -> fish.gif
    2,   // 3 -> wobble.gif
    3,   // 4 -> hearth.gif
    4,   // 5 -> fractal.gif
    5,   // 6 -> phenakistiscope.gif
    6,   // 7 -> tunnel.gif
    -1,  // 8
    -1,  // 9
    -1,  // 10
    -1,  // 11
    4,   // 12 -> fractal.gif (same as effect 5)
    -2,  // 13 -> hypno spiral
    -1,  // 14
    -1,  // 15
    -1,  // 16
    -1,  // 17
    -1,  // 18
    -1,  // 19
    -1,  // 20
    -1,  // 21
    -1,  // 22
    -1,  // 23
    -1,  // 24
    -1,  // 25
    -1,  // 26
    -1,  // 27
    -1,  // 28
    -1,  // 29
    -1,  // 30
    -1,  // 31
    -1,  // 32
    -1,  // 33
    -1,  // 34
    -1,  // 35
    -1,  // 36
    -1,  // 37
    -1,  // 38
    -1,  // 39
    -1   // 40
};
constexpr bool kEnableAutoSwitch = false;

int16_t mapEffectToProgram(uint8_t effect)
{
  if (effect >= (sizeof(kEffectToProgramMap) / sizeof(kEffectToProgramMap[0])))
  {
    return -1;
  }
  return kEffectToProgramMap[effect];
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
constexpr uint8_t kBleTypeSync = 1;
constexpr uint8_t kBleTypeEye = 2;

volatile bool g_blePending = false;
volatile uint8_t g_bleEffect = 0;
volatile uint8_t g_bleType = 0;
volatile uint32_t g_bleTimebase = 0;
uint16_t g_bleLastSeqSync = 0xFFFF;
uint16_t g_bleLastSeqEye = 0xFFFF;
volatile uint32_t g_bleLastMs = 0;
bool g_bleHasSync = false;

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
    const uint8_t type = bytes[4];
    const uint16_t seq = bytes[5] | (uint16_t(bytes[6]) << 8);
    const uint8_t effect = bytes[7];

    if (type == kBleTypeSync)
    {
      if (seq == g_bleLastSeqSync)
      {
        return;
      }
      g_bleLastSeqSync = seq;
      const uint32_t timebase = uint32_t(bytes[8]) |
                                (uint32_t(bytes[9]) << 8) |
                                (uint32_t(bytes[10]) << 16) |
                                (uint32_t(bytes[11]) << 24);
      g_bleEffect = effect;
      g_bleType = type;
      g_bleTimebase = timebase;
      g_blePending = true;
      g_bleLastMs = millis();
      g_bleHasSync = true;
    }
    else if (type == kBleTypeEye)
    {
      if (seq == g_bleLastSeqEye)
      {
        return;
      }
      g_bleLastSeqEye = seq;
      g_bleEffect = effect;
      g_bleType = type;
      g_bleTimebase = 0;
      g_blePending = true;
      g_bleLastMs = millis();
    }
  }
};

void bleSyncSetup()
{
  NimBLEDevice::init("RoundEyes");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new BleSyncCallbacks(), true);
  scan->setActiveScan(false);
  scan->setInterval(45);
  scan->setWindow(15);
  scan->start(0, nullptr, false);
}

void bleSyncLoop()
{
  if (!g_blePending)
  {
    return;
  }
  g_blePending = false;
  const uint8_t effect = g_bleEffect;
  const uint8_t type = g_bleType;
  const uint32_t timebase = g_bleTimebase;

  if (type == kBleTypeSync)
  {
    const int16_t mapped = mapEffectToProgram(effect);
    Serial.printf("BLE sync: effect %u -> map %d timebase %lu\n", effect, mapped,
                  static_cast<unsigned long>(timebase));
    applyMappedProgram(effect);
  }
  else if (type == kBleTypeEye)
  {
    const int16_t mapped = mapEffectToProgram(effect);
    Serial.printf("BLE eye: effect %u -> map %d\n", effect, mapped);
    applyMappedProgram(effect);
  }
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
