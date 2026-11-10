/* FusionEdge hardware configuration for the Elecrow CrowPanel Advance 2.8" HMI ESP32 AI Display:
 * ESP32-S3-WROOM-1 N16R8, 2.8" 240x320 ST7789 IPS, FT6336U capacitive touch, microSD and an NS4168 I2S
 * amplifier with a speaker on the board.
 * Pins from Elecrow's schematic V1.2 and its LovyanGFX example (CrowPanel-Advance-2.8-HMI-ESP32-AI-Display-320x240,
 * example/V1.0/Arduino_Code/lesson-03). Built, not run on a board yet. */
#pragma once

#ifndef ARDUINO_ESP32S3_DEV
#define ARDUINO_ESP32S3_DEV
#endif

#define LANGUAGE EN
#define CLOCK_TTS_LANGUAGE "en" //Default language TTS e.g. pl,en,de,ru,fr,hu
#define NAMEDAYS_FILE HU
// #define IMPERIALUNIT // English weather: Fahrenheit, inHg and mph (requires LANGUAGE EN)

// Optional current-track album art from Last.fm (WEB, SD, DLNA and Bluetooth metadata).
// #define USE_LASTFM_COVER
// #define LASTFM_API_KEY "your_lastfm_api_key"

// --- DLNA ---
//#define USE_DLNA
//#define dlnaHost "192.168.180.122" // your DLNA server IP address
//#define dlnaIDX  21 //(Music folder index: Synology = 21, MiniDLNA = 1, etc.)

/* GPIO 48 is the touch controller's reset on this board and GPIO 38 the backlight, so neither the
   built-in LED nor the LED strip may use them. */
#define USE_BUILTIN_LED false
#define LEDSTRIP_PIN 255

/* Integrated 2.8-inch ST7789 IPS display, 240x320, on its own SPI bus without MISO.
   Elecrow's example turns the panel by 3 with no offset: the same picture as rotation 3 here. */
#define DSP_MODEL DSP_ST7789
#define DEFAULT_SCREEN_ROTATION 3
#define LGFX_LCD_SPI_HOST 1

#define TFT_MISO -1
#define TFT_MOSI 39
#define TFT_SCK 42
#define TFT_CS 40
#define TFT_DC 41
#define TFT_RST -1

#define BRIGHTNESS_PIN 38

/* Integrated FT6336U capacitive touch controller. Elecrow's example says the touch of some panel
   batches is mirrored on X and others on Y: if a tap lands on the wrong side, switch the touch
   mirroring options in the WebUI. */
#define TS_MODEL TS_MODEL_FT6X36
#define TS_SDA 15
#define TS_SCL 16
#define TS_RST 48
#define TS_INT 47
#define TS_I2C_PORT 0
#define TS_I2C_ADDR 0x38
#define LGFX_TOUCH_BUS_SHARED false
#define LGFX_TOUCH_I2C_FREQ 400000

/* Integrated microSD slot on its own SPI bus. */
#define SDC_CS 7
#define SD_SPI_HOST HSPI
#define SD_SPIPINS 5, 4, 6
#define SDSPISPEED 4000000

/* Integrated NS4168 I2S amplifier and speaker. One speaker, so the player mixes to mono.
   GPIO 21 is the amplifier's control (board V1.1 and later): low switches it off while nothing plays. */
#define I2S_DOUT 12
#define I2S_BCLK 13
#define I2S_LRC 11
#define I2S_INTERNAL false
#define PLAYER_FORCE_MONO true
#define MUTE_PIN 21
#define MUTE_VAL LOW

#define EXT_WEATHER true

/* Not used here: the microphone (GPIO 9 and 10 behind a switch on 45), the buzzer on 8 and the
   wireless module socket. No encoder, IR receiver or Bluetooth module on the board. */
