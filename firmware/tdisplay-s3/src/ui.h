#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "bb/game.h"

// Landscape frame size for the T-Display-S3 (170x320 panel, rotated).
constexpr int kWidth = 320;
constexpr int kHeight = 170;

// Draws the whole counter screen into frame. battery_percent < 0 means USB power.
void ui_draw(LGFX_Sprite &frame, const bb_game_t &game, int step, int battery_percent);
