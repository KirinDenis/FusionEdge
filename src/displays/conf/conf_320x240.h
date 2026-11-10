// clang-format off
#pragma once

#include "../../core/config.h"
#include "../../core/display.h"
#include "../widgets/widgets.h"

/* 320x240: a 2.4" or 2.8" panel in landscape. The same elements as conf_480x320.h, placed for the smaller screen:
     0..84     station icon (80x80), station name, two title lines
     88..106   play mode on the left, bitrate on the right
     107       the line under them
     109..126  date and name day
     128..210  spectrum on the left, clock on the right, weather text under the clock
     108..213  the band where volume, stations and mode select open
     216..238  IP, volume, buffer, RSSI, WiFi
   There is no room for the status row, the EQ button or the weather icon. The EQ is opened by a tap that only
   the 480x320 layout has, so it is left out with its button. */

#define DSP_WIDTH       320
#define DSP_HEIGHT      240
#define TFT_FRAMEWDT      6
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define CLOCK_HEIGHT     70

#if BITRATE_FULL
  #define TITLE_FIX  30
#else
  #define TITLE_FIX   0
#endif
#define bootLogoTop 8

/* What display.cpp, touchscreen.cpp and clockWidget.cpp place by number on 480x320 */
#define VOL_AREA_TOP    108
#define VOL_AREA_HEIGHT 106
#define VOL_AREA_LEFT     0
#define VOL_AREA_WIDTH  DSP_WIDTH

#define SPECTRUM_TOP    128
#define SPECTRUM_RIGHT  124   // where the clock starts
#define SPECTRUM_HEIGHT  82
#define SPECTRUM_BANDS   10

#define STATION_ICON_TOP  4
#define CLOCK_SS_ZOOM   1.5f
#define CLOCK_SS_DATE_Y 192

#define VOL_OVL_TITLE_Y  12
#define VOL_OVL_NUM_Y    52
#define VOL_OVL_SPK_Y    18
#define VOL_OVL_SPK_X    10
#define VOL_OVL_SEG_W    10
#define VOL_OVL_SEG_GAP   4
#define VOL_OVL_SEG_H    12
#define VOL_OVL_BAR_UP   22
#define SD_OVL_TITLE_Y   14
#define SD_OVL_COUNT_Y   60

#define TS_SWIPE_TOP     38

#define HIDE_STATUS_WIDGET
#define HIDE_EQ_WIDGET
#define HIDE_WEATHER_ICON

/* SROLLS  */                            /* {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
const ScrollConfig metaConf       PROGMEM = {{ TFT_FRAMEWDT+84,  4, 18, WA_CENTER }, 140, true,  MAX_WIDTH-84, 5000, 5, 40 };
const ScrollConfig title1Conf     PROGMEM = {{ TFT_FRAMEWDT+84, 30, 16, WA_CENTER }, 140, true,  MAX_WIDTH-84, 5000, 5, 40 };
const ScrollConfig title2Conf     PROGMEM = {{ TFT_FRAMEWDT+84, 54, 16, WA_CENTER }, 140, false, MAX_WIDTH-84, 5000, 5, 40 };
const ScrollConfig playlistConf   PROGMEM = {{ TFT_FRAMEWDT, 118, 20, WA_LEFT }, 140, true, MAX_WIDTH, 1000, 5, 40 };
const ScrollConfig apTitleConf    PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 5, 40 };
const ScrollConfig apSettConf     PROGMEM = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 4, 40 };
const ScrollConfig weatherConf    PROGMEM = {{ SPECTRUM_RIGHT, 196, 12, WA_CENTER }, 254, false, DSP_WIDTH-TFT_FRAMEWDT-SPECTRUM_RIGHT, 0, 3, 40 };

/* BACKGROUNDS  */                       /* {{ left, top, fontsize, align }, width, height, outlined } */
const FillConfig   metaBGConf     PROGMEM = {{ 3, 107, 0, WA_CENTER }, DSP_WIDTH - 6, 1, true };
const FillConfig   metaBGConfInv  PROGMEM = {{ 0, 27, 0, WA_LEFT }, DSP_WIDTH, 2, false };
const FillConfig   volbarConf     PROGMEM = {{TFT_FRAMEWDT, DSP_HEIGHT - TFT_FRAMEWDT - 6, 0, WA_LEFT}, MAX_WIDTH, 4, true};
const FillConfig   playlBGConf    PROGMEM = {{ 0, 109, 0, WA_LEFT }, DSP_WIDTH, 28, false };
const FillConfig   heapbarConf    PROGMEM = {{ TFT_FRAMEWDT, DSP_HEIGHT-2, 0, WA_LEFT }, DSP_WIDTH-12, 2, false };

// left,top, width, height, textsize, align, border, radius, fill, paddingX, paddingY
const textBoxConfig bootstrConf   PROGMEM = {20, 190, 280, 30, 16, WA_CENTER, 1, 4, true, 0, 0};
const textBoxConfig ipBoxConf     PROGMEM = {TFT_FRAMEWDT, 216, 100, 22, 12, WA_CENTER, 1, 3, true, 0, 0};
// left, top, width, height, segments, segWidth, segGap, segHeight, iconW, iconH, radius, border
const VolumeWidgetConfig bufferWidgetConf PROGMEM = {178, 216, 54, 22, 10, 3, 1, 6, 10, 10, 3, 1}; // Audio buffer widget
const textBoxConfig rssiBoxConf   PROGMEM = {236, 216, 52, 22, 12, WA_CENTER, 1, 3, true, 0, 0};

/* WIDGETS  */ /* { left, top, fontsize, align } */
const WidgetConfig numConf        PROGMEM = {0, 130, 70, WA_CENTER};
const WidgetConfig apNameConf     PROGMEM = {TFT_FRAMEWDT, 46, 3, WA_CENTER};
const WidgetConfig apName2Conf    PROGMEM = {TFT_FRAMEWDT, 72, 3, WA_CENTER};
const WidgetConfig apPassConf     PROGMEM = {TFT_FRAMEWDT, 118, 3, WA_CENTER};
const WidgetConfig apPass2Conf    PROGMEM = {TFT_FRAMEWDT, 144, 3, WA_CENTER};
const WidgetConfig bootWdtConf    PROGMEM = {0, 170, 1, WA_CENTER};
const WidgetConfig clockConf      PROGMEM = { SPECTRUM_RIGHT, SPECTRUM_TOP, 70, WA_RIGHT };
const WidgetConfig dateConf       PROGMEM = { TFT_FRAMEWDT, 109, 16, WA_CENTER };

// Speed, width, barwidth
const ProgressConfig bootPrgConf  PROGMEM = {90, 14, 4};

// left, top, width, height, segments, segWidth, segGap, segHeight, iconW, iconH, radius, border
const VolumeWidgetConfig volConf PROGMEM = {110, 216, 64, 22, 14, 2, 1, 7, 8, 11, 3, 1};

// left, top, width, height, image1, image2, image3, image4
const WifiWidgetConfig wifiConf PROGMEM = {292, 216, 22, 22, "/images/wifi_1_30x25.png", "/images/wifi_2_30x25.png", "/images/wifi_3_30x25.png", "/images/wifi_4_30x25.png"};

//left, top, textsize, align, border, radius, fill, paddingX, paddingY, dimension
/* BITRATE */
const BitrateBoxConfig bitrateConf PROGMEM = { (uint16_t)(DSP_WIDTH - TFT_FRAMEWDT - 72), 88, 12, WA_RIGHT, 1, 3, false, 2, 1, 36 };

/* PLAYMODE */
const BitrateBoxConfig pmodeConf   PROGMEM = { TFT_FRAMEWDT, 88, 12, WA_LEFT, 1, 3, false, 2, 1, 36 };

/* STRINGS  */
const char numtxtFmt[]  PROGMEM = "%d";
const char rssiFmt[]    PROGMEM = "%ddBm";
const char iptxtFmt[]   PROGMEM = "%s";
const char voltxtFmt[]  PROGMEM = "\023\025%d%%";
const char bitrateFmt[] PROGMEM = "%d kBs";

/* MOVES  */
const MoveConfig clockMove PROGMEM = { SPECTRUM_RIGHT, SPECTRUM_TOP, -1 };

// clang-format on
