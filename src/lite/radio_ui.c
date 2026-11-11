/* radio_ui - the FusionEdge screen for boards without PSRAM (radio_ui.h). Everything here draws on
 * one strip of rows at a time and clips to it: a call that has nothing in the strip costs nothing.
 * The colours are OWLOS Flight Radar's themes; the letters its Cascadia Mono bakes and its
 * MICRO pixel font, in capitals, for the labels. */
#include "radio_ui.h"
#include "font.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    uint32_t bg0, bg1, glass, panel;
    uint32_t prim, quiet, focus, dim;
    uint32_t low, alarm;
} rtheme_t;

/* the radar's themes (core/ui.c there), its roles kept */
static const rtheme_t THEMES[] = {
    { "ARCTIC",   RGB(3, 8, 18),  RGB(9, 22, 46),  RGB(16, 42, 82), RGB(8, 26, 54),
                  RGB(92, 225, 255), RGB(46, 130, 178), RGB(234, 247, 255), RGB(120, 160, 196),
                  RGB(255, 184, 72), RGB(255, 80, 88) },
    { "PHOSPHOR", RGB(2, 9, 4),   RGB(6, 24, 10),  RGB(10, 46, 20), RGB(6, 28, 12),
                  RGB(90, 255, 130), RGB(40, 150, 70), RGB(225, 255, 230), RGB(130, 190, 145),
                  RGB(255, 200, 70), RGB(255, 90, 80) },
    { "AMBER",    RGB(10, 5, 0),  RGB(26, 14, 2),  RGB(46, 26, 4),  RGB(28, 16, 2),
                  RGB(255, 180, 40), RGB(160, 100, 20), RGB(255, 244, 214), RGB(200, 160, 110),
                  RGB(110, 215, 255), RGB(255, 70, 150) },
    { "CRIMSON",  RGB(10, 1, 4),  RGB(26, 5, 10),  RGB(46, 10, 18), RGB(28, 6, 12),
                  RGB(255, 80, 100), RGB(160, 40, 60), RGB(255, 226, 230), RGB(205, 140, 150),
                  RGB(255, 205, 90), RGB(255, 255, 255) },
    { "SYNTH",    RGB(6, 2, 16),  RGB(20, 6, 38),  RGB(34, 12, 64), RGB(20, 8, 40),
                  RGB(205, 125, 255), RGB(110, 60, 170), RGB(250, 238, 255), RGB(170, 145, 205),
                  RGB(255, 190, 80), RGB(255, 90, 130) },
};
#define NTHEMES ((int)(sizeof THEMES / sizeof THEMES[0]))

static const char *const INDICATORS[RADIO_INDICATORS] = { "LED", "SCOPE", "RADAR" };

int radio_themes(void)
{
    return NTHEMES;
}

const char *radio_theme_name(int theme)
{
    if (theme < 0 || theme >= NTHEMES) {
        theme = 0;
    }
    return THEMES[theme].name;
}

const char *radio_indicator_name(int indicator)
{
    if (indicator < 0 || indicator >= RADIO_INDICATORS) {
        indicator = 0;
    }
    return INDICATORS[indicator];
}

/* the screen's parts. 320x240 is the drawing; 480x320 takes the same parts, wider and taller */
#define W GFX_W
#define H GFX_H
#define BIG (H >= 300)                     // the 3.5": more room between and around
#define M  (BIG ? 6 : 4)                   // margin round the panels
#define R  (BIG ? 6 : 5)                   // their corners

/* the letters for each size: the 480x320 screen takes each a size up */
#if GFX_H >= 300
#define F_TEXT  (&FONT_XL)
#define F_SMALL (&FONT_MEDIUM)
#define F_NUM   (&FONT_HUGE)
#define F_CLOCK (&FONT_CLOCK_L)
#define F_BIG   (&FONT_GIANT)
#else
#define F_TEXT  (&FONT_MEDIUM)
#define F_SMALL (&FONT_SMALL)
#define F_NUM   (&FONT_LARGE)
#define F_CLOCK (&FONT_CLOCK_S)
#define F_BIG   (&FONT_HUGE)
#endif

/* a finger is about 7 mm: rows and targets no smaller than that on the 2.8" glass */
#define LIST_HEAD_H (BIG ? 40 : 32)
#define ROW_H       (BIG ? 36 : 30)
#define SC          2                      // the pixel font's size for labels

void radio_layout(radio_layout_t *l)
{
    l->head_y = 0;
    l->head_h = BIG ? 50 : 38;
    l->title_y = l->head_h;
    l->title_h = BIG ? 48 : 38;
    l->foot_h = BIG ? 74 : 56;
    l->foot_y = H - l->foot_h;
    l->mid_y = l->title_y + l->title_h;
    l->mid_h = l->foot_y - l->mid_y;
    l->list_y = M + LIST_HEAD_H + 4;
    l->list_rowh = ROW_H;
    l->list_rows = (H - l->list_y - M) / ROW_H;
}

void radio_calib_point(int step, int *x, int *y)
{
    int in = BIG ? 26 : 22;
    *x = (step == 1 || step == 2) ? W - 1 - in : in;
    *y = (step >= 2) ? H - 1 - in : in;
}

/* ---- small things to draw with ---- */

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

#define DEG(a) ((a) * 0.0174532925f)

// one row of a rounded rectangle: its span at row yy, the end pixels as much as the arc covers them
static void rrect_row(canvas_t *cv, int x, int y, int w, int h, int r, int yy, uint32_t c, int a)
{
    uint32_t *row = gfx_row(cv, yy);
    if (!row || yy < y || yy >= y + h || a <= 0) {
        return;
    }
    float inset = 0.0f;
    int d = 0;
    if (yy < y + r) {
        d = y + r - yy;
    } else if (yy >= y + h - r) {
        d = yy - (y + h - r) + 1;
    }
    if (d > 0) {
        float dy = d - 0.5f;
        float dx = sqrtf(fmaxf(0.0f, (float)(r * r) - dy * dy));
        inset = (float)r - dx;
    }
    float xl = x + inset, xr = x + w - inset;          // the span, in fractions of a pixel
    int il = (int)ceilf(xl), ir = (int)floorf(xr);
    for (int px = imax(il, 0); px < imin(ir, GFX_PW); px++) {
        row[px] = a >= 256 ? c : gfx_mix(row[px], c, (uint32_t)a);
    }
    int el = il - 1;
    if (el >= 0 && el < GFX_PW) {
        int k = (int)((il - xl) * a);
        if (k > 0) {
            row[el] = gfx_mix(row[el], c, (uint32_t)k);
        }
    }
    if (ir >= 0 && ir < GFX_PW && ir >= il) {
        int k = (int)((xr - ir) * a);
        if (k > 0) {
            row[ir] = gfx_mix(row[ir], c, (uint32_t)k);
        }
    }
}

// a panel: a soft shadow under it, a rim round it, its face a little lighter at the top, a light top edge
static void panel(canvas_t *cv, int x, int y, int w, int h, uint32_t fill, uint32_t rim)
{
    if (!gfx_touches(cv, y - 1, y + h + 3)) {
        return;
    }
    for (int yy = cv->y0; yy < cv->y0 + cv->h; yy++) {
        rrect_row(cv, x + 1, y + 3, w, h, R, yy, RGB(0, 0, 0), 70);
        rrect_row(cv, x - 1, y - 1, w + 2, h + 2, R + 1, yy, rim, 256);
        int k = (yy - y) * 106 / imax(1, h);
        rrect_row(cv, x, y, w, h, R, yy, gfx_mix(rim, fill, (uint32_t)imin(256, 150 + k)), 256);
    }
    gfx_hline(cv, x + R, x + w - R - 1, y, gfx_mix(rim, RGB(255, 255, 255), 48), 150);
}

// a line of text in the window x..x+w-1: as it is when it fits, scrolling round when it does not
static void line_in(canvas_t *cv, const font_t *f, int x, int y, int w, const char *s, uint32_t c, uint32_t now)
{
    if (!gfx_touches(cv, y, y + f->line_h)) {
        return;
    }
    int tw = font_width(f, s);
    if (tw <= w) {
        font_text(cv, f, x, y, s, c, 256);
        return;
    }
    // stands two seconds at its start, then goes round once at 40 pixels a second
    int gap = 40;
    int period = tw + gap;
    uint32_t t = now % (uint32_t)(2000 + period * 25);
    if (t < 2000) {
        font_text_clip(cv, f, x, y, s, c, 256, w);   // standing: its start sharp, its end fading
        return;
    }
    int off = (int)((t - 2000) / 25);
    font_text_span(cv, f, x - off, y, s, c, 256, x, x + w);
    font_text_span(cv, f, x - off + period, y, s, c, 256, x, x + w);
}

// text whose right edge is at x
static void text_right(canvas_t *cv, const font_t *f, int x, int y, const char *s, uint32_t c)
{
    font_text(cv, f, x - font_width(f, s), y, s, c, 256);
}

static void micro_right(canvas_t *cv, int x, int y, const char *s, uint32_t c, int scale)
{
    micro_text(cv, x - micro_width(s, scale), y, s, c, 256, scale);
}

// the theme's colour, warming through its second colour to its alarm as v goes 0..255
static uint32_t ramp(const rtheme_t *t, int v)
{
    if (v < 115) {
        return t->prim;
    }
    if (v < 190) {
        return gfx_mix(t->prim, t->low, (uint32_t)((v - 115) * 256 / 75));
    }
    if (v < 240) {
        return gfx_mix(t->low, t->alarm, (uint32_t)((v - 190) * 256 / 50));
    }
    return t->alarm;
}

// segments in a row, the first `on` lit, each in its place on the ramp, lit from above
static void segments(canvas_t *cv, const rtheme_t *t, int x, int y, int w, int h, int n, int on)
{
    if (n <= 0 || !gfx_touches(cv, y - 1, y + h + 1)) {
        return;
    }
    int gap = n > 24 ? 1 : 2;
    int sw = (w - gap * (n - 1)) / n;
    if (sw < 1) {
        sw = 1;
    }
    if (on > 0) {                                     // a glow round the lit ones
        gfx_fill(cv, x - 1, y - 1, on * (sw + gap) - gap + 2, h + 2, ramp(t, (on - 1) * 255 / imax(1, n - 1)), 40);
    }
    for (int i = 0; i < n; i++) {
        int sx = x + i * (sw + gap);
        if (i < on) {
            uint32_t c = ramp(t, i * 255 / imax(1, n - 1));
            gfx_fill(cv, sx, y, sw, h, c, 256);
            gfx_hline(cv, sx, sx + sw - 1, y, gfx_mix(c, RGB(255, 255, 255), 80), 256);
        } else {
            gfx_fill(cv, sx, y, sw, h, t->glass, 170);
        }
    }
}

// four bars rising, as many lit as the signal is strong, each darker at its foot
static void wifi_bars(canvas_t *cv, int x, int y, int h, int rssi, const rtheme_t *t)
{
    int lit = rssi == 0 ? 0 : rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1;
    for (int i = 0; i < 4; i++) {
        int bh = h * (i + 1) / 4;
        for (int k = 0; k < bh; k++) {
            uint32_t c = i < lit ? gfx_mix(t->quiet, t->prim, (uint32_t)(256 - k * 150 / imax(1, bh))) : t->glass;
            gfx_fill(cv, x + i * 4, y + h - bh + k, 3, 1, c, 256);
        }
    }
}

// the settings' sign: eight teeth round a ring
static void gear(canvas_t *cv, float cx, float cy, float r, uint32_t c, uint32_t hole)
{
    for (int k = 0; k < 8; k++) {
        float a = DEG(k * 45.0f);
        gfx_line(cv, cx + sinf(a) * r * 0.5f, cy - cosf(a) * r * 0.5f, cx + sinf(a) * r, cy - cosf(a) * r, r * 0.42f, c, 256);
    }
    gfx_disc(cv, cx, cy, r * 0.74f, c, 256);
    gfx_disc(cv, cx, cy, r * 0.34f, hole, 256);
}

// "<", the way back, in a heading
static void chevron(canvas_t *cv, int x, int y, int h, uint32_t c)
{
    float w = h * 0.45f;
    gfx_line(cv, x + w, (float)y, (float)x, y + h / 2.0f, 2.0f, c, 256);
    gfx_line(cv, (float)x, y + h / 2.0f, x + w, (float)(y + h), 2.0f, c, 256);
}

/* ---- the geometry the drawing and the hits share ---- */

#define STATE_W (BIG ? 12 : 9)             // the play state's sign, top right
#define STATE_HIT (BIG ? 64 : 48)          // and the width that takes a tap for it
#define MID_PAD (BIG ? 12 : 8)
#define GEAR_R  (BIG ? 13.0f : 10.0f)

static void head_box(const radio_t *r, int *nx, int *nw)
{
    int x = M, w = W - 2 * M;
    int numw = 0;
    if (r->station_no > 0) {
        char no[8];
        snprintf(no, sizeof no, "%02d", r->station_no % 100);
        numw = font_width(F_NUM, no) + 12;
    }
    *nx = x + 8 + numw;
    *nw = (x + w - 8) - STATE_W - 10 - *nx;
}

static void title_box(int *x, int *w)
{
    *x = M + 6;
    *w = W - 2 * M - 12;
}

// the indicator's box and the clock's, inside the middle panel: the golden section, 0.618 and 0.382
static void mid_boxes(const radio_layout_t *l, int *ix, int *iy, int *iw, int *ih, int *cx, int *cw)
{
    int x = M, y = l->mid_y, w = W - 2 * M, h = l->mid_h - M;
    int gw = (int)((w - 3 * MID_PAD) * 0.618f + 0.5f);
    *ix = x + MID_PAD;
    *iy = y + MID_PAD;
    *iw = gw;
    *ih = h - 2 * MID_PAD;
    *cx = *ix + gw + MID_PAD;
    *cw = x + w - MID_PAD - *cx;
}

static void lines_box(int *x, int *w)
{
    *x = 2 * M + 14;
    *w = W - 4 * M - 28;
}

// the footer: its two rows, the volume's bar, the gear
typedef struct {
    int x, y, w, h;          // the panel
    int lx, ly, by;          // the labels' x, the volume row's y, the buffer row's y
    int bx, bw, bh;          // the bars
    float gx, gy;            // the gear's middle
    int rx;                  // where the WiFi and the address end, left of the gear
} foot_t;

static void foot_geometry(const radio_layout_t *l, foot_t *f)
{
    f->x = M;
    f->y = l->foot_y;
    f->w = W - 2 * M;
    f->h = l->foot_h - M;
    f->lx = f->x + 8;
    f->ly = f->y + (BIG ? 12 : 9);
    f->by = f->ly + (BIG ? 26 : 19);
    f->bx = f->lx + micro_width("VOL", SC) + 8;
    f->bw = f->w * (BIG ? 50 : 46) / 100 - (f->bx - f->x);
    f->bh = MICRO_H * SC - SC;
    f->gx = f->x + f->w - 8 - GEAR_R;
    f->gy = f->y + f->h / 2.0f;
    f->rx = (int)(f->gx - GEAR_R - (BIG ? 14 : 10));
}

/* ---- the indicators ---- */

// LED: a column of segments a band on the ramp, a glow round the lit ones, the peak held with a fading
// trail, and under the floor the lit segments again, upside down, fading into the glass
static void draw_led(canvas_t *cv, const radio_t *r, const rtheme_t *t, int x, int y, int w, int h)
{
    int n = r->nbands > 0 ? imin(r->nbands, RADIO_BANDS_MAX) : 12;
    int gap = BIG ? 4 : 3;
    int bw = (w - gap * (n - 1)) / n;
    int seg = BIG ? 4 : 3, step = seg + 1;
    int segs = h * 78 / 100 / step;
    int base = y + segs * step;                         // the floor
    int ox = x + (w - (n * bw + (n - 1) * gap)) / 2;
    int rs = (y + h - base - 2) / step;                 // the reflection's rows
    for (int i = 0; i < n; i++) {
        int v = r->nbands > 0 ? r->bands[i] : 0;
        int on = (v * segs + 127) / 255;
        int pk = r->nbands > 0 ? (r->peaks[i] * segs + 127) / 255 - 1 : -1;
        int bx = ox + i * (bw + gap);
        if (on > 0) {
            gfx_fill(cv, bx - 1, base - on * step - 1, bw + 2, on * step + 1, ramp(t, (on - 1) * 255 / imax(1, segs - 1)), 40);
        }
        for (int s = 0; s < segs; s++) {
            int sy = base - (s + 1) * step;
            if (s < on) {
                uint32_t c = ramp(t, s * 255 / imax(1, segs - 1));
                gfx_fill(cv, bx, sy, bw, seg, c, 256);
                gfx_hline(cv, bx, bx + bw - 1, sy, gfx_mix(c, RGB(255, 255, 255), 70), 256);
            } else if (pk > 0 && s == pk) {
                gfx_fill(cv, bx, sy, bw, seg, t->focus, 256);
            } else if (pk > 0 && s < pk && s >= pk - 3) {
                gfx_fill(cv, bx, sy, bw, seg, t->focus, 90 - (pk - s) * 25);
            } else {
                gfx_fill(cv, bx, sy, bw, seg, t->glass, 150);
            }
        }
        for (int k = 0; k < rs && k < on; k++) {
            gfx_fill(cv, bx, base + 2 + k * step, bw, seg, ramp(t, k * 255 / imax(1, segs - 1)), 90 * (rs - k) / (rs + 1));
        }
    }
    gfx_hline(cv, x, x + w - 1, base, t->glass, 256);
}

// a band's value between the bands: smooth, as a curve through them
static float band_at(const uint8_t *v, int n, float pos)
{
    int i = (int)pos;
    if (i >= n - 1) {
        return v[n - 1];
    }
    float f = pos - (float)i;
    f = f * f * (3.0f - 2.0f * f);
    return v[i] + (v[i + 1] - v[i]) * f;
}

// a spectrum's curve as a line, sampled every few pixels
static void trace(canvas_t *cv, const uint8_t *v, int n, int x, int y, int w, int h, float wd, uint32_t c, int a)
{
    float lx = 0, ly = 0;
    for (int xx = 0; xx < w; xx += 3) {
        int xs = xx + 3 >= w ? w - 1 : xx;
        float px = (float)(x + xs);
        float py = y + h - 1 - band_at(v, n, xs * (n - 1) / (float)(w - 1)) * (h - 2) / 255.0f;
        if (xx > 0) {
            gfx_line(cv, lx, ly, px, py, wd, c, a);
        }
        lx = px;
        ly = py;
        if (xs == w - 1) {
            break;
        }
    }
}

// SCOPE: a dotted graticule, the last frames still glowing faintly like a scope's phosphor, the ground
// under the curve lit and fading to the floor, and the trace itself sharp over a wide soft glow
static void draw_scope(canvas_t *cv, const radio_t *r, const rtheme_t *t, int x, int y, int w, int h)
{
    for (int k = 1; k < 4; k++) {
        for (int xx = x; xx < x + w; xx += 3) {
            gfx_fill(cv, xx, y + h * k / 4, 1, 1, t->glass, 256);
        }
    }
    for (int k = 1; k < 6; k++) {
        for (int yy = y; yy < y + h; yy += 3) {
            gfx_fill(cv, x + (w - 1) * k / 6, yy, 1, 1, t->glass, 256);
        }
    }
    gfx_hline(cv, x, x + w - 1, y + h - 1, t->glass, 256);
    int n = r->nbands > 0 ? imin(r->nbands, RADIO_BANDS_MAX) : 0;
    if (n < 2) {
        return;
    }
    for (int g = 2; g >= 0; g--) {
        trace(cv, r->ghost[g], n, x, y, w, h, 1.2f, t->prim, 40 + (2 - g) * 30);
    }
    int floor_y = y + h - 1;
    for (int yy = imax(cv->y0, y); yy < imin(cv->y0 + cv->h, floor_y); yy++) {
        uint32_t *row = gfx_row(cv, yy);
        for (int xx = 0; xx < w; xx++) {
            float top = floor_y - band_at(r->bands, n, xx * (n - 1) / (float)(w - 1)) * (h - 2) / 255.0f;
            if (yy >= top) {
                int a = 24 + (int)(110.0f * (floor_y - yy) / fmaxf(1.0f, floor_y - top));
                row[x + xx] = gfx_mix(row[x + xx], t->prim, (uint32_t)a);
            }
        }
    }
    trace(cv, r->bands, n, x, y, w, h, 5.0f, t->prim, 60);
    trace(cv, r->bands, n, x, y, w, h, 1.6f, t->focus, 256);
    for (int i = 0; i < n; i++) {
        if (r->peaks[i] > 8) {
            float px = x + i * (w - 1) / (float)(n - 1);
            float py = floor_y - r->peaks[i] * (h - 2) / 255.0f;
            gfx_glow(cv, px, py, 4.0f, ramp(t, r->peaks[i]), 120);
            gfx_disc(cv, px, py, 1.3f, t->focus, 256);
        }
    }
}

// RADAR: the bands as rays round the scope, each bright where the beam has just passed and fading
// round the circle as a radar's echoes do, dim at the ring and bright at the tip; the beam with a
// wedge of light behind it, a faint light in the scope's middle
static void draw_radar(canvas_t *cv, const radio_t *r, const rtheme_t *t, int x, int y, int w, int h)
{
    float cx = x + w / 2.0f, cy = y + h / 2.0f;
    float rr = imin(w, h) / 2.0f - 2.0f;
    float r0 = rr * 0.28f;
    if (!gfx_touches(cv, (int)(cy - rr) - 4, (int)(cy + rr) + 4)) {
        return;
    }
    gfx_glow(cv, cx, cy, rr, t->prim, 22);
    gfx_ring(cv, cx, cy, rr, 1.2f, t->glass, 256);
    gfx_ring(cv, cx, cy, rr * 0.64f, 1.0f, t->glass, 150);
    gfx_ring(cv, cx, cy, r0, 1.0f, t->glass, 150);
    for (int k = 0; k < 12; k++) {                     // the bearing ticks
        float a = DEG(k * 30.0f);
        gfx_line(cv, cx + sinf(a) * (rr - 4), cy - cosf(a) * (rr - 4), cx + sinf(a) * rr, cy - cosf(a) * rr, 1.0f, t->glass, 256);
    }
    float sweep = (float)(r->now_ms % 3600u) * 0.1f;
    for (int k = 30; k >= 1; k--) {
        float a = DEG(sweep - k * 2.0f);
        float f = 1.0f - k / 31.0f;
        gfx_line(cv, cx, cy, cx + sinf(a) * rr, cy - cosf(a) * rr, 1.8f, t->prim, (int)(150 * f * f));
    }
    gfx_line(cv, cx, cy, cx + sinf(DEG(sweep)) * rr, cy - cosf(DEG(sweep)) * rr, 1.6f, t->focus, 230);
    int n = r->nbands > 0 ? imin(r->nbands, RADIO_BANDS_MAX) : 0;
    for (int i = 0; i < n; i++) {
        float ang = i * 360.0f / n;
        float behind = fmodf(sweep - ang + 720.0f, 360.0f) / 360.0f;   // 0 just swept .. 1 about to be
        float glow = 1.0f - behind;
        int a = 80 + (int)(176 * glow * glow);
        float s = sinf(DEG(ang)), c = cosf(DEG(ang));
        int v = r->bands[i];
        if (v > 4) {
            float len = r0 + v * (rr - r0) / 255.0f;
            float mid = (r0 + len) / 2.0f;
            uint32_t col = ramp(t, v);
            float wd = BIG ? 4.0f : 3.0f;
            gfx_line(cv, cx + s * r0, cy - c * r0, cx + s * mid, cy - c * mid, wd, col, a / 2);
            gfx_line(cv, cx + s * mid, cy - c * mid, cx + s * len, cy - c * len, wd, col, a);
            gfx_glow(cv, cx + s * len, cy - c * len, BIG ? 6.0f : 5.0f, col, a / 2);
        }
        if (r->peaks[i] > 8) {
            float pl = r0 + r->peaks[i] * (rr - r0) / 255.0f;
            gfx_disc(cv, cx + s * pl, cy - c * pl, 1.4f, t->focus, a);
        }
    }
    gfx_glow(cv, cx, cy, 6.0f, t->prim, 160);
    gfx_disc(cv, cx, cy, 2.2f, t->focus, 256);
}

/* ---- the player ---- */

static void draw_head(canvas_t *cv, const radio_t *r, const rtheme_t *t, const radio_layout_t *l)
{
    if (!gfx_touches(cv, l->head_y, l->head_y + l->head_h)) {
        return;
    }
    int x = M, y = l->head_y + M, w = W - 2 * M, h = l->head_h - M;
    panel(cv, x, y, w, h, t->panel, t->glass);

    // its number, big, in the theme's colour
    if (r->station_no > 0) {
        char no[8];
        snprintf(no, sizeof no, "%02d", r->station_no % 100);
        font_text(cv, F_NUM, x + 8, y + (h - F_NUM->line_h) / 2 + 1, no, t->prim, 256);
    }

    // what the stream is, and how it plays, on the right
    char what[24];
    if (r->bitrate > 0) {
        snprintf(what, sizeof what, "%s %dK", r->codec[0] ? r->codec : "", r->bitrate);
    } else {
        snprintf(what, sizeof what, "%s", r->codec);
    }
    int right = x + w - 8;
    // the state: a triangle playing, a square stopped, three dots that walk while it connects
    int sy = y + 7, sh = STATE_W;
    if (r->playing == 1) {
        gfx_tri(cv, (float)(right - sh + 1), (float)sy, (float)(right - sh + 1), (float)(sy + sh), (float)(right + 1),
                sy + sh / 2.0f, t->prim, 256);
    } else if (r->playing == 2) {
        int k = (int)(r->now_ms / 300) % 3;
        for (int i = 0; i < 3; i++) {
            gfx_fill(cv, right - sh + i * 4, sy + sh / 2 - 1, 2, 2, i == k ? t->prim : t->quiet, 256);
        }
    } else {
        gfx_fill(cv, right - sh + 1, sy, sh, sh, t->quiet, 256);
    }
    text_right(cv, F_SMALL, right + 1, y + h - F_SMALL->line_h - (BIG ? 6 : 2), what, t->quiet);

    // the station's name, between them
    int nx, nwidth;
    head_box(r, &nx, &nwidth);
    line_in(cv, F_TEXT, nx, y + (h - F_TEXT->line_h) / 2, nwidth, r->station[0] ? r->station : "-", t->focus, r->now_ms);
}

static void draw_title(canvas_t *cv, const radio_t *r, const rtheme_t *t, const radio_layout_t *l)
{
    if (!gfx_touches(cv, l->title_y, l->title_y + l->title_h)) {
        return;
    }
    int x, w;
    title_box(&x, &w);
    int y = l->title_y + (BIG ? 8 : 5);
    line_in(cv, F_TEXT, x, y, w, r->title1, t->focus, r->now_ms);
    if (r->title2[0]) {
        line_in(cv, F_SMALL, x, y + F_TEXT->line_h + (BIG ? 6 : 3), w, r->title2, t->dim, r->now_ms);
    }
}

// the clock, its date and the weather as one block, in the middle of the clock's golden part
static void draw_clock(canvas_t *cv, const radio_t *r, const rtheme_t *t, int x, int y, int w, int h)
{
    char hm[8];
    if (r->hh >= 0) {
        snprintf(hm, sizeof hm, "%02d:%02d", r->hh % 100, r->mm % 100);   /* % 100: the compiler sees it fits */
    } else {
        snprintf(hm, sizeof hm, "--:--");
    }
    const font_t *f = F_CLOCK;
    int sc = BIG ? 3 : 2;
    int gap = BIG ? 8 : 5;
    int block = f->line_h + gap + MICRO_H * sc;
    if (r->weather[0]) {
        block += gap + 2 + F_SMALL->line_h;
    }
    int top = y + (h - block) / 2;
    int tw = font_width(f, hm);
    font_text(cv, f, x + (w - tw) / 2, top, hm, t->focus, 256);
    int dy = top + f->line_h + gap;
    if (r->date[0]) {
        micro_text(cv, x + (w - micro_width(r->date, sc)) / 2, dy, r->date, t->quiet, 256, sc);
    }
    if (r->weather[0]) {
        int wy = dy + MICRO_H * sc + gap + 2;
        int ww = font_width(F_SMALL, r->weather);
        if (ww <= w) {
            font_text(cv, F_SMALL, x + (w - ww) / 2, wy, r->weather, t->dim, 256);
        } else {
            line_in(cv, F_SMALL, x, wy, w, r->weather, t->dim, r->now_ms);
        }
    }
}

static void draw_volume_big(canvas_t *cv, const radio_t *r, const rtheme_t *t, int x, int y, int w, int h)
{
    gfx_fill(cv, x, y, w, h, t->panel, 236);
    char v[8];
    snprintf(v, sizeof v, "%d", r->volume);
    const font_t *f = F_BIG;
    int vw = font_width(f, v);
    int top = y + (h - f->line_h - 26) / 2;
    micro_text(cv, x + 12, y + 8, "VOLUME", t->quiet, 256, SC);
    gfx_glow(cv, x + w / 2.0f, top + f->line_h / 2.0f, f->line_h * 1.2f, t->prim, 40);
    font_text(cv, f, x + (w - vw) / 2, top, v, t->focus, 256);
    segments(cv, t, x + 14, top + f->line_h + 10, w - 28, BIG ? 14 : 10, r->volume_max > 0 ? r->volume_max : 21, r->volume);
}

static void draw_mid(canvas_t *cv, const radio_t *r, const rtheme_t *t, const radio_layout_t *l)
{
    if (!gfx_touches(cv, l->mid_y - 1, l->mid_y + l->mid_h + 3)) {
        return;
    }
    int x = M, y = l->mid_y, w = W - 2 * M, h = l->mid_h - M;
    panel(cv, x, y, w, h, t->panel, t->glass);
    if (r->volume_big) {
        draw_volume_big(cv, r, t, x + 1, y + 1, w - 2, h - 2);
        return;
    }
    int ix, iy, iw, ih, cx, cw;
    mid_boxes(l, &ix, &iy, &iw, &ih, &cx, &cw);
    switch (r->indicator) {
        case RADIO_IND_SCOPE:
            draw_scope(cv, r, t, ix, iy, iw, ih);
            break;
        case RADIO_IND_RADAR:
            draw_radar(cv, r, t, ix, iy, iw, ih);
            break;
        default:
            draw_led(cv, r, t, ix, iy, iw, ih);
            break;
    }
    if (r->indicator_named) {                  // its name for a moment after a tap changed it
        const char *name = radio_indicator_name(r->indicator);
        gfx_fill(cv, ix, iy, micro_width(name, SC) + 8, MICRO_H * SC + 6, t->panel, 220);
        micro_text(cv, ix + 4, iy + 3, name, t->focus, 256, SC);
    }
    // the golden section's line: a hairline fading out at both ends
    int dx = cx - MID_PAD / 2 - 1;
    for (int yy = imax(cv->y0, iy); yy < imin(cv->y0 + cv->h, iy + ih); yy++) {
        float f = sinf(3.14159f * (yy - iy) / (float)ih);
        gfx_fill(cv, dx, yy, 1, 1, t->glass, (int)(256 * f));
    }
    draw_clock(cv, r, t, cx, iy, cw, ih);
}

static void draw_foot(canvas_t *cv, const radio_t *r, const rtheme_t *t, const radio_layout_t *l)
{
    if (!gfx_touches(cv, l->foot_y - 1, l->foot_y + l->foot_h + 3)) {
        return;
    }
    foot_t f;
    foot_geometry(l, &f);
    panel(cv, f.x, f.y, f.w, f.h, t->panel, t->glass);

    // the volume: the bar takes a tap where the volume should go
    char v[8];
    snprintf(v, sizeof v, "%d", r->volume);
    micro_text(cv, f.lx, f.ly, "VOL", t->quiet, 256, SC);
    segments(cv, t, f.bx, f.ly, f.bw, f.bh, r->volume_max > 0 ? r->volume_max : 21, r->volume);
    font_text(cv, F_SMALL, f.bx + f.bw + 6, f.ly - (F_SMALL->ascent - MICRO_H * SC) - 1, v, t->focus, 256);

    // the buffer: how far ahead of the speaker the stream is, lighter towards its edge
    micro_text(cv, f.lx, f.by, "BUF", t->quiet, 256, SC);
    int pct = imax(0, imin(100, r->buffer_pct));
    int bh = f.bh - 2 * SC;
    gfx_fill(cv, f.bx, f.by + 1, f.bw, bh, t->glass, 170);
    int fill = f.bw * pct / 100;
    uint32_t hi = pct < 20 ? t->low : t->prim;
    for (int xx = 0; xx < fill; xx++) {
        gfx_fill(cv, f.bx + xx, f.by + 1, 1, bh, gfx_mix(t->quiet, hi, (uint32_t)(80 + xx * 176 / imax(1, fill))), 256);
    }
    if (fill > 0) {
        gfx_glow(cv, (float)(f.bx + fill), f.by + 1 + bh / 2.0f, bh * 1.2f, hi, 90);
    }

    // WiFi and the address, left of the gear
    char db[12];
    if (r->rssi != 0) {
        snprintf(db, sizeof db, "%d", r->rssi);
    } else {
        snprintf(db, sizeof db, "--");
    }
    text_right(cv, F_SMALL, f.rx, f.ly - 2, db, t->focus);
    wifi_bars(cv, f.rx - font_width(F_SMALL, db) - 22, f.ly - 1, BIG ? 14 : 10, r->rssi, t);
    text_right(cv, F_SMALL, f.rx, f.by - 2, r->ip[0] ? r->ip : "no address", t->dim);
    gear(cv, f.gx, f.gy, GEAR_R, t->quiet, t->panel);
}

/* ---- the lists: stations and settings ---- */

static void list_head(canvas_t *cv, const rtheme_t *t, const char *head, const char *right)
{
    int hh = LIST_HEAD_H;
    if (!gfx_touches(cv, M, M + hh)) {
        return;
    }
    panel(cv, M, M, W - 2 * M, hh, t->panel, t->glass);
    chevron(cv, M + 10, M + hh / 2 - (BIG ? 8 : 6), BIG ? 16 : 12, t->prim);
    micro_text(cv, M + 26, M + (hh - MICRO_H * SC) / 2, head, t->prim, 256, SC);
    if (right && right[0]) {
        micro_right(cv, W - M - 10, M + (hh - MICRO_H * SC) / 2, right, t->quiet, SC);
    }
}

static void draw_list(canvas_t *cv, const radio_t *r, const rtheme_t *t)
{
    radio_layout_t l;
    radio_layout(&l);
    char total[16] = "";
    if (r->list_total > 0) {
        snprintf(total, sizeof total, "%d", r->list_total);
    }
    list_head(cv, t, r->head[0] ? r->head : "STATIONS", total);
    const font_t *f = F_TEXT;
    int rows = imin(r->list_rows, l.list_rows);
    for (int i = 0; i < rows; i++) {
        int y = l.list_y + i * l.list_rowh;
        if (!gfx_touches(cv, y, y + l.list_rowh)) {
            continue;
        }
        int picked = i == r->list_pick;
        if (picked) {
            panel(cv, M, y, W - 2 * M, l.list_rowh - 2, t->glass, t->prim);
            gfx_fill(cv, M + 2, y + 4, 3, l.list_rowh - 10, t->prim, 256);
        } else {
            gfx_hline(cv, M + 8, W - M - 8, y + l.list_rowh - 2, t->glass, 200);
        }
        char no[8];
        snprintf(no, sizeof no, "%d", r->list_no[i]);
        int ty = y + (l.list_rowh - 2 - f->line_h) / 2;
        text_right(cv, F_SMALL, M + 40, ty + (f->ascent - F_SMALL->ascent), no, picked ? t->prim : t->quiet);
        line_in(cv, f, M + 50, ty, W - 2 * M - 60, r->list[i], picked ? t->focus : t->dim, picked ? r->now_ms : 0);
    }
    // where these rows are in the whole list, a thin mark on the right edge
    if (r->list_total > rows && rows > 0 && r->list_no[0] > 0) {
        int top = l.list_y, span = rows * l.list_rowh;
        int my = top + span * (r->list_no[0] - 1) / r->list_total;
        int mh = imax(6, span * rows / r->list_total);
        gfx_fill(cv, W - M - 3, my, 2, mh, t->quiet, 256);
    }
}

static void draw_settings(canvas_t *cv, const radio_t *r, const rtheme_t *t)
{
    radio_layout_t l;
    radio_layout(&l);
    list_head(cv, t, r->head[0] ? r->head : "SETTINGS", "");
    int rows = imin(r->set_rows, l.list_rows);
    for (int i = 0; i < rows; i++) {
        int y = l.list_y + i * l.list_rowh;
        if (!gfx_touches(cv, y, y + l.list_rowh)) {
            continue;
        }
        panel(cv, M, y, W - 2 * M, l.list_rowh - 3, t->panel, t->glass);
        micro_text(cv, M + 12, y + (l.list_rowh - 3 - MICRO_H * SC) / 2, r->set_name[i], t->quiet, 256, SC);
        int ty = y + (l.list_rowh - 3 - F_TEXT->line_h) / 2;
        text_right(cv, F_TEXT, W - M - 12, ty, r->set_value[i], t->prim);
    }
}

/* ---- the station's numbers ---- */

static void stat_cell(canvas_t *cv, const rtheme_t *t, int x, int y, int w, const char *label, const char *value)
{
    micro_text(cv, x, y + (F_SMALL->ascent - MICRO_H * SC + 1), label, t->quiet, 256, SC);
    text_right(cv, F_SMALL, x + w, y, value, t->focus);
}

static void draw_stats(canvas_t *cv, const radio_t *r, const rtheme_t *t)
{
    // the heading: back, and the station
    int hh = LIST_HEAD_H;
    if (gfx_touches(cv, M, M + hh)) {
        panel(cv, M, M, W - 2 * M, hh, t->panel, t->glass);
        chevron(cv, M + 10, M + hh / 2 - (BIG ? 8 : 6), BIG ? 16 : 12, t->prim);
        line_in(cv, F_TEXT, M + 28, M + (hh - F_TEXT->line_h) / 2, W - 2 * M - 40, r->station[0] ? r->station : "-",
                t->focus, r->now_ms);
    }

    char v[12][24];
    snprintf(v[0], sizeof v[0], "%s", r->codec[0] ? r->codec : "-");
    if (r->samplerate > 0) {
        snprintf(v[1], sizeof v[1], "%d.%d kHz", r->samplerate / 1000, (r->samplerate % 1000) / 100);
    } else {
        snprintf(v[1], sizeof v[1], "-");
    }
    snprintf(v[2], sizeof v[2], r->bitrate > 0 ? "%d kbit/s" : "-", r->bitrate);
    snprintf(v[3], sizeof v[3], r->channels > 0 ? "%d" : "-", r->channels);
    snprintf(v[4], sizeof v[4], "%d%%", imax(0, imin(100, r->buffer_pct)));
    snprintf(v[5], sizeof v[5], "%u:%02u", (unsigned)(r->stream_s / 60), (unsigned)(r->stream_s % 60));
    snprintf(v[6], sizeof v[6], "%d kbit/s", r->kbps);
    if (r->total_kb >= 1024) {
        snprintf(v[7], sizeof v[7], "%u.%u MB", (unsigned)(r->total_kb / 1024), (unsigned)(r->total_kb % 1024 * 10 / 1024));
    } else {
        snprintf(v[7], sizeof v[7], "%u KB", (unsigned)r->total_kb);
    }
    snprintf(v[8], sizeof v[8], "%d", r->reconnects);
    snprintf(v[9], sizeof v[9], r->rssi != 0 ? "%d dBm" : "-", r->rssi);
    snprintf(v[10], sizeof v[10], "%d/%d KB", r->heap_kb, r->heap_big_kb);
    snprintf(v[11], sizeof v[11], "%s", r->ip[0] ? r->ip : "-");
    static const char *const LABELS[12] = { "CODEC", "RATE", "BITRATE", "CHANNELS", "BUFFER", "PLAYING",
                                            "IN NOW", "IN TOTAL", "RECONNECTS", "WIFI", "MEMORY", "ADDRESS" };
    int rowh = BIG ? 23 : 17;
    int y0 = M + hh + (BIG ? 10 : 6);
    int colw = (W - 2 * M - 36) / 2;
    for (int i = 0; i < 12; i++) {
        int cx = M + 12 + (i % 2) * (colw + 12);
        int cy = y0 + (i / 2) * rowh;
        if (gfx_touches(cv, cy, cy + rowh)) {
            stat_cell(cv, t, cx, cy, colw, LABELS[i], v[i]);
        }
    }

    // what came in, a second a bar, the stream's own rate as a line across
    int gy = y0 + 6 * rowh + (BIG ? 8 : 4);
    int gh = H - M - gy;
    if (!gfx_touches(cv, gy, gy + gh) || gh < 20) {
        return;
    }
    panel(cv, M, gy, W - 2 * M, gh, t->panel, t->glass);
    micro_text(cv, M + 10, gy + 6, "TRAFFIC", t->quiet, 256, SC);
    int top = gy + 8 + MICRO_H * SC, bottom = gy + gh - 6;
    int ph = bottom - top;
    int peak = imax(r->bitrate * 5 / 4, 64);
    for (int i = 0; i < r->traffic_n && i < RADIO_TRAFFIC_N; i++) {
        peak = imax(peak, r->traffic[i]);
    }
    char pk[16];
    snprintf(pk, sizeof pk, "%d", peak);
    micro_right(cv, W - M - 10, gy + 6, pk, t->quiet, SC);
    int gx = M + 10, gw = W - 2 * M - 20;
    int bw = imax(1, gw / RADIO_TRAFFIC_N - 1);
    for (int i = 0; i < r->traffic_n && i < RADIO_TRAFFIC_N; i++) {
        int k = RADIO_TRAFFIC_N - r->traffic_n + i;            // the newest at the right
        int bh = r->traffic[i] * ph / peak;
        gfx_fill(cv, gx + k * (bw + 1), bottom - bh, bw, bh, r->traffic[i] < r->bitrate * 9 / 10 ? t->low : t->prim, 256);
    }
    if (r->bitrate > 0) {
        int ly = bottom - r->bitrate * ph / peak;
        for (int xx = gx; xx < gx + gw; xx += 6) {
            gfx_hline(cv, xx, xx + 2, ly, t->focus, 200);
        }
    }
}

/* ---- the touch calibration ---- */

static void draw_calib(canvas_t *cv, const radio_t *r, const rtheme_t *t)
{
    int px, py;
    radio_calib_point(r->calib_step, &px, &py);
    float arm = BIG ? 18.0f : 14.0f;
    gfx_ring(cv, (float)px, (float)py, arm * 0.55f, 1.5f, t->prim, 256);
    gfx_line(cv, px - arm, (float)py, px + arm, (float)py, 1.2f, t->focus, 256);
    gfx_line(cv, (float)px, py - arm, (float)px, py + arm, 1.2f, t->focus, 256);
    gfx_glow(cv, (float)px, (float)py, arm * 1.4f, t->prim, 90);
    const char *head = "TOUCH THE CROSS";
    int sc = BIG ? 3 : 2;
    int y = H / 2 - MICRO_H * sc - 6;
    micro_text(cv, (W - micro_width(head, sc)) / 2, y, head, t->prim, 256, sc);
    char step[24];
    snprintf(step, sizeof step, "%d of 4", r->calib_step + 1);
    int sw = font_width(F_TEXT, step);
    font_text(cv, F_TEXT, (W - sw) / 2, y + MICRO_H * sc + 8, step, t->focus, 256);
    if (r->calib_left_s > 0) {
        char left[48];
        snprintf(left, sizeof left, "with no touch it is skipped in %d s", r->calib_left_s);
        int lw = font_width(F_SMALL, left);
        font_text(cv, F_SMALL, (W - lw) / 2, y + MICRO_H * sc + 12 + F_TEXT->line_h, left, t->dim, 256);
    }
}

/* ---- the other screens ---- */

static void draw_lines(canvas_t *cv, const radio_t *r, const rtheme_t *t, const char *head)
{
    int bw = W - 4 * M, bh = H * 2 / 3;
    int bx = 2 * M, by = (H - bh) / 2;
    panel(cv, bx, by, bw, bh, t->panel, t->glass);
    int sc = BIG ? 3 : 2;
    micro_text(cv, bx + 14, by + 14, head, t->prim, 256, sc);
    int y = by + 14 + MICRO_H * sc + (BIG ? 18 : 12);
    int lx, lw;
    lines_box(&lx, &lw);
    for (int i = 0; i < 4; i++) {
        if (!r->line[i][0]) {
            continue;
        }
        line_in(cv, i == 3 ? F_SMALL : F_TEXT, lx, y, lw, r->line[i], i == 3 ? t->dim : t->focus, r->now_ms);
        y += F_TEXT->line_h + (BIG ? 10 : 6);
    }
}

static void draw_boot(canvas_t *cv, const radio_t *r, const rtheme_t *t)
{
    int sc = BIG ? 5 : 4;
    const char *name = "FUSIONEDGE";
    int nw = micro_width(name, sc);
    int y = H / 2 - MICRO_H * sc;
    micro_text(cv, (W - nw) / 2, y, name, t->prim, 256, sc);
    // a light that runs under it, as the radar's beam does
    int ly = y + MICRO_H * sc + 8;
    gfx_hline(cv, (W - nw) / 2, (W + nw) / 2, ly, t->glass, 256);
    int k = (int)(r->now_ms / 8) % (nw + 40) - 20;
    for (int i = -20; i <= 20; i++) {
        int px = (W - nw) / 2 + k + i;
        if (px >= (W - nw) / 2 && px <= (W + nw) / 2) {
            gfx_fill(cv, px, ly - 1, 1, 3, t->prim, 256 - abs(i) * 12);
        }
    }
    const font_t *f = F_SMALL;
    if (r->line[0][0]) {
        int lw = font_width(f, r->line[0]);
        font_text(cv, f, (W - lw) / 2, ly + 12, r->line[0], t->dim, 256);
    }
    if (r->line[1][0]) {
        int lw = font_width(f, r->line[1]);
        font_text(cv, f, (W - lw) / 2, ly + 12 + f->line_h + 4, r->line[1], t->quiet, 256);
    }
}

void radio_draw(canvas_t *cv, const radio_t *r)
{
    if (r->screen == RADIO_BLANK) {                // asleep: black, the backlight is the firmware's
        gfx_fill(cv, 0, cv->y0, W, cv->h, 0, 256);
        return;
    }
    const rtheme_t *t = &THEMES[(r->theme >= 0 && r->theme < NTHEMES) ? r->theme : 0];
    // the ground: the theme's dark, a little lighter towards the bottom
    for (int y = cv->y0; y < cv->y0 + cv->h; y++) {
        uint32_t c = gfx_mix(t->bg0, t->bg1, (uint32_t)(y * 256 / H));
        gfx_fill(cv, 0, y, W, 1, c, 256);
    }
    radio_layout_t l;
    radio_layout(&l);
    switch (r->screen) {
        case RADIO_LIST:
            draw_list(cv, r, t);
            break;
        case RADIO_SETTINGS:
            draw_settings(cv, r, t);
            break;
        case RADIO_STATS:
            draw_stats(cv, r, t);
            break;
        case RADIO_CALIB:
            draw_calib(cv, r, t);
            break;
        case RADIO_SETUP:
            draw_lines(cv, r, t, "WIFI SETUP");
            break;
        case RADIO_MESSAGE:
            draw_lines(cv, r, t, r->head[0] ? r->head : "FUSIONEDGE");
            break;
        case RADIO_BOOT:
            draw_boot(cv, r, t);
            break;
        default:
            draw_head(cv, r, t, &l);
            draw_title(cv, r, t, &l);
            draw_mid(cv, r, t, &l);
            draw_foot(cv, r, t, &l);
            break;
    }
    // a finger just went down: a ring spreading from it, fading
    if (r->touch_age >= 0 && r->touch_age < RADIO_TOUCH_MS) {
        float k = r->touch_age / (float)RADIO_TOUCH_MS;
        float rad = (BIG ? 10.0f : 8.0f) + k * (BIG ? 34.0f : 26.0f);
        int a = (int)(230 * (1.0f - k));
        gfx_glow(cv, (float)r->touch_x, (float)r->touch_y, rad, t->prim, a / 3);
        gfx_ring(cv, (float)r->touch_x, (float)r->touch_y, rad, 2.0f, t->focus, a);
    }
}

/* ---- what to draw again ---- */

static int differ(const char *a, const char *b)
{
    return strcmp(a, b) != 0;
}

#define SAME(f) (memcmp(&a->f, &b->f, sizeof a->f) == 0)

int radio_dirty(const radio_t *a, const radio_t *b)
{
    if (a->screen != b->screen || a->theme != b->theme) {
        return RADIO_ALL;
    }
    switch (b->screen) {
        case RADIO_PLAYER: {
            int d = 0;
            if (differ(a->station, b->station) || a->station_no != b->station_no || differ(a->codec, b->codec) ||
                a->bitrate != b->bitrate || a->playing != b->playing) {
                d |= RADIO_HEAD;
            }
            if (differ(a->title1, b->title1) || differ(a->title2, b->title2)) {
                d |= RADIO_TITLE;
            }
            if (a->volume_big != b->volume_big || (b->volume_big && a->volume != b->volume) || a->hh != b->hh ||
                a->mm != b->mm || differ(a->date, b->date) || differ(a->weather, b->weather) || a->nbands != b->nbands ||
                !SAME(bands) || !SAME(peaks) || !SAME(ghost) || a->indicator != b->indicator || a->indicator_named != b->indicator_named) {
                d |= RADIO_MID;
            }
            if (a->volume != b->volume || a->volume_max != b->volume_max || a->buffer_pct != b->buffer_pct ||
                a->rssi != b->rssi || differ(a->ip, b->ip)) {
                d |= RADIO_FOOT;
            }
            return d;
        }
        case RADIO_LIST:
            return (a->list_rows != b->list_rows || a->list_pick != b->list_pick || a->list_total != b->list_total ||
                    differ(a->head, b->head) || !SAME(list) || !SAME(list_no)) ? RADIO_ALL : 0;
        case RADIO_SETTINGS:
            return (a->set_rows != b->set_rows || !SAME(set_name) || !SAME(set_value)) ? RADIO_ALL : 0;
        case RADIO_STATS:
            return (differ(a->station, b->station) || differ(a->codec, b->codec) || a->samplerate != b->samplerate ||
                    a->bitrate != b->bitrate || a->channels != b->channels || a->buffer_pct != b->buffer_pct ||
                    a->stream_s != b->stream_s || a->kbps != b->kbps || a->total_kb != b->total_kb ||
                    a->reconnects != b->reconnects || a->rssi != b->rssi || a->heap_kb != b->heap_kb ||
                    a->heap_big_kb != b->heap_big_kb || differ(a->ip, b->ip) || a->traffic_n != b->traffic_n ||
                    !SAME(traffic)) ? RADIO_ALL : 0;
        case RADIO_CALIB:
            return (a->calib_step != b->calib_step || a->calib_left_s != b->calib_left_s) ? RADIO_ALL : 0;
        case RADIO_BLANK:
            return 0;
        default:
            return memcmp(a->line, b->line, sizeof a->line) != 0 || differ(a->head, b->head) ? RADIO_ALL : 0;
    }
}

static int radio_moving_only(const radio_t *r);

int radio_moving(const radio_t *r)
{
    if (r->touch_age >= 0 && r->touch_age < RADIO_TOUCH_MS + 60) {
        if (r->screen != RADIO_PLAYER) {
            return RADIO_ALL;
        }
        // the parts the touch's ring crosses
        radio_layout_t l;
        radio_layout(&l);
        int ya = r->touch_y - 50, yb = r->touch_y + 50, m = 0;
        if (ya < l.head_y + l.head_h) {
            m |= RADIO_HEAD;
        }
        if (yb >= l.title_y && ya < l.title_y + l.title_h) {
            m |= RADIO_TITLE;
        }
        if (yb >= l.mid_y && ya < l.mid_y + l.mid_h) {
            m |= RADIO_MID;
        }
        if (yb >= l.foot_y) {
            m |= RADIO_FOOT;
        }
        return m | (radio_moving_only(r));
    }
    return radio_moving_only(r);
}

static int radio_moving_only(const radio_t *r)
{
    switch (r->screen) {
        case RADIO_PLAYER: {
            int m = 0;
            int x, w;
            head_box(r, &x, &w);
            if (r->playing == 2 || font_width(F_TEXT, r->station) > w) {
                m |= RADIO_HEAD;
            }
            title_box(&x, &w);
            if (font_width(F_TEXT, r->title1) > w || font_width(F_SMALL, r->title2) > w) {
                m |= RADIO_TITLE;
            }
            radio_layout_t l;
            radio_layout(&l);
            int ix, iy, iw, ih, cx, cw;
            mid_boxes(&l, &ix, &iy, &iw, &ih, &cx, &cw);
            if (!r->volume_big && font_width(F_SMALL, r->weather) > cw) {
                m |= RADIO_MID;
            }
            return m;
        }
        case RADIO_LIST:
            return (r->list_pick >= 0 && r->list_pick < r->list_rows &&
                    font_width(F_TEXT, r->list[r->list_pick]) > W - 2 * M - 60) ? RADIO_ALL : 0;
        case RADIO_STATS:
            return font_width(F_TEXT, r->station) > W - 2 * M - 40 ? RADIO_ALL : 0;
        case RADIO_BOOT:
            return RADIO_ALL;
        case RADIO_SETUP:
        case RADIO_MESSAGE: {
            int x, w;
            lines_box(&x, &w);
            for (int i = 0; i < 4; i++) {
                if (font_width(i == 3 ? F_SMALL : F_TEXT, r->line[i]) > w) {
                    return RADIO_ALL;
                }
            }
            return 0;
        }
        default:
            return 0;
    }
}

/* ---- where a finger lands ---- */

int radio_hit(const radio_t *r, int x, int y, int *arg)
{
    int dummy;
    if (!arg) {
        arg = &dummy;
    }
    *arg = 0;
    radio_layout_t l;
    radio_layout(&l);
    switch (r->screen) {
        case RADIO_PLAYER: {
            if (y < l.head_y + l.head_h) {
                return x >= W - M - STATE_HIT ? RADIO_HIT_PLAYSTOP : RADIO_HIT_STATION;
            }
            if (y < l.title_y + l.title_h) {
                return RADIO_HIT_TITLE;
            }
            if (y < l.mid_y + l.mid_h) {
                if (r->volume_big) {
                    return RADIO_HIT_NONE;
                }
                int ix, iy, iw, ih, cx, cw;
                mid_boxes(&l, &ix, &iy, &iw, &ih, &cx, &cw);
                return x < cx - MID_PAD / 2 ? RADIO_HIT_INDICATOR : RADIO_HIT_CLOCK;
            }
            foot_t f;
            foot_geometry(&l, &f);
            if (x >= f.rx + 4) {
                return RADIO_HIT_SETTINGS;
            }
            // the volume's row, its bar a little wider than drawn: a finger is not a pixel
            if (y < (f.ly + f.by) / 2 + f.bh / 2 && x >= f.bx - 10 && x <= f.bx + f.bw + 10) {
                int vmax = r->volume_max > 0 ? r->volume_max : 21;
                int v = ((x - f.bx) * vmax + f.bw / 2) / f.bw;
                *arg = imax(0, imin(vmax, v));
                return RADIO_HIT_VOLUME;
            }
            return RADIO_HIT_TITLE;               // the buffer and the address: the numbers
        }
        case RADIO_LIST:
        case RADIO_SETTINGS: {
            if (y < l.list_y) {
                return RADIO_HIT_BACK;
            }
            int row = (y - l.list_y) / l.list_rowh;
            int rows = r->screen == RADIO_LIST ? r->list_rows : r->set_rows;
            if (row < 0 || row >= rows || row >= l.list_rows) {
                return RADIO_HIT_NONE;
            }
            *arg = row;
            return RADIO_HIT_ROW;
        }
        case RADIO_STATS:
            return RADIO_HIT_BACK;
        default:
            return RADIO_HIT_NONE;
    }
}
