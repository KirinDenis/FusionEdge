/* FusionEdge hardware configuration for the ESP32-2432S028R, the 2.8" "Cheap Yellow Display":
 * ESP32-WROOM-32 with 4 MB of flash and no PSRAM, 2.8" 240x320 ILI9341 TFT, XPT2046 resistive touch
 * on pins of its own, microSD, an RGB LED, a light sensor and a speaker socket on GPIO 26 through a
 * small amplifier. Pins from the board's variant in the Arduino core (jczn_2432s028r).
 * The same build runs LCDWIKI's 2.8" ESP32-32E display, E32R28T (USB-C, a BAT socket, "ESP32-32E" on
 * the back): the same pins but for GPIO 4, which switches its FM8002E amplifier on (MUTE_PIN below),
 * and an RGB LED on 22, 16 and 17. Pins from lcdwiki.com (2.8inch ESP32-32E Display).
 * Use with this folder's platformio.ini, which builds for no PSRAM and the built-in DAC. */
#pragma once

#define LANGUAGE EN
#define CLOCK_TTS_LANGUAGE "en" //Default language TTS e.g. pl,en,de,ru,fr,hu
#define NAMEDAYS_FILE HU
// #define IMPERIALUNIT // English weather: Fahrenheit, inHg and mph (requires LANGUAGE EN)

/* The 2.8" ILI9341 on the HSPI pins, landscape. Sold in revisions with different panels: the first
   (micro USB) wants no colour inversion, later ILI9341 ones want it, switched in the WebUI.
   The one with an ST7789 (marked "7789") needs DSP_ST7789 instead. GPIO 12 (the panel's MISO) is a
   strapping pin and the panel is only written: left alone. */
#define DSP_MODEL DSP_ILI9341
#define DEFAULT_SCREEN_ROTATION 1
#define LGFX_LCD_SPI_HOST 1
#define LGFX_LCD_SPI_WRITE_FREQ 40000000
#define LGFX_PANEL_INVERT false

#define TFT_MISO -1
#define TFT_MOSI 13
#define TFT_SCK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1

#define BRIGHTNESS_PIN 21

/* XPT2046 resistive touch on its own pins, on the VSPI host. That host is the SD slot's too, on
   other pins, so the SD card is not used on this board. */
#define TS_MODEL TS_MODEL_XPT2046
#define TS_SPI_HOST 2
#define TS_SCK 25
#define TS_MOSI 32
#define TS_MISO 39
#define TS_CS 33
#define TS_INT 36

/* The speaker socket: GPIO 26 is the ESP32's DAC channel 2, through the board's amplifier. 8 bits,
   one channel: the player mixes to mono. On the E32R28T the amplifier plays only while GPIO 4 is
   low: the player holds it low while it plays and high when it stops. On the Cheap Yellow Display
   GPIO 4 is the RGB LED's red, lit while it plays. */
#define I2S_INTERNAL true
#define I2S_DOUT 26
#define I2S_BCLK 255
#define I2S_LRC 255
#define PLAYER_FORCE_MONO true
#define MUTE_PIN 4
#define MUTE_VAL HIGH

#define EXT_WEATHER true

/* Not used here: the SD slot (5, 18, 19, 23), the rest of the RGB LED (Cheap Yellow Display 16 and 17,
   E32R28T 22, 16 and 17, low = lit), the light sensor on 34 and the BOOT button on 0. */
