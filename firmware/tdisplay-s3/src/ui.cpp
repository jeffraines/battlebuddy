#include "ui.h"

#include <stdio.h>

namespace {

constexpr uint16_t kColorText = TFT_WHITE;
constexpr uint16_t kColorMuted = 0x7BEF; // grey
constexpr uint16_t kColorHint = 0x39E7;  // dark grey
constexpr uint16_t kColorWin = 0xFEA0;   // gold
constexpr uint16_t kColorLose = 0xF8A2;  // red
constexpr uint16_t kColorGain = 0x47E8;  // green

uint16_t value_color(bb_status_t status)
{
    switch (status) {
    case BB_STATUS_WON:
        return kColorWin;
    case BB_STATUS_LOST:
        return kColorLose;
    default:
        return kColorText;
    }
}

} // namespace

void ui_draw(LGFX_Sprite &frame, const bb_game_t &game, int step, int battery_percent)
{
    char buf[16];
    bb_status_t status = bb_game_status(&game);

    frame.fillScreen(TFT_BLACK);

    // Top bar: game name, step, battery.
    frame.setFont(&fonts::FreeSans9pt7b);
    frame.setTextColor(kColorMuted);
    frame.setTextDatum(textdatum_t::top_left);
    frame.drawString(game.profile->name, 6, 4);

    if (battery_percent < 0) {
        snprintf(buf, sizeof(buf), "USB");
    } else {
        snprintf(buf, sizeof(buf), "%d%%", battery_percent);
    }
    frame.setTextDatum(textdatum_t::top_right);
    frame.drawString(buf, kWidth - 6, 4);
    if (step != 1) {
        snprintf(buf, sizeof(buf), "x%d", step);
        frame.setTextColor(kColorGain);
        frame.setTextDatum(textdatum_t::top_center);
        frame.drawString(buf, kWidth / 2, 4);
    }

    // Tap-zone hints: a minus and a plus of the same size.
    constexpr int kHintHalf = 10;
    constexpr int kHintThick = 4;
    const int mid_y = kHeight / 2;
    frame.fillRect(26 - kHintHalf, mid_y - kHintThick / 2, kHintHalf * 2, kHintThick, kColorHint);
    frame.fillRect(kWidth - 26 - kHintHalf, mid_y - kHintThick / 2, kHintHalf * 2, kHintThick, kColorHint);
    frame.fillRect(kWidth - 26 - kHintThick / 2, mid_y - kHintHalf, kHintThick, kHintHalf * 2, kColorHint);

    // The count, in the 75 px numeric font (digits and '-' only).
    snprintf(buf, sizeof(buf), "%d", game.value);
    frame.setFont(&fonts::Font8);
    frame.setTextColor(value_color(status));
    frame.setTextDatum(textdatum_t::middle_center);
    frame.drawString(buf, kWidth / 2, 74);

    // Pending change, or the result once the game is decided.
    frame.setFont(&fonts::FreeSansBold12pt7b);
    if (game.pending != 0) {
        snprintf(buf, sizeof(buf), "%+d", game.pending);
        frame.setTextColor(game.pending > 0 ? kColorGain : kColorLose);
        frame.drawString(buf, kWidth / 2, 136);
    } else if (status == BB_STATUS_WON) {
        frame.setTextColor(kColorWin);
        frame.drawString("WIN", kWidth / 2, 136);
    } else if (status == BB_STATUS_LOST) {
        frame.setTextColor(kColorLose);
        frame.drawString("LOSE", kWidth / 2, 136);
    }

    // Recent history, newest first.
    bb_entry_t recent[6];
    size_t n = bb_game_history(&game, recent, 6);
    frame.setFont(&fonts::FreeSans9pt7b);
    frame.setTextDatum(textdatum_t::bottom_left);
    int x = 6;
    for (size_t i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "%+d", recent[i].delta);
        frame.setTextColor(recent[i].delta > 0 ? kColorGain : kColorLose);
        x += frame.drawString(buf, x, kHeight - 4) + 10;
    }
}
