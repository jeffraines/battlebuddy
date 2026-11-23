// BattleBuddy prototype for the LilyGO T-Display-S3 Touch.
//
// Buttons (board held landscape, USB on the right). What each press, hold and
// swipe does is set in More > Controls; the defaults are:
//   A = KEY (GPIO14, top):     press undo, hold quick menu; wakes from sleep
//   B = BOOT (GPIO0, bottom):  press menu, hold "Turn off?"
// The case labels the buttons A and B, not by what they do, since that can change.
//   Swipe up: menu. Swipe down: dice.
// While a menu, the dice, a picker or a pop-up is open, KEY / BOOT back out of
// it instead (KEY rolls on the dice screen).
//   Circle below the screen (if the case leaves it open): open/close the dice
// Menu: game (opens the game picker), players (1-4, or two teams), layout,
// flip, best-of match, timer, restart, dice. Yu-Gi-Oh! opens a number pad on
// tap; Digimon shows a shared memory gauge. The game, match and timer survive sleep (RTC memory and the
// RTC clock) but not a power cut or reset.

#include <Arduino.h>
#include <driver/rtc_io.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <time.h>

#include "bb/dice.h"
#include "bb/game.h"
#include "bb/match.h"
#include "bb/profile.h"
#include "board.h"
#include "display.h"
#include "ui.h"

namespace {

constexpr uint32_t kRepeatDelayMs = 500;
constexpr uint32_t kTapMaxMs = 700;       // longer presses without repeat don't count as taps
constexpr uint32_t kButtonGuardMs = 250;  // ignore touches this soon after a physical button
constexpr uint32_t kRepeatIntervalMs = 150;
constexpr uint32_t kLongPressMs = 1000;
// Screen settings (More > Screen): presets shown as chips; any value in range can be set.
constexpr uint8_t kBrightnessPresets[] = {25, 50, 80, 100};
constexpr uint16_t kDimAfterPresets[] = {0, 15, 30, 60, 120, 300}; // seconds, 0 = never
constexpr uint8_t kDimToPresets[] = {0, 4, 8, 20};                 // percent, 0 = screen off
constexpr uint16_t kSleepAfterPresets[] = {0, 5, 10, 20, 30, 60};  // minutes, 0 = never
constexpr uint8_t kMinBrightness = 5;
constexpr uint8_t kMaxDimTo = 60;
constexpr uint16_t kMaxDimAfterS = 3600;
constexpr uint16_t kMaxSleepMin = 120;
constexpr uint32_t kSleepingMs = 1500;     // "Turning off" shows this long
constexpr uint32_t kChargeSaveS = 600;     // since-last-charge: save to flash this often
constexpr uint16_t kChargeRiseMv = 150;    // a rise this big above the low point means it was charged
constexpr uint32_t kGameSaveMs = 30000;    // save the game to flash (if changed) this often
constexpr int kSwipeMinX = 60;             // px of travel for a swipe
constexpr int kSwipeMinY = 45;
constexpr uint32_t kSwipeMaxMs = 800;
constexpr int kMaxHistory = 40;
constexpr uint32_t kBatteryPollMs = 1000;
// The LilyGO can't see whether a cable is plugged in; it is guessed from the
// battery voltage: clearly high means a charger, and plugging / unplugging
// shows up as a sudden step up / down.
constexpr uint16_t kUsbSureMv = 4350;   // at or above: on a charger
constexpr uint16_t kUsbStepMv = 70;     // a step this big between readings: cable in / out
constexpr uint32_t kToastMs = 1500;
constexpr uint32_t kRollMs = 900;      // tumble before showing a roll
constexpr uint32_t kPopMs = 350;       // landing effect after a roll
constexpr uint32_t kCelebrateMs = 1100; // longer effect for the top or bottom face
constexpr uint32_t kAnimFrameMs = 25;  // redraw rate while animating
constexpr uint32_t kBurstMs = 260;     // game picker: formats bursting out of a game tile
constexpr int32_t kMaxEntry = 32000;   // number pad limit (fits the counter's range)
constexpr uint32_t kSpinMs = 420;      // counter change animation (roll, spin, shake, LP count)

// Raw touch y at or beyond this is the circle key below the visible screen.
constexpr int kRawHomeKeyY = 320;

constexpr uint32_t kSavedMagic = 0xBB0B000D;
constexpr uint32_t kTestSaveS = 60; // battery test: save progress to flash this often

constexpr uint8_t kBestOfChoices[] = {0, 1, 3, 5}; // 0 = off
constexpr uint8_t kTimerChoices[] = {0, 30, 50, 60}; // minutes, 0 = off

struct Saved {
    uint32_t magic;
    uint8_t profile_index;
    int16_t step;
    Layout layout;
    bool flipped;
    bool vivid;
    uint8_t counter_style; // 0 = themed, 1 = d20, 2 = digits
    uint8_t best_of;       // 0 = match tracking off
    uint8_t timer_minutes; // 0 = no timer
    uint32_t match_start;  // RTC seconds when the timer started; 0 = waiting for a tap
    uint32_t timer_paused_at; // RTC seconds when it was paused; 0 = not paused
    uint32_t game_start;   // RTC seconds at the game's first change; 0 = not yet
    uint8_t legend[2];     // Riftbound legends for this match (kNoLegend = not picked)
    uint16_t match_no;     // counts up with each new match (history groups games by it)
    bb_match_t match;
    bb_game_t game;
};

RTC_DATA_ATTR Saved saved;
bb_game_t &game = saved.game;

Display lcd;
LGFX_Sprite frame(&lcd);

bool menu_open = false;
int menu_page = 0;
bool end_prompt = false;
bool dirty = true;

uint16_t spin[BB_MAX_PLAYERS] = {1000, 1000, 1000, 1000};
int8_t spin_dir[BB_MAX_PLAYERS] = {1, 1, 1, 1};
int16_t prev_value[BB_MAX_PLAYERS] = {};
uint32_t spin_start_ms[BB_MAX_PLAYERS] = {};

bool dice_open = false;
DiceState dice;

bool picker_open = false;
PickerState picker;
uint32_t burst_start_ms = 0;

bool entry_open = false;
EntryState entry;
uint32_t roll_start_ms = 0;
uint32_t roll_face_ms = 0;  // when the shown face last changed
uint32_t land_ms = 0;
uint32_t anim_frame_ms = 0;

uint32_t last_input_ms = 0;
bool dimmed = false;
int battery_percent = -1; // -1 = on USB / unknown
uint16_t battery_mv = 0;

// Screen settings, saved to flash so they survive power-off.
struct ScreenPrefs {
    uint8_t brightness = 80; // percent
    uint16_t dim_after = 60; // seconds, 0 = never
    uint8_t dim_to = 8;      // percent, 0 = screen off
    uint16_t sleep_after = 30; // minutes, 0 = never
};
ScreenPrefs screen;
char brightness_text[8], dim_after_text[12], dim_to_text[8], sleep_after_text[12];

// What buttons and swipes do (More > Controls), saved to flash.
enum Control : uint8_t { CtlNone, CtlUndo, CtlMenu, CtlQuick, CtlDice, CtlOff, CtlCount };
const char *const kControlNames[] = {"None", "Undo", "Menu", "Quick menu", "Dice", "Turn off"};
// A press, A hold, B press, B hold, swipe up, down, left, right
uint8_t controls[8] = {CtlUndo, CtlQuick, CtlMenu, CtlOff, CtlMenu, CtlDice, CtlNone, CtlNone};

// Pop-ups.
Overlay overlay = Overlay::None;
uint32_t sleeping_at = 0;
EditorView editor;
uint8_t editor_which = 0; // 0 brightness, 1 dim after, 2 dim to, 3 sleep after
char editor_value[16];

// Game history (newest first) and legends, saved to flash.
HistoryEntry history[kMaxHistory];
int history_count = 0;
uint16_t history_next = 1;
int history_page = 0;
int history_sel = -1;
int history_detail_page = 0;
bool history_delete_armed = false;
char custom_legends[kMaxCustomLegends][kLegendNameLen + 1];
int custom_legend_count = 0;
uint8_t legend_side = 0;
int legend_page = 0;
uint8_t legend_sort = 0; // saved to flash
bool legend_for_history = false; // editing a past game's legend, not this match's
bool legend_chain = false;       // after picking yours, go on to theirs
char keyboard_text[kLegendNameLen + 1];

// Time on battery since the last charge (charging shows up as a voltage rise).
uint32_t charge_secs = 0;
uint16_t charge_min_mv = 0;
RTC_DATA_ATTR uint32_t charge_tick_s = 0;
uint32_t charge_saved_s = 0;
uint8_t charge_rises = 0;
char since_charge_text[16];

// Card-name language (Display > Card names), saved to flash.
uint8_t card_lang = kCardEnglish;
const char *const kCardLangNames[] = {"English", "한국어"};

// Battery test: runs the screen always on and counts how long the battery
// lasts. Progress is saved to flash every minute, so when the battery dies the
// total survives; the next power-up files it as the last test.
struct BatteryTest {
    bool running = false;
    uint32_t secs = 0;      // time so far (includes any sleep in between)
    uint16_t start_mv = 0;
    uint16_t end_mv = 0;    // latest reading
    uint32_t last_secs = 0; // the last finished test; 0 = none
    uint16_t last_start_mv = 0;
    uint16_t last_end_mv = 0;
};
BatteryTest test;
RTC_DATA_ATTR uint32_t test_tick_s = 0; // RTC seconds when test.secs was last advanced
uint32_t test_saved_s = 0;
char test_text[16];
char last_test_text[16];

char toast_text[40];
uint32_t toast_until = 0;

struct Button {
    explicit Button(uint8_t p) : pin(p) {}

    uint8_t pin;
    bool down = false;
    bool long_fired = false;
    uint32_t pressed_ms = 0;
};

Button boot_button(PIN_BUTTON_BOOT);
Button key_button(PIN_BUTTON_KEY);

bool touch_down = false;
bool touch_fired = false;     // the action already ran (auto-repeat, wake, circle key)
bool touch_cancelled = false; // a physical button was involved; ignore this touch
uint32_t last_button_ms = 0;
UiAction touch_action;
uint32_t touch_start_ms = 0;
int touch_x0 = 0, touch_y0 = 0;   // where the touch started (screen coordinates)
int touch_x = 0, touch_y = 0;     // where it is now
uint32_t last_repeat_ms = 0;

// Seconds on the RTC clock, which keeps running through deep sleep.
uint32_t now_s()
{
    return (uint32_t)time(nullptr);
}

int32_t timer_left_s()
{
    int32_t total = saved.timer_minutes * 60;
    if (saved.match_start == 0) {
        return total;
    }
    uint32_t now = saved.timer_paused_at ? saved.timer_paused_at : now_s();
    return total - (int32_t)(now - saved.match_start);
}

Preferences prefs;

void new_match()
{
    uint16_t n = (uint16_t)(prefs.getUShort("match_n", 0) + 1);
    if (n == 0) {
        n = 1;
    }
    prefs.putUShort("match_n", n);
    saved.match_no = n;
    bb_match_init(&saved.match, saved.best_of ? saved.best_of : 1);
    saved.match_start = 0;
    saved.timer_paused_at = 0;
}

bool screen_always_on();

UiState ui_state()
{
    UiState s;
    s.game = &game;
    s.step = saved.step;
    s.battery_percent = battery_percent;
    s.layout = saved.layout;
    s.flipped = saved.flipped;
    s.vivid = saved.vivid;
    s.menu_page = menu_page;
    s.counter_style = saved.counter_style;
    s.spin = spin;
    s.spin_dir = spin_dir;
    s.prev = prev_value;
    s.end_prompt = end_prompt;
    s.menu = menu_open;
    s.toast = toast_until ? toast_text : nullptr;
    s.match = saved.best_of && game.players <= 2 ? &saved.match : nullptr;
    s.best_of = saved.best_of;
    s.timer_minutes = saved.timer_minutes;
    s.timer_running = saved.timer_minutes > 0;
    s.timer_left_s = timer_left_s();
    s.timer_state = saved.match_start == 0 ? 0 : saved.timer_paused_at ? 2 : 1;
    s.dice = dice_open ? &dice : nullptr;
    s.picker = picker_open ? &picker : nullptr;
    s.entry = entry_open ? &entry : nullptr;
    s.always_on = test.running || screen_always_on();
    s.brightness_text = brightness_text;
    s.dim_after_text = dim_after_text;
    s.dim_to_text = dim_to_text;
    s.sleep_after_text = sleep_after_text;
    s.test_text = test.running ? test_text : nullptr;
    s.last_test = test.last_secs ? last_test_text : nullptr;
    s.since_charge = since_charge_text;
    static char hint[40];
    snprintf(hint, sizeof(hint), "A %s  B %s", kControlNames[controls[0] % CtlCount],
             kControlNames[controls[2] % CtlCount]);
    s.button_hint = hint;
    for (int i = 0; i < 8; i++) {
        s.controls[i] = kControlNames[controls[i] % CtlCount];
    }
    s.overlay = overlay;
    s.editor = &editor;
    s.history = history;
    s.history_count = history_count;
    s.history_page = history_page;
    s.history_sel = history_sel;
    s.history_detail_page = history_detail_page;
    s.history_delete_armed = history_delete_armed;
    s.match_legend[0] = saved.legend[0];
    s.match_legend[1] = saved.legend[1];
    s.legend_side = legend_side;
    s.legend_current = legend_for_history && history_sel >= 0 ? history[history_sel].legend[legend_side]
                                                              : saved.legend[legend_side];
    s.legend_page = legend_page;
    s.legend_sort = legend_sort;
    s.custom_legends = custom_legends;
    s.custom_legend_count = custom_legend_count;
    s.keyboard_text = keyboard_text;
    s.card_lang = card_lang;
    s.card_lang_text = kCardLangNames[card_lang % kCardLangCount];
    return s;
}

void show_toast(const char *msg, uint32_t now)
{
    snprintf(toast_text, sizeof(toast_text), "%s", msg);
    toast_until = now + kToastMs;
    if (toast_until == 0) {
        toast_until = 1;
    }
    dirty = true;
}

void start_game(size_t index, uint8_t players)
{
    saved.profile_index = (uint8_t)(index % bb_profile_count());
    const bb_profile_t *profile = bb_profile_at(saved.profile_index);
    if (profile->style == BB_STYLE_GAUGE) {
        players = 2; // a shared gauge always has two sides
    }
    bb_game_init(&game, profile, players);
    new_match();
    dirty = true;
}

uint8_t pct_to_level(uint8_t pct)
{
    return (uint8_t)((pct * 255 + 50) / 100);
}

uint8_t full_brightness()
{
    return pct_to_level(screen.brightness);
}

void format_seconds(char *out, size_t n, uint16_t secs)
{
    if (secs == 0) {
        snprintf(out, n, "Never");
    } else if (secs < 60) {
        snprintf(out, n, "%u s", secs);
    } else if (secs % 60 == 0) {
        snprintf(out, n, "%u min", secs / 60);
    } else {
        snprintf(out, n, "%u:%02u", secs / 60, secs % 60);
    }
}

void format_minutes(char *out, size_t n, uint16_t mins)
{
    if (mins == 0) {
        snprintf(out, n, "Never");
    } else {
        snprintf(out, n, "%u min", mins);
    }
}

void screen_texts()
{
    snprintf(brightness_text, sizeof(brightness_text), "%u%%", screen.brightness);
    format_seconds(dim_after_text, sizeof(dim_after_text), screen.dim_after);
    snprintf(dim_to_text, sizeof(dim_to_text), screen.dim_to ? "%u%%" : "Off", screen.dim_to);
    format_minutes(sleep_after_text, sizeof(sleep_after_text), screen.sleep_after);
}

void screen_load()
{
    screen.brightness = constrain(prefs.getUChar("b_pct", screen.brightness), kMinBrightness, 100);
    screen.dim_after = min<uint16_t>(prefs.getUShort("da_s", screen.dim_after), kMaxDimAfterS);
    screen.dim_to = min<uint8_t>(prefs.getUChar("dt_pct", screen.dim_to), kMaxDimTo);
    screen.sleep_after = min<uint16_t>(prefs.getUShort("sa_min", screen.sleep_after), kMaxSleepMin);
    screen_texts();
}

void screen_save()
{
    prefs.putUChar("b_pct", screen.brightness);
    prefs.putUShort("da_s", screen.dim_after);
    prefs.putUChar("dt_pct", screen.dim_to);
    prefs.putUShort("sa_min", screen.sleep_after);
    screen_texts();
}

bool screen_always_on()
{
    return screen.dim_after == 0 && screen.sleep_after == 0;
}

// Fills the editor view for the setting being edited.
void editor_refresh()
{
    editor = EditorView();
    editor.value = editor_value;
    switch (editor_which) {
    case 0:
        editor.title = "BRIGHTNESS";
        snprintf(editor_value, sizeof(editor_value), "%u%%", screen.brightness);
        editor.slider = true;
        editor.frac = (uint16_t)((screen.brightness - kMinBrightness) * 1000 / (100 - kMinBrightness));
        editor.preset_count = sizeof(kBrightnessPresets);
        for (int i = 0; i < editor.preset_count; i++) {
            static char labels[4][6];
            snprintf(labels[i], sizeof(labels[i]), "%u%%", kBrightnessPresets[i]);
            editor.presets[i] = labels[i];
            if (kBrightnessPresets[i] == screen.brightness) {
                editor.preset_sel = (int8_t)i;
            }
        }
        break;
    case 1:
        editor.title = "DIM AFTER";
        format_seconds(editor_value, sizeof(editor_value), screen.dim_after);
        editor.preset_count = sizeof(kDimAfterPresets) / sizeof(kDimAfterPresets[0]);
        for (int i = 0; i < editor.preset_count; i++) {
            static char labels[6][8];
            if (kDimAfterPresets[i] == 0) {
                snprintf(labels[i], sizeof(labels[i]), "Never");
            } else if (kDimAfterPresets[i] < 60) {
                snprintf(labels[i], sizeof(labels[i]), "%us", kDimAfterPresets[i]);
            } else {
                snprintf(labels[i], sizeof(labels[i]), "%um", kDimAfterPresets[i] / 60);
            }
            editor.presets[i] = labels[i];
            if (kDimAfterPresets[i] == screen.dim_after) {
                editor.preset_sel = (int8_t)i;
            }
        }
        break;
    case 2:
        editor.title = "DIM TO";
        snprintf(editor_value, sizeof(editor_value), screen.dim_to ? "%u%%" : "Off", screen.dim_to);
        editor.slider = true;
        editor.frac = (uint16_t)(screen.dim_to * 1000 / kMaxDimTo);
        editor.preset_count = sizeof(kDimToPresets);
        for (int i = 0; i < editor.preset_count; i++) {
            static char labels[4][6];
            snprintf(labels[i], sizeof(labels[i]), kDimToPresets[i] ? "%u%%" : "Off", kDimToPresets[i]);
            editor.presets[i] = labels[i];
            if (kDimToPresets[i] == screen.dim_to) {
                editor.preset_sel = (int8_t)i;
            }
        }
        break;
    default:
        editor.title = "SLEEP AFTER";
        format_minutes(editor_value, sizeof(editor_value), screen.sleep_after);
        editor.preset_count = sizeof(kSleepAfterPresets) / sizeof(kSleepAfterPresets[0]);
        for (int i = 0; i < editor.preset_count; i++) {
            static char labels[6][8];
            snprintf(labels[i], sizeof(labels[i]), kSleepAfterPresets[i] ? "%um" : "Never", kSleepAfterPresets[i]);
            editor.presets[i] = labels[i];
            if (kSleepAfterPresets[i] == screen.sleep_after) {
                editor.preset_sel = (int8_t)i;
            }
        }
        break;
    }
}

// - / + steps: finer near zero, coarser further out.
uint16_t step_dim_after(uint16_t v, int dir)
{
    if (dir > 0) {
        v = v == 0 ? 5 : v < 60 ? v + 5 : v < 300 ? v + 15 : v + 60;
        return min<uint16_t>(v, kMaxDimAfterS);
    }
    return v <= 5 ? 0 : v <= 60 ? v - 5 : v <= 300 ? v - 15 : v - 60;
}

uint16_t step_sleep_after(uint16_t v, int dir)
{
    if (dir > 0) {
        v = v < 10 ? v + 1 : v + 5;
        return min<uint16_t>(v, kMaxSleepMin);
    }
    return v <= 1 ? 0 : v <= 10 ? v - 1 : v - 5;
}

// ---- Flash storage: controls, history, legends, the game ----------------------

void controls_load()
{
    uint8_t buf[8];
    if (prefs.getBytes("ctl", buf, sizeof(buf)) == sizeof(buf)) {
        for (int i = 0; i < 8; i++) {
            controls[i] = buf[i] < CtlCount ? buf[i] : CtlNone;
        }
    }
}

void controls_save()
{
    prefs.putBytes("ctl", controls, sizeof(controls));
}

void history_save();

// History as saved before r33 (no match number or scoring order).
struct HistoryEntryV1 {
    uint16_t number;
    uint8_t profile_index, players, result, best_of, game_in_match;
    uint8_t legend[2];
    uint16_t minutes;
    int16_t value[2];
    int16_t tally[2][2];
};

void history_load()
{
    history_count = 0;
    size_t len = prefs.getBytesLength("hist");
    if (len > 0 && len % sizeof(HistoryEntry) == 0 && len <= sizeof(history)) {
        prefs.getBytes("hist", history, len);
        history_count = (int)(len / sizeof(HistoryEntry));
    } else if (len > 0 && len % sizeof(HistoryEntryV1) == 0 && len / sizeof(HistoryEntryV1) <= kMaxHistory) {
        static HistoryEntryV1 old[kMaxHistory];
        prefs.getBytes("hist", old, len);
        history_count = (int)(len / sizeof(HistoryEntryV1));
        for (int i = 0; i < history_count; i++) {
            HistoryEntry &e = history[i];
            e = HistoryEntry();
            e.number = old[i].number;
            e.profile_index = old[i].profile_index;
            e.players = old[i].players;
            e.result = old[i].result;
            e.best_of = old[i].best_of;
            e.game_in_match = old[i].game_in_match;
            e.legend[0] = old[i].legend[0];
            e.legend[1] = old[i].legend[1];
            e.minutes = old[i].minutes;
            memcpy(e.value, old[i].value, sizeof(e.value));
            memcpy(e.tally, old[i].tally, sizeof(e.tally));
        }
        history_save(); // in the new format from now on
    }
    history_next = prefs.getUShort("hist_n", 1);
}

void history_save()
{
    if (history_count > 0) {
        prefs.putBytes("hist", history, history_count * sizeof(HistoryEntry));
    } else {
        prefs.remove("hist");
    }
    prefs.putUShort("hist_n", history_next);
}

void legends_load()
{
    custom_legend_count = 0;
    size_t len = prefs.getBytesLength("legends");
    if (len > 0 && len % sizeof(custom_legends[0]) == 0 && len <= sizeof(custom_legends)) {
        prefs.getBytes("legends", custom_legends, len);
        custom_legend_count = (int)(len / sizeof(custom_legends[0]));
        for (int i = 0; i < custom_legend_count; i++) {
            custom_legends[i][kLegendNameLen] = 0;
        }
    }
}

void legends_save()
{
    prefs.putBytes("legends", custom_legends, custom_legend_count * sizeof(custom_legends[0]));
}

// The game and its settings, so a flat battery or a reset doesn't lose them.
// Only written when something changed, at most every kGameSaveMs.
Saved last_stored;

void game_store(bool force)
{
    static uint32_t last_ms = 0;
    uint32_t now = millis();
    if (!force && now - last_ms < kGameSaveMs) {
        return;
    }
    last_ms = now;
    Saved copy = saved;
    copy.magic = kSavedMagic;
    if (memcmp(&copy, &last_stored, sizeof(copy)) == 0) {
        return;
    }
    prefs.putBytes("game", &copy, sizeof(copy));
    last_stored = copy;
}

bool game_restore()
{
    Saved copy;
    if (prefs.getBytesLength("game") != sizeof(copy) || prefs.getBytes("game", &copy, sizeof(copy)) != sizeof(copy)) {
        return false;
    }
    if (copy.magic != kSavedMagic || copy.profile_index >= bb_profile_count()) {
        return false;
    }
    last_stored = copy;
    saved = copy;
    game.profile = bb_profile_at(saved.profile_index);
    // The RTC clock restarted, so times measured on it are gone.
    saved.match_start = 0;
    saved.timer_paused_at = 0;
    saved.game_start = 0;
    return true;
}

// Since the last charge.
void charge_texts()
{
    uint32_t m = charge_secs / 60;
    if (m >= 24 * 60) {
        snprintf(since_charge_text, sizeof(since_charge_text), "%lud %luh", (unsigned long)(m / 1440),
                 (unsigned long)(m % 1440 / 60));
    } else if (m >= 60) {
        snprintf(since_charge_text, sizeof(since_charge_text), "%luh %02lum", (unsigned long)(m / 60),
                 (unsigned long)(m % 60));
    } else {
        snprintf(since_charge_text, sizeof(since_charge_text), "%lum", (unsigned long)m);
    }
}

void charge_save()
{
    prefs.putUInt("c_secs", charge_secs);
    prefs.putUShort("c_min", charge_min_mv);
}

void charge_load(bool woke)
{
    charge_secs = prefs.getUInt("c_secs", 0);
    charge_min_mv = prefs.getUShort("c_min", 0);
    if (!woke) {
        charge_tick_s = 0; // the RTC clock restarted
    }
    charge_texts();
}

// Called with each battery reading.
void charge_track(uint16_t mv)
{
    uint32_t t = now_s();
    if (charge_tick_s == 0 || t < charge_tick_s) { // long sleeps count: the RTC clock keeps running
        charge_tick_s = t;
    }
    bool usb = mv > 4300;
    bool rose = charge_min_mv != 0 && mv >= charge_min_mv + kChargeRiseMv;
    charge_rises = rose ? charge_rises + 1 : 0;
    if (usb || charge_rises >= 2 || charge_min_mv == 0) {
        // Charging (or first reading ever): start counting again from here.
        bool changed = charge_secs != 0 || charge_min_mv != mv;
        charge_secs = 0;
        charge_min_mv = mv;
        charge_rises = 0;
        charge_tick_s = t;
        if (changed && t - charge_saved_s >= 60) {
            charge_saved_s = t;
            charge_save();
        }
    } else {
        charge_secs += t - charge_tick_s;
        charge_tick_s = t;
        if (mv < charge_min_mv) {
            charge_min_mv = mv;
        }
        if (t - charge_saved_s >= kChargeSaveS) {
            charge_saved_s = t;
            charge_save();
        }
    }
    char before[16];
    snprintf(before, sizeof(before), "%s", since_charge_text);
    charge_texts();
    if (strcmp(before, since_charge_text) != 0) {
        dirty = true;
    }
}

// Returns true if this input should be ignored because it only woke the screen.
bool register_input(uint32_t now)
{
    last_input_ms = now;
    if (dimmed) {
        dimmed = false;
        lcd.setBrightness(full_brightness());
        return true;
    }
    return false;
}

void format_duration(char *out, size_t n, uint32_t secs)
{
    uint32_t m = secs / 60;
    if (m >= 60) {
        snprintf(out, n, "%luh %02lum", (unsigned long)(m / 60), (unsigned long)(m % 60));
    } else {
        snprintf(out, n, "%lum", (unsigned long)m);
    }
}

void test_texts()
{
    format_duration(test_text, sizeof(test_text), test.secs);
    format_duration(last_test_text, sizeof(last_test_text), test.last_secs);
}

void test_save()
{
    prefs.putBool("t_run", test.running);
    prefs.putUInt("t_secs", test.secs);
    prefs.putUShort("t_mv0", test.start_mv);
    prefs.putUShort("t_mv", test.end_mv);
    prefs.putUInt("l_secs", test.last_secs);
    prefs.putUShort("l_mv0", test.last_start_mv);
    prefs.putUShort("l_mv", test.last_end_mv);
}

void test_load()
{
    test.running = prefs.getBool("t_run", false);
    test.secs = prefs.getUInt("t_secs", 0);
    test.start_mv = prefs.getUShort("t_mv0", 0);
    test.end_mv = prefs.getUShort("t_mv", 0);
    test.last_secs = prefs.getUInt("l_secs", 0);
    test.last_start_mv = prefs.getUShort("l_mv0", 0);
    test.last_end_mv = prefs.getUShort("l_mv", 0);
}

// Files the running test as the last one.
void test_finish()
{
    test.running = false;
    test.last_secs = test.secs;
    test.last_start_mv = test.start_mv;
    test.last_end_mv = test.end_mv;
    test_save();
    test_texts();
    Serial.printf("battery test: %lu s, %u -> %u mV\n", (unsigned long)test.last_secs, test.last_start_mv,
                  test.last_end_mv);
}

void test_start()
{
    test.running = true;
    test.secs = 0;
    test.start_mv = test.end_mv = battery_mv;
    test_tick_s = now_s();
    test_saved_s = test_tick_s;
    test_save();
    test_texts();
}

// Advances the running test; called with each battery reading.
void test_tick()
{
    if (!test.running) {
        return;
    }
    uint32_t t = now_s();
    uint32_t d = t - test_tick_s;
    test_tick_s = t;
    if (d > 24 * 3600) {
        d = 0; // the RTC clock was reset; skip rather than count nonsense
    }
    test.secs += d;
    test.end_mv = battery_mv;
    char before[16];
    snprintf(before, sizeof(before), "%s", test_text);
    test_texts();
    if (strcmp(before, test_text) != 0) {
        dirty = true;
    }
    if (t - test_saved_s >= kTestSaveS) {
        test_saved_s = t;
        test_save();
    }
}

void go_to_sleep()
{
    Serial.println("sleeping");
    bb_game_commit(&game);
    saved.magic = kSavedMagic;
    if (test.running) {
        test_tick_s = now_s();
        test_save();
    }
    charge_save();
    game_store(true);
    overlay = Overlay::None;

    lcd.setBrightness(0);
    lcd.sleep();
    digitalWrite(PIN_POWER_ON, LOW);

    // Don't wake straight back up if KEY is still held.
    while (digitalRead(PIN_BUTTON_KEY) == LOW) {
        delay(10);
    }
    rtc_gpio_pullup_en((gpio_num_t)PIN_BUTTON_KEY);
    rtc_gpio_pulldown_dis((gpio_num_t)PIN_BUTTON_KEY);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BUTTON_KEY, 0);
    esp_deep_sleep_start();
}

uint32_t hardware_random(void *)
{
    return esp_random();
}

void start_roll(uint32_t now)
{
    if (dice.rolling) {
        return;
    }
    dice.rolling = true;
    dice.anim = 0;
    roll_start_ms = now;
    roll_face_ms = 0;
    anim_frame_ms = 0;
    dirty = true;
}

// Tumbles with random faces that change more and more slowly, then lands on
// the real result and plays a short pop.
void update_roll(uint32_t now)
{
    if (!dice.rolling) {
        if (dice.pop < 1000) {
            uint32_t e = now - land_ms;
            uint8_t sides = bb_die_at(dice.die)->sides;
            bool extreme = sides >= 4 && (dice.value == sides || dice.value == 1);
            uint32_t len = extreme ? kCelebrateMs : kPopMs;
            dice.pop = e >= len ? 1000 : (uint16_t)(e * 1000 / len);
            if (now - anim_frame_ms >= kAnimFrameMs || dice.pop == 1000) {
                anim_frame_ms = now;
                dirty = true;
            }
        }
        return;
    }
    uint8_t sides = bb_die_at(dice.die)->sides;
    uint32_t elapsed = now - roll_start_ms;
    if (elapsed >= kRollMs) {
        dice.value = bb_dice_roll(sides, hardware_random, nullptr);
        dice.rolling = false;
        dice.anim = 1000;
        dice.pop = 0;
        land_ms = now;
        memmove(dice.history + 1, dice.history, DiceState::kHistory - 1);
        memmove(dice.history_die + 1, dice.history_die, DiceState::kHistory - 1);
        dice.history[0] = dice.value;
        dice.history_die[0] = dice.die;
        if (dice.history_count < DiceState::kHistory) {
            dice.history_count++;
        }
        dirty = true;
        return;
    }

    dice.anim = (uint16_t)(elapsed * 1000 / kRollMs);
    // Faces change every 40 ms at first, slowing to every ~200 ms near the end.
    uint32_t face_interval = 40 + 160 * dice.anim / 1000 * dice.anim / 1000;
    if (roll_face_ms == 0 || now - roll_face_ms >= face_interval) {
        roll_face_ms = now;
        dice.value = bb_dice_roll(sides, hardware_random, nullptr);
    }
    if (now - anim_frame_ms >= kAnimFrameMs) {
        anim_frame_ms = now;
        dirty = true;
    }
}

void undo(uint32_t now)
{
    bb_entry_t e;
    if (!bb_game_undo(&game, &e)) {
        show_toast("Nothing to undo", now);
        return;
    }

    char msg[40];
    int len = snprintf(msg, sizeof(msg), "Undo");
    if (game.players > 1) {
        len += snprintf(msg + len, sizeof(msg) - len, " %s", ui_player_label(game, e.player));
    }
    len += snprintf(msg + len, sizeof(msg) - len, " %+d", e.delta);
    if (game.profile->kind_count > 0) {
        snprintf(msg + len, sizeof(msg) - len, " %s", game.profile->kinds[e.kind]);
    }
    show_toast(msg, now);
}

void adjust(uint8_t player, uint8_t kind, int16_t delta, uint32_t now)
{
    int16_t before = player < BB_MAX_PLAYERS ? game.value[player] : 0;
    int16_t applied = bb_game_adjust(&game, player, kind, delta, now);
    if (applied != 0) {
        if (player < BB_MAX_PLAYERS) {
            prev_value[player] = before;
        }
        dirty = true;
        if (player < BB_MAX_PLAYERS) {
            spin[player] = 0;
            spin_dir[player] = applied > 0 ? 1 : -1;
            spin_start_ms[player] = now;
        }
        if (saved.match_start == 0) {
            saved.match_start = now_s(); // the timer starts with the first change
        }
        if (saved.game_start == 0) {
            saved.game_start = now_s();
        }
    }
}

// Adds the game just finished to the history. result: 0 / 1 = that side won, 2 = draw.
void record_result(uint8_t result)
{
    HistoryEntry e = {};
    e.number = history_next++;
    e.profile_index = saved.profile_index;
    e.players = game.players;
    e.result = result;
    e.best_of = saved.best_of;
    e.game_in_match = (uint8_t)(saved.match.games + 1);
    e.legend[0] = saved.legend[0];
    e.legend[1] = saved.legend[1];
    e.minutes = saved.game_start ? (uint16_t)min<uint32_t>((now_s() - saved.game_start + 30) / 60, 0xFFFF) : 0;
    for (int side = 0; side < 2 && side < game.players; side++) {
        e.value[side] = game.value[side];
        e.tally[side][0] = game.tally[side][0];
        e.tally[side][1] = game.profile->kind_count >= 2 ? game.tally[side][1] : 0;
    }
    e.match_no = saved.match_no;
    // The scoring in order (the game's undo history is newest first).
    bb_game_commit(&game);
    static bb_entry_t past[BB_HISTORY_LEN];
    size_t n = bb_game_history(&game, past, BB_HISTORY_LEN);
    for (size_t i = n; i-- > 0 && e.event_count < kHistoryEvents;) {
        if (past[i].player < 2 && past[i].delta != 0) {
            e.events[e.event_count++] = ui_history_pack(past[i].player, past[i].kind, past[i].delta);
        }
    }
    int keep = history_count < kMaxHistory ? history_count : kMaxHistory - 1;
    memmove(&history[1], &history[0], keep * sizeof(HistoryEntry));
    history[0] = e;
    history_count = keep + 1;
    history_save();
    saved.game_start = 0;
}

void open_legends(bool for_history, uint8_t side, bool chain)
{
    legend_for_history = for_history;
    legend_side = side;
    legend_chain = chain;
    legend_page = 0;
    overlay = Overlay::Legends;
    dirty = true;
}

void set_legend(uint8_t code)
{
    if (legend_for_history) {
        if (history_sel >= 0 && history_sel < history_count) {
            int end = ui_history_group_end(history, history_count, history_sel);
            for (int i = history_sel; i < end; i++) {
                history[i].legend[legend_side] = code; // every game of the match
            }
            history_save();
        }
        overlay = Overlay::HistoryDetail;
    } else {
        saved.legend[legend_side] = code;
        if (legend_chain && legend_side == 0) {
            legend_side = 1; // now theirs
            legend_page = 0;
        } else {
            overlay = Overlay::None;
        }
    }
    dirty = true;
}

void toggle_menu();
void toggle_dice();

// Runs what a button or swipe is set to do.
void run_control(uint8_t control, uint32_t now)
{
    switch (control) {
    case CtlUndo:
        if (overlay == Overlay::None) {
            undo(now);
        }
        break;
    case CtlMenu:
        overlay = Overlay::None;
        toggle_menu();
        break;
    case CtlQuick:
        overlay = overlay == Overlay::Quick ? Overlay::None : Overlay::Quick;
        dirty = true;
        break;
    case CtlDice:
        overlay = Overlay::None;
        toggle_dice();
        break;
    case CtlOff:
        overlay = Overlay::OffPrompt;
        dirty = true;
        break;
    default:
        break;
    }
}

// KEY / BOOT pressed while a pop-up is open: back out of it.
void close_overlay()
{
    switch (overlay) {
    case Overlay::HistoryDetail:
        overlay = Overlay::History;
        break;
    case Overlay::Keyboard:
        overlay = Overlay::Legends;
        break;
    case Overlay::Legends:
        overlay = legend_for_history ? Overlay::HistoryDetail : Overlay::None;
        break;
    case Overlay::Sleeping:
        return; // too late
    default:
        overlay = Overlay::None;
        break;
    }
    dirty = true;
}

void perform(const UiAction &a, uint32_t now)
{
    switch (a.type) {
    case UiAction::Adjust:
        adjust(a.player, a.kind, a.delta, now);
        break;
    case UiAction::PickFamily: {
        size_t idx[4];
        size_t n = bb_family_profiles(a.kind, idx, 4);
        if (n == 1) {
            start_game(idx[0], game.players);
            picker_open = false;
        } else {
            picker.family = a.kind;
            picker.origin = a.kind;
            picker.anim = 0;
            burst_start_ms = now;
        }
        dirty = true;
        break;
    }
    case UiAction::PickProfile:
        start_game(a.kind, game.players); // clamped to the new game's limit
        picker_open = false;
        dirty = true;
        break;
    case UiAction::PickerBack:
        picker.family = -1;
        dirty = true;
        break;
    case UiAction::PickerClose:
        picker_open = false;
        dirty = true;
        break;
    case UiAction::GaugeSet:
        adjust(0, 0, (int16_t)(a.delta - game.value[0]), now);
        break;
    case UiAction::GaugePass: {
        // Passing leaves the opponent 3 memory on their side.
        int16_t target = bb_gauge_owner(&game) == 0 ? 3 : -3;
        adjust(0, 0, (int16_t)(target - game.value[0]), now);
        break;
    }
    case UiAction::OpenEntry:
        entry_open = true;
        entry.player = a.player;
        entry.amount = 0;
        dirty = true;
        break;
    case UiAction::EntryDigit: {
        int32_t next = a.kind == 10 ? entry.amount * 100 : entry.amount * 10 + a.kind;
        if (next <= kMaxEntry) {
            entry.amount = next;
            dirty = true;
        }
        break;
    }
    case UiAction::EntryDelete:
        entry.amount /= 10;
        dirty = true;
        break;
    case UiAction::EntryApply:
        if (entry.amount > 0) {
            adjust(entry.player, 0, (int16_t)(a.delta * entry.amount), now);
            bb_game_commit(&game); // a typed amount is one history entry on its own
        }
        entry_open = false;
        dirty = true;
        break;
    case UiAction::EntryHalve:
        adjust(entry.player, 0, (int16_t)-(game.value[entry.player] / 2), now);
        bb_game_commit(&game);
        entry_open = false;
        dirty = true;
        break;
    case UiAction::EntryClose:
        entry_open = false;
        dirty = true;
        break;
    case UiAction::NextGame: {
        int winner = bb_game_winner(&game);
        if (winner < 0) {
            break;
        }
        record_result((uint8_t)winner);
        bb_match_record(&saved.match, (uint8_t)winner);
        if (bb_match_winner(&saved.match) >= 0) {
            new_match();
        }
        bb_game_reset(&game);
        dirty = true;
        break;
    }
    case UiAction::MenuMatch: {
        size_t i = 0;
        while (i < sizeof(kBestOfChoices) && kBestOfChoices[i] != saved.best_of) {
            i++;
        }
        saved.best_of = kBestOfChoices[(i + 1) % sizeof(kBestOfChoices)];
        bb_match_init(&saved.match, saved.best_of ? saved.best_of : 1); // keeps the timer
        dirty = true;
        break;
    }
    case UiAction::MenuTimer: {
        size_t i = 0;
        while (i < sizeof(kTimerChoices) && kTimerChoices[i] != saved.timer_minutes) {
            i++;
        }
        saved.timer_minutes = kTimerChoices[(i + 1) % sizeof(kTimerChoices)];
        saved.match_start = 0; // waits for a tap (or the first change)
        saved.timer_paused_at = 0;
        dirty = true;
        break;
    }
    case UiAction::ToggleStep:
        saved.step = saved.step == 1 ? 5 : 1;
        dirty = true;
        break;
    case UiAction::MenuGame:
        menu_open = false;
        picker_open = true;
        picker.family = -1;
        picker.anim = 1000;
        dirty = true;
        break;
    case UiAction::MenuPlayers:
        start_game(saved.profile_index, game.players < game.profile->max_players ? game.players + 1 : 1);
        break;
    case UiAction::MenuLayout:
        saved.layout = ui_next_layout(game, saved.layout);
        dirty = true;
        break;
    case UiAction::TimerTap: {
        uint32_t t = now_s();
        if (saved.match_start == 0) {
            saved.match_start = t;
            saved.timer_paused_at = 0;
            show_toast("Timer started", now);
        } else if (saved.timer_paused_at) {
            saved.match_start += t - saved.timer_paused_at;
            saved.timer_paused_at = 0;
            show_toast("Timer running", now);
        } else {
            saved.timer_paused_at = t;
            show_toast("Timer paused - tap to resume", now);
        }
        dirty = true;
        break;
    }
    case UiAction::MenuTab:
        menu_page = a.kind;
        dirty = true;
        break;
    case UiAction::MenuMore:
        menu_page = kMenuLook;
        dirty = true;
        break;
    case UiAction::MenuBack:
        menu_page = menu_page == kMenuDisplay ? kMenuLook : kMenuSystem;
        dirty = true;
        break;
    case UiAction::MenuCounter:
        saved.counter_style = (uint8_t)((saved.counter_style + 1) % 3);
        dirty = true;
        break;
    case UiAction::MenuOff:
        overlay = Overlay::OffPrompt;
        dirty = true;
        break;
    case UiAction::MenuControls:
        menu_page = kMenuControls;
        dirty = true;
        break;
    case UiAction::MenuBattery:
        menu_page = kMenuBattery;
        dirty = true;
        break;
    case UiAction::MenuHistory:
        history_page = 0;
        history_sel = -1;
        overlay = Overlay::History;
        dirty = true;
        break;
    case UiAction::MenuLegends:
        open_legends(false, 0, true);
        break;
    case UiAction::ControlCycle:
        if (a.kind < 8) {
            controls[a.kind] = (uint8_t)((controls[a.kind] + 1) % CtlCount);
            controls_save();
            dirty = true;
        }
        break;
    case UiAction::QuickPick:
        overlay = Overlay::None;
        if (a.kind == 0) {
            undo(now);
        } else if (a.kind == 1) {
            menu_open = picker_open = entry_open = end_prompt = false;
            dice_open = true;
        } else if (a.kind == 2) {
            picker_open = entry_open = dice_open = end_prompt = false;
            menu_open = true;
            menu_page = 0;
        } else {
            overlay = Overlay::OffPrompt;
        }
        dirty = true;
        break;
    case UiAction::QuickClose:
    case UiAction::OffNo:
        overlay = Overlay::None;
        dirty = true;
        break;
    case UiAction::OffYes:
        overlay = Overlay::Sleeping;
        sleeping_at = now;
        dirty = true;
        break;
    case UiAction::ScreenOpen:
        editor_which = a.kind;
        editor_refresh();
        overlay = Overlay::Editor;
        dirty = true;
        break;
    case UiAction::EditorSlide: {
        if (editor_which == 0) {
            screen.brightness = (uint8_t)(kMinBrightness + (a.delta * (100 - kMinBrightness) + 500) / 1000);
            lcd.setBrightness(full_brightness());
        } else if (editor_which == 2) {
            screen.dim_to = (uint8_t)((a.delta * kMaxDimTo + 500) / 1000);
        }
        screen_save();
        editor_refresh();
        dirty = true;
        break;
    }
    case UiAction::EditorStep:
        if (editor_which == 1) {
            screen.dim_after = step_dim_after(screen.dim_after, a.delta);
        } else if (editor_which == 3) {
            screen.sleep_after = step_sleep_after(screen.sleep_after, a.delta);
        }
        screen_save();
        editor_refresh();
        dirty = true;
        break;
    case UiAction::EditorPreset:
        switch (editor_which) {
        case 0:
            screen.brightness = kBrightnessPresets[a.kind % sizeof(kBrightnessPresets)];
            lcd.setBrightness(full_brightness());
            break;
        case 1:
            screen.dim_after = kDimAfterPresets[a.kind % (sizeof(kDimAfterPresets) / sizeof(kDimAfterPresets[0]))];
            break;
        case 2:
            screen.dim_to = kDimToPresets[a.kind % sizeof(kDimToPresets)];
            break;
        default:
            screen.sleep_after =
                kSleepAfterPresets[a.kind % (sizeof(kSleepAfterPresets) / sizeof(kSleepAfterPresets[0]))];
            break;
        }
        screen_save();
        editor_refresh();
        dirty = true;
        break;
    case UiAction::EditorClose:
        overlay = Overlay::None;
        dirty = true;
        break;
    case UiAction::HistoryOpen:
        if (a.kind < history_count) {
            history_sel = a.kind;
            history_detail_page = 0;
            history_delete_armed = false;
            overlay = Overlay::HistoryDetail;
            dirty = true;
        }
        break;
    case UiAction::HistoryGames: {
        int games = ui_history_group_end(history, history_count, history_sel) - history_sel;
        history_detail_page = (history_detail_page + 1) * 3 < games ? history_detail_page + 1 : 0;
        dirty = true;
        break;
    }
    case UiAction::HistoryPage: {
        int pages = (ui_history_group_count(history, history_count) + 2) / 3;
        history_page = constrain(history_page + a.delta, 0, pages > 0 ? pages - 1 : 0);
        dirty = true;
        break;
    }
    case UiAction::HistoryClose:
        overlay = Overlay::None;
        dirty = true;
        break;
    case UiAction::DetailBack:
        overlay = Overlay::History;
        dirty = true;
        break;
    case UiAction::HistoryLegend:
        open_legends(true, a.player, false);
        break;
    case UiAction::HistoryDelete:
        if (!history_delete_armed) {
            history_delete_armed = true;
        } else if (history_sel >= 0 && history_sel < history_count) {
            int end = ui_history_group_end(history, history_count, history_sel); // the whole match
            memmove(&history[history_sel], &history[end], (history_count - end) * sizeof(HistoryEntry));
            history_count -= end - history_sel;
            history_save();
            history_sel = -1;
            history_delete_armed = false;
            int pages = (ui_history_group_count(history, history_count) + 2) / 3;
            history_page = constrain(history_page, 0, pages > 0 ? pages - 1 : 0);
            overlay = Overlay::History;
        }
        dirty = true;
        break;
    case UiAction::LegendPick:
        set_legend(a.kind);
        break;
    case UiAction::LegendPage: {
        int n = custom_legend_count + (int)ui_legend_count();
        int pages = (n + 11) / 12;
        legend_page = constrain(legend_page + a.delta, 0, pages - 1);
        dirty = true;
        break;
    }
    case UiAction::LegendSort:
        legend_sort = legend_sort ? 0 : 1;
        legend_page = 0;
        prefs.putUChar("lg_sort", legend_sort);
        dirty = true;
        break;
    case UiAction::LegendAdd:
        if (custom_legend_count >= kMaxCustomLegends) {
            show_toast("12 own legends max", now);
            break;
        }
        keyboard_text[0] = 0;
        overlay = Overlay::Keyboard;
        dirty = true;
        break;
    case UiAction::LegendClose:
        overlay = legend_for_history ? Overlay::HistoryDetail : Overlay::None;
        dirty = true;
        break;
    case UiAction::KeyChar: {
        size_t len = strlen(keyboard_text);
        if (len < kLegendNameLen && !(a.kind == ' ' && (len == 0 || keyboard_text[len - 1] == ' '))) {
            keyboard_text[len] = (char)a.kind;
            keyboard_text[len + 1] = 0;
            dirty = true;
        }
        break;
    }
    case UiAction::KeyDelete: {
        size_t len = strlen(keyboard_text);
        if (len > 0) {
            keyboard_text[len - 1] = 0;
            dirty = true;
        }
        break;
    }
    case UiAction::KeyDone: {
        size_t len = strlen(keyboard_text);
        while (len > 0 && keyboard_text[len - 1] == ' ') {
            keyboard_text[--len] = 0;
        }
        if (len == 0) {
            overlay = Overlay::Legends;
            dirty = true;
            break;
        }
        // "MISS FORTUNE" -> "Miss Fortune"
        for (size_t i = 0; i < len; i++) {
            bool word_start = i == 0 || keyboard_text[i - 1] == ' ' || keyboard_text[i - 1] == '-';
            keyboard_text[i] = word_start ? (char)toupper(keyboard_text[i]) : (char)tolower(keyboard_text[i]);
        }
        int idx = custom_legend_count++;
        snprintf(custom_legends[idx], sizeof(custom_legends[idx]), "%s", keyboard_text);
        legends_save();
        overlay = Overlay::Legends;
        set_legend((uint8_t)(kCustomLegend + idx));
        break;
    }
    case UiAction::KeyCancel:
        overlay = Overlay::Legends;
        dirty = true;
        break;
    case UiAction::MenuUndo:
        menu_open = false;
        undo(now);
        dirty = true;
        break;
    case UiAction::MenuEnd:
        menu_open = false;
        end_prompt = true;
        dirty = true;
        break;
    case UiAction::DeclareWinner: {
        char msg[40];
        uint8_t game_no = saved.match.games + 1;
        record_result(a.player);
        bb_match_record(&saved.match, a.player);
        const char *who = game.players == 1 ? "You" : ui_player_label(game, a.player);
        const char *verb = strcmp(who, "YOU") == 0 ? "win" : "wins";
        if (game.players == 1) {
            verb = a.player == 0 ? "won" : "lost";
        }
        if (bb_match_winner(&saved.match) >= 0 && saved.match.best_of > 1) {
            snprintf(msg, sizeof(msg), "%s %s the match %d-%d", who, verb, saved.match.wins[0],
                     saved.match.wins[1]);
            new_match();
        } else {
            snprintf(msg, sizeof(msg), "%s %s game %d", who, verb, game_no);
            if (bb_match_winner(&saved.match) >= 0) {
                new_match();
            }
        }
        bb_game_reset(&game);
        end_prompt = false;
        show_toast(msg, now);
        break;
    }
    case UiAction::EndDraw: {
        char msg[40];
        record_result(2);
        snprintf(msg, sizeof(msg), "Draw - game %d again", saved.match.games + 1);
        bb_game_reset(&game);
        end_prompt = false;
        show_toast(msg, now);
        break;
    }
    case UiAction::CardLangNext:
        card_lang = (uint8_t)((card_lang + 1) % kCardLangCount);
        prefs.putUChar("lang_card", card_lang);
        dirty = true;
        break;
    case UiAction::EndRestart:
        bb_game_reset(&game);
        end_prompt = false;
        show_toast("New game", now);
        break;
    case UiAction::EndCancel:
        end_prompt = false;
        dirty = true;
        break;
    case UiAction::MenuScreen:
        menu_page = kMenuDisplay;
        dirty = true;
        break;
    case UiAction::MenuBatteryTest:
        if (test.running) {
            test_finish();
            show_toast("Battery test stopped", millis());
        } else if (battery_percent < 0) {
            show_toast("Unplug USB first", millis());
        } else {
            test_start();
            show_toast("Test on: screen stays lit", millis());
        }
        dirty = true;
        break;
    case UiAction::MenuLook:
        saved.vivid = !saved.vivid;
        dirty = true;
        break;
    case UiAction::MenuFlip:
        saved.flipped = !saved.flipped;
        lcd.setRotation(saved.flipped ? 3 : 1);
        dirty = true;
        break;
    case UiAction::MenuRestart:
        bb_game_reset(&game);
        new_match();
        menu_open = false;
        show_toast("New match", now);
        break;
    case UiAction::MenuDice:
        menu_open = false;
        dice_open = true;
        dirty = true;
        break;
    case UiAction::DiceSelect:
        if (!dice.rolling && a.kind != dice.die) {
            dice.die = a.kind;
            dice.value = 0;
            dirty = true;
        }
        break;
    case UiAction::DiceRoll:
        start_roll(now);
        break;
    case UiAction::DiceClose:
        dice_open = false;
        dirty = true;
        break;
    case UiAction::MenuClose:
        menu_open = false;
        menu_page = 0;
        dirty = true;
        break;
    case UiAction::None:
        break;
    }
}

void toggle_menu()
{
    menu_page = 0;
    if (end_prompt) {
        end_prompt = false;
    } else if (picker_open && picker.family >= 0) {
        picker.family = -1; // BOOT steps back from a game's formats to the games
    } else if (picker_open || entry_open || dice_open) {
        picker_open = entry_open = dice_open = false; // BOOT backs out to the counter
    } else {
        menu_open = !menu_open;
    }
    dirty = true;
}

void toggle_dice()
{
    menu_open = picker_open = entry_open = false;
    dice_open = !dice_open;
    dirty = true;
}

// Taps act when the finger lifts, so a touch can be cancelled: if a physical
// button is pressed during it (a thumb grazing the screen while reaching for
// the button), or it starts right after one, nothing happens. +/- still
// repeat while held.
void handle_touch(uint32_t now)
{
    lgfx::touch_point_t tp;
    if (lcd.getTouchRaw(&tp, 1) == 0) {
        if (touch_down && !touch_fired && !touch_cancelled) {
            int dx = touch_x - touch_x0;
            int dy = touch_y - touch_y0;
            bool swipe = (abs(dx) >= kSwipeMinX || abs(dy) >= kSwipeMinY) && now - touch_start_ms <= kSwipeMaxMs;
            bool on_counter = overlay == Overlay::None && !menu_open && !picker_open && !entry_open && !dice_open &&
                              !end_prompt;
            if (swipe && on_counter) {
                // up, down, left, right
                int dir = abs(dx) * kSwipeMinY > abs(dy) * kSwipeMinX ? (dx < 0 ? 2 : 3) : (dy < 0 ? 0 : 1);
                Serial.printf("swipe %d\n", dir);
                run_control(controls[4 + dir], now);
            } else if (!swipe && now - touch_start_ms <= kTapMaxMs) {
                perform(touch_action, now);
            }
        }
        touch_down = false;
        return;
    }

    {
        lgfx::touch_point_t sp = tp;
        lcd.convertRawXY(&sp, 1);
        touch_x = sp.x;
        touch_y = sp.y;
    }

    if (touch_down) {
        // Held press: auto-repeat where the action allows it.
        last_input_ms = now;
        // Dragging a slider follows the finger.
        if (touch_action.type == UiAction::EditorSlide && !touch_cancelled) {
            UiAction a = ui_hit(ui_state(), touch_x, touch_y);
            if (a.type == UiAction::EditorSlide && a.delta != touch_action.delta) {
                touch_action = a;
                perform(a, now);
            }
            touch_fired = true;
            return;
        }
        if (boot_button.down || key_button.down) {
            touch_cancelled = true;
        }
        if (touch_action.repeat && !touch_cancelled && now - touch_start_ms >= kRepeatDelayMs &&
            now - last_repeat_ms >= kRepeatIntervalMs) {
            last_repeat_ms = now;
            touch_fired = true;
            perform(touch_action, now);
        }
        return;
    }

    // New press.
    touch_down = true;
    touch_fired = false;
    touch_cancelled = boot_button.down || key_button.down || now - last_button_ms < kButtonGuardMs;
    touch_action = UiAction();
    touch_start_ms = now;
    touch_x0 = touch_x;
    touch_y0 = touch_y;
    last_repeat_ms = now;
    Serial.printf("touch raw x=%d y=%d%s\n", tp.x, tp.y, touch_cancelled ? " (ignored)" : "");

    if (touch_cancelled) {
        return;
    }
    if (register_input(now)) {
        touch_fired = true; // this press only woke the screen
        return;
    }
    if (tp.y >= kRawHomeKeyY) {
        touch_fired = true;
        toggle_dice();
        return;
    }

    touch_action = ui_hit(ui_state(), touch_x, touch_y);
    if (touch_action.type == UiAction::EditorSlide) {
        touch_fired = true; // a slider moves on touch, not on release
        perform(touch_action, now);
    }
}

// Calls on_short when released before kLongPressMs, on_long once when held that long.
template <typename Short, typename Long>
void poll_button(Button &b, uint32_t now, Short on_short, Long on_long)
{
    bool down = digitalRead(b.pin) == LOW;

    if (down != b.down) {
        last_button_ms = now;
        if (touch_down) {
            touch_cancelled = true;
        }
    }

    if (down && !b.down) {
        b.down = true;
        b.long_fired = false;
        b.pressed_ms = now;
    } else if (down && !b.long_fired && now - b.pressed_ms >= kLongPressMs) {
        b.long_fired = true;
        Serial.printf("button %d long\n", b.pin);
        if (!register_input(now)) {
            on_long();
        }
    } else if (!down && b.down) {
        b.down = false;
        if (!b.long_fired && now - b.pressed_ms >= 30) {
            Serial.printf("button %d short\n", b.pin);
            if (!register_input(now)) {
                on_short();
            }
        }
    }
}

void poll_battery(uint32_t now)
{
    static uint32_t last_poll = 0;
    if (last_poll != 0 && now - last_poll < kBatteryPollMs) {
        return;
    }
    last_poll = now;

    // Average a few readings: single ADC samples are noisy.
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogReadMilliVolts(PIN_BAT_VOLT);
    }
    uint32_t mv = sum / 8 * 2;

    static bool on_usb = false;
    static uint16_t prev_mv = 0;
    if (mv >= kUsbSureMv) {
        on_usb = true;
    } else if (on_usb && prev_mv && prev_mv >= mv + kUsbStepMv) {
        on_usb = false; // the voltage dropped: unplugged
    } else if (!on_usb && prev_mv && mv >= prev_mv + kUsbStepMv) {
        on_usb = true;  // the voltage jumped: plugged in
    }
    if (prev_mv && abs((int)mv - (int)prev_mv) >= 30) {
        Serial.printf("battery %u -> %u mV%s\n", prev_mv, (unsigned)mv, on_usb ? " (usb)" : "");
    }
    prev_mv = (uint16_t)mv;

    battery_mv = (uint16_t)mv;
    test_tick();
    charge_track(battery_mv);
    int percent = on_usb ? -1 : constrain(((int)mv - 3300) * 100 / 900, 0, 100);
    if (percent != battery_percent) {
        battery_percent = percent;
        dirty = true;
    }
}

} // namespace

void setup()
{
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    pinMode(PIN_BUTTON_BOOT, INPUT_PULLUP);
    pinMode(PIN_BUTTON_KEY, INPUT_PULLUP);
    Serial.begin(115200);

    bool woke = esp_reset_reason() == ESP_RST_DEEPSLEEP && saved.magic == kSavedMagic &&
                saved.profile_index < bb_profile_count();
    if (woke) {
        rtc_gpio_deinit((gpio_num_t)PIN_BUTTON_KEY);
        pinMode(PIN_BUTTON_KEY, INPUT_PULLUP);
        game.profile = bb_profile_at(saved.profile_index);
        // The KEY press that woke us must not also count as an undo.
        if (digitalRead(PIN_BUTTON_KEY) == LOW) {
            key_button.down = true;
            key_button.long_fired = true;
        }
    } else if (!(prefs.begin("bb", false) && game_restore())) {
        saved.step = 1;
        saved.layout = Layout::SideYouLeft;
        saved.flipped = false;
        saved.vivid = false;
        saved.counter_style = 0;
        saved.best_of = 1;
        saved.timer_minutes = 0;
        saved.game_start = 0;
        saved.legend[0] = saved.legend[1] = kNoLegend;
        start_game(0, 1);
    }

    prefs.begin("bb", false); // (already open after a cold start; opening twice is harmless)
    controls_load();
    card_lang = prefs.getUChar("lang_card", kCardEnglish) % kCardLangCount;
    legend_sort = prefs.getUChar("lg_sort", 0) ? 1 : 0;
    history_load();
    legends_load();
    charge_load(woke);
    test_load();
    if (test.running && !woke) {
        // Power came back after the battery ran out (or a reset): that test is done.
        test_finish();
    }
    test_texts();
    screen_load();
    saved.magic = 0; // only trusted again after the next sleep

    lcd.init();
    lcd.setRotation(saved.flipped ? 3 : 1); // landscape; unflipped = USB port on the right
    lcd.setBrightness(full_brightness());

    frame.setColorDepth(16);
    frame.setPsram(psramFound());
    frame.createSprite(kWidth, kHeight);

    last_input_ms = millis();
}

void loop()
{
    uint32_t now = millis();

    handle_touch(now);
    // KEY = A (top button), BOOT = B (bottom), with the USB on the right.
    poll_button(
        key_button, now,
        [now] {
            if (overlay != Overlay::None) {
                close_overlay();
            } else if (menu_open || picker_open || entry_open || end_prompt) {
                menu_open = picker_open = entry_open = end_prompt = false;
                menu_page = 0;
                dirty = true;
            } else if (dice_open) {
                start_roll(now);
            } else {
                run_control(controls[0], now);
            }
        },
        [now] { run_control(controls[1], now); });
    poll_button(
        boot_button, now,
        [now] {
            if (overlay != Overlay::None) {
                close_overlay();
            } else if (menu_open || picker_open || entry_open || dice_open || end_prompt) {
                toggle_menu(); // backs out
            } else {
                run_control(controls[2], now);
            }
        },
        [now] { run_control(controls[3], now); });
    poll_battery(now);

    if (bb_game_tick(&game, now)) {
        dirty = true;
    }
    update_roll(now);

    // d20 counter: spin each changed player's die to its new face.
    for (int p = 0; p < BB_MAX_PLAYERS; p++) {
        if (spin[p] < 1000) {
            uint32_t e = now - spin_start_ms[p];
            spin[p] = e >= kSpinMs ? 1000 : (uint16_t)(e * 1000 / kSpinMs);
            if (now - anim_frame_ms >= kAnimFrameMs || spin[p] == 1000) {
                dirty = true;
                anim_frame_ms = now;
            }
        }
    }

    if (picker_open && picker.anim < 1000) {
        uint32_t e = now - burst_start_ms;
        picker.anim = e >= kBurstMs ? 1000 : (uint16_t)(e * 1000 / kBurstMs);
        dirty = true;
    }
    if (toast_until && (int32_t)(now - toast_until) >= 0) {
        toast_until = 0;
        dirty = true;
    }

    // Redraw once a second while the timer is counting.
    static int32_t shown_timer_s = 0;
    if (saved.timer_minutes && saved.match_start && !saved.timer_paused_at) {
        int32_t left = timer_left_s();
        if (left != shown_timer_s) {
            shown_timer_s = left;
            dirty = true;
        }
    }

    uint32_t idle = now - last_input_ms;
    if (overlay == Overlay::Sleeping && now - sleeping_at >= kSleepingMs) {
        go_to_sleep();
    }
    game_store(false);

    uint32_t dim_ms = (uint32_t)screen.dim_after * 1000;
    uint32_t sleep_ms = (uint32_t)screen.sleep_after * 60 * 1000;
    if (test.running) {
        dim_ms = sleep_ms = 0; // the battery test keeps the screen lit
    }
    if (!dimmed && dim_ms && idle >= dim_ms) {
        dimmed = true;
        lcd.setBrightness(pct_to_level(screen.dim_to));
    }
    if (sleep_ms && idle >= sleep_ms) {
        go_to_sleep();
    }

    if (dirty) {
        dirty = false;
        ui_draw(frame, ui_state());
        frame.pushSprite(0, 0);
    }

    delay(10);
}
