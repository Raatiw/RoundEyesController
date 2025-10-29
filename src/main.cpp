#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_random.h>
#endif

// SPI wiring between the Adafruit Feather ESP32 V2 and the GC9A01 display.
static constexpr uint8_t TFT_SCK = SCK;               // GPIO5
static constexpr uint8_t TFT_MOSI = MOSI;             // GPIO19
static constexpr uint8_t TFT_MISO = GFX_NOT_DEFINED;  // Unused
static constexpr uint8_t TFT_CS = 33;                 // GPIO33 (D10 / SS)
static constexpr uint8_t TFT_DC = 26;                 // GPIO26 (A0)
static constexpr uint8_t TFT_RST = 25;                // GPIO25 (A1)

Arduino_DataBus *bus =
    new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, TFT_RST, /*rotation=*/0, /*IPS=*/true);

void user_setup(void);
void user_loop(void);

typedef struct
{
  int8_t  select;
  int8_t  wink;
  uint8_t rotation;
  int16_t xposition;
} eyeInfo_t;

#include "config.h"
#include "eye_functions.h"

uint16_t eyeFrameBuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
EyeState eye[NUM_EYES];
uint32_t startTime = 0;

void user_setup(void) {}
void user_loop(void) {}

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

#if (DISPLAY_BACKLIGHT >= 0)
  pinMode(DISPLAY_BACKLIGHT, OUTPUT);
  digitalWrite(DISPLAY_BACKLIGHT, LOW);
#endif

  user_setup();
  initEyes();

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
gfx->fillScreen(GREEN);
      delay(1000);

  if (NUM_EYES > 0)
  {
    gfx->setRotation(eye[0].rotation);
  }
  gfx->fillScreen(GREEN);

#if (DISPLAY_BACKLIGHT >= 0)
  digitalWrite(DISPLAY_BACKLIGHT, HIGH);
#endif

  startTime = millis();
  Serial.println("Eye animation initialized");
}

void loop()
{
  updateEye();
}
