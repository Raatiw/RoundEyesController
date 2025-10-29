#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x2886
#define USB_PID 0x0056
#define USB_MANUFACTURER "Seeed Studio"
#define USB_PRODUCT      "XIAO ESP32S3"
#define USB_SERIAL       ""

static const uint8_t LED_BUILTIN = 21;
#define BUILTIN_LED LED_BUILTIN
#define RGB_BUILTIN LED_BUILTIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 5;
static const uint8_t SCL = 6;

static const uint8_t SS  = 44;
static const uint8_t MOSI = 9;
static const uint8_t MISO = 8;
static const uint8_t SCK  = 7;

#define PIN_WIRE_SDA SDA
#define PIN_WIRE_SCL SCL

#define PIN_SPI_MISO MISO
#define PIN_SPI_MOSI MOSI
#define PIN_SPI_SCK  SCK
#define PIN_SPI_SS   SS

static const uint8_t A0  = 1;
static const uint8_t A1  = 2;
static const uint8_t A2  = 3;
static const uint8_t A3  = 4;
static const uint8_t A4  = 5;
static const uint8_t A5  = 6;
static const uint8_t A8  = 7;
static const uint8_t A9  = 8;
static const uint8_t A10 = 9;

static const uint8_t D0  = 1;
static const uint8_t D1  = 2;
static const uint8_t D2  = 3;
static const uint8_t D3  = 4;
static const uint8_t D4  = 5;
static const uint8_t D5  = 6;
static const uint8_t D6  = 43;
static const uint8_t D7  = 44;
static const uint8_t D8  = 7;
static const uint8_t D9  = 8;
static const uint8_t D10 = 9;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;

#define PIN_SERIAL_RX RX
#define PIN_SERIAL_TX TX

#define PIN_SERIAL1_RX 8
#define PIN_SERIAL1_TX 9

#define SERIAL_PORT_USBVIRTUAL Serial
#define SERIAL_PORT_MONITOR Serial
#define SERIAL_PORT_HARDWARE Serial1
#define SERIAL_PORT_HARDWARE_OPEN Serial1

#endif /* Pins_Arduino_h */
