# Elecrow CrowPanel Advance 2.8" configuration

This directory contains a FusionEdge `myoptions.h` for the Elecrow CrowPanel
Advance 2.8" HMI ESP32 AI Display, with its integrated 240x320 ST7789 IPS
display, FT6336U capacitive touch controller, microSD slot and NS4168 I2S
amplifier with a speaker. It is not the plain CrowPanel 2.8" HMI, which is a
classic ESP32 without PSRAM.

The configuration was prepared by Denys Kirin from Elecrow's V1.2 schematic and
its LovyanGFX example. It builds, but it has not been run on a board yet. If you
have one, please report whether the picture, the colours, the orientation, the
touch and the sound are right.

## Installation

1. Back up the `myoptions.h` file in the project root.
2. Copy `CrowPanel-Advance-2.8_options/myoptions.h` to the project root.
3. Review the language and the optional settings.
4. Run a clean build, then upload the firmware and LittleFS data.

## Integrated hardware configuration

- ESP32-S3-WROOM-1 N16R8: 16 MB flash, 8 MB octal PSRAM
- ST7789 SPI display: SCK 42, MOSI 39, CS 40, DC 41, backlight 38, reset by the
  board
- FT6336U touch controller at 0x38: SDA 15, SCL 16, reset 48, interrupt 47
- microSD: SCK 5, MISO 4, MOSI 6, CS 7, on the separate HSPI host
- NS4168 I2S amplifier: LRCLK 11, data 12, BCLK 13, control 21

## Notes

- GPIO 48 is the touch controller's reset on this board, so the LED strip and
  the built-in LED are switched off in this configuration.
- Elecrow's example says the touch of some panel batches is mirrored on X and of
  others on Y. If a tap lands on the wrong side, switch the touch mirroring
  options in the WebUI.
- The board has one speaker, so `PLAYER_FORCE_MONO` mixes both channels.
- GPIO 21 switches the amplifier off while nothing plays. Elecrow's schematic
  shows that control from board V1.1.
- Either USB-C port can be used for flashing: one goes through a CH340K, the
  other is the ESP32-S3's own USB.
- The microphone, the buzzer and the wireless module socket are not used.

## The 320x240 screen

This board uses the 320x240 layout in `src/displays/conf/conf_320x240.h` and the
ST7789 driver in `src/displays/displayST7789.*`. The layout has the same parts as
the 480x320 one, placed for the smaller screen, with these differences:

- There is no room for the status row (FADE, TTS, RGB), the EQ button or the
  weather icon. The weather text stays, under the clock.
- Touch: swipes for volume and stations, and the mode list, work as on 480x320.
  The taps that only the 480x320 layout has (presets, the EQ, the status row,
  clock and spectrum styles) are not there.
- The album-cover and cassette screensavers need 480x320 and stay off.
