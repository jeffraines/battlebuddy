#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "bb/game.h"

// Landscape frame size for the T-Display-S3 (170x320 panel, rotated).
constexpr int kWidth = 320;
constexpr int kHeight = 170;

// How two players share the screen.
enum class Layout : uint8_t {
    SideYouLeft,  // side by side, YOU on the left
    SideYouRight, // side by side, YOU on the right
    Facing,       // YOU along the bottom, OPP along the top upside down (across a table)
    Count,
};

struct UiState {
    const bb_game_t *game;
    int step;            // single-player +/- step (1 or 5)
    int battery_percent; // < 0 means on USB power
    Layout layout;       // used when the game has two players
    bool flipped;        // whole screen rotated 180 degrees (menu label only)
    bool menu;           // menu screen instead of the counter
    const char *toast;   // short message, or nullptr
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
        MenuRestart,
        MenuSleep,
        MenuClose,
    };

    Type type = None;
    uint8_t player = 0;
    uint8_t kind = 0;
    int16_t delta = 0;
    bool repeat = false; // keep applying while held
};

// Draws the current screen into frame.
void ui_draw(LGFX_Sprite &frame, const UiState &state);

// Maps a tap at screen coordinates (x, y) to an action. Uses the same layout
// as ui_draw, so what is drawn is what is tapped.
UiAction ui_hit(const UiState &state, int x, int y);

// Short label for player p in two-player mode ("YOU" / "OPP").
const char *ui_player_label(uint8_t player);
