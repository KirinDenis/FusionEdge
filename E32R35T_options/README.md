# LCDWIKI 3.5" ESP32-32E (E32R35T) configuration

This directory builds FusionEdge for LCDWIKI's 3.5" ESP32-32E display with
resistive touch, E32R35T: an ESP32-32E with 4 MB of flash and no PSRAM, a
320x480 ST7796 IPS panel, XPT2046 resistive touch on the panel's bus, a microSD
slot, an RGB LED, a battery input and a speaker socket with an FM8002E
amplifier. Pins from lcdwiki.com, "3.5inch ESP32-32E Display".

The configuration was prepared by Denys Kirin and run on a board: it plays MP3
streams with about 50 KB of memory free in one piece, the touch works, and its
WiFi list can be written over the USB cable. An AAC stream ("aac+") and an
HTTPS MP3 stream do not play: the decoder cannot open in the memory that is
left.

## Installation

1. Back up `myoptions.h` and `platformio.ini` in the project root.
2. Copy this folder's `myoptions.h` and `platformio.ini` to the project root.
3. Fill `build-data/` for the LittleFS image: `bash .github/board-data.sh E32R35T_options`
4. Build and upload the firmware, then build and upload the LittleFS image.

## A board without PSRAM

As on the 2.8" CYD (see `ESP32-2432S028R_options/README.md`), `platformio.ini`
sets `LITE_UI`, `BOARD_NO_PSRAM`, `AUDIO_INTERNAL_DAC` and the screen's size,
480x320: the new screen in place of FusionEdge's own, drawn two pixel rows at a
time, MP3 streams over plain HTTP, and the sound from the ESP32's own DAC on
GPIO 26 in mono. The screen, its taps and swipes are described in that README.
The LittleFS image holds the web pages, the boot logo and the weather icons
(`data.txt`). The firmware is updated from the flasher page or over USB.

## Hardware configuration

- ST7796 display on HSPI: SCK 14, MOSI 13, MISO 12, CS 15, DC 2, backlight 27,
  reset by the ESP32's EN.
- XPT2046 touch on the same bus: CS 33, IRQ 36, read through MISO 12.
- microSD on VSPI: SCK 18, MISO 19, MOSI 23, CS 5.
- Speaker: GPIO 26 (DAC channel 2) into the FM8002E amplifier; GPIO 4 switches
  the amplifier on when low, off while nothing plays (`MUTE_PIN`).
- BOOT button (GPIO 0): opens the touch calibration.

## Notes

- The RGB LED (22, 16, 17, low = lit) and the battery voltage (34) are not used.
