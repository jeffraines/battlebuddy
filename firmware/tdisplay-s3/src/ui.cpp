#include "ui.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
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
    const lgfx::IFont *title_font; // game name on picker tiles and in the header
    bool vivid;                    // set by styled(): richer colors and glows
};

// Per-game palettes. Colors only; no game logos or artwork.
const Theme kThemes[] = {
    // Navy and teal with gold trim; gold = Conquer, violet = Hold.
    {"riftbound", 0x0A1428, 0x0B3A44, 0x10283A, 0xC8AA6E, 0x0AC8B9, 0xE8604C, 0xF0E6D2, 0x7E8A96,
     {0xE0B85A, 0xA78BFA}, &fonts::FreeSerifBold12pt7b, false},
    // Charcoal and warm brown with amber.
    {"mtg", 0x15110E, 0x33221A, 0x2A1E17, 0xE0A040, 0x6FB5E0, 0xE8604C, 0xF2E8DC, 0x8C8072,
     {0xE0A040, 0x6FB5E0}, &fonts::FreeSerifBold12pt7b, false},
    // Charcoal and plum with lilac.
    {"mtg-commander", 0x120F1A, 0x2E1F40, 0x251A36, 0xC9A0FF, 0x6FB5E0, 0xE8604C, 0xF0EAFA, 0x8A7FA0,
     {0xC9A0FF, 0x6FB5E0}, &fonts::FreeSerifBold12pt7b, false},
    // Ink blue and violet with gold.
    {"lorcana", 0x0B0F2E, 0x2D1B4E, 0x1C1840, 0xE8C25A, 0x7FB2FF, 0xF07FB0, 0xF4F0FF, 0x8A88AA,
     {0xE8C25A, 0x7FB2FF}, &fonts::Satisfy_24, false},
    // Deep navy with digital orange and cyan.
    {"digimon", 0x07131F, 0x0E2A3A, 0x0F2333, 0xF28C28, 0x2EC4F1, 0xF2545B, 0xEAF6FF, 0x7F95A8,
     {0xF28C28, 0x2EC4F1}, &fonts::Orbitron_Light_24, false},
    // Dark umber with gold.
    {"yugioh", 0x140F05, 0x3A2A0A, 0x2E2410, 0xE6B422, 0x7FB2FF, 0xE8604C, 0xFFF4DA, 0x9C8C6A,
     {0xE6B422, 0x7FB2FF}, &fonts::FreeSerifBoldItalic12pt7b, false},
    // Black and blood red with bronze.
    {"fab", 0x140808, 0x3C1210, 0x2E1210, 0xD4A24C, 0x6FB5E0, 0xE04B3A, 0xF4E6DC, 0x907670,
     {0xD4A24C, 0x6FB5E0}, &fonts::FreeSerifBold12pt7b, false},
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

// Pushes a color away from its own grey (more saturated) by k/256 and adds lift
// to each channel (brighter).
uint32_t boost(uint32_t c, int k, int lift)
{
    int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
    int grey = (r * 77 + g * 151 + b * 28) >> 8;
    int ch[3] = {r, g, b};
    for (int &v : ch) {
        v = grey + (v - grey) * k / 256 + lift;
        v = v < 0 ? 0 : v > 255 ? 255 : v;
    }
    return ((uint32_t)ch[0] << 16) | ((uint32_t)ch[1] << 8) | (uint32_t)ch[2];
}

// The Look setting: Subdued uses the palette as is; Vivid saturates and lifts
// the game's colors and adds glows.
Theme styled(const Theme &base, bool vivid)
{
    Theme t = base;
    t.vivid = vivid;
    if (vivid) {
        t.bg_top = boost(base.bg_top, 560, 4);
        t.bg_bottom = boost(base.bg_bottom, 600, 14);
        t.surface = boost(base.surface, 520, 8);
        t.accent = boost(base.accent, 330, 10);
        t.p1 = boost(base.p1, 320, 8);
        t.p2 = boost(base.p2, 320, 8);
        t.kind[0] = boost(base.kind[0], 320, 8);
        t.kind[1] = boost(base.kind[1], 320, 8);
    }
    return t;
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

bool amount_style(const bb_game_t &g) { return g.profile->style == BB_STYLE_AMOUNT; }

bool gauge_style(const bb_game_t &g) { return g.profile->style == BB_STYLE_GAUGE; }

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
    // Paired buttons leave a gap wider than twice the tap margin, so there is a
    // dead strip between them rather than one button's area touching the other's.
    if (quads(s)) {
        return {86, kind == 0 ? 3 : 38, 70, 28};
    }
    if (facing(s)) {
        return {206, kind == 0 ? 4 : 46, 108, 34};
    }
    if (s.game->players == 1) {
        return {176, kind == 0 ? 30 : 94, 138, 54};
    }
    Rect panel = player_panel(s, p);
    return {panel.x + (kind == 0 ? 5 : 87), 110, 68, 36};
}

constexpr int kMenuTiles = 9;

Rect menu_tile(int i)
{
    return {6 + (i % 3) * 104, 28 + (i / 3) * 47, 100, 44};
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
void draw_title(LGFX_Sprite &f, const Theme &t, const char *text, int x, int cy, int max_w, int max_h,
                uint32_t color, bool two_lines, bool left = false);

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

void draw_header(LGFX_Sprite &f, const Theme &t, const UiState &s, const char *title, bool game_title = false)
{
    if (game_title) {
        draw_title(f, t, title, 8, kHeaderH / 2 + 1, 150, 18, t.accent, false, true);
    } else {
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextSize(1);
        f.setTextColor(t.accent);
        f.setTextDatum(textdatum_t::middle_left);
        f.drawString(title, 8, kHeaderH / 2);
    }
    if (t.vivid) {
        // A second, brighter header rule.
        for (int x = 0; x < kWidth; x++) {
            int d = x < kWidth / 2 ? x : kWidth - 1 - x;
            f.drawPixel(x, kHeaderH + 1, mix(t.bg_top, t.accent, d * 200 / (kWidth / 2)));
        }
    }
    draw_battery(f, t, s.battery_percent);

    if (s.menu || s.dice || s.picker || s.entry) {
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

// Shadow under big numbers: dark when subdued, a colored glow-like offset when vivid.
uint32_t number_shadow(const Theme &t)
{
    return t.vivid ? mix(t.bg_bottom, t.accent, 120) : mix(t.bg_bottom, 0x000000, 128);
}

// Splits text at the space nearest its middle. Returns false if there is no space.
bool split_middle(const char *text, char *a, char *b, size_t n)
{
    size_t len = strlen(text);
    int best = -1;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == ' ' && (best < 0 || abs((int)i - (int)len / 2) < abs(best - (int)len / 2))) {
            best = (int)i;
        }
    }
    if (best < 0 || (size_t)best >= n) {
        return false;
    }
    memcpy(a, text, best);
    a[best] = '\0';
    snprintf(b, n, "%s", text + best + 1);
    return true;
}

void draw_text_glow(LGFX_Sprite &f, const Theme &t, const char *text, int x, int y, uint32_t color)
{
    if (t.vivid) {
        uint32_t g = mix(t.bg_top, color, 90);
        f.setTextColor(g);
        f.drawString(text, x - 1, y);
        f.drawString(text, x + 1, y);
        f.drawString(text, x, y - 1);
        f.drawString(text, x, y + 1);
    }
    f.setTextColor(color);
    f.drawString(text, x, y);
}

// A game's name in its own title font, shrunk to fit max_w x max_h. With
// two_lines it may wrap at a space instead of shrinking too far. When
// left is true, x is the left edge; otherwise the center.
void draw_title(LGFX_Sprite &f, const Theme &t, const char *text, int x, int cy, int max_w, int max_h,
                uint32_t color, bool two_lines, bool left)
{
    f.setFont(t.title_font);
    f.setTextSize(1);
    float h = f.fontHeight();
    float w = f.textWidth(text);
    float scale = max_h / h < 1 ? max_h / h : 1;
    char a[32], b[32];

    if (w * scale > max_w && two_lines && split_middle(text, a, b, sizeof(a))) {
        float wa = f.textWidth(a), wb = f.textWidth(b);
        float ls = (max_h / 2.0f) / (h * 0.8f);
        float wmax = wa > wb ? wa : wb;
        if (ls > 1) {
            ls = 1;
        }
        if (wmax * ls > max_w) {
            ls = max_w / wmax;
        }
        f.setTextSize(ls);
        f.setTextDatum(left ? textdatum_t::middle_left : textdatum_t::middle_center);
        int lh = (int)(h * ls * 0.78f);
        draw_text_glow(f, t, a, x, cy - lh / 2, color);
        draw_text_glow(f, t, b, x, cy + lh / 2, color);
        f.setTextSize(1);
        return;
    }
    if (w * scale > max_w) {
        scale = max_w / w;
    }
    f.setTextSize(scale);
    f.setTextDatum(left ? textdatum_t::middle_left : textdatum_t::middle_center);
    draw_text_glow(f, t, text, x, cy, color);
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

    if (n == 0 && g.pending.delta == 0 && amount_style(g)) {
        f.setFont(&fonts::FreeSans9pt7b);
        f.setTextSize(1);
        f.setTextColor(t.muted);
        f.setTextDatum(textdatum_t::bottom_center);
        f.drawString(g.players > 1 ? "Tap a player to enter an amount" : "Tap to enter an amount",
                     kWidth / 2, kHeight - 3);
        return;
    }

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
    uint32_t shadow = number_shadow(t);

    if (has_kinds(g)) {
        // Number on the left, kind buttons on the right, point track along the bottom.
        draw_number(f, buf, 86, 82, 150, 90, vc, shadow);
        draw_kind_button(f, t, s, 0, 0);
        draw_kind_button(f, t, s, 0, 1);
        draw_track(f, t, g, 0, 10, kFooterTop + 2, 300, 17, 4, 1);
    } else {
        if (!amount_style(g)) {
            uint32_t hint = mix(t.bg_bottom, t.accent, 110);
            draw_plus_minus(f, 28, 87, false, hint, 10);
            draw_plus_minus(f, kWidth - 28, 87, true, hint, 10);
        }
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
    uint32_t shadow = number_shadow(t);

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
        } else if (amount_style(g)) {
            draw_number(f, buf, panel.cx(), 92, 140, 80, vc, shadow);
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
    uint32_t shadow = number_shadow(t);

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
        if (!amount_style(g)) {
            uint32_t hint = mix(t.bg_bottom, t.accent, 110);
            draw_plus_minus(f, 28, 48, false, hint, 9);
            draw_plus_minus(f, kWidth - 28, 48, true, hint, 9);
        }
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
    uint32_t shadow = number_shadow(t);

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
        if (!amount_style(g)) {
            uint32_t hint = mix(t.bg_bottom, t.accent, 110);
            draw_plus_minus(f, x0 + 14, 50, false, hint, 6);
            draw_plus_minus(f, x0 + kQuadW - 14, 50, true, hint, 6);
        }
        draw_number(f, buf, x0 + kQuadW / 2, 50, amount_style(g) ? 136 : 96, 56, vc, shadow);
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

    const char *labels[kMenuTiles] = {"Game", "Players", "Layout", "Match", "Timer",
                                      "Look", "Flip", "Restart", "Dice"};
    const char *values[kMenuTiles] = {
        g.profile->name,
        players,
        g.players > 1 && !gauge_style(g) ? ui_layout_name(g, s.layout) : "-",
        g.players <= 2 ? match : "-",
        timer,
        s.vivid ? "Vivid" : "Subdued",
        s.flipped ? "On" : "Off",
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
        bool enabled = !(i == 2 && (g.players < 2 || gauge_style(g))) && !(i == 3 && g.players > 2) &&
                       !(i == 1 && gauge_style(g));
        f.fillRoundRect(r.x, r.y, r.w, r.h, 8, t.surface);
        f.drawRoundRect(r.x, r.y, r.w, r.h, 8, mix(t.surface, t.accent, enabled ? 140 : 40));

        f.setFont(&fonts::Font2);
        f.setTextSize(1);
        f.setTextColor(t.muted);
        f.setTextDatum(textdatum_t::middle_left);
        f.drawString(labels[i], r.x + 8, r.y + 11);

        fit_font(f, values[i], r.w - 12);
        if (f.getFont() == &fonts::FreeSansBold12pt7b) {
            f.setFont(&fonts::FreeSansBold9pt7b); // keep values to one size in the short tiles
        }
        f.setTextColor(enabled ? t.accent : t.muted);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(values[i], r.cx(), r.y + 29);
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
    bool max_roll = !d.rolling && d.value == die->sides && die->sides >= 4;
    bool min_roll = !d.rolling && d.value == 1 && die->sides >= 4;

    // How good the roll was, 0 (lowest face) to 1 (highest), sets its color:
    // cool for low rolls, warm for high ones.
    float frac = die->sides > 1 && d.value ? (d.value - 1) / (float)(die->sides - 1) : 0.5f;
    uint32_t tier = frac < 0.34f ? mix(t.text, t.p1, 170) : frac > 0.66f ? mix(t.text, t.accent, 190) : t.text;
    if (max_roll) {
        tier = kWinColor;
    } else if (min_roll) {
        tier = kLoss;
    }

    if (!d.rolling && d.value && pop < 1) {
        float e = ease_out_cubic(pop);
        float rr = base_r * scale;
        // Every roll: a shock ring and a burst of sparks flying outward.
        f.drawCircle(cx, cy, (int)(rr + 4 + e * 26), mix(t.bg_bottom, tier, (int)((1 - pop) * 200)));
        for (int i = 0; i < 12; i++) {
            float a = i * (2 * kPi / 12) + d.value * 0.37f;
            float dist = rr + 6 + e * 34;
            int px = cx + (int)(cosf(a) * dist);
            int py = cy + (int)(sinf(a) * dist * 0.8f);
            int size = pop < 0.5f ? 3 : pop < 0.8f ? 2 : 1;
            f.fillCircle(px, py, size, mix(t.bg_bottom, tier, (int)((1 - pop) * 256)));
        }
        if (max_roll || min_roll) {
            // The top or bottom face is a celebration: glow rings, then confetti
            // thrown across the screen that falls as it fades (embers for a 1).
            for (int i = 0; i < 3; i++) {
                int fade = (int)((1 - pop) * (240 - i * 60));
                f.drawCircle(cx, cy, (int)rr + 4 + i * 4 + (int)(e * 10), mix(t.bg_bottom, tier, fade > 0 ? fade : 0));
            }
            const uint32_t confetti[] = {kWinColor, t.accent, t.p1, t.p2, t.kind[1], 0xFFFFFF};
            const uint32_t embers[] = {kLoss, 0xF08A4B, 0x8A1C1C, t.muted};
            for (int i = 0; i < 28; i++) {
                // Fixed pseudo-random spread so each piece has its own path.
                uint32_t h = (uint32_t)(i * 2654435761u) ^ (uint32_t)(d.value * 40503u);
                float a = (h % 628) / 100.0f;
                float speed = 50 + (h >> 10) % 80;
                float dist = rr * 0.6f + e * speed;
                int px = cx + (int)(cosf(a) * dist * 1.4f);
                int py = cy + (int)(sinf(a) * dist * 0.75f + pop * pop * 40); // gravity
                uint32_t c = max_roll ? confetti[i % 6] : embers[i % 4];
                uint32_t faded = mix(t.bg_bottom, c, (int)((1 - pop * pop) * 256));
                if (max_roll) {
                    f.fillRect(px, py, (h & 1) ? 4 : 2, (h & 1) ? 2 : 4, faded);
                } else {
                    f.fillCircle(px, py, 1 + (h & 1), faded);
                }
            }
        }
    }

    // The top or bottom face gets called out in the header: NAT 20!, NAT 1.
    if (!d.rolling && (max_roll || min_roll)) {
        snprintf(buf, sizeof(buf), max_roll ? "NAT %d!" : "NAT %d", d.value);
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextSize(1);
        f.setTextDatum(textdatum_t::middle_center);
        draw_text_glow(f, t, buf, kWidth / 2, kHeaderH / 2, tier);
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
                        : max_roll || min_roll ? tier
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
            uint32_t c = d.rolling ? t.muted : tier;
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

// ---- Close mark ---------------------------------------------------------------

void draw_close_mark(LGFX_Sprite &f, const Theme &t)
{
    for (int i = -1; i <= 1; i++) {
        f.drawLine(kWidth - 52 + i, 7, kWidth - 42 + i, 17, t.muted);
        f.drawLine(kWidth - 42 + i, 7, kWidth - 52 + i, 17, t.muted);
    }
}

// ---- Game picker -----------------------------------------------------------

Rect picker_tile(int i)
{
    return {6 + (i % 3) * 104, 30 + (i / 3) * 70, 100, 64};
}

Rect format_tile(int i, int n)
{
    int w = (kWidth - 12 - (n - 1) * 8) / n;
    return {6 + i * (w + 8), 32, w, 132};
}

// One line describing a profile, e.g. "First to 8", "Life 20", "8000 LP".
void profile_summary(const bb_profile_t *p, char *buf, size_t n)
{
    if (p->style == BB_STYLE_GAUGE) {
        snprintf(buf, n, "Memory gauge");
    } else if (p->style == BB_STYLE_AMOUNT) {
        snprintf(buf, n, "%d LP", p->start);
    } else if (p->win_at != BB_NONE) {
        snprintf(buf, n, p->teams ? "Teams, first to %d" : "First to %d", p->win_at);
    } else {
        snprintf(buf, n, "Life %d", p->start);
    }
}

Rect lerp_rect(Rect a, Rect b, float t)
{
    return {a.x + (int)((b.x - a.x) * t), a.y + (int)((b.y - a.y) * t),
            a.w + (int)((b.w - a.w) * t), a.h + (int)((b.h - a.h) * t)};
}

// A tile drawn in the game's own colors.
void draw_game_tile(LGFX_Sprite &f, const bb_profile_t *p, Rect r, const char *title,
                    const char *sub, bool current, bool vivid)
{
    const Theme t = styled(theme_for(p), vivid);
    f.fillRoundRect(r.x, r.y, r.w, r.h, 8, t.bg_top);
    for (int y = 3; y < r.h - 3; y++) {
        f.drawFastHLine(r.x + 3, r.y + y, r.w - 6, mix(t.bg_top, t.bg_bottom, y * 256 / r.h));
    }
    f.drawRoundRect(r.x, r.y, r.w, r.h, 8, current ? t.accent : mix(t.bg_bottom, t.accent, 120));
    if (current) {
        f.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 7, t.accent);
    }

    if (r.w < 30 || r.h < 24) {
        return; // too small mid-animation for text
    }
    int title_h = sub ? r.h - 30 : r.h - 12;
    if (title_h > 44) {
        title_h = 44;
    }
    int title_y = sub ? r.cy() - 8 : r.cy();
    draw_title(f, t, title, r.cx(), title_y, r.w - 12, title_h, t.accent, true);
    if (sub) {
        f.setFont(&fonts::Font2);
        f.setTextSize(1);
        f.setTextColor(t.text);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(sub, r.cx(), r.y + r.h - 13);
    }
}

void draw_picker(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    const PickerState &pk = *s.picker;
    draw_close_mark(f, t);
    size_t current_family = bb_family_of(s.game->profile);
    char sub[24];

    if (pk.family < 0) {
        for (int i = 0; i < (int)bb_family_count() && i < 6; i++) {
            size_t idx[4];
            size_t n = bb_family_profiles((size_t)i, idx, 4);
            const bb_profile_t *p = bb_profile_at(idx[0]);
            if (n > 1) {
                snprintf(sub, sizeof(sub), "%d formats", (int)n);
            } else {
                profile_summary(p, sub, sizeof(sub));
            }
            draw_game_tile(f, p, picker_tile(i), bb_family_name((size_t)i), sub,
                           (size_t)i == current_family, s.vivid);
        }
        return;
    }

    // Formats burst out of the game's tile.
    size_t idx[4];
    int n = (int)bb_family_profiles((size_t)pk.family, idx, 4);
    float e = ease_out_cubic(pk.anim / 1000.0f);
    Rect origin = picker_tile(pk.origin);
    for (int i = 0; i < n; i++) {
        const bb_profile_t *p = bb_profile_at(idx[i]);
        profile_summary(p, sub, sizeof(sub));
        Rect r = lerp_rect(origin, format_tile(i, n), e);
        draw_game_tile(f, p, r, p->format ? p->format : p->name, sub, p == s.game->profile, s.vivid);
    }
}

UiAction picker_hit(const UiState &s, int x, int y)
{
    const PickerState &pk = *s.picker;
    UiAction a;
    if (y < kHeaderH) {
        a.type = (pk.family >= 0 && x < kWidth - 70) ? UiAction::PickerBack : UiAction::PickerClose;
        return a;
    }
    if (pk.family < 0) {
        for (int i = 0; i < (int)bb_family_count() && i < 6; i++) {
            if (picker_tile(i).contains(x, y)) {
                a.type = UiAction::PickFamily;
                a.kind = (uint8_t)i;
                return a;
            }
        }
        return a;
    }
    size_t idx[4];
    int n = (int)bb_family_profiles((size_t)pk.family, idx, 4);
    for (int i = 0; i < n; i++) {
        if (format_tile(i, n).contains(x, y)) {
            a.type = UiAction::PickProfile;
            a.kind = (uint8_t)idx[i];
            return a;
        }
    }
    return a;
}

// ---- Shared gauge (Digimon memory) ------------------------------------------

constexpr int kGaugeY = 64;
constexpr int kGaugeH = 28;
constexpr int kCellW = 14;
constexpr int kCellStep = 15;

int gauge_cell_x(int i)
{
    return 3 + i * kCellStep; // i = 0..20 for values -10..10
}

Rect gauge_button(int i)
{
    return {4 + i * 106, 118, 100, 32};
}

void draw_gauge(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    const bb_game_t &g = *s.game;
    int v = g.value[0];
    uint8_t owner = bb_gauge_owner(&g);
    uint32_t oc = player_color(t, owner);
    char buf[24];

    // Whose turn, and how much memory they have.
    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    f.setTextDatum(textdatum_t::middle_left);
    f.setTextColor(t.p1);
    f.drawString("YOU", 8, 40);
    f.setTextDatum(textdatum_t::middle_right);
    f.setTextColor(t.p2);
    f.drawString("OPP", kWidth - 8, 40);

    snprintf(buf, sizeof(buf), "%s  %d", owner == 0 ? "YOUR TURN" : "OPP'S TURN", v < 0 ? -v : v);
    f.setFont(&fonts::FreeSansBold12pt7b);
    f.setTextColor(oc);
    f.setTextDatum(textdatum_t::middle_center);
    f.drawString(buf, kWidth / 2, 40);

    // The track: YOU's 10..1 on the left, 0, OPP's 1..10 on the right.
    f.setFont(&fonts::Font0);
    for (int i = 0; i <= 20; i++) {
        int val = i - 10;
        int x = gauge_cell_x(i);
        uint32_t side = val < 0 ? t.p1 : val > 0 ? t.p2 : t.accent;
        if (val == v) {
            f.fillRoundRect(x - 1, kGaugeY - 3, kCellW + 2, kGaugeH + 6, 3, side);
            f.fillTriangle(x + kCellW / 2 - 6, kGaugeY - 12, x + kCellW / 2 + 6, kGaugeY - 12,
                           x + kCellW / 2, kGaugeY - 5, side);
        } else {
            int strength = val == 0 ? 90 : 40 + (10 - (val < 0 ? -val : val)) * 4;
            f.fillRoundRect(x, kGaugeY, kCellW, kGaugeH, 3, mix(t.surface, side, strength));
        }
        snprintf(buf, sizeof(buf), "%d", val < 0 ? -val : val);
        f.setTextColor(val == v ? t.text : t.muted);
        f.drawString(buf, x + kCellW / 2, kGaugeY + kGaugeH + 9);
    }

    // Buttons: move toward YOU, pass the turn, move toward OPP.
    for (int i = 0; i < 3; i++) {
        Rect r = gauge_button(i);
        uint32_t c = i == 0 ? t.p1 : i == 2 ? t.p2 : t.accent;
        f.fillRoundRect(r.x, r.y, r.w, r.h, 8, t.surface);
        f.drawRoundRect(r.x, r.y, r.w, r.h, 8, mix(t.surface, c, 200));
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextColor(c);
        f.setTextDatum(textdatum_t::middle_center);
        if (i == 1) {
            f.drawString("PASS", r.cx(), r.cy() + 1);
        } else {
            int ax = i == 0 ? r.x + 18 : r.x + r.w - 18;
            int dir = i == 0 ? -1 : 1;
            f.fillTriangle(ax + dir * 7, r.cy(), ax - dir * 5, r.cy() - 8, ax - dir * 5, r.cy() + 8, c);
            f.drawString(i == 0 ? "YOU" : "OPP", r.cx() + (i == 0 ? 8 : -8), r.cy() + 1);
        }
    }
}

UiAction gauge_hit(const UiState &s, int x, int y)
{
    UiAction a;
    if (y >= kGaugeY - 14 && y < kGaugeY + kGaugeH + 14) {
        int i = (x - 3) / kCellStep;
        a.type = UiAction::GaugeSet;
        a.delta = (int16_t)((i < 0 ? 0 : i > 20 ? 20 : i) - 10);
        return a;
    }
    for (int i = 0; i < 3; i++) {
        Rect r = gauge_button(i);
        Rect padded{r.x - 2, r.y - 4, r.w + 4, r.h + 8};
        if (padded.contains(x, y)) {
            if (i == 1) {
                a.type = UiAction::GaugePass;
            } else {
                a.type = UiAction::Adjust;
                a.delta = i == 0 ? -1 : 1;
                a.repeat = true;
            }
            return a;
        }
    }
    (void)s;
    return a;
}

// ---- Number pad (Yu-Gi-Oh! LP) ---------------------------------------------

const char *const kKeyLabels[4][4] = {
    {"7", "8", "9", "-"},
    {"4", "5", "6", "+"},
    {"1", "2", "3", "1/2"},
    {"0", "00", "DEL", "X"},
};

Rect key_rect(int row, int col)
{
    return {120 + col * 49, 28 + row * 35, 46, 32};
}

void draw_entry(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    const bb_game_t &g = *s.game;
    const EntryState &e = *s.entry;
    int lp = g.value[e.player];
    char buf[24];

    draw_close_mark(f, t);

    // Who, their LP now, the amount typed, and what - / + would make it.
    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    f.setTextDatum(textdatum_t::middle_left);
    f.setTextColor(player_color(t, e.player));
    f.drawString(g.players > 1 ? ui_player_label(g, e.player) : "LP", 8, 38);
    snprintf(buf, sizeof(buf), "%d", lp);
    f.setTextColor(t.muted);
    f.setTextDatum(textdatum_t::middle_right);
    f.drawString(buf, 112, 38);

    snprintf(buf, sizeof(buf), "%ld", (long)e.amount);
    draw_number(f, buf, 60, 84, 104, 46, e.amount ? t.text : t.muted, number_shadow(t));

    int32_t minus = lp - e.amount < g.profile->min ? g.profile->min : lp - e.amount;
    int32_t plus = lp + e.amount > g.profile->max ? g.profile->max : lp + e.amount;
    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextDatum(textdatum_t::middle_left);
    snprintf(buf, sizeof(buf), "- %ld", (long)minus);
    f.setTextColor(kLoss);
    f.drawString(buf, 8, 128);
    snprintf(buf, sizeof(buf), "+ %ld", (long)plus);
    f.setTextColor(kGain);
    f.drawString(buf, 8, 152);

    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            Rect r = key_rect(row, col);
            const char *label = kKeyLabels[row][col];
            uint32_t fill = t.surface;
            uint32_t text = t.text;
            if (col == 3 && row == 0) {
                fill = mix(t.surface, kLoss, 90);
                text = kLoss;
            } else if (col == 3 && row == 1) {
                fill = mix(t.surface, kGain, 90);
                text = kGain;
            } else if (col == 3) {
                text = t.accent;
            } else if (row == 3 && col == 2) {
                text = t.muted;
            }
            f.fillRoundRect(r.x, r.y, r.w, r.h, 6, fill);
            f.drawRoundRect(r.x, r.y, r.w, r.h, 6, mix(t.surface, t.accent, 90));
            if (strlen(label) > 2) {
                f.setFont(&fonts::FreeSansBold9pt7b);
            } else {
                f.setFont(&fonts::FreeSansBold12pt7b);
            }
            if (col == 3 && row < 2) {
                f.fillRect(r.cx() - 9, r.cy() - 2, 18, 4, text); // bold minus / plus
                if (row == 1) {
                    f.fillRect(r.cx() - 2, r.cy() - 9, 4, 18, text);
                }
                continue;
            }
            f.setTextColor(text);
            f.setTextDatum(textdatum_t::middle_center);
            f.drawString(label, r.cx(), r.cy() + 1);
        }
    }
}

UiAction entry_hit(const UiState &s, int x, int y)
{
    UiAction a;
    if (y < kHeaderH) {
        a.type = UiAction::EntryClose;
        return a;
    }
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            Rect r = key_rect(row, col);
            Rect padded{r.x - 1, r.y - 1, r.w + 3, r.h + 3};
            if (!padded.contains(x, y)) {
                continue;
            }
            if (col < 3 && row < 3) {
                a.type = UiAction::EntryDigit;
                a.kind = (uint8_t)(7 - row * 3 + col);
            } else if (row == 3 && col < 2) {
                a.type = UiAction::EntryDigit;
                a.kind = col == 0 ? 0 : 10;
            } else if (row == 3 && col == 2) {
                a.type = UiAction::EntryDelete;
            } else if (row == 3) {
                a.type = UiAction::EntryClose;
            } else if (row == 2) {
                a.type = UiAction::EntryHalve;
            } else {
                a.type = UiAction::EntryApply;
                a.delta = row == 0 ? -1 : 1;
            }
            a.player = s.entry->player;
            return a;
        }
    }
    return a;
}

// Kind buttons only respond to taps on the button itself (plus a small margin),
// so a tap next to a button does nothing rather than hitting the wrong one.
constexpr int kTapMargin = 2;

UiAction kind_hit(const UiState &s, uint8_t p, int x, int y)
{
    UiAction a;
    for (uint8_t k = 0; k < 2; k++) {
        Rect r = kind_button(s, p, k);
        Rect padded{r.x - kTapMargin, r.y - kTapMargin, r.w + 2 * kTapMargin, r.h + 2 * kTapMargin};
        if (padded.contains(x, y)) {
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
    const Theme t = styled(theme_for(state.game->profile), state.vivid);

    if (state.picker) {
        draw_background(frame, t, false);
        char title[32] = "GAME";
        if (state.picker->family >= 0) {
            char name[24];
            upper(name, sizeof(name), bb_family_name((size_t)state.picker->family), 20);
            snprintf(title, sizeof(title), "< %s", name);
        }
        draw_header(frame, t, state, title);
        draw_picker(frame, t, state);
        return;
    }

    if (state.entry && !state.menu) {
        draw_background(frame, t, false);
        draw_header(frame, t, state, "LIFE POINTS");
        draw_entry(frame, t, state);
        return;
    }

    if (state.dice && !state.menu) {
        draw_background(frame, t, false);
        draw_header(frame, t, state, "DICE");
        draw_dice(frame, t, state);
        return;
    }

    if (!state.menu && !gauge_style(*state.game) && (facing(state) || quads(state))) {
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

    draw_background(frame, t, !state.menu && !gauge_style(*state.game));
    draw_header(frame, t, state, state.menu ? "MENU" : state.game->profile->name, !state.menu);

    if (state.menu) {
        draw_menu(frame, t, state);
        return;
    }

    if (gauge_style(*state.game)) {
        draw_gauge(frame, t, state);
    } else if (state.game->players == 1) {
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
            UiAction::MenuGame,  UiAction::MenuPlayers, UiAction::MenuLayout,
            UiAction::MenuMatch, UiAction::MenuTimer,   UiAction::MenuLook,
            UiAction::MenuFlip,  UiAction::MenuRestart, UiAction::MenuDice};
        for (int i = 0; i < kMenuTiles; i++) {
            Rect r = menu_tile(i);
            // Tiles are hit-tested with the gaps included so near-misses still count.
            Rect padded{r.x - 2, r.y - 1, r.w + 4, r.h + 3};
            if (padded.contains(x, y)) {
                if ((kTileActions[i] == UiAction::MenuLayout && (g.players < 2 || gauge_style(g))) ||
                    (kTileActions[i] == UiAction::MenuPlayers && gauge_style(g)) ||
                    (kTileActions[i] == UiAction::MenuMatch && g.players > 2)) {
                    return a;
                }
                a.type = kTileActions[i];
                return a;
            }
        }
        return a;
    }

    if (state.picker) {
        return picker_hit(state, x, y);
    }
    if (state.entry) {
        return entry_hit(state, x, y);
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

    if (gauge_style(g)) {
        return gauge_hit(state, x, y);
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
        if (amount_style(g)) {
            a.type = UiAction::OpenEntry;
            return a;
        }
        if (has_kinds(g)) {
            return kind_hit(state, p, qx, ly); // quadrant coordinates
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
        if (amount_style(g)) {
            a.type = UiAction::OpenEntry;
            return a;
        }
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
    if (amount_style(g)) {
        a.type = UiAction::OpenEntry;
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
