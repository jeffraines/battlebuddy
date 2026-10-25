#include "ui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace {

// Colors are RGB888 in uint32_t; LovyanGFX treats uint32_t colors as RGB888.
struct Theme {
    const char *profile_id;
    uint32_t bg_top;
    uint32_t bg_bottom;
    uint32_t surface; // buttons, menu rows, toast
    uint32_t accent;  // titles, borders, ornaments
    uint32_t p1;      // "YOU" in two-player mode
    uint32_t p2;      // "OPP" in two-player mode
    uint32_t text;
    uint32_t muted;
};

// Per-game palettes. Colors only; no game logos or artwork.
const Theme kThemes[] = {
    // Navy and teal with gold trim.
    {"riftbound", 0x0A1428, 0x0B3A44, 0x10283A, 0xC8AA6E, 0x0AC8B9, 0xE8604C, 0xF0E6D2, 0x7E8A96},
    // Charcoal and warm brown with amber.
    {"mtg", 0x15110E, 0x33221A, 0x2A1E17, 0xE0A040, 0x6FB5E0, 0xE8604C, 0xF2E8DC, 0x8C8072},
    // Charcoal and plum with lilac.
    {"mtg-commander", 0x120F1A, 0x2E1F40, 0x251A36, 0xC9A0FF, 0x6FB5E0, 0xE8604C, 0xF0EAFA, 0x8A7FA0},
    // Ink blue and violet with gold.
    {"lorcana", 0x0B0F2E, 0x2D1B4E, 0x1C1840, 0xE8C25A, 0x7FB2FF, 0xF07FB0, 0xF4F0FF, 0x8A88AA},
    // Black and blood red with bronze.
    {"fab", 0x140808, 0x3C1210, 0x2E1210, 0xD4A24C, 0x6FB5E0, 0xE04B3A, 0xF4E6DC, 0x907670},
};

const Theme &theme_for(const bb_profile_t *profile)
{
    for (const Theme &t : kThemes) {
        if (strcmp(t.profile_id, profile->id) == 0) {
            return t;
        }
    }
    return kThemes[1];
}

constexpr uint32_t kGain = 0x5BD96A;
constexpr uint32_t kLoss = 0xF0564A;
constexpr uint32_t kWinColor = 0xFFD45A;

constexpr int kHeaderH = 24;
constexpr int kFooterTop = 150;

uint32_t mix(uint32_t a, uint32_t b, int t256)
{
    uint32_t r = (((a >> 16) & 0xFF) * (256 - t256) + ((b >> 16) & 0xFF) * t256) >> 8;
    uint32_t g = (((a >> 8) & 0xFF) * (256 - t256) + ((b >> 8) & 0xFF) * t256) >> 8;
    uint32_t bl = ((a & 0xFF) * (256 - t256) + (b & 0xFF) * t256) >> 8;
    return (r << 16) | (g << 8) | bl;
}

struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    int cx() const { return x + w / 2; }
    int cy() const { return y + h / 2; }
};

// ---- Layout -----------------------------------------------------------------

bool has_kinds(const bb_game_t &g) { return g.profile->kind_count >= 2; }

Rect player_panel(const bb_game_t &g, uint8_t p)
{
    if (g.players == 1) {
        return {0, kHeaderH, kWidth, kFooterTop - kHeaderH};
    }
    return {p * kWidth / 2, kHeaderH, kWidth / 2, kFooterTop - kHeaderH};
}

// Kind buttons (e.g. Conquer / Hold) for player p.
Rect kind_button(const bb_game_t &g, uint8_t p, uint8_t kind)
{
    if (g.players == 1) {
        return {176, kind == 0 ? 30 : 92, 138, 56};
    }
    Rect panel = player_panel(g, p);
    return {panel.x + (kind == 0 ? 6 : 82), 110, 72, 36};
}

constexpr int kMenuRowY = 28;
constexpr int kMenuRowH = 30;
constexpr int kMenuRowGap = 4;
constexpr int kMenuRows = 4;

Rect menu_row(int i)
{
    return {6, kMenuRowY + i * (kMenuRowH + kMenuRowGap), kWidth - 12, kMenuRowH};
}

// ---- Drawing helpers --------------------------------------------------------

void draw_background(LGFX_Sprite &f, const Theme &t, bool brackets)
{
    for (int y = 0; y < kHeight; y++) {
        f.drawFastHLine(0, y, kWidth, mix(t.bg_top, t.bg_bottom, y * 256 / kHeight));
    }

    // Header rule, brightest in the middle.
    for (int x = 0; x < kWidth; x++) {
        int d = x < kWidth / 2 ? x : kWidth - 1 - x;
        f.drawPixel(x, kHeaderH, mix(t.bg_top, t.accent, d * 256 / (kWidth / 2)));
    }

    if (!brackets) {
        return;
    }

    // Corner brackets around the body.
    uint32_t c = mix(t.bg_bottom, t.accent, 150);
    const int l = 10, x0 = 3, y0 = kHeaderH + 3, x1 = kWidth - 4, y1 = kFooterTop - 2;
    f.drawFastHLine(x0, y0, l, c);
    f.drawFastVLine(x0, y0, l, c);
    f.drawFastHLine(x1 - l + 1, y0, l, c);
    f.drawFastVLine(x1, y0, l, c);
    f.drawFastHLine(x0, y1, l, c);
    f.drawFastVLine(x0, y1 - l + 1, l, c);
    f.drawFastHLine(x1 - l + 1, y1, l, c);
    f.drawFastVLine(x1, y1 - l + 1, l, c);
}

void draw_battery(LGFX_Sprite &f, const Theme &t, int percent)
{
    const int x = kWidth - 30, y = 6, w = 22, h = 12;
    f.drawRoundRect(x, y, w, h, 2, t.muted);
    f.fillRect(x + w, y + 4, 2, h - 8, t.muted);

    if (percent < 0) {
        // On USB: lightning bolt.
        f.fillTriangle(x + 12, y + 2, x + 7, y + 7, x + 11, y + 7, t.accent);
        f.fillTriangle(x + 11, y + 5, x + 15, y + 5, x + 10, y + 10, t.accent);
        return;
    }
    int fill = (w - 4) * percent / 100;
    if (fill > 0) {
        f.fillRect(x + 2, y + 2, fill, h - 4, percent < 20 ? kLoss : kGain);
    }
}

void draw_header(LGFX_Sprite &f, const Theme &t, const char *title, int battery_percent)
{
    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    f.setTextColor(t.accent);
    f.setTextDatum(textdatum_t::middle_left);
    f.drawString(title, 8, kHeaderH / 2);
    draw_battery(f, t, battery_percent);
}

// Draws a value in the 75 px numeric font, shrunk if needed to fit max_w x max_h.
void draw_number(LGFX_Sprite &f, const char *text, int cx, int cy, int max_w, int max_h,
                 uint32_t color, uint32_t shadow)
{
    f.setFont(&fonts::Font8);
    f.setTextSize(1);
    float scale = 1.0f;
    int w = f.textWidth(text);
    int h = f.fontHeight();
    if (w > max_w) {
        scale = (float)max_w / w;
    }
    if (h * scale > max_h) {
        scale = (float)max_h / h;
    }
    f.setTextSize(scale);
    f.setTextDatum(textdatum_t::middle_center);
    f.setTextColor(shadow);
    f.drawString(text, cx + 2, cy + 3);
    f.setTextColor(color);
    f.drawString(text, cx, cy);
    f.setTextSize(1);
}

uint32_t value_color(const Theme &t, bb_status_t s)
{
    switch (s) {
    case BB_STATUS_WON:
        return kWinColor;
    case BB_STATUS_LOST:
        return kLoss;
    default:
        return t.text;
    }
}

uint32_t player_color(const Theme &t, uint8_t p) { return p == 0 ? t.p1 : t.p2; }

void upper(char *dst, size_t n, const char *src, size_t max_chars)
{
    size_t i = 0;
    for (; src[i] && i < max_chars && i + 1 < n; i++) {
        dst[i] = (char)toupper((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

// Pending change for player p ("+2"), or the result once decided, or "".
// Returns the color to draw it in.
uint32_t status_text(const bb_game_t &g, uint8_t p, char *buf, size_t n)
{
    buf[0] = '\0';
    if (g.pending.delta != 0 && g.pending.player == p) {
        snprintf(buf, n, "%+d", g.pending.delta);
        return g.pending.delta > 0 ? kGain : kLoss;
    }
    switch (bb_game_status(&g, p)) {
    case BB_STATUS_WON:
        snprintf(buf, n, "WIN");
        return kWinColor;
    case BB_STATUS_LOST:
        snprintf(buf, n, "LOSE");
        return kLoss;
    default:
        return 0;
    }
}

void draw_plus_minus(LGFX_Sprite &f, int cx, int cy, bool plus, uint32_t color, int half = 8)
{
    f.fillRect(cx - half, cy - 2, half * 2, 4, color);
    if (plus) {
        f.fillRect(cx - 2, cy - half, 4, half * 2, color);
    }
}

void draw_kind_button(LGFX_Sprite &f, const Theme &t, const bb_game_t &g, uint8_t p, uint8_t kind)
{
    Rect r = kind_button(g, p, kind);
    char name[12];
    char buf[20];

    f.fillRoundRect(r.x, r.y, r.w, r.h, 8, t.surface);
    f.drawRoundRect(r.x, r.y, r.w, r.h, 8, mix(t.surface, t.accent, 200));
    f.setTextDatum(textdatum_t::middle_center);

    if (g.players == 1) {
        upper(name, sizeof(name), g.profile->kinds[kind], 10);
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextColor(t.accent);
        f.drawString(name, r.cx(), r.y + 17);
        snprintf(buf, sizeof(buf), "%d", g.tally[0][kind]);
        f.setFont(&fonts::FreeSansBold12pt7b);
        f.setTextColor(t.text);
        f.drawString(buf, r.cx(), r.y + 39);
    } else {
        upper(name, sizeof(name), g.profile->kinds[kind], 4);
        snprintf(buf, sizeof(buf), "%s %d", name, g.tally[p][kind]);
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextColor(t.text);
        f.drawString(buf, r.cx(), r.cy());
    }
}

void draw_footer(LGFX_Sprite &f, const Theme &t, const bb_game_t &g)
{
    bb_entry_t recent[8];
    size_t n = bb_game_history(&g, recent, 8);
    char buf[16];

    f.setFont(&fonts::FreeSans9pt7b);
    f.setTextSize(1);
    f.setTextDatum(textdatum_t::bottom_left);
    int x = 8;
    for (size_t i = 0; i < n && x < kWidth - 30; i++) {
        const bb_entry_t &e = recent[i];
        if (has_kinds(g)) {
            snprintf(buf, sizeof(buf), "%+d%c", e.delta, g.profile->kinds[e.kind][0]);
        } else {
            snprintf(buf, sizeof(buf), "%+d", e.delta);
        }
        uint32_t c = g.players > 1 ? player_color(t, e.player) : (e.delta > 0 ? kGain : kLoss);
        f.setTextColor(c);
        x += f.drawString(buf, x, kHeight - 3) + 9;
    }
}

void draw_toast(LGFX_Sprite &f, const Theme &t, const char *msg)
{
    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    int w = f.textWidth(msg) + 24;
    Rect r{(kWidth - w) / 2, kFooterTop - 2, w, kHeight - kFooterTop};
    f.fillRoundRect(r.x, r.y, r.w, r.h, 8, t.surface);
    f.drawRoundRect(r.x, r.y, r.w, r.h, 8, t.accent);
    f.setTextColor(t.text);
    f.setTextDatum(textdatum_t::middle_center);
    f.drawString(msg, r.cx(), r.cy());
}

// ---- Screens ----------------------------------------------------------------

void draw_single(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    const bb_game_t &g = *s.game;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", g.value[0]);
    uint32_t vc = value_color(t, bb_game_status(&g, 0));
    uint32_t shadow = mix(t.bg_bottom, 0x000000, 128);

    if (has_kinds(g)) {
        // Number on the left, kind buttons on the right.
        draw_number(f, buf, 86, 80, 150, 90, vc, shadow);
        draw_kind_button(f, t, g, 0, 0);
        draw_kind_button(f, t, g, 0, 1);
    } else {
        uint32_t hint = mix(t.bg_bottom, t.accent, 110);
        draw_plus_minus(f, 28, 87, false, hint, 10);
        draw_plus_minus(f, kWidth - 28, 87, true, hint, 10);
        draw_number(f, buf, kWidth / 2, 80, 200, 90, vc, shadow);
        if (s.step != 1) {
            snprintf(buf, sizeof(buf), "x%d", s.step);
            f.setFont(&fonts::FreeSansBold9pt7b);
            f.setTextColor(t.accent);
            f.setTextDatum(textdatum_t::middle_center);
            f.drawString(buf, kWidth / 2, kHeaderH / 2);
        }
    }

    uint32_t sc = status_text(g, 0, buf, sizeof(buf));
    if (buf[0]) {
        f.setFont(&fonts::FreeSansBold12pt7b);
        f.setTextColor(sc);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(buf, has_kinds(g) ? 86 : kWidth / 2, 136);
    }
}

void draw_double(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    const bb_game_t &g = *s.game;
    char buf[16];
    uint32_t shadow = mix(t.bg_bottom, 0x000000, 128);

    // Divider between the two players.
    for (int y = kHeaderH + 8; y < kFooterTop - 6; y++) {
        int d = y - kHeaderH;
        int span = kFooterTop - kHeaderH;
        int fade = d < span / 2 ? d : span - d;
        f.drawPixel(kWidth / 2, y, mix(t.bg_bottom, t.accent, fade * 300 / span));
    }

    for (uint8_t p = 0; p < 2; p++) {
        Rect panel = player_panel(g, p);

        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextSize(1);
        f.setTextColor(player_color(t, p));
        f.setTextDatum(textdatum_t::middle_left);
        f.drawString(ui_player_label(p), panel.x + 10, panel.y + 14);

        uint32_t sc = status_text(g, p, buf, sizeof(buf));
        if (buf[0]) {
            f.setTextColor(sc);
            f.setTextDatum(textdatum_t::middle_right);
            f.drawString(buf, panel.x + panel.w - 10, panel.y + 14);
        }

        snprintf(buf, sizeof(buf), "%d", g.value[p]);
        uint32_t vc = value_color(t, bb_game_status(&g, p));
        if (has_kinds(g)) {
            draw_number(f, buf, panel.cx(), 72, 140, 64, vc, shadow);
            draw_kind_button(f, t, g, p, 0);
            draw_kind_button(f, t, g, p, 1);
        } else {
            uint32_t hint = mix(t.bg_bottom, t.accent, 110);
            draw_plus_minus(f, panel.x + panel.w - 16, 58, true, hint, 6);
            draw_plus_minus(f, panel.x + panel.w - 16, 128, false, hint, 6);
            draw_number(f, buf, panel.cx() - 6, 92, 124, 80, vc, shadow);
        }
    }
}

void draw_menu(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    const bb_game_t &g = *s.game;
    char players[8];
    snprintf(players, sizeof(players), "%d", g.players);

    const char *labels[kMenuRows] = {"Game", "Players", "Restart game", "Sleep"};
    const char *values[kMenuRows] = {g.profile->name, players, "", "KEY wakes"};

    // Close mark in the header.
    uint32_t xc = t.muted;
    for (int i = -1; i <= 1; i++) {
        f.drawLine(kWidth - 52 + i, 7, kWidth - 42 + i, 17, xc);
        f.drawLine(kWidth - 42 + i, 7, kWidth - 52 + i, 17, xc);
    }

    for (int i = 0; i < kMenuRows; i++) {
        Rect r = menu_row(i);
        f.fillRoundRect(r.x, r.y, r.w, r.h, 7, t.surface);
        f.drawRoundRect(r.x, r.y, r.w, r.h, 7, mix(t.surface, t.accent, 90));

        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextSize(1);
        f.setTextColor(t.text);
        f.setTextDatum(textdatum_t::middle_left);
        f.drawString(labels[i], r.x + 12, r.cy());

        f.setFont(&fonts::FreeSans9pt7b);
        f.setTextColor(t.accent);
        f.setTextDatum(textdatum_t::middle_right);
        f.drawString(values[i], r.x + r.w - 12, r.cy());
    }
}

} // namespace

const char *ui_player_label(uint8_t player)
{
    return player == 0 ? "YOU" : "OPP";
}

void ui_draw(LGFX_Sprite &frame, const UiState &state)
{
    const Theme &t = theme_for(state.game->profile);

    draw_background(frame, t, !state.menu);
    draw_header(frame, t, state.menu ? "MENU" : state.game->profile->name, state.battery_percent);

    if (state.menu) {
        draw_menu(frame, t, state);
        return;
    }

    if (state.game->players == 1) {
        draw_single(frame, t, state);
    } else {
        draw_double(frame, t, state);
    }

    if (state.toast) {
        draw_toast(frame, t, state.toast);
    } else {
        draw_footer(frame, t, *state.game);
    }
}

UiAction ui_hit(const UiState &state, int x, int y)
{
    const bb_game_t &g = *state.game;
    UiAction a;

    if (state.menu) {
        if (y < kHeaderH) {
            a.type = UiAction::MenuClose;
            return a;
        }
        static const UiAction::Type kRowActions[kMenuRows] = {
            UiAction::MenuGame, UiAction::MenuPlayers, UiAction::MenuRestart, UiAction::MenuSleep};
        for (int i = 0; i < kMenuRows; i++) {
            Rect r = menu_row(i);
            // Rows are hit-tested with the gaps included so near-misses still count.
            if (y >= r.y - kMenuRowGap / 2 && y < r.y + r.h + kMenuRowGap / 2) {
                a.type = kRowActions[i];
                return a;
            }
        }
        return a;
    }

    if (y < kHeaderH || y >= kFooterTop + 6) {
        return a;
    }

    uint8_t p = (g.players == 2 && x >= kWidth / 2) ? 1 : 0;
    Rect panel = player_panel(g, p);
    a.player = p;

    if (has_kinds(g)) {
        // Kind buttons only; mistakes are fixed with undo.
        for (uint8_t k = 0; k < 2; k++) {
            Rect r = kind_button(g, p, k);
            // Generous targets: the button plus its half of the column.
            bool hit = g.players == 1 ? (x >= r.x - 8 && (k == 0 ? y < 89 : y >= 89))
                                      : (y >= r.y - 10 && (k == 0 ? x < panel.cx() : x >= panel.cx()));
            if (hit) {
                a.type = UiAction::Adjust;
                a.kind = k;
                a.delta = 1;
                return a;
            }
        }
        return a;
    }

    if (g.players == 1) {
        if (x < kWidth / 3) {
            a.type = UiAction::Adjust;
            a.delta = (int16_t)-state.step;
            a.repeat = true;
        } else if (x >= kWidth * 2 / 3) {
            a.type = UiAction::Adjust;
            a.delta = (int16_t)state.step;
            a.repeat = true;
        } else {
            a.type = UiAction::ToggleStep;
        }
        return a;
    }

    // Two players without kinds: top half +1, bottom half -1.
    a.type = UiAction::Adjust;
    a.delta = y < panel.cy() ? 1 : -1;
    a.repeat = true;
    return a;
}
