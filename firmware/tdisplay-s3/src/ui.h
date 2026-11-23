#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "bb/dice.h"
#include "bb/game.h"
#include "bb/match.h"

// Landscape frame size for the T-Display-S3 (170x320 panel, rotated).
constexpr int kWidth = 320;
constexpr int kHeight = 170;

// How players share the screen. With 3-4 players only Facing vs. the others matters
// (upright quadrants vs. the top row upside down).
enum class Layout : uint8_t {
    SideYouLeft,  // side by side, YOU on the left
    SideYouRight, // side by side, YOU on the right
    Facing,       // YOU along the bottom, OPP along the top upside down (across a table)
    Count,
};

// The dice screen.
struct DiceState {
    static constexpr int kHistory = 8;

    uint8_t die = 2;     // index into bb_die_at(); d6 by default
    uint8_t value = 0;   // last roll (or the face shown mid-roll), 0 = not rolled yet
    bool rolling = false; // tumbling before the result
    uint16_t anim = 0;    // roll progress while rolling, 0..1000
    uint16_t pop = 1000;  // landing effect progress after a roll, 0..1000 (1000 = settled)
    uint8_t history[kHistory] = {}; // previous results, newest first
    uint8_t history_die[kHistory] = {};
    uint8_t history_count = 0;
};

// The game picker: a grid of games; a game with several formats bursts open
// into format tiles.
struct PickerState {
    int family = -1;      // -1 = the list of games, else that game's formats
    uint16_t anim = 1000; // format tiles bursting out of the game tile, 0..1000
    int origin = 0;       // tile the formats burst out of
};

// The number pad for typing an amount (Yu-Gi-Oh! LP).
struct EntryState {
    uint8_t player = 0;
    int32_t amount = 0;
};

// Menu: four tabs down the left, and sub-pages opened from a tab.
enum MenuPage : int {
    kMenuPlay,
    kMenuMatch,
    kMenuLook,
    kMenuSystem,
    kMenuTabCount,
    kMenuDisplay = kMenuTabCount, // from Look
    kMenuControls,                // from System
    kMenuBattery,                 // from System
};

// Pop-up screens drawn over everything else.
enum class Overlay : uint8_t {
    None,
    Quick,         // quick menu: undo / dice / menu / turn off
    OffPrompt,     // "Turn off?"
    Sleeping,      // shown for a moment before sleeping
    Editor,        // one Screen setting: slider or - / +, with presets
    History,       // list of past games
    HistoryDetail, // one past game
    Legends,       // pick a Riftbound legend
    Keyboard,      // type a new legend name
};

// One Screen setting being edited.
struct EditorView {
    const char *title = "";
    const char *value = "";
    bool slider = false;   // drag a slider (else - / + buttons)
    uint16_t frac = 0;     // slider position, 0..1000
    uint8_t preset_count = 0;
    const char *presets[6] = {};
    int8_t preset_sel = -1; // highlighted preset, -1 = none
};

constexpr uint8_t kNoLegend = 0xFF;
constexpr uint8_t kCustomLegend = 0x80; // codes from here up are the player's own legends
constexpr int kMaxCustomLegends = 12;
constexpr int kLegendNameLen = 15;      // characters, plus the terminator

constexpr int kHistoryEvents = 30; // scoring changes kept per game

// One finished game, as kept in the history.
struct HistoryEntry {
    uint16_t number;      // counts up from the first game recorded
    uint8_t profile_index;
    uint8_t players;
    uint8_t result;       // 0 = side 0 (YOU) won, 1 = side 1 won, 2 = draw
    uint8_t best_of;      // 0 = no match
    uint8_t game_in_match; // 1-based
    uint8_t legend[2];    // kNoLegend, an official legend index, or kCustomLegend + i
    uint16_t minutes;     // how long the game took
    int16_t value[2];     // final scores (side 1 is 0 with one counter)
    int16_t tally[2][2];  // per side, per kind (Riftbound: Conquer, Hold)
    uint16_t match_no;    // games of one best-of share this (0 = not known)
    uint8_t event_count;
    uint8_t events[kHistoryEvents]; // scoring in order: see ui_history_event()
};

// One scoring change in a recorded game.
struct HistoryEvent {
    uint8_t player; // 0 or 1
    uint8_t kind;   // 0 or 1 (Riftbound: Conquer, Hold)
    int8_t delta;
};
uint8_t ui_history_pack(uint8_t player, uint8_t kind, int delta);
HistoryEvent ui_history_event(uint8_t packed);

// History entries are newest first; the games of one match sit next to each
// other. These walk them as matches ("groups").
int ui_history_group_end(const HistoryEntry *h, int count, int start); // index after the group
int ui_history_group_count(const HistoryEntry *h, int count);
int ui_history_group_start(const HistoryEntry *h, int count, int n);  // entry index of group n

// Official Riftbound legends, A-Z. legend_name() also resolves the player's own.
size_t ui_legend_count();
const char *ui_legend_official(size_t i);
// Set code for a legend ("OGN", "UNL", ...; "OWN" for the player's own, "" for none).
const char *ui_legend_set(uint8_t code);

struct UiState {
    const bb_game_t *game;
    int step;            // single-player +/- step (1 or 5)
    int battery_percent; // < 0 means on USB power
    Layout layout;       // used when the game has more than one player
    bool flipped;        // whole screen rotated 180 degrees (menu label only)
    bool vivid;          // Look setting: vivid colors and glows instead of subdued
    bool menu;           // menu screen instead of the counter
    int menu_page;       // MenuPage: a tab (Play / Match / Look / System) or a sub-page
    int counter_style;   // Counter setting: 0 = themed (each game's own look), 1 = d20, 2 = digits
    const uint16_t *spin;  // per-player change animation progress (0..1000, 1000 = still), or nullptr
    const int8_t *spin_dir; // per-player direction of the last change (+1 / -1)
    const int16_t *prev;    // per-player value before the last change, or nullptr
    bool end_prompt;     // "who won?" screen
    const char *toast;   // short message, or nullptr

    const bb_match_t *match; // best-of match in progress, or nullptr when off
    int best_of;             // menu value: 0 = off, 1 / 3 / 5
    int timer_minutes;       // menu value: 0 = off
    bool timer_running;      // show the countdown
    uint8_t timer_state = 1; // 0 = waiting for a tap to start, 1 = running, 2 = paused
    int32_t timer_left_s;    // seconds left; negative = over time

    const DiceState *dice; // dice screen instead of the counter, or nullptr
    const PickerState *picker; // game picker, or nullptr
    const EntryState *entry;   // number pad, or nullptr

    bool always_on = false;         // screen never dims or sleeps (settings, or a battery test)
    // Screen page values (menu_page 2), already formatted.
    const char *brightness_text = nullptr;
    const char *dim_after_text = nullptr;
    const char *dim_to_text = nullptr;
    const char *sleep_after_text = nullptr;
    const char *test_text = nullptr; // Battery test tile: "Start" or the time so far
    const char *last_test = nullptr; // Last test tile: how long the last test ran, or nullptr
    const char *since_charge = nullptr; // Battery page: time on battery since the last charge
    const char *button_hint = nullptr;  // main menu header: what A and B do, e.g. "A Undo  B Menu"

    // Controls page: what each button press / hold and swipe does, as text
    // (A press, A hold, B press, B hold, swipe up, down, left, right). A is the
    // top button (KEY, also wakes the board), B the bottom one (BOOT).
    const char *controls[8] = {};

    Overlay overlay = Overlay::None;
    const EditorView *editor = nullptr;

    // History and legends.
    const HistoryEntry *history = nullptr; // newest first
    int history_count = 0;
    int history_page = 0;
    int history_sel = -1;           // first entry of the match shown in HistoryDetail
    int history_detail_page = 0;    // which 3 games of a best-of-5 are shown
    bool history_delete_armed = false;
    uint8_t match_legend[2] = {kNoLegend, kNoLegend}; // legends for the match being played
    uint8_t legend_side = 0;        // whose legend the picker is choosing
    uint8_t legend_current = kNoLegend;
    int legend_page = 0;
    uint8_t legend_sort = 0;        // 0 = A-Z, 1 = by set (oldest set first)
    const char (*custom_legends)[kLegendNameLen + 1] = nullptr;
    int custom_legend_count = 0;
    const char *keyboard_text = "";

    // Languages. Menus and card names are separate settings: you can read the
    // menus in one language and play with cards printed in another.
    uint8_t card_lang = 0;          // CardLang: names of game characters (legends)
    const char *card_lang_text = nullptr;
};

// Card-name languages (more to come: Simplified Chinese, French).
enum CardLang : uint8_t { kCardEnglish, kCardKorean, kCardLangCount };

// Name for a legend code (kNoLegend gives "-").
const char *ui_legend_name(const UiState &s, uint8_t code);
// The name, plus the set code when two cards share the name ("Master Yi UNL"). Writes to buf.
const char *ui_legend_label(const UiState &s, uint8_t code, char *buf, size_t n);

struct UiAction {
    enum Type : uint8_t {
        None,
        Adjust,
        ToggleStep,
        MenuGame,
        MenuPlayers,
        MenuLayout,
        MenuFlip,
        MenuLook,
        MenuMatch,
        MenuTimer,
        MenuRestart,
        MenuDice,
        MenuClose,
        MenuMore,
        MenuBack,
        MenuCounter,
        MenuUndo,
        MenuTab,         // switch to menu tab `kind`
        TimerTap,        // start / pause / resume the timer
        MenuScreen,      // open the Screen settings page
        MenuControls,    // open the Controls page
        MenuBattery,     // open the Battery page
        MenuHistory,     // open the game history
        MenuLegends,     // pick the legends for this match
        MenuOff,         // "Turn off?"
        MenuBatteryTest, // start / stop the battery test
        ScreenOpen,      // edit Screen setting `kind` (0 brightness, 1 dim after, 2 dim to, 3 sleep after)
        EditorSlide,     // slider moved to `delta` (0..1000)
        EditorStep,      // - / + by `delta`
        EditorPreset,    // preset `kind`
        EditorClose,
        ControlCycle,    // next action for control `kind` (0..7)
        QuickPick,       // quick menu choice `kind`: 0 undo, 1 dice, 2 menu, 3 turn off
        QuickClose,
        OffYes,
        OffNo,
        HistoryOpen,     // show game `kind` (index, newest first)
        HistoryPage,     // page by `delta`
        HistoryClose,
        HistoryLegend,   // edit side `player`'s legend for the open game
        HistoryDelete,   // first tap arms, second deletes
        HistoryGames,    // show the next games of a long match
        DetailBack,
        LegendPick,      // legend code `kind`
        LegendPage,      // page by `delta`
        LegendAdd,       // open the keyboard
        LegendClose,
        LegendSort,      // toggle A-Z / by set
        KeyChar,         // type character `kind`
        KeyDelete,
        KeyDone,
        KeyCancel,
        EndDraw,         // one-counter game: record a draw
        CardLangNext,    // next card-name language
        MenuEnd,       // open the "who won?" screen
        DeclareWinner, // `player` won this game
        EndRestart,    // restart without recording a result
        EndCancel,
        NextGame, // dismiss the game-over banner and record the result
        DiceSelect, // choose die `kind`
        DiceRoll,
        DiceClose,
        PickFamily,  // open game `kind` (its formats, or the game itself if it has one)
        PickProfile, // start profile index `kind`
        PickerBack,
        PickerClose,
        GaugeSet,   // move the shared marker to `delta`
        GaugePass,  // pass the turn: marker to 3 on the other side
        OpenEntry,  // number pad for `player`
        EntryDigit, // `kind` 0-9, or 10 for "00"
        EntryDelete,
        EntryApply, // apply the amount with sign `delta` (+1 / -1)
        EntryHalve,
        EntryClose,
    };

    Type type = None;
    uint8_t player = 0;
    uint8_t kind = 0;
    int16_t delta = 0;
    bool repeat = false; // keep applying while held
};

// Draws the current screen into frame.
void ui_draw(LGFX_Sprite &frame, const UiState &state);

// Winner of the current game when the game-over banner should show, else -1.
int ui_banner_winner(const UiState &state);

// Maps a tap at screen coordinates (x, y) to an action. Uses the same layout
// as ui_draw, so what is drawn is what is tapped.
UiAction ui_hit(const UiState &state, int x, int y);

// Short label for counter p: "YOU" / "OPP" with two players, "P1".."P4" with
// more, "TEAM A" / "TEAM B" for team games.
const char *ui_player_label(const bb_game_t &game, uint8_t player);

// Name of the current layout as shown in the menu, and the layout after it.
const char *ui_layout_name(const bb_game_t &game, Layout layout);
Layout ui_next_layout(const bb_game_t &game, Layout layout);
