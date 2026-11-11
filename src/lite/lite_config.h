/* lite_config.h - the screen size for the strip renderer (gfx.h), from the board's platformio.ini:
 * -DLITE_SCREEN_W=320 -DLITE_SCREEN_H=240. gfx.c, font.c and the fonts come from OWLOS Flight Radar
 * (MIT, Denys Kirin), where config.h picks one of many panels; here it is one plain panel, drawn
 * pixel for pixel, its 16-bit pixels sent high byte first as the SPI panels take them. */
#pragma once

#ifndef LITE_SCREEN_W
#define LITE_SCREEN_W 320
#endif
#ifndef LITE_SCREEN_H
#define LITE_SCREEN_H 240
#endif

#define GFX_W LITE_SCREEN_W
#define GFX_H LITE_SCREEN_H
#define GFX_PW GFX_W
#define GFX_PH GFX_H
#define GFX_SCALED 0
#define RADAR_RGB_BUS 0
#define RADAR_EPAPER 0
#define RADAR_ROUND 0
