#include "ui.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

// Colors are RGB888 in uint32_t; LovyanGFX treats uint32_t colors as RGB888.
struct Theme {
    const char *profile_id;
    uint32_t bg_top;
    uint32_t bg_bottom;
    uint32_t surface; // buttons, menu tiles, toast
    uint32_t accent;  // titles, borders, ornaments
    uint32_t p1;      // "YOU" in two-player mode
    uint32_t p2;      // "OPP" in two-player mode
    uint32_t text;
    uint32_t muted;
    uint32_t kind[BB_MAX_KINDS]; // e.g. Conquer / Hold slots on the score track
};

// Per-game palettes. Colors only; no game logos or artwork.
const Theme kThemes[] = {
    // Navy and teal with gold trim; gold = Conquer, violet = Hold.
    {"riftbound", 0x0A1428, 0x0B3A44, 0x10283A, 0xC8AA6E, 0x0AC8B9, 0xE8604C, 0xF0E6D2, 0x7E8A96,
     {0xE0B85A, 0xA78BFA}},
    // Charcoal and warm brown with amber.
    {"mtg", 0x15110E, 0x33221A, 0x2A1E17, 0xE0A040, 0x6FB5E0, 0xE8604C, 0xF2E8DC, 0x8C8072,
     {0xE0A040, 0x6FB5E0}},
    // Charcoal and plum with lilac.
    {"mtg-commander", 0x120F1A, 0x2E1F40, 0x251A36, 0xC9A0FF, 0x6FB5E0, 0xE8604C, 0xF0EAFA, 0x8A7FA0,
     {0xC9A0FF, 0x6FB5E0}},
    // Ink blue and violet with gold.
    {"lorcana", 0x0B0F2E, 0x2D1B4E, 0x1C1840, 0xE8C25A, 0x7FB2FF, 0xF07FB0, 0xF4F0FF, 0x8A88AA,
     {0xE8C25A, 0x7FB2FF}},
    // Black and blood red with bronze.
    {"fab", 0x140808, 0x3C1210, 0x2E1210, 0xD4A24C, 0x6FB5E0, 0xE04B3A, 0xF4E6DC, 0x907670,
     {0xD4A24C, 0x6FB5E0}},
};

// Exact id match first, then a family prefix ("riftbound-2v2" uses "riftbound").
const Theme &theme_for(const bb_profile_t *profile)
{
    for (const Theme &t : kThemes) {
        if (strcmp(t.profile_id, profile->id) == 0) {
            return t;
        }
    }
    for (const Theme &t : kThemes) {
        size_t n = strlen(t.profile_id);
        if (strncmp(t.profile_id, profile->id, n) == 0 && profile->id[n] == '-') {
            return t;
        }
    }
    return kThemes[1];
}

constexpr uint32_t kGain = 0x5BD96A;
constexpr uint32_t kLoss = 0xF0564A;
constexpr uint32_t kWinColor = 0xFFD45A;
constexpr uint32_t kInk = 0x10141C; // dark text on filled slots

constexpr int kHeaderH = 24;
constexpr int kFooterTop = 150;
constexpr int kStripH = kHeight / 2; // facing / 4-player layouts: one strip per row
constexpr int kQuadW = kWidth / 2;

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

bool facing(const UiState &s) { return s.game->players == 2 && s.layout == Layout::Facing; }

bool quads(const UiState &s) { return s.game->players > 2; }

// Which player is drawn on the left in the side-by-side layout.
uint8_t left_player(const UiState &s) { return s.layout == Layout::SideYouRight ? 1 : 0; }

Rect player_panel(const UiState &s, uint8_t p)
{
    if (s.game->players == 1) {
        return {0, kHeaderH, kWidth, kFooterTop - kHeaderH};
    }
    int side = p == left_player(s) ? 0 : 1;
    return {side * kWidth / 2, kHeaderH, kWidth / 2, kFooterTop - kHeaderH};
}

// Kind buttons (e.g. Conquer / Hold). Facing-layout rects are in strip coordinates,
// quadrant rects in quadrant coordinates.
Rect kind_button(const UiState &s, uint8_t p, uint8_t kind)
{
    if (quads(s)) {
        return {86, kind == 0 ? 4 : 36, 70, 29};
    }
    if (facing(s)) {
        return {206, kind == 0 ? 5 : 45, 108, 35};
    }
    if (s.game->players == 1) {
        return {176, kind == 0 ? 30 : 92, 138, 56};
    }
    Rect panel = player_panel(s, p);
    return {panel.x + (kind == 0 ? 6 : 82), 110, 72, 36};
}

constexpr int kMenuTiles = 8;

Rect menu_tile(int i)
{
    return {4 + (i % 4) * 79, 30 + (i / 4) * 70, 75, 64};
}

// ---- Drawing helpers --------------------------------------------------------

void draw_gradient(LGFX_Sprite &f, const Theme &t, int h)
{
    for (int y = 0; y < h; y++) {
        f.drawFastHLine(0, y, kWidth, mix(t.bg_top, t.bg_bottom, y * 256 / h));
    }
}

void draw_background(LGFX_Sprite &f, const Theme &t, bool brackets)
{
    draw_gradient(f, t, kHeight);

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

void draw_battery(LGFX_Sprite &f, const Theme &t, int percent, int x = kWidth - 30, int y = 6)
{
    const int w = 22, h = 12;
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

void fit_font(LGFX_Sprite &f, const char *text, int max_w);

// Match score ("1-0") and countdown ("28:14", "+2:10" over time). Returns false
// when there is nothing to show.
bool match_info(const UiState &s, char *score, size_t score_n, char *clock, size_t clock_n,
                uint32_t *clock_color, const Theme &t)
{
    score[0] = clock[0] = '\0';
    if (s.match) {
        snprintf(score, score_n, "%d-%d", s.match->wins[0], s.match->wins[1]);
    }
    if (s.timer_running) {
        int32_t left = s.timer_left_s;
        int32_t a = left < 0 ? -left : left;
        snprintf(clock, clock_n, "%s%ld:%02ld", left < 0 ? "+" : "", (long)(a / 60), (long)(a % 60));
        *clock_color = left < 0 ? kLoss : left < 5 * 60 ? t.accent : t.text;
    }
    return score[0] || clock[0];
}

void draw_header(LGFX_Sprite &f, const Theme &t, const UiState &s, const char *title)
{
    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    f.setTextColor(t.accent);
    f.setTextDatum(textdatum_t::middle_left);
    f.drawString(title, 8, kHeaderH / 2);
    draw_battery(f, t, s.battery_percent);

    if (s.menu || s.dice) {
        return;
    }
    char score[8], clock[12];
    uint32_t clock_color = t.text;
    if (!match_info(s, score, sizeof(score), clock, sizeof(clock), &clock_color, t)) {
        return;
    }
    f.setTextDatum(textdatum_t::middle_right);
    int x = kWidth - 38;
    if (clock[0]) {
        f.setTextColor(clock_color);
        x -= f.drawString(clock, x, kHeaderH / 2) + 10;
    }
    if (score[0]) {
        f.setTextColor(t.muted);
        f.drawString(score, x, kHeaderH / 2);
    }
}

// Match score and countdown in a small pill on the row divider, for layouts
// without a header.
void draw_info_pill(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    char score[8], clock[12], text[24];
    uint32_t clock_color = t.text;
    if (!match_info(s, score, sizeof(score), clock, sizeof(clock), &clock_color, t)) {
        return;
    }
    snprintf(text, sizeof(text), "%s%s%s", score, score[0] && clock[0] ? "  " : "", clock);
    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    int w = f.textWidth(text) + 16;
    Rect r{(kWidth - w) / 2, kStripH - 9, w, 18};
    f.fillRoundRect(r.x, r.y, r.w, r.h, 8, t.surface);
    f.drawRoundRect(r.x, r.y, r.w, r.h, 8, mix(t.surface, t.accent, 160));
    f.setTextDatum(textdatum_t::middle_center);
    f.setTextColor(clock[0] && s.timer_left_s < 0 ? clock_color : t.text);
    f.drawString(text, r.cx(), r.cy());
}

// Game-over banner: who won this game, the match score, what a tap does next.
void draw_banner(LGFX_Sprite &f, const Theme &t, const UiState &s, int winner)
{
    const bb_game_t &g = *s.game;
    const bb_match_t &m = *s.match;
    char line1[32], line2[16], line3[32];

    bb_match_t after = m;
    bb_match_record(&after, (uint8_t)winner);
    bool match_over = bb_match_winner(&after) >= 0;
    const char *who = ui_player_label(g, (uint8_t)winner);
    const char *verb = strcmp(who, "YOU") == 0 ? "WIN" : "WINS";

    if (match_over && m.best_of > 1) {
        snprintf(line1, sizeof(line1), "%s %s THE MATCH", who, verb);
    } else {
        snprintf(line1, sizeof(line1), "%s %s GAME %d", who, verb, m.games + 1);
    }
    snprintf(line2, sizeof(line2), "%d - %d", after.wins[0], after.wins[1]);
    if (match_over) {
        snprintf(line3, sizeof(line3), "Tap for a new match");
    } else {
        snprintf(line3, sizeof(line3), "Tap for game %d", m.games + 2);
    }

    Rect r{24, 36, kWidth - 48, 98};
    f.fillRoundRect(r.x + 3, r.y + 4, r.w, r.h, 12, mix(t.bg_bottom, 0x000000, 160));
    f.fillRoundRect(r.x, r.y, r.w, r.h, 12, t.surface);
    f.drawRoundRect(r.x, r.y, r.w, r.h, 12, kWinColor);
    f.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 11, mix(t.surface, kWinColor, 120));

    f.setTextSize(1);
    f.setTextDatum(textdatum_t::middle_center);
    fit_font(f, line1, r.w - 20);
    f.setTextColor(kWinColor);
    f.drawString(line1, r.cx(), r.y + 22);

    f.setFont(&fonts::FreeSansBold18pt7b);
    f.setTextColor(t.text);
    f.drawString(line2, r.cx(), r.y + 54);

    f.setFont(&fonts::FreeSans9pt7b);
    f.setTextColor(t.muted);
    f.drawString(line3, r.cx(), r.y + 84);
}

// Picks the largest of a few fonts that fits text into max_w.
void fit_font(LGFX_Sprite &f, const char *text, int max_w)
{
    static const lgfx::IFont *const kFonts[] = {
        &fonts::FreeSansBold12pt7b, &fonts::FreeSansBold9pt7b, &fonts::FreeSans9pt7b, &fonts::Font2};
    f.setTextSize(1);
    for (const lgfx::IFont *font : kFonts) {
        f.setFont(font);
        if (f.textWidth(text) <= max_w) {
            return;
        }
    }
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

uint32_t player_color(const Theme &t, uint8_t p)
{
    static const uint32_t kExtra[] = {0x7FD36E, 0xE58AD8}; // P3, P4
    return p == 0 ? t.p1 : p == 1 ? t.p2 : kExtra[(p - 2) & 1];
}

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

void draw_kind_button(LGFX_Sprite &f, const Theme &t, const UiState &s, uint8_t p, uint8_t kind)
{
    const bb_game_t &g = *s.game;
    Rect r = kind_button(s, p, kind);
    char name[12];
    char buf[20];
    uint32_t kc = t.kind[kind];

    f.fillRoundRect(r.x, r.y, r.w, r.h, 8, t.surface);
    f.drawRoundRect(r.x, r.y, r.w, r.h, 8, mix(t.surface, kc, 200));
    f.setTextDatum(textdatum_t::middle_center);

    if (g.players == 1) {
        upper(name, sizeof(name), g.profile->kinds[kind], 10);
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextColor(kc);
        f.drawString(name, r.cx(), r.y + 17);
        snprintf(buf, sizeof(buf), "%d", g.tally[0][kind]);
        f.setFont(&fonts::FreeSansBold12pt7b);
        f.setTextColor(t.text);
        f.drawString(buf, r.cx(), r.y + 39);
    } else {
        upper(name, sizeof(name), g.profile->kinds[kind], quads(s) ? 4 : facing(s) ? 10 : 4);
        snprintf(buf, sizeof(buf), "%s %d", name, g.tally[p][kind]);
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextSize(1);
        f.setTextColor(kc);
        f.drawString(buf, r.cx(), r.cy());
    }
}

// The order a player's points came in, oldest first (kind per point).
size_t point_track(const bb_game_t &g, uint8_t p, uint8_t *kinds, size_t max)
{
    bb_entry_t h[BB_HISTORY_LEN + 1];
    size_t n = bb_game_history(&g, h, BB_HISTORY_LEN);
    if (g.pending.delta != 0) {
        // Newest first, so the pending change goes in front.
        memmove(h + 1, h, n * sizeof(h[0]));
        h[0] = g.pending;
        n++;
    }

    size_t count = 0;
    for (size_t i = n; i-- > 0;) {
        const bb_entry_t &e = h[i];
        if (e.player != p) {
            continue;
        }
        for (int d = e.delta; d > 0 && count < max; d--) {
            kinds[count++] = e.kind;
        }
        for (int d = e.delta; d < 0 && count > 0; d++) {
            count--;
        }
    }
    return count;
}

// Slots for each point toward the win, filled with the kind's initial in order.
// The slots fill a box w wide, in the given number of rows.
void draw_track(LGFX_Sprite &f, const Theme &t, const bb_game_t &g, uint8_t p, int x, int y,
                int w, int slot_h, int gap, int rows)
{
    constexpr int kMaxSlots = 12;
    int slots = g.profile->win_at != BB_NONE ? g.profile->win_at : 8;
    if (slots > kMaxSlots) {
        slots = kMaxSlots;
    }
    int per_row = (slots + rows - 1) / rows;
    int slot_w = (w - (per_row - 1) * gap) / per_row;
    uint8_t kinds[kMaxSlots];
    size_t n = point_track(g, p, kinds, slots);

    if (slot_w >= 15 && slot_h >= 15) {
        f.setFont(&fonts::FreeSansBold9pt7b);
    } else {
        f.setFont(&fonts::Font0);
    }
    f.setTextSize(1);
    f.setTextDatum(textdatum_t::middle_center);
    for (int i = 0; i < slots; i++) {
        int sx = x + (i % per_row) * (slot_w + gap);
        int sy = y + (i / per_row) * (slot_h + gap);
        int r = slot_h >= 12 ? 4 : 2;
        if ((size_t)i < n) {
            uint32_t kc = t.kind[kinds[i]];
            char letter[2] = {g.profile->kinds[kinds[i]][0], '\0'};
            f.fillRoundRect(sx, sy, slot_w, slot_h, r, kc);
            f.setTextColor(kInk);
            f.drawString(letter, sx + slot_w / 2, sy + slot_h / 2 + 1);
        } else {
            f.drawRoundRect(sx, sy, slot_w, slot_h, r, mix(t.bg_bottom, t.accent, 70));
        }
    }
}

// Mixed history for games without kinds, newest first.
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
        snprintf(buf, sizeof(buf), "%+d", e.delta);
        uint32_t c = g.players > 1 ? player_color(t, e.player) : (e.delta > 0 ? kGain : kLoss);
        f.setTextColor(c);
        x += f.drawString(buf, x, kHeight - 3) + 9;
    }
}

void draw_toast(LGFX_Sprite &f, const Theme &t, const char *msg, int cy)
{
    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    int w = f.textWidth(msg) + 24;
    Rect r{(kWidth - w) / 2, cy - 10, w, 20};
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
        // Number on the left, kind buttons on the right, point track along the bottom.
        draw_number(f, buf, 86, 82, 150, 90, vc, shadow);
        draw_kind_button(f, t, s, 0, 0);
        draw_kind_button(f, t, s, 0, 1);
        draw_track(f, t, g, 0, 10, kFooterTop + 2, 300, 17, 4, 1);
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
        draw_footer(f, t, g);
    }

    uint32_t sc = status_text(g, 0, buf, sizeof(buf));
    if (buf[0] && !(has_kinds(g) && g.pending.delta != 0)) {
        f.setFont(&fonts::FreeSansBold12pt7b);
        f.setTextColor(sc);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(buf, has_kinds(g) ? 86 : kWidth / 2, has_kinds(g) ? 32 : 136);
    }
}

void draw_side_by_side(LGFX_Sprite &f, const Theme &t, const UiState &s)
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
        Rect panel = player_panel(s, p);

        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextSize(1);
        f.setTextColor(player_color(t, p));
        f.setTextDatum(textdatum_t::middle_left);
        f.drawString(ui_player_label(g, p), panel.x + 10, panel.y + 14);

        uint32_t sc = status_text(g, p, buf, sizeof(buf));
        if (buf[0]) {
            f.setTextColor(sc);
            f.setTextDatum(textdatum_t::middle_right);
            f.drawString(buf, panel.x + panel.w - 10, panel.y + 14);
        }

        snprintf(buf, sizeof(buf), "%d", g.value[p]);
        uint32_t vc = value_color(t, bb_game_status(&g, p));
        if (has_kinds(g)) {
            draw_number(f, buf, panel.cx(), 76, 140, 58, vc, shadow);
            draw_kind_button(f, t, s, p, 0);
            draw_kind_button(f, t, s, p, 1);
            draw_track(f, t, g, p, panel.x + 9, kFooterTop + 2, 142, 17, 2, 1);
        } else {
            uint32_t hint = mix(t.bg_bottom, t.accent, 110);
            draw_plus_minus(f, panel.x + panel.w - 16, 58, true, hint, 6);
            draw_plus_minus(f, panel.x + panel.w - 16, 128, false, hint, 6);
            draw_number(f, buf, panel.cx() - 6, 92, 124, 80, vc, shadow);
        }
    }

    if (!has_kinds(g)) {
        draw_footer(f, t, g);
    }
}

// One player's half of the facing layout, drawn upright into a kWidth x kStripH sprite.
void draw_strip(LGFX_Sprite &f, const Theme &t, const UiState &s, uint8_t p)
{
    const bb_game_t &g = *s.game;
    char buf[16];
    uint32_t shadow = mix(t.bg_bottom, 0x000000, 128);

    draw_gradient(f, t, kStripH);

    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    f.setTextColor(player_color(t, p));
    f.setTextDatum(textdatum_t::middle_left);
    f.drawString(ui_player_label(g, p), 8, 12);

    char status[16];
    uint32_t sc = status_text(g, p, status, sizeof(status));

    snprintf(buf, sizeof(buf), "%d", g.value[p]);
    uint32_t vc = value_color(t, bb_game_status(&g, p));

    if (has_kinds(g)) {
        draw_number(f, buf, 60, 50, 90, 58, vc, shadow);
        draw_track(f, t, g, p, 106, 20, 96, 18, 3, 2);
        draw_kind_button(f, t, s, p, 0);
        draw_kind_button(f, t, s, p, 1);
        if (status[0]) {
            f.setFont(&fonts::FreeSansBold9pt7b);
            f.setTextColor(sc);
            f.setTextDatum(textdatum_t::middle_center);
            f.drawString(status, 154, 72);
        }
    } else {
        uint32_t hint = mix(t.bg_bottom, t.accent, 110);
        draw_plus_minus(f, 28, 48, false, hint, 9);
        draw_plus_minus(f, kWidth - 28, 48, true, hint, 9);
        draw_number(f, buf, kWidth / 2, 46, 180, 70, vc, shadow);
        if (status[0]) {
            f.setFont(&fonts::FreeSansBold9pt7b);
            f.setTextColor(sc);
            f.setTextDatum(textdatum_t::middle_right);
            f.drawString(status, kWidth - 8, 12);
        }
    }
}

// Rotates a sprite's contents 180 degrees: reversing the pixel order does exactly that.
void rotate_180(LGFX_Sprite &sprite)
{
    uint16_t *px = (uint16_t *)sprite.getBuffer();
    for (int i = 0, j = sprite.width() * sprite.height() - 1; i < j; i++, j--) {
        uint16_t tmp = px[i];
        px[i] = px[j];
        px[j] = tmp;
    }
}

// Horizontal rule between the top and bottom rows, brightest in the middle.
void draw_row_divider(LGFX_Sprite &f, const Theme &t)
{
    for (int x = 0; x < kWidth; x++) {
        int d = x < kWidth / 2 ? x : kWidth - 1 - x;
        f.drawPixel(x, kStripH, mix(t.bg_bottom, t.accent, d * 256 / (kWidth / 2)));
    }
}

LGFX_Sprite *strip_sprite(LGFX_Sprite &frame)
{
    static LGFX_Sprite strip;
    if (!strip.getBuffer()) {
        strip.setColorDepth(16);
#ifdef ESP_PLATFORM
        strip.setPsram(true);
#endif
        strip.createSprite(kWidth, kStripH);
    }
    (void)frame;
    return &strip;
}

void draw_facing(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    LGFX_Sprite *strip = strip_sprite(f);

    // YOU along the bottom, upright.
    draw_strip(*strip, t, s, 0);
    strip->pushSprite(&f, 0, kHeight - kStripH);

    // OPP along the top, upside down.
    draw_strip(*strip, t, s, 1);
    rotate_180(*strip);
    strip->pushSprite(&f, 0, 0);

    draw_row_divider(f, t);
}

// One counter's quarter of the 3-4 player layout, at x0 in a strip sprite.
void draw_quadrant(LGFX_Sprite &f, const Theme &t, const UiState &s, uint8_t p, int x0)
{
    const bb_game_t &g = *s.game;
    char buf[16];
    uint32_t shadow = mix(t.bg_bottom, 0x000000, 128);

    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    f.setTextColor(player_color(t, p));
    f.setTextDatum(textdatum_t::middle_left);
    f.drawString(ui_player_label(g, p), x0 + 8, 11);

    snprintf(buf, sizeof(buf), "%d", g.value[p]);
    uint32_t vc = value_color(t, bb_game_status(&g, p));

    if (has_kinds(g)) {
        draw_number(f, buf, x0 + 42, 44, 74, 40, vc, shadow);
        for (uint8_t k = 0; k < 2; k++) {
            Rect r = kind_button(s, p, k);
            r.x += x0;
            char name[8];
            upper(name, sizeof(name), g.profile->kinds[k], 1);
            snprintf(buf, sizeof(buf), "%s %d", name, g.tally[p][k]);
            f.fillRoundRect(r.x, r.y, r.w, r.h, 7, t.surface);
            f.drawRoundRect(r.x, r.y, r.w, r.h, 7, mix(t.surface, t.kind[k], 200));
            f.setFont(&fonts::FreeSansBold9pt7b);
            f.setTextColor(t.kind[k]);
            f.setTextDatum(textdatum_t::middle_center);
            f.drawString(buf, r.cx(), r.cy());
        }
        draw_track(f, t, g, p, x0 + 6, 70, 148, 11, 2, 1);
    } else {
        char status[16];
        uint32_t sc = status_text(g, p, status, sizeof(status));
        if (status[0]) {
            f.setTextColor(sc);
            f.setTextDatum(textdatum_t::middle_right);
            f.drawString(status, x0 + kQuadW - 8, 11);
        }
        uint32_t hint = mix(t.bg_bottom, t.accent, 110);
        draw_plus_minus(f, x0 + 14, 50, false, hint, 6);
        draw_plus_minus(f, x0 + kQuadW - 14, 50, true, hint, 6);
        draw_number(f, buf, x0 + kQuadW / 2, 50, 96, 56, vc, shadow);
    }
}

// The spare quarter when only three counters are in play.
void draw_info_quadrant(LGFX_Sprite &f, const Theme &t, const UiState &s, int x0)
{
    fit_font(f, s.game->profile->name, kQuadW - 16);
    f.setTextColor(t.accent);
    f.setTextDatum(textdatum_t::middle_center);
    f.drawString(s.game->profile->name, x0 + kQuadW / 2, 34);
    draw_battery(f, t, s.battery_percent, x0 + kQuadW / 2 - 11, 54);
}

void draw_quads(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    LGFX_Sprite *strip = strip_sprite(f);

    for (int row = 0; row < 2; row++) {
        // Row 0 (bottom): P1, P2. Row 1 (top): P3, P4 (or the info quarter).
        draw_gradient(*strip, t, kStripH);
        for (int col = 0; col < 2; col++) {
            uint8_t p = (uint8_t)(row * 2 + col);
            if (p < s.game->players) {
                draw_quadrant(*strip, t, s, p, col * kQuadW);
            } else {
                draw_info_quadrant(*strip, t, s, col * kQuadW);
            }
        }
        for (int y = 6; y < kStripH - 6; y++) {
            strip->drawPixel(kQuadW, y, mix(t.bg_bottom, t.accent, 120));
        }
        if (row == 1 && s.layout == Layout::Facing) {
            rotate_180(*strip);
        }
        strip->pushSprite(&f, 0, row == 0 ? kHeight - kStripH : 0);
    }

    draw_row_divider(f, t);
}

void draw_menu(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    const bb_game_t &g = *s.game;
    static const char *const kPlayerNames[] = {"Just you", "Two", "Three", "Four"};
    static const char *const kTeamNames[] = {"One team", "Two teams"};
    const char *players = g.profile->teams ? kTeamNames[(g.players - 1) & 1] : kPlayerNames[g.players - 1];

    char match[8], timer[8];
    snprintf(match, sizeof(match), s.best_of ? "Bo%d" : "Off", s.best_of);
    snprintf(timer, sizeof(timer), s.timer_minutes ? "%d min" : "Off", s.timer_minutes);

    const char *labels[kMenuTiles] = {"Game", "Players", "Layout", "Flip",
                                      "Match", "Timer", "Restart", "Dice"};
    const char *values[kMenuTiles] = {
        g.profile->name,
        players,
        g.players > 1 ? ui_layout_name(g, s.layout) : "-",
        s.flipped ? "On" : "Off",
        g.players <= 2 ? match : "-",
        timer,
        "New",
        "Roll",
    };

    // Close mark in the header.
    for (int i = -1; i <= 1; i++) {
        f.drawLine(kWidth - 52 + i, 7, kWidth - 42 + i, 17, t.muted);
        f.drawLine(kWidth - 42 + i, 7, kWidth - 52 + i, 17, t.muted);
    }

    for (int i = 0; i < kMenuTiles; i++) {
        Rect r = menu_tile(i);
        bool enabled = !(i == 2 && g.players < 2) && !(i == 4 && g.players > 2);
        f.fillRoundRect(r.x, r.y, r.w, r.h, 8, t.surface);
        f.drawRoundRect(r.x, r.y, r.w, r.h, 8, mix(t.surface, t.accent, enabled ? 140 : 40));

        f.setFont(&fonts::FreeSans9pt7b);
        f.setTextSize(1);
        f.setTextColor(t.muted);
        f.setTextDatum(textdatum_t::middle_left);
        f.drawString(labels[i], r.x + 8, r.y + 15);

        fit_font(f, values[i], r.w - 10);
        f.setTextColor(enabled ? t.accent : t.muted);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(values[i], r.cx(), r.y + 44);
    }
}

constexpr int kDiceChipY = 30;
constexpr int kDiceChipH = 26;

Rect dice_chip(int i)
{
    int n = (int)bb_dice_count();
    int w = (kWidth - 8 - (n - 1) * 4) / n;
    return {4 + i * (w + 4), kDiceChipY, w, kDiceChipH};
}

// ---- Dice -------------------------------------------------------------------

constexpr float kPi = 3.14159265f;

// Unit-circle outline of each die's silhouette (x right, y down), and how much
// of the radius is free for the number inside it.
struct DieShape {
    int n;
    float px[6];
    float py[6];
    float inner; // fraction of the radius usable for the number
};

DieShape die_shape(uint8_t sides)
{
    DieShape d{};
    auto regular = [&](int n, float start) {
        d.n = n;
        for (int i = 0; i < n; i++) {
            float a = start + i * 2 * kPi / n;
            d.px[i] = cosf(a);
            d.py[i] = sinf(a);
        }
    };
    switch (sides) {
    case 4:
        regular(3, -kPi / 2); // triangle, point up
        d.inner = 0.48f;
        break;
    case 6:
        regular(4, -kPi / 4); // square
        d.inner = 0.66f;
        break;
    case 8:
        regular(4, -kPi / 2); // diamond, stretched tall
        for (int i = 0; i < 4; i++) {
            d.px[i] *= 0.85f;
            d.py[i] *= 1.1f;
        }
        d.inner = 0.5f;
        break;
    case 10: {
        const float x[4] = {0.f, 0.82f, 0.f, -0.82f};
        const float y[4] = {-1.05f, 0.12f, 0.85f, 0.12f}; // kite
        d.n = 4;
        for (int i = 0; i < 4; i++) {
            d.px[i] = x[i];
            d.py[i] = y[i];
        }
        d.inner = 0.5f;
        break;
    }
    case 12:
        regular(5, -kPi / 2); // pentagon
        d.inner = 0.62f;
        break;
    default:
        regular(6, -kPi / 2); // d20: hexagon
        d.inner = 0.6f;
        break;
    }
    return d;
}

// Draws a die silhouette rotated by angle and scaled to radius r.
void draw_die_body(LGFX_Sprite &f, uint8_t sides, int cx, int cy, float r, float angle,
                   uint32_t fill, uint32_t edge, uint32_t facet)
{
    DieShape d = die_shape(sides);
    float c = cosf(angle), sn = sinf(angle);
    int x[6], y[6];
    for (int i = 0; i < d.n; i++) {
        x[i] = cx + (int)lroundf((d.px[i] * c - d.py[i] * sn) * r);
        y[i] = cy + (int)lroundf((d.px[i] * sn + d.py[i] * c) * r);
    }
    for (int i = 0; i < d.n; i++) {
        f.fillTriangle(cx, cy, x[i], y[i], x[(i + 1) % d.n], y[(i + 1) % d.n], fill);
    }
    // Facet lines from the center give a bit of depth.
    for (int i = 0; i < d.n; i++) {
        f.drawLine(cx, cy, x[i], y[i], facet);
    }
    if (sides == 20) {
        // The classic d20 look: a triangle face inside the hexagon.
        for (int i = 0; i < 3; i++) {
            int a = i * 2, b = (i * 2 + 2) % 6;
            f.drawLine(x[a], y[a], x[b], y[b], facet);
        }
    }
    for (int i = 0; i < d.n; i++) {
        int j = (i + 1) % d.n;
        f.drawLine(x[i], y[i], x[j], y[j], edge);
        f.drawLine(x[i] + (cx > x[i] ? 1 : -1), y[i], x[j] + (cx > x[j] ? 1 : -1), y[j], edge);
    }
}

// A coin seen edge-on as it flips: width follows |cos| of the flip angle.
void draw_coin(LGFX_Sprite &f, const Theme &t, int cx, int cy, int r, float flip, uint8_t face,
               bool settled)
{
    float squash = fabsf(cosf(flip));
    int rx = (int)(r * squash);
    bool heads = cosf(flip) >= 0 ? face != 2 : face == 2;
    uint32_t rim = heads ? kWinColor : 0xC8CCD4;
    uint32_t body = heads ? 0x5E4A1E : 0x4A505C; // bronze / steel
    if (rx < 2) {
        f.fillRect(cx - 1, cy - r, 3, r * 2, rim);
        return;
    }
    f.fillEllipse(cx, cy, rx, r, body);
    f.drawEllipse(cx, cy, rx, r, rim);
    if (rx > 6) {
        f.drawEllipse(cx, cy, rx - 4, r - 4, mix(body, rim, 140));
    }
    if (face != 0 && (settled || squash > 0.7f)) {
        f.setFont(&fonts::FreeSansBold12pt7b);
        f.setTextSize(squash * 0.85f);
        f.setTextColor(settled ? t.text : mix(body, t.text, 150));
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(heads ? "HEADS" : "TAILS", cx, cy + 1);
        f.setTextSize(1);
    }
}

float ease_out_cubic(float t)
{
    float u = 1 - t;
    return 1 - u * u * u;
}

void draw_dice(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    const DiceState &d = *s.dice;
    const bb_die_t *die = bb_die_at(d.die);
    char buf[16];

    // Close mark in the header.
    for (int i = -1; i <= 1; i++) {
        f.drawLine(kWidth - 52 + i, 7, kWidth - 42 + i, 17, t.muted);
        f.drawLine(kWidth - 42 + i, 7, kWidth - 52 + i, 17, t.muted);
    }

    // Die picker.
    for (int i = 0; i < (int)bb_dice_count(); i++) {
        Rect r = dice_chip(i);
        bool sel = i == d.die;
        f.fillRoundRect(r.x, r.y, r.w, r.h, 6, sel ? t.accent : t.surface);
        f.drawRoundRect(r.x, r.y, r.w, r.h, 6, mix(t.surface, t.accent, sel ? 256 : 110));
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextSize(1);
        f.setTextColor(sel ? kInk : t.text);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(bb_die_at(i)->name, r.cx(), r.cy() + 1);
    }

    // The die: tumbles while rolling (spin slows to a stop), pops when it lands.
    const int cx = kWidth / 2, cy = 103;
    const float base_r = 44;
    float t_roll = d.rolling ? d.anim / 1000.0f : 1.0f;
    float spin = d.rolling ? (1 - ease_out_cubic(t_roll)) * 3.2f * 2 * kPi : 0;
    float pop = d.pop / 1000.0f;
    float scale = d.rolling ? 0.92f + 0.08f * t_roll : 1 + 0.08f * (1 - ease_out_cubic(pop));
    bool max_roll = !d.rolling && d.value == die->sides && die->sides >= 6;
    bool min_roll = !d.rolling && d.value == 1 && die->sides >= 6;

    if (!d.rolling && d.value && pop < 1 && (max_roll || min_roll)) {
        // Glow rings that fade out as the pop settles.
        uint32_t glow = max_roll ? kWinColor : kLoss;
        for (int i = 0; i < 3; i++) {
            int fade = (int)((1 - pop) * (220 - i * 60));
            f.drawCircle(cx, cy, (int)(base_r * scale) + 4 + i * 3, mix(t.bg_bottom, glow, fade > 0 ? fade : 0));
        }
    }

    float r = base_r * scale;
    if (die->sides == 2) {
        float flip = d.rolling ? (1 - ease_out_cubic(t_roll)) * 7 * kPi : 0;
        draw_coin(f, t, cx, cy, (int)(r * 0.95f), flip, d.value, !d.rolling && d.value);
        if (!d.value) {
            f.setFont(&fonts::FreeSansBold12pt7b);
            f.setTextColor(t.text);
            f.setTextDatum(textdatum_t::middle_center);
            f.drawString("FLIP", cx, cy + 1);
        }
    } else {
        // Triangles sit lower so they look centered.
        int dy = die->sides == 4 ? (int)(r * 0.18f) : 0;
        uint32_t edge = d.rolling ? mix(t.surface, t.accent, 150)
                        : max_roll  ? kWinColor
                        : min_roll  ? kLoss
                                    : t.accent;
        draw_die_body(f, die->sides, cx, cy + dy, r, spin, t.surface,
                      edge, mix(t.surface, t.accent, 45));
        DieShape shape = die_shape(die->sides);
        int box = (int)(r * shape.inner * 2);
        if (d.value == 0) {
            f.setFont(&fonts::FreeSansBold12pt7b);
            f.setTextColor(t.text);
            f.setTextDatum(textdatum_t::middle_center);
            bool narrow = die->sides == 4 || die->sides == 8 || die->sides == 10;
            f.drawString(narrow ? "TAP" : "ROLL", cx, cy + dy + 2);
        } else {
            snprintf(buf, sizeof(buf), "%d", d.value);
            uint32_t c = d.rolling ? t.muted : max_roll ? kWinColor : min_roll ? kLoss : t.text;
            draw_number(f, buf, cx, cy + dy + 2, box, (int)(box * 0.8f), c,
                        mix(t.surface, 0x000000, 160));
        }
    }

    // Previous rolls, newest first.
    f.setFont(&fonts::FreeSans9pt7b);
    f.setTextSize(1);
    f.setTextDatum(textdatum_t::bottom_left);
    int x = 8;
    for (int i = 0; i < d.history_count && x < kWidth - 40; i++) {
        const bb_die_t *hd = bb_die_at(d.history_die[i]);
        if (hd->sides == 2) {
            snprintf(buf, sizeof(buf), "%s", d.history[i] == 1 ? "H" : "T");
        } else {
            snprintf(buf, sizeof(buf), "%d", d.history[i]);
        }
        f.setTextColor(i == 0 && !d.rolling ? t.text : t.muted);
        x += f.drawString(buf, x, kHeight - 3) + 4;
        f.setTextColor(t.muted);
        x += f.drawString(hd->sides == 2 ? "" : hd->name, x, kHeight - 3) + 10;
    }
}

UiAction kind_hit(const UiState &s, uint8_t p, int x, int y)
{
    UiAction a;
    for (uint8_t k = 0; k < 2; k++) {
        Rect r = kind_button(s, p, k);
        // Generous targets: each button owns its half of the button column.
        bool hit;
        if (facing(s)) {
            hit = x >= r.x - 6 && (k == 0 ? y < kStripH / 2 : y >= kStripH / 2);
        } else if (s.game->players == 1) {
            hit = x >= r.x - 8 && (k == 0 ? y < 89 : y >= 89);
        } else {
            Rect panel = player_panel(s, p);
            hit = y >= r.y - 10 && y < kFooterTop && (k == 0 ? x < panel.cx() : x >= panel.cx());
        }
        if (hit) {
            a.type = UiAction::Adjust;
            a.player = p;
            a.kind = k;
            a.delta = 1;
            return a;
        }
    }
    return a;
}

} // namespace

const char *ui_player_label(const bb_game_t &game, uint8_t player)
{
    static const char *const kNumbered[] = {"P1", "P2", "P3", "P4"};
    if (game.profile->teams) {
        return player == 0 ? "TEAM A" : "TEAM B";
    }
    if (game.players > 2) {
        return kNumbered[player & 3];
    }
    return player == 0 ? "YOU" : "OPP";
}

const char *ui_layout_name(const bb_game_t &game, Layout layout)
{
    if (game.players > 2) {
        return layout == Layout::Facing ? "Facing" : "Upright";
    }
    static const char *const kNames[] = {"YOU left", "YOU right", "Facing"};
    if (game.profile->teams) {
        static const char *const kTeamNames[] = {"A left", "A right", "Facing"};
        return kTeamNames[(int)layout];
    }
    return kNames[(int)layout];
}

Layout ui_next_layout(const bb_game_t &game, Layout layout)
{
    if (game.players > 2) {
        return layout == Layout::Facing ? Layout::SideYouLeft : Layout::Facing;
    }
    return (Layout)(((int)layout + 1) % (int)Layout::Count);
}

void ui_draw(LGFX_Sprite &frame, const UiState &state)
{
    const Theme &t = theme_for(state.game->profile);

    if (state.dice && !state.menu) {
        draw_background(frame, t, false);
        draw_header(frame, t, state, "DICE");
        draw_dice(frame, t, state);
        return;
    }

    if (!state.menu && (facing(state) || quads(state))) {
        if (quads(state)) {
            draw_quads(frame, t, state);
        } else {
            draw_facing(frame, t, state);
        }
        draw_info_pill(frame, t, state);
        int winner = ui_banner_winner(state);
        if (winner >= 0) {
            draw_banner(frame, t, state, winner);
        }
        if (state.toast) {
            draw_toast(frame, t, state.toast, kStripH);
        }
        return;
    }

    draw_background(frame, t, !state.menu);
    draw_header(frame, t, state, state.menu ? "MENU" : state.game->profile->name);

    if (state.menu) {
        draw_menu(frame, t, state);
        return;
    }

    if (state.game->players == 1) {
        draw_single(frame, t, state);
    } else {
        draw_side_by_side(frame, t, state);
    }

    int winner = ui_banner_winner(state);
    if (winner >= 0) {
        draw_banner(frame, t, state, winner);
    }
    if (state.toast) {
        draw_toast(frame, t, state.toast, (kFooterTop + kHeight) / 2);
    }
}

int ui_banner_winner(const UiState &state)
{
    if (state.menu || state.dice || !state.match) {
        return -1;
    }
    return bb_game_winner(state.game);
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
        static const UiAction::Type kTileActions[kMenuTiles] = {
            UiAction::MenuGame,  UiAction::MenuPlayers, UiAction::MenuLayout,  UiAction::MenuFlip,
            UiAction::MenuMatch, UiAction::MenuTimer,   UiAction::MenuRestart, UiAction::MenuDice};
        for (int i = 0; i < kMenuTiles; i++) {
            Rect r = menu_tile(i);
            // Tiles are hit-tested with the gaps included so near-misses still count.
            Rect padded{r.x - 2, r.y - 3, r.w + 4, r.h + 6};
            if (padded.contains(x, y)) {
                if ((kTileActions[i] == UiAction::MenuLayout && g.players < 2) ||
                    (kTileActions[i] == UiAction::MenuMatch && g.players > 2)) {
                    return a;
                }
                a.type = kTileActions[i];
                return a;
            }
        }
        return a;
    }

    if (state.dice) {
        if (y < kHeaderH) {
            a.type = UiAction::DiceClose;
            return a;
        }
        for (int i = 0; i < (int)bb_dice_count(); i++) {
            Rect r = dice_chip(i);
            Rect padded{r.x - 2, r.y - 4, r.w + 4, r.h + 8};
            if (padded.contains(x, y)) {
                a.type = UiAction::DiceSelect;
                a.kind = (uint8_t)i;
                return a;
            }
        }
        a.type = UiAction::DiceRoll;
        return a;
    }

    // While the game-over banner shows, any tap moves on to the next game.
    if (ui_banner_winner(state) >= 0) {
        a.type = UiAction::NextGame;
        return a;
    }

    if (quads(state)) {
        // Map to the touched quarter's own (upright) coordinates.
        bool top = y < kStripH;
        bool turned = top && state.layout == Layout::Facing;
        int lx = turned ? kWidth - 1 - x : x;
        int ly = turned ? kStripH - 1 - y : (top ? y : y - (kHeight - kStripH));
        uint8_t p = (uint8_t)((top ? 2 : 0) + (lx >= kQuadW ? 1 : 0));
        if (p >= g.players) {
            return a;
        }
        int qx = lx % kQuadW;
        a.player = p;
        if (has_kinds(g)) {
            if (qx >= 80 && ly < 68) {
                a.type = UiAction::Adjust;
                a.kind = ly < 35 ? 0 : 1;
                a.delta = 1;
            }
        } else if (qx < 64) {
            a.type = UiAction::Adjust;
            a.delta = -1;
            a.repeat = true;
        } else if (qx >= kQuadW - 64) {
            a.type = UiAction::Adjust;
            a.delta = 1;
            a.repeat = true;
        }
        return a;
    }

    if (facing(state)) {
        // Map to the touched strip's own (upright) coordinates.
        uint8_t p = y < kStripH ? 1 : 0;
        int lx = p == 1 ? kWidth - 1 - x : x;
        int ly = p == 1 ? kStripH - 1 - y : y - (kHeight - kStripH);
        if (has_kinds(g)) {
            return kind_hit(state, p, lx, ly);
        }
        a.player = p;
        if (lx < kWidth / 3) {
            a.type = UiAction::Adjust;
            a.delta = -1;
            a.repeat = true;
        } else if (lx >= kWidth * 2 / 3) {
            a.type = UiAction::Adjust;
            a.delta = 1;
            a.repeat = true;
        }
        return a;
    }

    if (y < kHeaderH || y >= kFooterTop + 6) {
        return a;
    }

    uint8_t p = 0;
    if (g.players == 2) {
        p = (x < kWidth / 2) ? left_player(state) : (uint8_t)(1 - left_player(state));
    }
    Rect panel = player_panel(state, p);
    a.player = p;

    if (has_kinds(g)) {
        // Kind buttons only; mistakes are fixed with undo.
        return kind_hit(state, p, x, y);
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
