# The new screen (LITE_UI)

A screen for FusionEdge in place of its own (`src/core/display.cpp`), the rest
of the firmware unchanged. `src/core/display_lite.cpp` implements the same
`Display` class; `radio_ui.c` draws the screen a strip of rows at a time into a
small buffer, the strip goes to the panel, and only the parts that changed are
drawn again.

## Building a board with it

Add three flags to the board's `build_flags`:

    -D LITE_UI -D LITE_SCREEN_W=480 -D LITE_SCREEN_H=320

`LITE_SCREEN_W` and `LITE_SCREEN_H` are the panel's size as the board turns it,
landscape: 480x320 or 320x240, the two layouts it has. A board without PSRAM
(`BOARD_NO_PSRAM`) must have it: FusionEdge's own screen does not fit there.

## What it shows

- The player: the station and its number, the song, a sound indicator (bars,
  a scope or a radar), the clock and the date. Nothing on it to aim at: a tap
  anywhere opens the buttons. Swipe up or down for the volume, left or right
  for the next or the previous station.
- The buttons, big enough for a finger: the station (its list), the WiFi's bars
  (the stream's numbers), the gear (the settings); previous, play or stop,
  next; the volume with a minus and a plus, and on a board that plays through
  the ESP32's own DAC (`AUDIO_INTERNAL_DAC`) its level too. Untouched for 8
  seconds, they give the screen back to the player.
- The stream's numbers: codec, bitrate, sample rate, the data coming in and its
  last minute as a graph, reconnects, free memory and the WiFi network.
- The settings: colour theme, indicator, brightness, turning the screen round,
  and on a resistive touch (XPT2046) its calibration. Kept in NVS.
- The encoders and the IR remote reach it through FusionEdge's own requests:
  the station list they scroll shows here, a number typed on the remote too.
  This part has not been run on a board with encoders or a remote yet.
- The radio's web page takes the screen's colours, its first theme, and has
  no logo: the firmware sends its own `theme.css` and an empty `logo.svg`
  (`src/core/netserver.cpp`), the files in LittleFS stay as they are.

## The files

`gfx.c`, `font.c` and the `font_*.c` letters (Cascadia Mono, baked) come from
OWLOS Flight Radar, MIT, Denys Kirin; `lite_config.h` sets them to one plain
panel. `radio_ui.c` and `radio_ui.h` are the screen itself.
