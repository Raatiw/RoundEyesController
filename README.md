# RoundEyesController

PlatformIO project targeting the Seeed Studio XIAO ESP32S3 to drive a 240x240 GC9A01 round TFT display. The firmware cycles full-screen red, green, blue, and yellow fills.

## Requirements
- PlatformIO Core CLI or the VS Code PlatformIO extension
- USB-C cable for flashing the XIAO ESP32S3

## Wiring Overview
Adjust the wiring and the pin assignments in `src/main.cpp` as needed.

| Display Signal | XIAO ESP32S3 Pin (default in code) | Notes |
| -------------- | ---------------------------------- | ----- |
| SCK            | GPIO7 (D8)                         | SPI clock |
| MOSI           | GPIO9 (D10)                        | SPI MOSI |
| MISO (optional)| Not connected                      | GC9A01 does not require MISO |
| CS             | GPIO5 (D4)                         | Chip select |
| DC             | GPIO4 (D3)                         | Data/command |
| RST            | GPIO3 (D2)                         | Reset |
| BL             | GPIO2 (D1)                         | Backlight enable (tie high if hardware switch present) |
| VCC            | 3V3                                | Power |
| GND            | GND                                | Ground |

## Building
```
pio run
```

## Flashing
```
pio run -t upload
```
Ensure the board is in the correct download mode before flashing (double-tap reset).