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
constexpr uint32_t kInk = 0x10141C; // dark text on filled slots

constexpr int kHeaderH = 24;
constexpr int kFooterTop = 150;
constexpr int kStripH = kHeight / 2; // facing layout: one strip per player

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

// Kind buttons (e.g. Conquer / Hold). Facing-layout rects are in strip coordinates.
Rect kind_button(const UiState &s, uint8_t p, uint8_t kind)
{
    if (facing(s)) {
        return {206, kind == 0 ? 5 : 45, 108, 35};
    }
    if (s.game->players == 1) {
        return {176, kind == 0 ? 30 : 92, 138, 56};
    }
    Rect panel = player_panel(s, p);
    return {panel.x + (kind == 0 ? 6 : 82), 110, 72, 36};
}

constexpr int kMenuTiles = 6;

Rect menu_tile(int i)
{
    return {6 + (i % 3) * 104, 30 + (i / 3) * 70, 100, 64};
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
        upper(name, sizeof(name), g.profile->kinds[kind], facing(s) ? 10 : 4);
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
void draw_track(LGFX_Sprite &f, const Theme &t, const bb_game_t &g, uint8_t p, int x, int y,
                int slot_w, int slot_h, int gap, int per_row)
{
    int slots = g.profile->win_at != BB_NONE ? g.profile->win_at : 8;
    if (slots > 10) {
        slots = 10;
    }
    uint8_t kinds[10];
    size_t n = point_track(g, p, kinds, slots);

    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    f.setTextDatum(textdatum_t::middle_center);
    for (int i = 0; i < slots; i++) {
        int sx = x + (i % per_row) * (slot_w + gap);
        int sy = y + (i / per_row) * (slot_h + gap);
        if ((size_t)i < n) {
            uint32_t kc = t.kind[kinds[i]];
            char letter[2] = {g.profile->kinds[kinds[i]][0], '\0'};
            f.fillRoundRect(sx, sy, slot_w, slot_h, 4, kc);
            f.setTextColor(kInk);
            f.drawString(letter, sx + slot_w / 2, sy + slot_h / 2 + 1);
        } else {
            f.drawRoundRect(sx, sy, slot_w, slot_h, 4, mix(t.bg_bottom, t.accent, 70));
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
        draw_track(f, t, g, 0, 10, kFooterTop + 2, 34, 17, 4, 8);
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
            draw_kind_button(f, t, s, p, 0);
            draw_kind_button(f, t, s, p, 1);
            draw_track(f, t, g, p, panel.x + 9, kFooterTop + 2, 16, 17, 2, 8);
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
    f.drawString(ui_player_label(p), 8, 12);

    char status[16];
    uint32_t sc = status_text(g, p, status, sizeof(status));

    snprintf(buf, sizeof(buf), "%d", g.value[p]);
    uint32_t vc = value_color(t, bb_game_status(&g, p));

    if (has_kinds(g)) {
        draw_number(f, buf, 60, 50, 90, 58, vc, shadow);
        draw_track(f, t, g, p, 108, 20, 21, 18, 3, 4);
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

    // OPP along the top, rotated 180 degrees: reversing the pixel order does exactly that.
    draw_strip(*strip, t, s, 1);
    uint16_t *px = (uint16_t *)strip->getBuffer();
    for (int i = 0, j = kWidth * kStripH - 1; i < j; i++, j--) {
        uint16_t tmp = px[i];
        px[i] = px[j];
        px[j] = tmp;
    }
    strip->pushSprite(&f, 0, 0);

    for (int x = 0; x < kWidth; x++) {
        int d = x < kWidth / 2 ? x : kWidth - 1 - x;
        f.drawPixel(x, kStripH, mix(t.bg_bottom, t.accent, d * 256 / (kWidth / 2)));
    }
}

void draw_menu(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    const bb_game_t &g = *s.game;
    static const char *const kLayoutNames[] = {"YOU left", "YOU right", "Facing"};
    char players[16];
    snprintf(players, sizeof(players), g.players == 1 ? "Just you" : "Both");

    const char *labels[kMenuTiles] = {"Game", "Players", "Layout", "Flip", "Restart", "Sleep"};
    const char *values[kMenuTiles] = {
        g.profile->name,
        players,
        g.players == 2 ? kLayoutNames[(int)s.layout] : "-",
        s.flipped ? "On" : "Off",
        "New game",
        "KEY wakes",
    };

    // Close mark in the header.
    for (int i = -1; i <= 1; i++) {
        f.drawLine(kWidth - 52 + i, 7, kWidth - 42 + i, 17, t.muted);
        f.drawLine(kWidth - 42 + i, 7, kWidth - 52 + i, 17, t.muted);
    }

    for (int i = 0; i < kMenuTiles; i++) {
        Rect r = menu_tile(i);
        bool enabled = !(i == 2 && g.players != 2);
        f.fillRoundRect(r.x, r.y, r.w, r.h, 8, t.surface);
        f.drawRoundRect(r.x, r.y, r.w, r.h, 8, mix(t.surface, t.accent, enabled ? 140 : 40));

        f.setFont(&fonts::FreeSans9pt7b);
        f.setTextSize(1);
        f.setTextColor(t.muted);
        f.setTextDatum(textdatum_t::middle_left);
        f.drawString(labels[i], r.x + 9, r.y + 15);

        fit_font(f, values[i], r.w - 12);
        f.setTextColor(enabled ? t.accent : t.muted);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(values[i], r.cx(), r.y + 44);
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

const char *ui_player_label(uint8_t player)
{
    return player == 0 ? "YOU" : "OPP";
}

void ui_draw(LGFX_Sprite &frame, const UiState &state)
{
    const Theme &t = theme_for(state.game->profile);

    if (!state.menu && facing(state)) {
        draw_facing(frame, t, state);
        if (state.toast) {
            draw_toast(frame, t, state.toast, kStripH);
        }
        return;
    }

    draw_background(frame, t, !state.menu);
    draw_header(frame, t, state.menu ? "MENU" : state.game->profile->name, state.battery_percent);

    if (state.menu) {
        draw_menu(frame, t, state);
        return;
    }

    if (state.game->players == 1) {
        draw_single(frame, t, state);
    } else {
        draw_side_by_side(frame, t, state);
    }

    if (state.toast) {
        draw_toast(frame, t, state.toast, (kFooterTop + kHeight) / 2);
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
        static const UiAction::Type kTileActions[kMenuTiles] = {
            UiAction::MenuGame, UiAction::MenuPlayers, UiAction::MenuLayout,
            UiAction::MenuFlip, UiAction::MenuRestart, UiAction::MenuSleep};
        for (int i = 0; i < kMenuTiles; i++) {
            Rect r = menu_tile(i);
            // Tiles are hit-tested with the gaps included so near-misses still count.
            Rect padded{r.x - 2, r.y - 3, r.w + 4, r.h + 6};
            if (padded.contains(x, y)) {
                if (kTileActions[i] == UiAction::MenuLayout && g.players != 2) {
                    return a;
                }
                a.type = kTileActions[i];
                return a;
            }
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
