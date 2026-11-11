/* radio_ui - the FusionEdge screen for boards without PSRAM, drawn in strips of rows (gfx.h): the
 * whole screen is never in memory, a strip of it is, and what changed is drawn again and sent.
 * Portable C: the firmware (core/display_lite.cpp) fills radio_t from the player and sends the strips
 * to the panel, a PC or browser build draws the same pictures from a radio_t of its own.
 * Where a finger lands is answered here too (radio_hit), from the same geometry as the drawing. */
#pragma once
#include "gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    RADIO_PLAYER, RADIO_LIST, RADIO_SETUP, RADIO_BOOT, RADIO_MESSAGE, RADIO_BLANK,
    RADIO_STATS, RADIO_SETTINGS, RADIO_CALIB
};

// the indicator in the player's middle, changed by a tap on it
enum { RADIO_IND_LED, RADIO_IND_SCOPE, RADIO_IND_RADAR, RADIO_INDICATORS };

// the parts of the screen, as bits: what radio_dirty() and radio_moving() say to draw again
#define RADIO_HEAD  1
#define RADIO_TITLE 2
#define RADIO_MID   4
#define RADIO_FOOT  8
#define RADIO_ALL   15

#define RADIO_BANDS_MAX 24
#define RADIO_TOUCH_MS  450
#define RADIO_LIST_ROWS 12
#define RADIO_SET_ROWS  8
#define RADIO_TRAFFIC_N 60

typedef struct {
    int      screen;            // RADIO_PLAYER ...
    uint32_t now_ms;            // what the scrolling text, the sweep and the blinking go by
    int      theme;             // radio_theme_name()

    // what plays
    char     station[96];
    int      station_no;        // its number in the list, 0: none
    char     title1[128];       // the artist, or the stream's whole title
    char     title2[128];       // the song, when the stream splits them
    char     codec[8];          // MP3, AAC...
    int      bitrate;           // kbit/s, 0: not known yet
    int      playing;           // 0 stopped, 1 playing, 2 connecting

    // the time
    int      hh, mm;            // -1: not set yet
    char     date[24];          // "WED 11 NOV"
    char     weather[64];

    // the knobs and the line
    int      volume, volume_max;
    int      volume_big;        // 1: the volume shown big over the middle, the knob was just turned
    int      buffer_pct;        // the stream's buffer, 0..100
    int      rssi;              // dBm, 0: no WiFi
    char     ip[16];

    // the indicator
    int      indicator;         // RADIO_IND_...
    int      indicator_named;   // 1: its name shown in its corner, it was just changed
    uint8_t  bands[RADIO_BANDS_MAX];   // the spectrum, 0..255 a band
    uint8_t  peaks[RADIO_BANDS_MAX];   // each band's recent highest, falling slowly
    uint8_t  ghost[3][RADIO_BANDS_MAX];// the spectrum's last frames, newest first: the scope's afterglow
    int      nbands;

    // a finger just went down here: a ring spreads from it and fades (RADIO_TOUCH_MS)
    int      touch_x, touch_y;
    int      touch_age;         // ms since, -1: none

    // the list (RADIO_LIST): rows on screen, the one playing among them
    char     list[RADIO_LIST_ROWS][48];
    int      list_no[RADIO_LIST_ROWS];
    int      list_rows;
    int      list_pick;         // index into list[], -1: none
    int      list_total;        // stations in all, for the scroll mark

    // the station's numbers (RADIO_STATS)
    int      samplerate;        // Hz
    int      channels;
    uint32_t stream_s;          // seconds this stream has played
    int      kbps;              // what comes in now, kbit/s
    uint32_t total_kb;          // what came in since the radio started
    uint16_t traffic[RADIO_TRAFFIC_N];   // kbit/s a second, the newest last
    int      traffic_n;
    int      reconnects;
    int      heap_kb, heap_big_kb;
    char     ssid[33];

    // the settings (RADIO_SETTINGS): a name and a value a row
    char     set_name[RADIO_SET_ROWS][16];
    char     set_value[RADIO_SET_ROWS][28];
    int      set_rows;

    // the touch calibration (RADIO_CALIB): which of the four crosses, and how long it waits yet
    int      calib_step;
    int      calib_left_s;

    // the setup, the boot and a message (RADIO_SETUP, RADIO_BOOT, RADIO_MESSAGE): up to four lines,
    // and a heading (capitals: the pixel font has no others), also the list's and the settings'
    char     line[4][64];
    char     head[24];
} radio_t;

// the parts of the screen, as rows: what the firmware draws again when only that part changed
typedef struct {
    int head_y, head_h;         // the station
    int title_y, title_h;       // the song
    int mid_y, mid_h;           // the indicator, the clock, and the big volume over them
    int foot_y, foot_h;         // volume, buffer, WiFi, address, settings
    int list_y, list_rowh;      // the list and settings screens: their first row's top, a row's height
    int list_rows;              // and how many rows fit
} radio_layout_t;

void        radio_layout(radio_layout_t *l);
int         radio_themes(void);
const char *radio_theme_name(int theme);
const char *radio_indicator_name(int indicator);

// draws the rows of this strip of the screen
void radio_draw(canvas_t *cv, const radio_t *r);

// the parts that look different from `was` to `now` (RADIO_HEAD...), RADIO_ALL for another screen
int radio_dirty(const radio_t *was, const radio_t *now);

// the parts that move by themselves: a text too long that scrolls, the dots while it connects, the
// boot's light. drawn again every frame or so while this says so
int radio_moving(const radio_t *r);

// what a tap at x, y on this screen means. *arg: the row (RADIO_HIT_ROW) or the volume (RADIO_HIT_VOLUME)
enum {
    RADIO_HIT_NONE, RADIO_HIT_STATION, RADIO_HIT_PLAYSTOP, RADIO_HIT_TITLE, RADIO_HIT_INDICATOR,
    RADIO_HIT_CLOCK, RADIO_HIT_VOLUME, RADIO_HIT_SETTINGS, RADIO_HIT_BACK, RADIO_HIT_ROW
};
int radio_hit(const radio_t *r, int x, int y, int *arg);

// where the calibration's cross number step (0..3: top left, top right, bottom right, bottom left) is
void radio_calib_point(int step, int *x, int *y);

#ifdef __cplusplus
}
#endif
