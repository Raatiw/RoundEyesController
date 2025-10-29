#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Adjust these pin assignments to match your wiring between the Adafruit Feather ESP32 V2 and the GC9A01 display.
static constexpr uint8_t TFT_SCK = SCK;                // Feather hardware SPI SCK (GPIO5)
static constexpr uint8_t TFT_MOSI = MOSI;              // Feather hardware SPI MOSI (GPIO19)
static constexpr uint8_t TFT_MISO = GFX_NOT_DEFINED;   // GC9A01 does not require MISO
static constexpr uint8_t TFT_CS = 33;                  // Example: Feather pin D10 / SS (GPIO33)
static constexpr uint8_t TFT_DC = 25;                  // Example: Feather pin A1 (GPIO25)
static constexpr uint8_t TFT_RST = 26;                 // Example: Feather pin A0 (GPIO26)
static constexpr uint8_t TFT_BL = 32;                  // Example: Feather pin A7 (GPIO32) for backlight enable

Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, TFT_RST, /*rotation=*/0, /*IPS=*/true);

struct ColorEntry
{
  uint16_t value;
  const char *name;
};

static const ColorEntry COLOR_SEQUENCE[] = {
    {RED, "RED"},
    {GREEN, "GREEN"},
    {BLUE, "BLUE"},
    {YELLOW, "YELLOW"}
};

static size_t colorIndex = 0;

void setup()
{
  Serial.begin(115200);

  delay(100);
  const uint32_t serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart) < 3000)
  {
    delay(10);
  }

  Serial.println();
  Serial.println("RoundEyesController booting...");

  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH); // Keep display deselected until init completes

  pinMode(TFT_DC, OUTPUT);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // Turn on the backlight

  if (!gfx->begin())
  {
    Serial.println("GC9A01 init failed");
    while (true)
    {
      delay(1000);
    }
  }

  Serial.println("GC9A01 initialized");

  gfx->fillScreen(BLACK);
}

void loop()
{
  const ColorEntry &entry = COLOR_SEQUENCE[colorIndex];
  Serial.print("Filling color: ");
  Serial.println(entry.name);
  gfx->fillScreen(entry.value);
  colorIndex = (colorIndex + 1) % (sizeof(COLOR_SEQUENCE) / sizeof(COLOR_SEQUENCE[0]));

  delay(1000);
}
