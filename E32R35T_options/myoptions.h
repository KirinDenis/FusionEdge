/* FusionEdge hardware configuration for the LCDWIKI 3.5" ESP32-32E display, E32R35T (resistive touch):
 * ESP32-32E with 4 MB of flash and no PSRAM, 3.5" 320x480 ST7796 IPS, XPT2046 resistive touch on the
 * panel's SPI bus, microSD on a bus of its own, an RGB LED and a speaker socket on GPIO 26 through an
 * FM8002E amplifier. Pins from lcdwiki.com (3.5inch ESP32-32E Display).
 * Use with this folder's platformio.ini, which builds for no PSRAM and the built-in DAC. */
#pragma once

#define LANGUAGE EN
#define CLOCK_TTS_LANGUAGE "en" //Default language TTS e.g. pl,en,de,ru,fr,hu
#define NAMEDAYS_FILE HU
// #define IMPERIALUNIT // English weather: Fahrenheit, inHg and mph (requires LANGUAGE EN)

/* The 3.5" ST7796 on the HSPI pins, landscape 480x320, the same way up as the 2.8" CYD. No colour
   inversion on this panel. The panel's reset is the ESP32's EN. */
#define DSP_MODEL DSP_ST7796
#define DEFAULT_SCREEN_ROTATION 3
#define LGFX_LCD_SPI_HOST 1
#define LGFX_LCD_SPI_WRITE_FREQ 40000000
#define LGFX_PANEL_INVERT false

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1

#define BRIGHTNESS_PIN 27

/* XPT2046 resistive touch on the panel's bus, read through its MISO (12); its pen interrupt on 36
   keeps the reads off that bus while nobody touches the glass. */
#define TS_MODEL TS_MODEL_XPT2046
#define TS_MISO 12
#define TS_CS 33
#define TS_INT 36

/* microSD on the VSPI host and pins, apart from the panel. */
#define SDC_CS 5
#define SD_SPI_HOST VSPI
#define SD_SPIPINS 18, 19, 23 // SCK, MISO, MOSI
#define SDSPISPEED 4000000

/* The speaker socket: GPIO 26 is the ESP32's DAC channel 2, into the FM8002E amplifier, which GPIO 4
   switches on when low. 8 bits, one channel: the player mixes to mono. */
#define I2S_INTERNAL true
#define I2S_DOUT 26
#define I2S_BCLK 255
#define I2S_LRC 255
#define PLAYER_FORCE_MONO true
#define MUTE_PIN 4
#define MUTE_VAL HIGH

#define EXT_WEATHER true

/* Not used here: the RGB LED (22, 16, 17, low = lit), the battery's voltage on 34 and the BOOT
   button on 0. */
