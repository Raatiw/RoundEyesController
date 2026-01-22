# RoundEyesController

PlatformIO project targeting the Adafruit Feather ESP32 V2 driving a 240×240 GC9A01 round TFT display. The firmware now offers both the original uncanny-eye animation as well as a procedural spiral hypnosis mode rendered with the Arduino_GFX library.

## Requirements
- PlatformIO Core CLI or the VS Code PlatformIO extension
- USB-C cable for flashing the Feather ESP32 V2

## Wiring Overview
Adjust the wiring or the pin assignments in `src/main.cpp` as needed.

| Display Signal | Feather ESP32 V2 pin (default in code) | Notes |
| -------------- | -------------------------------------- | ----- |
| SCK            | GPIO5 (`SCK`)                          | SPI clock |
| MOSI           | GPIO19 (`MOSI`)                        | SPI MOSI |
| MISO (optional)| Not connected                          | GC9A01 does not require MISO |
| CS             | GPIO33 (`D10 / SS`)                    | Chip select |
| DC             | GPIO26 (`A0`)                          | Data/command |
| RST            | GPIO25 (`A1`)                          | Reset |
| BL             | tied to 3V3                            | Panel uses fixed backlight |
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

## BLE Control (WLED VisualRemote)

When used with the WLED `VisualRemote` usermod, the controller broadcasts a BLE manufacturer payload containing a preset ID. This firmware listens for those broadcasts and maps the preset number via `mapEffectToProgram()` in `src/main.cpp`.

Payload layout (18 bytes, little-endian): `company(2) appId(2) flags(1) seq(2) preset(1) timebase(4) group(6)`.

Configuration lives in `include/config.h`:
- `VISUALREMOTE_ACCEPT_GLOBAL_PRESETS` (default 0) controls whether "global" preset broadcasts are accepted.
- `VISUALREMOTE_GROUP_FILTER` can be set to a 12-hex-digit group ID (e.g. `"aabbccddeeff"`) to only react to that group.
- Hold the pairing button (`VISUALREMOTE_PAIR_BUTTON_PIN`) for `VISUALREMOTE_PAIR_HOLD_MS` to enter a pairing window (`VISUALREMOTE_PAIR_WINDOW_MS`). The first received packet during that window is stored (NVS) and becomes the new group filter.

## Animation Modes

Animation selection lives in `include/config.h`:

- Enable the spiral hypnosis renderer by leaving `#define ENABLE_HYPNO_SPIRAL` uncommented. The look and feel can be tuned with the `HYPNO_*` constants:
  - `HYPNO_PRIMARY_COLOR`, `HYPNO_SECONDARY_COLOR`, `HYPNO_BACKGROUND_COLOR` control the 16‑bit RGB565 palette.
  - `HYPNO_STRIPE_COUNT` adjusts how many bright/dark arms orbit the centre.
  - `HYPNO_TWIST_FACTOR` changes how tightly the bands spiral from centre to edge.
  - `HYPNO_RADIUS_EXPONENT` eases the stripe spacing toward the middle (values < 1 tighten the centre).
  - `HYPNO_STRIPE_DUTY` controls the bright/dark ratio of each arm.
  - `HYPNO_PHASE_INCREMENT` sets the rotation speed (higher = faster).

- Enable the in-flash GIF player by defining `ENABLE_ANIMATED_GIF`. Pick the header and symbol via macros in `config.h`, e.g.
  ```c++
  #define ANIMATED_GIF_HEADER "fractal.h"
  #define ANIMATED_GIF_SYMBOL fractal
  ```
  (defaults point at `wobble.h`). `ANIMATED_GIF_BACKGROUND` and `ANIMATED_GIF_DEFAULT_DELAY` can also be overridden if a clip needs special settings.

- Toggle `ENABLE_EYELIDS` if you want the procedural eyelids and blinking to render. Comment it out to keep the full eye surface visible at all times.
- If the panel is mounted rotated, adjust `DISPLAY_ROTATION` (0–3) in `config.h`; this rotates both the eye renderer and the animated GIF/spiral modes without touching the assets.

- Comment out `#define ENABLE_HYPNO_SPIRAL` to restore the uncanny-eye animation. In that mode the large sprite headers in `include/` (for example `defaultEye.h`, `catEye.h`, etc.) provide the artwork. Pick the eye style you want by enabling the corresponding `#include` near the top of `config.h`.

## Creating Custom Eye Sprites

The classic eye animation expects 16-bit RGB565 images stored in PROGMEM arrays. You can generate compatible headers from your own artwork by converting PNGs with the short Python script below:

```bash
python - <<'PY'
from pathlib import Path
from PIL import Image

INPUT = Path("my_eye.png")          # square image
OUTPUT = Path("include/myEye.h")    # destination header
name = "myEye"

img = Image.open(INPUT).convert("RGB")
if img.width != img.height:
    raise SystemExit("Image must be square")

pixels = list(img.getdata())

def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

with OUTPUT.open("w") as f:
    f.write(f"#define SCLERA_WIDTH  {img.width}\n")
    f.write(f"#define SCLERA_HEIGHT {img.height}\n\n")
    f.write(f"const uint16_t sclera[SCLERA_HEIGHT * SCLERA_WIDTH] PROGMEM = {{\n  ")
    rows = []
    for i, p in enumerate(pixels):
        rows.append(f"0x{rgb565(*p):04X}")
        if (i + 1) % img.width == 0:
            f.write(", ".join(rows) + ",\n  ")
            rows.clear()
    f.write("\n};\n")
PY
```

Drop the resulting header into `include/`, include it from `config.h`, and rebuild. For more elaborate scenes (iris maps, eyelids, etc.) model your file after the existing assets such as `defaultEye.h`. The repository keeps those examples for reference.

## Running the Spiral Hypnosis Mode

The spiral mode procedurally renders into PSRAM, so no sprite assets are required. If you tweak the `HYPNO_*` constants, just rebuild with `pio run -t upload` and the effect will update immediately.
