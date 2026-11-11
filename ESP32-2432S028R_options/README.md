# ESP32-2432S028R (2.8" "Cheap Yellow Display") configuration

This directory builds FusionEdge for the ESP32-2432S028R, the 2.8" board sold as
the "Cheap Yellow Display": an ESP32-WROOM-32 with 4 MB of flash and no PSRAM,
a 240x320 ILI9341 TFT, XPT2046 resistive touch, a microSD slot, an RGB LED, a
light sensor and a speaker socket.

The configuration was prepared by Denys Kirin and run on a board of the first
revision (micro USB): it starts with the screen described below and plays MP3
streams with about 55 KB of memory free in one piece. Its touch is still to be
checked.

## Installation

1. Back up `myoptions.h` and `platformio.ini` in the project root.
2. Copy this folder's `myoptions.h` and `platformio.ini` to the project root.
3. Fill `build-data/` for the LittleFS image: `bash .github/board-data.sh ESP32-2432S028R_options`
4. Build and upload the firmware, then build and upload the LittleFS image.

## A board without PSRAM

FusionEdge is written for boards with PSRAM. This board has none, so its
`platformio.ini` sets these build flags:

- `LITE_UI`: FusionEdge's own screen (`src/core/display.cpp`) keeps its fonts
  and sprites in RAM and does not fit beside the player and the WiFi, so the
  new screen takes its place: `src/core/display_lite.cpp` draws it two pixel
  rows at a time (`src/lite`, see "The screen" below). `LITE_SCREEN_W` and
  `LITE_SCREEN_H` give its size, 320x240. A board with PSRAM can have the same
  screen with these three flags alone, and the flasher site builds every board
  with PSRAM both ways (`src/lite/README.md`).
- `BOARD_NO_PSRAM`: the audio buffers are small: MP3 streams play; AAC, FLAC,
  Opus and Vorbis do not, their decoders need more memory than is left. An
  HTTPS stream does not play either: its encryption leaves about 5 KB in one
  piece, too little for the MP3 decoder. A station that cannot play is
  retried, it does not restart the board. The LittleFS image holds only the web
  pages and the boot logo (`data.txt`).
- `AUDIO_INTERNAL_DAC`: the sound comes from the ESP32's own 8-bit DAC on GPIO
  26, the board's speaker socket, in mono.

The 4 MB partition table `partitions_4mb.csv` has one firmware slot, so the
firmware is updated from the flasher page or over USB, not from the radio's own
web page.

## The screen

- The player: the station and its number, the song, the clock and the date, an
  indicator of the sound, the volume, the stream's buffer and the WiFi signal.
  Tap the station for the station list, the song or the clock for the stream's
  numbers, the indicator to change it (bars, a scope, a radar), the volume to
  set it, the gear for the settings. Swipe up or down for the volume, left or
  right for the next or the previous station.
- The stream's numbers: codec, bitrate, sample rate, the data coming in and its
  last minute as a graph, reconnects, free memory and the WiFi network.
- The settings: colour theme, indicator, brightness, flip the screen, calibrate
  the touch. They are kept in the ESP32's NVS.
- Touch calibration: four crosses to tap. It opens on the first start, from the
  settings, or with the BOOT button.

## Hardware configuration

- ILI9341 display on HSPI: SCK 14, MOSI 13, CS 15, DC 2, backlight 21, reset by
  the board. GPIO 12 (MISO) is a strapping pin and is not used.
- XPT2046 touch on VSPI with its own pins: SCK 25, MOSI 32, MISO 39, CS 33,
  IRQ 36.
- Speaker: GPIO 26 (DAC channel 2) through the board's amplifier.
- BOOT button (GPIO 0): opens the touch calibration.

## Notes

- The microSD slot (5, 18, 19, 23) is on the same VSPI host as the touch, on
  other pins, so the SD card is not used.
- The board is sold in revisions with different panels. The first (micro USB)
  wants no colour inversion; later ILI9341 ones want it, switched in the WebUI.
  The one with an ST7789 (marked "7789") needs `DSP_ST7789` in `myoptions.h`.
- The RGB LED (4, 16, 17) and the light sensor (34) are not used.
