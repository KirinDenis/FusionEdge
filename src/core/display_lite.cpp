// FusionEdge's Display, the new screen (LITE_UI): every board without PSRAM, and a board with PSRAM
// built with -D LITE_UI.
//
// display.cpp builds the screen from LovyanGFX sprites, which want more RAM than the classic ESP32
// has beside WiFi and the audio. Here the same class draws with src/lite instead: radio_ui.c draws
// the screen a few rows at a time into a small buffer, the rows go to the panel, and only the parts
// that changed are drawn again. The screen and the touch are its own: the player, a station list,
// the station's numbers, settings and, for a resistive touch, a calibration (radio_ui.h). The
// requests from the player, the encoders, the IR remote and the web page still come through the same
// queue; what they show is read from config, the player and WiFi each frame.
#include "options.h"
#ifdef LITE_UI
#include "Arduino.h"
#include "WiFi.h"
#include <Preferences.h>
#include "config.h"
#include "display.h"
#include "player.h"
#include "network.h"
#include "netserver.h"
#include "timekeeper.h"
#include "spectrumAnalyzer.h"
#include "../displays/display_select.h"
#include "../lite/radio_ui.h"
#include "freertos/semphr.h"

Display display;
DspCore dsp;

#ifndef LITE_STRIP
#ifdef BOARD_NO_PSRAM
#define LITE_STRIP 2                     // rows drawn at once: LITE_SCREEN_W * 2 * 6 bytes of buffers
#else
#define LITE_STRIP 8                     // with PSRAM the internal RAM has room for fewer, longer writes
#endif
#endif
// a resistive touch (XPT2046) is calibrated here, once; a capacitive one (FT6x36, GT911, AXS15231B)
// reports where the finger is, and LovyanGFX turns it with the screen
#if TS_MODEL == TS_MODEL_XPT2046
#define LITE_CALIBRATE 1
#else
#define LITE_CALIBRATE 0
#endif
#ifndef LITE_TASK_STACK
#define LITE_TASK_STACK (1024 * 6)
#endif
#ifndef LITE_TASK_DELAY
#define LITE_TASK_DELAY pdMS_TO_TICKS(30)
#endif
#define LITE_BANDS (LITE_SCREEN_W >= 480 ? 16 : 12)
#define LITE_QUEUE 20
#define LITE_MOVE_MS 40                  // a scrolling text moves one step this often
#define VIEW_IDLE_MS 20000               // a list, the numbers or the settings go back to the player
#define CALIB_WAIT_MS 30000              // the calibration gives up when nobody touches it
#define NAMED_MS 1500                    // the indicator's name, after a tap changed it
#define VOLUME_BIG_MS 1500
#define MS_AUTO_CLOSE_MS 5000
#define SWIPE_PX 24                      // a finger moving this far is a swipe, not a tap
#define RELEASE_MS 80                    // the XPT2046 drops out for a moment now and then: that is no lift

// the screens of this board, over FusionEdge's modes
enum View { V_PLAYER, V_LIST, V_STATS, V_SETTINGS, V_CALIB };
enum { SET_THEME, SET_INDICATOR, SET_BRIGHTNESS, SET_SCREEN, SET_TOUCH, SET_VERSION, SET_ROWS };
// the rows the settings show, top down: the touch row only where there is a calibration
static const int SET_SHOWN[] = {SET_THEME, SET_INDICATOR, SET_BRIGHTNESS, SET_SCREEN,
#if TS_MODEL == TS_MODEL_XPT2046
                                SET_TOUCH,
#endif
                                SET_VERSION};
static const int SET_SHOWN_N = (int)(sizeof SET_SHOWN / sizeof SET_SHOWN[0]);

static QueueHandle_t     s_queue = nullptr;
static SemaphoreHandle_t s_bus = nullptr;   // the panel's SPI: the drawing, the touch, lock(), the I2S
static uint32_t*         s_px = nullptr;    // a strip, 0x00RRGGBB
static uint16_t*         s_565 = nullptr;   // the same as the panel takes it
static radio_t           s_now;
static radio_t           s_drawn;
static int               s_redraw = RADIO_ALL;
static uint32_t          s_lastMove = 0;
static uint32_t          s_lastSlow = 0;    // the buffer, the signal, the traffic: once a second
static int               s_bufPct = 0;
static int               s_rssi = 0;
static char              s_bootLine[64] = "";
static int               s_sdFiles = -1;

static View      s_view = V_PLAYER;
static uint32_t  s_viewUntil = 0;
static View      s_calibBack = V_PLAYER;
static uint16_t  s_listFirst = 1;           // the station in the list's first row
static uint32_t  s_volumeBigUntil = 0;
static uint32_t  s_namedUntil = 0;

// kept in NVS
static Preferences s_prefs;
static int         s_theme = 0;
static int         s_indicator = RADIO_IND_LED;

// the touch: raw to screen, screen = (a b c; d e f) * (raw x, raw y, 1), at the rotation it was taken in
struct TouchCal {
    float  a, b, c, d, e, f;
    int8_t rot;
    bool   ok;
};
static TouchCal s_cal{};
static int      s_calStep = 0;
static uint32_t s_calDeadline = 0;
static int32_t  s_calRaw[4][2];
static int32_t  s_calSumX = 0, s_calSumY = 0, s_calN = 0;

static bool     s_down = false;
static int      s_downX = 0, s_downY = 0, s_lastX = 0, s_lastY = 0;
static uint32_t s_lastSeen = 0;
static bool     s_swipe = false, s_swipeAcross = false;
static int      s_swipeDone = 0;            // volume steps or list rows the swipe has made

// the station's numbers
static uint32_t s_lastBytes = 0;
static uint32_t s_lastBytesMs = 0;
static uint16_t s_traffic[RADIO_TRAFFIC_N];
static int      s_trafficN = 0;
static int      s_kbps = 0;
static uint32_t s_streamS = 0;
static int      s_reconnects = 0;
static uint16_t s_statsStation = 0;
static uint8_t  s_peaks[RADIO_BANDS_MAX];
static uint8_t  s_lastBands[RADIO_BANDS_MAX];
static uint8_t  s_ghost[3][RADIO_BANDS_MAX];  // the spectrum's last frames, newest first

// a finger's last landing, for its ring; the BOOT button, which opens the touch calibration
static uint32_t s_touchMs = 0;
static int      s_touchX = 0, s_touchY = 0;
static bool     s_bootDown = false;
static uint32_t s_bootMs = 0;

class BusGuard {
  public:
    explicit BusGuard(TickType_t wait) {
        _taken = s_bus != nullptr && xSemaphoreTakeRecursive(s_bus, wait) == pdTRUE;
    }
    ~BusGuard() {
        if (_taken) {
            xSemaphoreGiveRecursive(s_bus);
        }
    }
    bool taken() const { return _taken; }

  private:
    bool _taken;
};

static void setView(View v) {
    s_view = v;
    s_viewUntil = millis() + VIEW_IDLE_MS;
    s_redraw = RADIO_ALL;
}

// ---------------------------------------------------------------------------------------------
// settings and the touch calibration, in NVS

static void loadPrefs() {
    s_prefs.begin("lite", true);
    s_theme = s_prefs.getUChar("theme", 0) % radio_themes();
    s_indicator = s_prefs.getUChar("ind", RADIO_IND_LED) % RADIO_INDICATORS;
    if (s_prefs.getBytesLength("cal") == sizeof(TouchCal)) {
        s_prefs.getBytes("cal", &s_cal, sizeof(TouchCal));
    }
    s_prefs.end();
}

static void savePref(const char* key, uint8_t value) {
    s_prefs.begin("lite", false);
    s_prefs.putUChar(key, value);
    s_prefs.end();
}

static void saveCal() {
    s_prefs.begin("lite", false);
    s_prefs.putBytes("cal", &s_cal, sizeof(TouchCal));
    s_prefs.end();
}

// the least squares fit of screen = A * raw through the four crosses (3x3 normal equations, Cramer)
static bool solveCal(TouchCal& out) {
    double m[3][3] = {}, vx[3] = {}, vy[3] = {};
    for (int i = 0; i < 4; i++) {
        int sx, sy;
        radio_calib_point(i, &sx, &sy);
        const double r[3] = {(double)s_calRaw[i][0], (double)s_calRaw[i][1], 1.0};
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++) {
                m[j][k] += r[j] * r[k];
            }
            vx[j] += r[j] * sx;
            vy[j] += r[j] * sy;
        }
    }
    auto det3 = [](double a[3][3]) {
        return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
               a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    };
    const double d = det3(m);
    if (fabs(d) < 1e-6) {
        return false;                    // the crosses were not told apart: the same point four times
    }
    double sol[2][3];
    for (int axis = 0; axis < 2; axis++) {
        const double* v = axis == 0 ? vx : vy;
        for (int col = 0; col < 3; col++) {
            double t[3][3];
            for (int j = 0; j < 3; j++) {
                for (int k = 0; k < 3; k++) {
                    t[j][k] = k == col ? v[j] : m[j][k];
                }
            }
            sol[axis][col] = det3(t) / d;
        }
    }
    out.a = sol[0][0];
    out.b = sol[0][1];
    out.c = sol[0][2];
    out.d = sol[1][0];
    out.e = sol[1][1];
    out.f = sol[1][2];
    out.rot = (int8_t)dsp.getRotation();
    // each cross must come out where it was drawn: a wrong touch is no calibration
    for (int i = 0; i < 4; i++) {
        int sx, sy;
        radio_calib_point(i, &sx, &sy);
        const float x = out.a * s_calRaw[i][0] + out.b * s_calRaw[i][1] + out.c;
        const float y = out.d * s_calRaw[i][0] + out.e * s_calRaw[i][1] + out.f;
        if (fabsf(x - sx) > 30 || fabsf(y - sy) > 30) {
            return false;
        }
    }
    out.ok = true;
    return true;
}

// ---------------------------------------------------------------------------------------------
// the touch

static bool touchRaw(int32_t& rx, int32_t& ry) {
    uint16_t x = 0, y = 0;
    if (dsp.getTouchRaw(&x, &y) == 0) {
        return false;
    }
    rx = x;
    ry = y;
    return true;
}

static bool touchScreen(int& sx, int& sy) {
    if (!LITE_CALIBRATE || !s_cal.ok) {
        uint16_t x = 0, y = 0;               // LovyanGFX's own: a capacitive touch, or one not calibrated yet
        if (!dsp.getTouch(&x, &y)) {
            return false;
        }
        sx = x;
        sy = y;
        if (config.store.xTouchMirroring) {  // the radio's web page mirrors a touch that lands on the wrong side
            sx = LITE_SCREEN_W - 1 - sx;
        }
        if (config.store.yTouchMirroring) {
            sy = LITE_SCREEN_H - 1 - sy;
        }
        return true;
    }
    int32_t rx, ry;
    if (!touchRaw(rx, ry)) {
        return false;
    }
    float x = s_cal.a * rx + s_cal.b * ry + s_cal.c;
    float y = s_cal.d * rx + s_cal.e * ry + s_cal.f;
    if (((dsp.getRotation() - s_cal.rot) & 3) == 2) {   // turned round since: flipscreen
        x = LITE_SCREEN_W - 1 - x;
        y = LITE_SCREEN_H - 1 - y;
    }
    sx = constrain((int)lroundf(x), 0, LITE_SCREEN_W - 1);
    sy = constrain((int)lroundf(y), 0, LITE_SCREEN_H - 1);
    return true;
}

static void startCalib(View back) {
    s_calibBack = back;
    s_calStep = 0;
    s_calSumX = s_calSumY = s_calN = 0;
    s_calDeadline = millis() + CALIB_WAIT_MS;
    s_down = false;
    setView(V_CALIB);
}

static void calibTouch() {
    int32_t rx, ry;
    const uint32_t now = millis();
    if (touchRaw(rx, ry)) {
        if (!s_down && config.store.dbgtouch) {
            log_i("##[LITE]# calibration touch raw %d %d", (int)rx, (int)ry);
        }
        s_calSumX += rx;
        s_calSumY += ry;
        s_calN++;
        s_lastSeen = now;
        s_down = true;
        s_calDeadline = now + CALIB_WAIT_MS;
        return;
    }
    if (!s_down || now - s_lastSeen < RELEASE_MS) {
        return;
    }
    s_down = false;                          // lifted: this cross is done
    if (s_calN >= 3) {
        s_calRaw[s_calStep][0] = s_calSumX / s_calN;
        s_calRaw[s_calStep][1] = s_calSumY / s_calN;
        log_i("##[LITE]# touch cross %d raw %d %d", s_calStep, (int)s_calRaw[s_calStep][0], (int)s_calRaw[s_calStep][1]);
        s_calStep++;
    }
    s_calSumX = s_calSumY = s_calN = 0;
    if (s_calStep < 4) {
        return;
    }
    TouchCal cal{};
    if (solveCal(cal)) {
        s_cal = cal;
        saveCal();
        log_i("##[LITE]# touch calibrated");
        setView(s_calibBack);
    } else {
        log_w("##[LITE]# the four touches did not fit together: again");
        s_calStep = 0;
    }
}

static void userActivity() {
    config.screensaverTicks = 0;
    config.screensaverPlayingTicks = 0;
    s_viewUntil = millis() + VIEW_IDLE_MS;
}

static void showVolume() {
    s_volumeBigUntil = millis() + VOLUME_BIG_MS;
}

static void settingTap(int row) {
    switch (row) {
        case SET_THEME:
            s_theme = (s_theme + 1) % radio_themes();
            savePref("theme", (uint8_t)s_theme);
            break;
        case SET_INDICATOR:
            s_indicator = (s_indicator + 1) % RADIO_INDICATORS;
            savePref("ind", (uint8_t)s_indicator);
            break;
        case SET_BRIGHTNESS: {
            const uint8_t b = config.store.brightness;
            config.store.brightness = b >= 100 ? 20 : (uint8_t)min(100, (b / 20 + 1) * 20);
            config.setBrightness(true);
            break;
        }
        case SET_SCREEN:
            config.saveValue(&config.store.flipscreen, !config.store.flipscreen);
            dsp.flip();
            s_redraw = RADIO_ALL;
            break;
        case SET_TOUCH:
            startCalib(V_SETTINGS);
            break;
        default: break;
    }
}

static void tap(Display& d, int x, int y) {
    int arg = 0;
    const int hit = radio_hit(&s_now, x, y, &arg);
    switch (hit) {
        case RADIO_HIT_STATION:
            s_listFirst = 0;                 // centred on the one playing (readList)
            setView(V_LIST);
            break;
        case RADIO_HIT_PLAYSTOP: player.toggle(); break;
        case RADIO_HIT_TITLE:
        case RADIO_HIT_CLOCK: setView(V_STATS); break;
        case RADIO_HIT_INDICATOR:
            s_indicator = (s_indicator + 1) % RADIO_INDICATORS;
            savePref("ind", (uint8_t)s_indicator);
            s_namedUntil = millis() + NAMED_MS;
            break;
        case RADIO_HIT_VOLUME:
            config.store.volume = (uint8_t)arg;
            player.setVol((uint8_t)arg);
            showVolume();
            break;
        case RADIO_HIT_SETTINGS: setView(V_SETTINGS); break;
        case RADIO_HIT_BACK: setView(V_PLAYER); break;
        case RADIO_HIT_ROW:
            if (s_view == V_LIST && arg < s_now.list_rows) {
                player.sendCommand({PR_PLAY, s_now.list_no[arg]});
                setView(V_PLAYER);
            } else if (s_view == V_SETTINGS && arg >= 0 && arg < SET_SHOWN_N) {
                settingTap(SET_SHOWN[arg]);
            }
            break;
        default: break;
    }
    (void)d;
}

// the BOOT button: the touch calibration, for a board whose touch lands nowhere near the finger
static void bootButton() {
    if (!LITE_CALIBRATE) {
        return;
    }
    const bool down = digitalRead(0) == LOW;
    const uint32_t now = millis();
    if (down && !s_bootDown && now - s_bootMs > 300 && s_view != V_CALIB) {
        s_bootMs = now;
        startCalib(s_view);
    }
    s_bootDown = down;
}

// a finger down, moving, lifted: taps where it went down, swipes as it goes
static void liteTouch(Display& d) {
    if (s_view == V_CALIB) {
        calibTouch();
        return;
    }
    const uint32_t now = millis();
    int x, y;
    if (touchScreen(x, y)) {
        s_lastSeen = now;
        if (!s_down) {
            s_touchMs = now;
            s_touchX = x;
            s_touchY = y;
            if (config.store.dbgtouch) {
                int32_t rx = 0, ry = 0;
                touchRaw(rx, ry);
                log_i("##[LITE]# touch at %d %d (raw %d %d, %s)", x, y, (int)rx, (int)ry, s_cal.ok ? "calibrated" : "not calibrated");
            }
            s_down = true;
            s_downX = s_lastX = x;
            s_downY = s_lastY = y;
            s_swipe = false;
            s_swipeDone = 0;
            userActivity();
            if (d.mode() == SCREENSAVER || d.mode() == SCREENBLANK || d.mode() == SLEEPING) {
                d.putRequest(NEWMODE, PLAYER);   // the first touch only wakes it
                s_swipe = true;
            }
            return;
        }
        s_lastX = x;
        s_lastY = y;
        const int dx = x - s_downX, dy = y - s_downY;
        if (!s_swipe && (abs(dx) > SWIPE_PX || abs(dy) > SWIPE_PX)) {
            s_swipe = true;
            s_swipeAcross = abs(dx) > abs(dy);
        }
        if (s_swipe && !s_swipeAcross) {
            // up and down: the volume on the player, the rows in the list, a step each 20 pixels
            const int steps = -dy / 20;
            if (steps != s_swipeDone && s_view == V_PLAYER) {
                const int dir = steps > s_swipeDone ? 1 : -1;
                for (int i = s_swipeDone; i != steps; i += dir) {
                    player.stepVol(dir > 0);
                }
                s_swipeDone = steps;
                showVolume();
            } else if (steps != s_swipeDone && s_view == V_LIST) {
                const int total = (int)config.playlistLength();
                int first = (int)s_listFirst - (steps - s_swipeDone) * -1;   // a finger up shows the later ones
                first = constrain(first, 1, max(1, total - s_now.list_rows + 1));
                s_listFirst = (uint16_t)first;
                s_swipeDone = steps;
                s_viewUntil = now + VIEW_IDLE_MS;
            }
        }
        return;
    }
    if (!s_down || now - s_lastSeen < RELEASE_MS) {
        return;
    }
    s_down = false;                          // lifted
    if (!s_swipe) {
        tap(d, s_downX, s_downY);
        return;
    }
    if (s_swipeAcross && s_view == V_PLAYER && abs(s_lastX - s_downX) > 2 * SWIPE_PX) {
        if (s_lastX < s_downX) {
            player.next();                   // the finger goes left: the next station comes in
        } else {
            player.prev();
        }
    }
}

// ---------------------------------------------------------------------------------------------
// what the screen shows, read from the radio

static const char* codecName(BitrateFormat f) {
    switch (f) {
        case BF_MP3: return "MP3";
        case BF_AAC: return "AAC";
        case BF_FLAC: return "FLAC";
        case BF_OGG: return "OGG";
        case BF_WAV: return "WAV";
        case BF_VOR: return "VORBIS";
        case BF_OPU: return "OPUS";
        default: return "";
    }
}

static void readTime(radio_t& r) {
    static const char* DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    static const char* MONTHS[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    tm t{};
    network_get_timeinfo_snapshot(&t);
    if (t.tm_year < 100) {               // before the first SNTP answer
        r.hh = -1;
        r.mm = 0;
        r.date[0] = 0;
        return;
    }
    r.hh = t.tm_hour;
    r.mm = t.tm_min;
    snprintf(r.date, sizeof r.date, "%s %d %s", DAYS[t.tm_wday % 7], t.tm_mday, MONTHS[t.tm_mon % 12]);
}

// the title as the old screen splits it: "artist - song" in two lines
static void readTitle(radio_t& r) {
    char buf[BUFLEN];
    strlcpy(buf, config.station.title, sizeof buf);
    char* dash = strstr(buf, " - ");
    if (dash) {
        *dash = 0;
        strlcpy(r.title1, buf, sizeof r.title1);
        strlcpy(r.title2, dash + 3, sizeof r.title2);
    } else {
        strlcpy(r.title1, buf, sizeof r.title1);
        r.title2[0] = 0;
    }
}

// pick: the station playing, or the one an encoder or the IR remote moved to (FusionEdge's STATIONS)
static void readList(radio_t& r, uint16_t pick, const char* head) {
    static uint16_t lastPick = 0;
    radio_layout_t l;
    radio_layout(&l);
    const int total = (int)config.playlistLength();
    int rows = min((int)RADIO_LIST_ROWS, l.list_rows);
    rows = min(rows, total);
    if (s_listFirst == 0) {                  // just opened: the pick in the middle
        s_listFirst = (uint16_t)max(1, (int)pick - rows / 2);
    } else if (pick != lastPick && rows > 0) {   // the pick moved: it stays on the screen
        if (pick < s_listFirst) {
            s_listFirst = pick;
        } else if (pick >= s_listFirst + rows) {
            s_listFirst = (uint16_t)(pick - rows + 1);
        }
    }
    lastPick = pick;
    int first = constrain((int)s_listFirst, 1, max(1, total - rows + 1));
    s_listFirst = (uint16_t)first;
    r.list_rows = rows;
    r.list_total = total;
    r.list_pick = (int)pick - first;
    if (r.list_pick < 0 || r.list_pick >= rows) {
        r.list_pick = -1;
    }
    for (int i = 0; i < rows; i++) {
        r.list_no[i] = first + i;
        const char* name = config.stationByNum((uint16_t)(first + i));
        strlcpy(r.list[i], name ? name : "", sizeof r.list[i]);
    }
    strlcpy(r.head, head, sizeof r.head);
}

static void message(radio_t& r, const char* head, const char* a, const char* b = "", const char* c = "") {
    r.screen = RADIO_MESSAGE;
    strlcpy(r.head, head, sizeof r.head);
    strlcpy(r.line[0], a, sizeof r.line[0]);
    strlcpy(r.line[1], b, sizeof r.line[1]);
    strlcpy(r.line[2], c, sizeof r.line[2]);
}

static void readSettings(radio_t& r) {
    static const char* const NAMES[SET_ROWS] = {"THEME", "INDICATOR", "BRIGHTNESS", "SCREEN", "TOUCH", "VERSION"};
    r.set_rows = SET_SHOWN_N;
    for (int i = 0; i < SET_SHOWN_N; i++) {
        const int id = SET_SHOWN[i];
        char* value = r.set_value[i];
        const size_t size = sizeof r.set_value[0];
        strlcpy(r.set_name[i], NAMES[id], sizeof r.set_name[i]);
        switch (id) {
            case SET_THEME: strlcpy(value, radio_theme_name(s_theme), size); break;
            case SET_INDICATOR: strlcpy(value, radio_indicator_name(s_indicator), size); break;
            case SET_BRIGHTNESS: snprintf(value, size, "%d%%", config.store.brightness); break;
            case SET_SCREEN: strlcpy(value, config.store.flipscreen ? "TURNED" : "NORMAL", size); break;
            case SET_TOUCH: strlcpy(value, s_cal.ok ? "CALIBRATE" : "NOT SET", size); break;
            case SET_VERSION: strlcpy(value, FW_VERSION, size); break;
            default: value[0] = 0; break;
        }
    }
    strlcpy(r.head, "SETTINGS", sizeof r.head);
}

static void readPlayer(radio_t& r, displayMode_e mode) {
    const uint32_t now = millis();
    strlcpy(r.station, config.station.name, sizeof r.station);
    r.station_no = config.lastStation();
    readTitle(r);
    strlcpy(r.codec, codecName(config.configFmt), sizeof r.codec);
    r.bitrate = config.station.bitrate;
    r.playing = player.isRunning() ? 1 : 0;
    readTime(r);
#ifdef EXT_WEATHER
    if (config.store.showweather && timekeeper.weatherBuf) {
        strlcpy(r.weather, timekeeper.weatherBuf, sizeof r.weather);
    }
#endif
    r.volume = config.store.volume;
    r.volume_max = 21;
    r.volume_big = mode == VOL || now < s_volumeBigUntil;
    r.buffer_pct = s_bufPct;
    r.rssi = s_rssi;
    strlcpy(r.ip, WiFi.localIP().toString().c_str(), sizeof r.ip);
    r.indicator = s_indicator;
    r.indicator_named = now < s_namedUntil;
    r.nbands = LITE_BANDS;
    if (player.isRunning()) {
        uint8_t spec[SA_BANDS_MAX];
        const bool fresh = spectrumAnalyzer.process();
        spectrumAnalyzer.getData(spec, nullptr);
        if (fresh) {                                // a new frame: the last one joins the afterglow
            memmove(s_ghost[1], s_ghost[0], 2 * RADIO_BANDS_MAX);
            memcpy(s_ghost[0], s_lastBands, RADIO_BANDS_MAX);
            memcpy(s_lastBands, spec, LITE_BANDS);
        }
        memcpy(r.bands, spec, LITE_BANDS);
    }
    memcpy(r.ghost, s_ghost, sizeof r.ghost);
    for (int i = 0; i < LITE_BANDS; i++) {          // each band's peak, falling about 70 a second
        s_peaks[i] = r.bands[i] > s_peaks[i] ? r.bands[i] : (uint8_t)max(0, (int)s_peaks[i] - 2);
    }
    memcpy(r.peaks, s_peaks, LITE_BANDS);
    // the numbers
    r.samplerate = player.isRunning() ? (int)player.getSampleRate() : 0;
    r.channels = player.isRunning() ? player.getChannels() : 0;
    r.stream_s = s_streamS;
    r.kbps = s_kbps;
    r.total_kb = s_lastBytes / 1024;
    memcpy(r.traffic, s_traffic, sizeof r.traffic);
    r.traffic_n = s_trafficN;
    r.reconnects = s_reconnects;
    r.heap_kb = (int)(heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024);
    r.heap_big_kb = (int)(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) / 1024);
}

static void snapshot(radio_t& r, Display& d, uint8_t bootStep) {
    memset(&r, 0, sizeof r);
    r.now_ms = millis();
    r.theme = s_theme;
    r.touch_age = -1;
    if (s_touchMs != 0 && r.now_ms - s_touchMs < RADIO_TOUCH_MS + 60) {
        r.touch_x = s_touchX;
        r.touch_y = s_touchY;
        r.touch_age = (int)(r.now_ms - s_touchMs);
    }
    const displayMode_e mode = d.mode();

    if (bootStep < 2) {
        r.screen = RADIO_BOOT;
        snprintf(r.line[0], sizeof r.line[0], "v%s", FW_VERSION);
        strlcpy(r.line[1], s_bootLine, sizeof r.line[1]);
        return;
    }
    if (s_view == V_CALIB) {
        r.screen = RADIO_CALIB;
        r.calib_step = s_calStep;
        const int32_t left = (int32_t)(s_calDeadline - millis());
        r.calib_left_s = left > 0 ? (left + 999) / 1000 : 0;
        return;
    }
    if (network.status != CONNECTED && network.status != SDREADY) {
        r.screen = RADIO_SETUP;
        snprintf(r.line[0], sizeof r.line[0], "Join  %s", apSsid);
        snprintf(r.line[1], sizeof r.line[1], "Password  %s", apPassword);
        snprintf(r.line[2], sizeof r.line[2], "Open  http://%s", WiFi.softAPIP().toString().c_str());
        strlcpy(r.line[3], "or send your WiFi from the flasher page, over the USB cable", sizeof r.line[3]);
        return;
    }
    switch (mode) {
        case CLEAR:
        case SLEEPING:
        case SCREENBLANK:
            r.screen = RADIO_BLANK;
            return;
        case LOST:
            message(r, "NO STREAM", "The connection was lost.", "It tries again by itself.");
            return;
        case UPDATING:
            message(r, "UPDATING", "The firmware is being updated.", "Leave the power on.");
            return;
        case SDCHANGE: {
            char n[32] = "";
            if (s_sdFiles >= 0) {
                snprintf(n, sizeof n, "%d files so far", s_sdFiles);
            }
            message(r, "SD CARD", "Reading the card...", n);
            return;
        }
        case MODESELECT:
            r.screen = RADIO_LIST;
            r.list_rows = d.msModeCount();
            r.list_pick = -1;
            for (int i = 0; i < r.list_rows && i < RADIO_LIST_ROWS; i++) {
                r.list_no[i] = i + 1;
                strlcpy(r.list[i], i == 0 ? "WEB" : "SD", sizeof r.list[i]);
            }
            strlcpy(r.head, "MODE", sizeof r.head);
            return;
        case NUMBERS: {                      // a station number typed on the IR remote
            char n[16];
            snprintf(n, sizeof n, "%u", (unsigned)d.numOfNextStation);
            const char* name = d.numOfNextStation ? config.stationByNum(d.numOfNextStation) : nullptr;
            message(r, "STATION NUMBER", n, name ? name : "");
            return;
        }
        default:
            break;
    }
    switch (s_view) {
        case V_LIST:
            r.screen = RADIO_LIST;
            readList(r, mode == STATIONS ? d.currentPlItem : config.lastStation(), "STATIONS");
            return;
        case V_SETTINGS:
            r.screen = RADIO_SETTINGS;
            readSettings(r);
            return;
        case V_STATS:
            readPlayer(r, mode);
            r.screen = RADIO_STATS;
            return;
        default:
            readPlayer(r, mode);
            r.screen = RADIO_PLAYER;
            return;
    }
}

// once a second: the buffer, the signal, what came in, how long it has played
static void slowNumbers() {
    const uint32_t now = millis();
    const uint32_t size = player.getInBufferSize();
    s_bufPct = (player.isRunning() && size > 0) ? (int)((uint64_t)player.inBufferFilled() * 100 / size) : 0;
    s_rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    const uint32_t bytes = player.streamBytes();
    const uint32_t ms = now - s_lastBytesMs;
    s_kbps = (ms > 0 && s_lastBytesMs > 0) ? (int)((uint64_t)(bytes - s_lastBytes) * 8 / ms) : 0;
    s_lastBytes = bytes;
    s_lastBytesMs = now;
    if (s_trafficN < RADIO_TRAFFIC_N) {
        s_traffic[s_trafficN++] = (uint16_t)s_kbps;
    } else {
        memmove(s_traffic, s_traffic + 1, sizeof(s_traffic) - sizeof(s_traffic[0]));
        s_traffic[RADIO_TRAFFIC_N - 1] = (uint16_t)s_kbps;
    }
    if (config.lastStation() != s_statsStation) {
        s_statsStation = config.lastStation();
        s_streamS = 0;
        s_reconnects = 0;
    } else if (player.isRunning()) {
        s_streamS++;
    }
}

// ---------------------------------------------------------------------------------------------
// drawing: the parts asked for, a strip at a time

static void drawRows(int y0, int y1) {
    for (int y = y0; y < y1; y += LITE_STRIP) {
        canvas_t cv = {s_px, y, min(LITE_STRIP, y1 - y), 0};
        radio_draw(&cv, &s_now);
        gfx_pack565(&cv, s_565);
        dsp.pushImage(0, y, LITE_SCREEN_W, cv.h, (const lgfx::swap565_t*)s_565);
    }
}

static void drawParts(int parts) {
    if (!s_px || !s_565) {
        return;
    }
    dsp.startWrite();
    if (parts == RADIO_ALL || s_now.screen != RADIO_PLAYER) {
        drawRows(0, LITE_SCREEN_H);
    } else {
        radio_layout_t l;
        radio_layout(&l);
        if (parts & RADIO_HEAD) {
            drawRows(l.head_y, l.head_y + l.head_h);
        }
        if (parts & RADIO_TITLE) {
            drawRows(l.title_y, l.title_y + l.title_h);
        }
        if (parts & RADIO_MID) {
            drawRows(l.mid_y, l.mid_y + l.mid_h);
        }
        if (parts & RADIO_FOOT) {
            drawRows(l.foot_y, l.foot_y + l.foot_h);
        }
    }
    dsp.endWrite();
}

static void liteTask(void*) {
    for (;;) {
        if (timekeeper.loop0()) {
            {
                BusGuard guard(0);
                if (guard.taken()) {
                    if (!display.isLocked() && display.ready()) {
                        bootButton();
                        liteTouch(display);
                    }
                    display.loop();
                }
            }
#ifndef NETSERVER_LOOP1
            netserver.loop();
#endif
        }
        vTaskDelay(LITE_TASK_DELAY);
    }
}

// ---------------------------------------------------------------------------------------------
// Display

Display::~Display() {}

void Display::init() {
    Serial.print("##[BOOT]#\tdisplay.init (lite)\t\n");
    _bootStep = 0;
    _mode = PLAYER;
    _panelAwake = true;
    if (!s_bus) {
        s_bus = xSemaphoreCreateRecursiveMutex();
    }
    loadPrefs();
    if (LITE_CALIBRATE) {
        pinMode(0, INPUT_PULLUP);                // BOOT
    }
    dsp.initDisplay();
    if (dsp.width() != LITE_SCREEN_W || dsp.height() != LITE_SCREEN_H) {
        log_e("##[LITE]# the panel is %dx%d, the build draws %dx%d: LITE_SCREEN_W/H", dsp.width(), dsp.height(),
              LITE_SCREEN_W, LITE_SCREEN_H);
    }
    if (!s_px) {
        s_px = (uint32_t*)heap_caps_malloc(LITE_SCREEN_W * LITE_STRIP * sizeof(uint32_t), MALLOC_CAP_8BIT);
        s_565 = (uint16_t*)heap_caps_malloc(LITE_SCREEN_W * LITE_STRIP * sizeof(uint16_t), MALLOC_CAP_DMA);
    }
    s_queue = xQueueCreate(LITE_QUEUE, sizeof(requestParams_t));
    while (s_queue == NULL) {
        delay(1);
    }
    _bootStep = 1;
    {
        BusGuard guard(portMAX_DELAY);
        snapshot(s_now, *this, _bootStep);
        drawParts(RADIO_ALL);
        s_drawn = s_now;
        s_redraw = 0;
    }
    xTaskCreatePinnedToCore(liteTask, "DspTask", LITE_TASK_STACK, NULL, 3, NULL, 0);
    Serial.println("done");
}

void Display::_start() {
    spectrumAnalyzer.begin(LITE_BANDS);
    currentPlItem = config.lastStation();
    s_redraw = RADIO_ALL;
    _bootStep = 2;
    if (LITE_CALIBRATE && !s_cal.ok) {
        startCalib(V_PLAYER);               // a board never calibrated: ask once a start, until it is
    }
}

// FusionEdge's modes, as the old screen switches to them (display.cpp): the timers that bring the
// player back. This board's own screens stay over them, but the ones that must show
void Display::_swichMode(displayMode_e newmode) {
    if (newmode == _mode) {
        return;
    }
    if (network.status != CONNECTED && network.status != SDREADY) {
        return;                          // the setup screen stays until the radio has a network
    }
    const displayMode_e was = _mode;
    _mode = newmode;
    if (newmode == PLAYER && was == STATIONS && s_view == V_LIST) {
        setView(V_PLAYER);               // the encoder's list closes with FusionEdge's, picked or timed out
    }
    if (newmode == STATIONS) {
        currentPlItem = config.lastStation();
        s_listFirst = 0;
        setView(V_LIST);
        timekeeper.waitAndReturnPlayer(config.store.stationsListReturnTime);
    }
    if (newmode == VOL) {
        timekeeper.waitAndReturnPlayer(2);
        showVolume();
    }
    if (newmode == MODESELECT) {
        _msCount = 0;
        _msValues[_msCount] = PM_WEB;
        _msLabels[_msCount++] = "WEB";
#ifdef USE_SD
        if (SDC_CS != 255) {
            _msValues[_msCount] = PM_SDCARD;
            _msLabels[_msCount++] = "SD";
        }
#endif
        _msIdx = 0;
    }
    s_redraw = RADIO_ALL;
}

void Display::loop() {
    if (_bootStep == 0 || s_queue == nullptr) {
        return;
    }
    requestParams_t req;
    while (xQueueReceive(s_queue, &req, 0) == pdTRUE) {
        switch (req.type) {
            case NEWMODE: _swichMode((displayMode_e)req.payload); break;
            case DSP_START: _start(); break;
            case BOOTSTRING:
                if (req.payload >= 0 && req.payload < 5) {
                    snprintf(s_bootLine, sizeof s_bootLine, "connecting to %s", config.ssids[req.payload].ssid);
                }
                break;
            case WAITFORSD: strlcpy(s_bootLine, "looking for the SD card", sizeof s_bootLine); break;
            case SDFILEINDEX: s_sdFiles = req.payload; break;
            case DRAWPLAYLIST: currentPlItem = (uint16_t)req.payload; break;
            case NEXTSTATION: numOfNextStation = (uint16_t)req.payload; break;
            case CLOSEPLAYLIST: player.sendCommand({PR_PLAY, req.payload}); break;
            case DRAWVOL: showVolume(); break;
            case PSTART:
                if (s_streamS > 0 && config.lastStation() == s_statsStation) {
                    s_reconnects++;          // the same station starting again: it had dropped
                }
                break;
            case INVALIDATETHEMEWIDGETS:
            case PLAYERREBUILD: s_redraw = RADIO_ALL; break;
            default: break;               // the rest is read from the radio below
        }
    }
    modeSelectorTick();

    const uint32_t now = millis();
    if (now - s_lastSlow >= 1000) {
        s_lastSlow = now;
        slowNumbers();
    }
    if (s_view == V_CALIB && (int32_t)(now - s_calDeadline) > 0) {
        log_w("##[LITE]# no touch for the calibration: skipped");
        setView(s_calibBack);
    } else if (s_view != V_PLAYER && s_view != V_CALIB && (int32_t)(now - s_viewUntil) > 0) {
        setView(V_PLAYER);
    }
    if (!_panelAwake) {
        return;
    }
    snapshot(s_now, *this, _bootStep);
    int parts = radio_dirty(&s_drawn, &s_now) | s_redraw;
    if (now - s_lastMove >= LITE_MOVE_MS) {
        const int moving = radio_moving(&s_now);
        if (moving) {
            parts |= moving;
            s_lastMove = now;
        }
    }
    if (parts) {
        drawParts(parts);
        s_drawn = s_now;
        s_redraw = 0;
    }
}

void Display::resetQueue() {
    if (s_queue) {
        xQueueReset(s_queue);
    }
}

void Display::purgeQueuedRequestType(displayRequestType_e type) {
    if (!s_queue) {
        return;
    }
    requestParams_t keep[LITE_QUEUE];
    int n = 0;
    requestParams_t req;
    while (n < LITE_QUEUE && xQueueReceive(s_queue, &req, 0) == pdTRUE) {
        if (req.type != type) {
            keep[n++] = req;
        }
    }
    for (int i = 0; i < n; i++) {
        xQueueSend(s_queue, &keep[i], 0);
    }
}

void Display::beginContentChange() {}

bool Display::waitQueueEmpty(uint32_t timeoutMs) {
    const uint32_t until = millis() + timeoutMs;
    while (s_queue && uxQueueMessagesWaiting(s_queue) > 0 && millis() < until) {
        delay(5);
    }
    return !s_queue || uxQueueMessagesWaiting(s_queue) == 0;
}

bool Display::waitContentReady(uint32_t timeoutMs) {
    return waitQueueEmpty(timeoutMs);
}

void Display::putRequest(displayRequestType_e type, int payload) {
    if (s_queue == nullptr) {
        return;
    }
    if (type == PSTART) {
        purgeQueuedRequestType(PSTOP);
    }
    if (type == PSTOP) {
        purgeQueuedRequestType(PSTART);
    }
    requestParams_t req{type, payload, 0};
    xQueueSend(s_queue, &req, pdMS_TO_TICKS(20));
}

void Display::flip() {
    BusGuard guard(portMAX_DELAY);
    dsp.flip();
    s_redraw = RADIO_ALL;
}

void Display::invert() {
    BusGuard guard(portMAX_DELAY);
    dsp.invert();
    s_redraw = RADIO_ALL;
}

bool Display::deepsleep() {
#if BRIGHTNESS_PIN != 255
    BusGuard guard(portMAX_DELAY);
    if (!_panelAwake) {
        return true;
    }
    dsp.sleep();
    _panelAwake = false;
    return true;
#else
    return false;
#endif
}

void Display::wakeup() {
#if BRIGHTNESS_PIN != 255
    BusGuard guard(portMAX_DELAY);
    if (_panelAwake) {
        return;
    }
    dsp.wake();
    dsp.flip();
    dsp.invert();
    _panelAwake = true;
    s_redraw = RADIO_ALL;
#endif
}

void Display::setContrast() {}

void Display::lock() {
    if (s_bus) {
        xSemaphoreTakeRecursive(s_bus, portMAX_DELAY);
    }
    _locked = true;
}

void Display::unlock() {
    _locked = false;
    s_redraw = RADIO_ALL;
    if (s_bus) {
        xSemaphoreGiveRecursive(s_bus);
    }
}

void Display::i2sReconfigBegin() {
    if (s_bus) {
        xSemaphoreTakeRecursive(s_bus, portMAX_DELAY);
    }
}

void Display::i2sReconfigEnd() {
    if (s_bus) {
        xSemaphoreGiveRecursive(s_bus);
    }
}

void Display::waitDMA() {
    BusGuard guard(portMAX_DELAY);
    dsp.waitDMA();
}

uint16_t Display::width() {
    return dsp.width();
}

uint16_t Display::height() {
    return dsp.height();
}

void Display::setBrightnessPercent(uint8_t percent) {
    percent = constrain(percent, 0, 100);
#if (BRIGHTNESS_PIN != 255)
    analogWrite(BRIGHTNESS_PIN, map(percent, 0, 100, 0, 255));
#endif
}

uint8_t Display::effectiveBrightnessPercent(uint8_t normalPercent) const {
    normalPercent = constrain(normalPercent, 0, 100);
    if (!_clockScreensaverBrightnessActive) {
        return normalPercent;
    }
    return constrain(config.store.screensaverIdleBrightness, 0, 100);
}

void Display::applyVuModeChange() {
    s_redraw = RADIO_ALL;
}

void Display::invalidateThemeWidgets() {
    s_redraw = RADIO_ALL;
}

// no EQ overlay on this screen: the EQ is set on the radio's web page
bool Display::eqToggle() { return false; }
bool Display::isEqOpen() const { return false; }
void Display::eqForceClose() {}
void Display::_eqInlineDraw() {}

// FusionEdge's own touch code calls this; this board's touch is liteTouch() above
bool Display::tapPlaylistItem(uint16_t) {
    return false;
}

void Display::modeSelectorOpen() {
    if (_mode == MODESELECT) {
        return;
    }
    _msAutoCloseMs = millis() + MS_AUTO_CLOSE_MS;
    putRequest(NEWMODE, MODESELECT);
}

void Display::modeSelectorScroll(int dir) {
    if (_mode != MODESELECT || _msCount == 0) {
        return;
    }
    _msAutoCloseMs = millis() + MS_AUTO_CLOSE_MS;
    if (dir > 0) {
        _msIdx = (_msIdx + 1) % _msCount;
    } else {
        _msIdx = (_msIdx + _msCount - 1) % _msCount;
    }
}

void Display::modeSelectorConfirm() {
    if (_mode != MODESELECT) {
        return;
    }
    const int8_t val = _msValues[_msIdx];
    putRequest(NEWMODE, PLAYER);
    if (val == -1) {
        config.toggleMode();
    } else {
        config.changeMode(val);
    }
}

void Display::modeSelectorClose() {
    if (_mode != MODESELECT) {
        return;
    }
    putRequest(NEWMODE, PLAYER);
}

void Display::modeSelectorTick() {
    if (_mode != MODESELECT || millis() < _msAutoCloseMs) {
        return;
    }
    resetQueue();
    putRequest(NEWMODE, PLAYER);
}

// ---------------------------------------------------------------------------------------------
// the free functions display.cpp has

void returnPlayer() {
    display.putRequest(NEWMODE, PLAYER);
}

void display_eq_touch(uint16_t, uint16_t, bool) {}

// before the radio starts, while the serial LittleFS tool has it (main.cpp)
void display_show_maintenance_screen() {
    BusGuard guard(portMAX_DELAY);
    dsp.initDisplay();
    dsp.wake();
#if BRIGHTNESS_PIN != 255
    pinMode(BRIGHTNESS_PIN, OUTPUT);
    analogWrite(BRIGHTNESS_PIN, 255);
#endif
    if (!s_px) {
        s_px = (uint32_t*)heap_caps_malloc(LITE_SCREEN_W * LITE_STRIP * sizeof(uint32_t), MALLOC_CAP_8BIT);
        s_565 = (uint16_t*)heap_caps_malloc(LITE_SCREEN_W * LITE_STRIP * sizeof(uint16_t), MALLOC_CAP_DMA);
    }
    memset(&s_now, 0, sizeof s_now);
    message(s_now, "MAINTENANCE", "LittleFS over the serial cable", "the radio starts when it is done");
    drawParts(RADIO_ALL);
}

// the same table as display.cpp's, for config.cpp's auto EQ by genre
static const char* const eqPresetNames[] = {"Flat", "Pop", "Blues", "Rock", "Metal", "Jazz", "Country", "Alternative"};
static const int8_t      eqPresetValues[][3] = {
    {0, 0, 0}, {-1, 5, 5}, {4, 6, 0}, {3, 0, 5}, {6, -3, 5}, {-2, 0, 3}, {1, 2, 4}, {4, -2, 4},
};

bool findEqPresetByGenre(const char* genre, int8_t& bass, int8_t& mid, int8_t& treb) {
    if (!genre || genre[0] == '\0') {
        return false;
    }
    for (uint8_t i = 0; i < sizeof(eqPresetNames) / sizeof(eqPresetNames[0]); i++) {
        if (strcasecmp(eqPresetNames[i], genre) == 0) {
            bass = eqPresetValues[i][0];
            mid = eqPresetValues[i][1];
            treb = eqPresetValues[i][2];
            return true;
        }
    }
    return false;
}

#endif // LITE_UI
