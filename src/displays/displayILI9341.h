#pragma once

#include <FS.h>
#include <LittleFS.h>

// ILI9341 240x320 SPI panel, used in landscape as 320x240 (conf/conf_320x240.h).
#ifndef LGFX_WIDTH
#define LGFX_WIDTH 240
#endif

#ifndef LGFX_HEIGHT
#define LGFX_HEIGHT 320
#endif

#ifndef LGFX_ROTATION
#define LGFX_ROTATION 0
#endif

#include "lgfx/lgfx_base.h"

class LGFX_ILI9341 : public LGFX_Base<lgfx::Panel_ILI9341> {
	public:
	LGFX_ILI9341() : LGFX_Base(LGFX_WIDTH, LGFX_HEIGHT, LGFX_ROTATION) {}
};

typedef LGFX_ILI9341 yoDisplay;
typedef lgfx::LGFX_Sprite Canvas;

#include "tools/commongfx.h"

#include "conf/conf_320x240.h"

#define ILI9341_SLPIN   0x10
#define ILI9341_SLPOUT  0x11
#define ILI9341_DISPOFF 0x28
#define ILI9341_DISPON  0x29
