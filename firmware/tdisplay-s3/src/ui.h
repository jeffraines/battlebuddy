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

struct UiState {
    const bb_game_t *game;
    int step;            // single-player +/- step (1 or 5)
    int battery_percent; // < 0 means on USB power
    Layout layout;       // used when the game has more than one player
    bool flipped;        // whole screen rotated 180 degrees (menu label only)
    bool vivid;          // Look setting: vivid colors and glows instead of subdued
    bool menu;           // menu screen instead of the counter
    const char *toast;   // short message, or nullptr

    const bb_match_t *match; // best-of match in progress, or nullptr when off
    int best_of;             // menu value: 0 = off, 1 / 3 / 5
    int timer_minutes;       // menu value: 0 = off
    bool timer_running;      // show the countdown
    int32_t timer_left_s;    // seconds left; negative = over time

    const DiceState *dice; // dice screen instead of the counter, or nullptr
    const PickerState *picker; // game picker, or nullptr
    const EntryState *entry;   // number pad, or nullptr
};

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
