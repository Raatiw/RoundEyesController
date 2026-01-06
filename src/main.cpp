#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include <SPI.h>

#if defined(ARDUINO_ARCH_ESP32)
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
  Eye
};

ProgramMode currentProgram = ProgramMode::Gif;
size_t gifProgramCount = 0;
size_t eyeProgramCount = 0;
size_t programIndex = 0;
uint32_t programStartMs = 0;
uint8_t eyeBaseRotation = 0;

void setProgramRotation(ProgramMode mode)
{
  if (mode == ProgramMode::Eye)
  {
    gfx->setRotation(static_cast<uint8_t>((eyeBaseRotation + DISPLAY_ROTATION) & 0x03));
  }
  else
  {
    gfx->setRotation(DISPLAY_ROTATION);
  }
}

void fallbackToDefaultEye()
{
  currentProgram = ProgramMode::Eye;
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
} // namespace
#endif

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
  if (!SD.begin(SD_CS))
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
  enterProgram(0);
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
#if defined(ENABLE_ANIMATED_GIF) && defined(ENABLE_EYE_PROGRAM)
  const uint32_t now = millis();
  if (ANIMATED_GIF_SWITCH_INTERVAL_MS > 0 &&
      (now - programStartMs) >= ANIMATED_GIF_SWITCH_INTERVAL_MS)
  {
    enterProgram(programIndex + 1);
  }

  if (currentProgram == ProgramMode::Eye)
  {
    updateEye();
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
