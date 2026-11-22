#include "ui.h"

#include <ctype.h>
#include <strings.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

namespace {

// How a game's life / score is framed with the Themed counter style.
enum Skin : uint8_t {
    SkinPlain,  // just the number
    SkinMana,   // Magic: a ring of five colored orbs
    SkinLore,   // Lorcana: a ring of diamonds filling toward 20
    SkinShield, // Flesh and Blood: a bronze shield and a blood bar
};

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
    const lgfx::IFont *title_font;  // game name on picker tiles
    const lgfx::IFont *header_font; // game name in the header bar
    Skin skin;
    bool vivid;                    // set by styled(): richer colors and glows
};

// Per-game palettes. Colors only; no game logos or artwork.
const Theme kThemes[] = {
    // Navy and teal with gold trim; gold = Conquer, violet = Hold.
    {"riftbound", 0x0A1428, 0x0B3A44, 0x10283A, 0xC8AA6E, 0x0AC8B9, 0xE8604C, 0xF0E6D2, 0x7E8A96,
     {0xE0B85A, 0xA78BFA}, &fonts::FreeSerifBold12pt7b, &fonts::FreeSerifBold9pt7b, SkinPlain, false},
    // Charcoal and warm brown with amber.
    {"mtg", 0x15110E, 0x33221A, 0x2A1E17, 0xE0A040, 0x6FB5E0, 0xE8604C, 0xF2E8DC, 0x8C8072,
     {0xE0A040, 0x6FB5E0}, &fonts::FreeSerifBold12pt7b, &fonts::FreeSerifBold9pt7b, SkinMana, false},
    // Charcoal and plum with lilac.
    {"mtg-commander", 0x120F1A, 0x2E1F40, 0x251A36, 0xC9A0FF, 0x6FB5E0, 0xE8604C, 0xF0EAFA, 0x8A7FA0,
     {0xC9A0FF, 0x6FB5E0}, &fonts::FreeSerifBold12pt7b, &fonts::FreeSerifBold9pt7b, SkinMana, false},
    // Ink blue and violet with gold.
    {"lorcana", 0x0B0F2E, 0x2D1B4E, 0x1C1840, 0xE8C25A, 0x7FB2FF, 0xF07FB0, 0xF4F0FF, 0x8A88AA,
     {0xE8C25A, 0x7FB2FF}, &fonts::Satisfy_24, &fonts::Satisfy_24, SkinLore, false},
    // Deep navy with digital orange and cyan.
    {"digimon", 0x07131F, 0x0E2A3A, 0x0F2333, 0xF28C28, 0x2EC4F1, 0xF2545B, 0xEAF6FF, 0x7F95A8,
     {0xF28C28, 0x2EC4F1}, &fonts::FreeSansBold12pt7b, &fonts::FreeSansBold9pt7b, SkinPlain, false},
    // Dark umber with gold.
    {"yugioh", 0x140F05, 0x3A2A0A, 0x2E2410, 0xE6B422, 0x7FB2FF, 0xE8604C, 0xFFF4DA, 0x9C8C6A,
     {0xE6B422, 0x7FB2FF}, &fonts::FreeSerifBoldItalic12pt7b, &fonts::FreeSerifBoldItalic9pt7b, SkinPlain, false},
    // Black and blood red with bronze.
    {"fab", 0x140808, 0x3C1210, 0x2E1210, 0xD4A24C, 0x6FB5E0, 0xE04B3A, 0xF4E6DC, 0x907670,
     {0xD4A24C, 0x6FB5E0}, &fonts::FreeSerifBold12pt7b, &fonts::FreeSerifBold9pt7b, SkinShield, false},
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

constexpr float kPi = 3.14159265f;
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
        Theme ht = t;
        ht.title_font = t.header_font; // a font made for this size, not a scaled-down one
        draw_title(f, ht, title, 8, kHeaderH / 2 + 1, 150, 18, t.accent, false, true);
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

    if (s.menu || s.dice || s.picker || s.entry || s.end_prompt) {
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

bool counter_style_plain(const bb_game_t &g);
void draw_close_mark(LGFX_Sprite &f, const Theme &t);
float ease_out_cubic(float t);
void draw_die_body(LGFX_Sprite &f, uint8_t sides, int cx, int cy, float r, float angle,
                   uint32_t fill, uint32_t edge, uint32_t facet, float sx, float sy);
float ease_out_back(float t);

void draw_text_glow(LGFX_Sprite &f, const Theme &t, const char *text, int x, int y, uint32_t color);

// A number that rolls like an odometer from prev to value: the old number
// slides out and the new one slides in (up for a gain, down for a loss).
void draw_rolling_number(LGFX_Sprite &f, int value, int prev, float prog, int dir, int cx, int cy, int w,
                         int h, uint32_t color, uint32_t shadow)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", value);
    if (prog >= 1 || value == prev) {
        draw_number(f, buf, cx, cy, w, h, color, shadow);
        return;
    }
    float e = ease_out_cubic(prog);
    int travel = (int)(h * 0.95f);
    f.setClipRect(cx - w / 2 - 4, cy - h / 2, w + 8, h);
    char old[12];
    snprintf(old, sizeof(old), "%d", prev);
    draw_number(f, old, cx, cy - dir * (int)(e * travel), w, h, color, shadow);
    draw_number(f, buf, cx, cy + dir * (int)((1 - e) * travel), w, h, color, shadow);
    f.clearClipRect();
}

// Magic: a quiet row of five colored dots under the life total. On a change
// they ripple in a short wave (left to right for a gain, right to left for
// damage).
void draw_mana_dots(LGFX_Sprite &f, const Theme &t, int cx, int y, int r, float prog, int dir)
{
    static const uint32_t kDots[5] = {0xF4EED2, 0x3D8FDA, 0x6C5F78, 0xDB4A3C, 0x41A552};
    int gap = r * 4;
    for (int k = 0; k < 5; k++) {
        int lift = 0;
        if (prog < 1) {
            // Each dot bobs once, one after another along the row.
            int order = dir > 0 ? k : 4 - k;
            float local = prog * 1.8f - order * 0.2f;
            if (local > 0 && local < 1) {
                lift = (int)(sinf(local * kPi) * r * 2.2f);
            }
        }
        f.fillCircle(cx + (k - 2) * gap, y - lift, r, kDots[k]);
    }
}

void fill_diamond(LGFX_Sprite &f, int x, int y, int sz, uint32_t c)
{
    f.fillTriangle(x, y - sz, x - sz, y, x + sz, y, c);
    f.fillTriangle(x, y + sz, x - sz, y, x + sz, y, c);
}

// Lorcana: a diamond for each lore toward the win, lighting up as lore is
// gained; the newest one sparkles.
void draw_lore_ring(LGFX_Sprite &f, const Theme &t, const bb_game_t &g, int value, int cx, int cy, int rx,
                    int ry, float prog, int dir)
{
    int n = g.profile->win_at != BB_NONE ? g.profile->win_at : 20;
    if (n > 24) {
        n = 24;
    }
    int sz = ry / 10 < 2 ? 2 : ry / 10;
    for (int i = 0; i < n; i++) {
        float a = -kPi / 2 + i * 2 * kPi / n;
        int x = cx + (int)(cosf(a) * rx);
        int y = cy + (int)(sinf(a) * ry);
        bool lit = i < value;
        fill_diamond(f, x, y, sz, lit ? t.accent : mix(t.bg_bottom, t.accent, 55));
        if (lit && i == value - 1 && prog < 1 && dir > 0) {
            int len = (int)(sz * 4 * (1 - ease_out_cubic(prog))) + sz;
            uint32_t c = mix(t.bg_bottom, 0xFFFFFF, (int)((1 - prog) * 255));
            f.drawLine(x - len, y, x + len, y, c);
            f.drawLine(x, y - len, x, y + len, c);
            f.drawLine(x - len / 2, y - len / 2, x + len / 2, y + len / 2, c);
            f.drawLine(x - len / 2, y + len / 2, x + len / 2, y - len / 2, c);
        }
    }
}

// Flesh and Blood: a bronze shield behind the life total that shakes and
// flushes red when hit, and a blood bar showing life left of the starting total.
void draw_shield(LGFX_Sprite &f, const Theme &t, const bb_game_t &g, int value, int cx, int cy, int sw, int sh,
                 float prog, int dir)
{
    static const float kX[7] = {-1.f, 1.f, 1.f, 0.72f, 0.f, -0.72f, -1.f};
    static const float kY[7] = {-1.f, -1.f, 0.1f, 0.62f, 1.f, 0.62f, 0.1f};
    int shake = prog < 1 && dir < 0 ? (int)(sinf(prog * 7 * kPi) * 5 * (1 - prog)) : 0;
    cx += shake;
    uint32_t fill = t.surface;
    if (prog < 1) {
        fill = mix(t.surface, dir < 0 ? kLoss : kGain, (int)((1 - prog) * 110));
    }
    int x[7], y[7];
    for (int i = 0; i < 7; i++) {
        x[i] = cx + (int)(kX[i] * sw);
        y[i] = cy + (int)(kY[i] * sh);
    }
    for (int i = 0; i < 7; i++) {
        f.fillTriangle(cx, cy, x[i], y[i], x[(i + 1) % 7], y[(i + 1) % 7], fill);
    }
    for (int i = 0; i < 7; i++) {
        int j = (i + 1) % 7;
        f.drawLine(x[i], y[i], x[j], y[j], t.accent);
        f.drawLine(x[i] + (x[i] < cx ? 1 : -1), y[i] + 1, x[j] + (x[j] < cx ? 1 : -1), y[j] + (y[j] > cy ? -1 : 1),
                   mix(t.surface, t.accent, 120));
    }
    // Blood bar in the shield's tail.
    const uint32_t kBlood = 0xC0162E, kBloodLow = 0xFF3B30;
    int start = g.profile->start > 0 ? g.profile->start : 40;
    int bw = (int)(sw * 1.1f), bh = sh / 11 < 3 ? 3 : sh / 11;
    int bx = cx - bw / 2, by = cy + (int)(sh * 0.40f);
    float frac = value <= 0 ? 0 : value >= start ? 1 : value / (float)start;
    f.fillRoundRect(bx - 1, by - 1, bw + 2, bh + 2, 2, mix(t.bg_bottom, 0x000000, 140));
    if (frac > 0) {
        f.fillRoundRect(bx, by, (int)(bw * frac) > 2 ? (int)(bw * frac) : 2, bh, 2, frac < 0.25f ? kBloodLow : kBlood);
    }
}

// A counter value, drawn according to the Counter setting: the game's own
// look (Themed), on a spinning d20, or plain digits. Typed amounts (Yu-Gi-Oh!
// LP) count toward the new value instead.
void draw_value(LGFX_Sprite &f, const Theme &t, const UiState &s, uint8_t p, const char *text, int cx,
                int cy, int max_w, int max_h, uint32_t color, uint32_t shadow)
{
    const bb_game_t &g = *s.game;
    float prog = s.spin ? s.spin[p] / 1000.0f : 1.0f;
    int dir = s.spin_dir ? s.spin_dir[p] : 1;
    int value = g.value[p];
    int prev = s.prev ? s.prev[p] : value;

    if (amount_style(g)) {
        int shown = prog < 1 ? prev + (int)((value - prev) * ease_out_cubic(prog)) : value;
        char buf[12];
        snprintf(buf, sizeof(buf), "%d", shown);
        draw_number(f, buf, cx, cy, max_w, max_h, color, shadow);
        return;
    }
    if (!counter_style_plain(g)) {
        draw_number(f, text, cx, cy, max_w, max_h, color, shadow);
        return;
    }

    if (s.counter_style == 1) {
        float r = max_h / 2.0f;
        if (r > max_w / 1.8f) {
            r = max_w / 1.8f;
        }
        float angle = prog < 1 ? (1 - ease_out_back(prog)) * (kPi / 3) * -dir : 0;
        draw_die_body(f, 20, cx, cy, r, angle, t.surface, t.accent, mix(t.surface, t.accent, 45), 1, 1);
        draw_number(f, text, cx, cy + 2, (int)(r * 1.25f), (int)(r * 0.95f), color, shadow);
        return;
    }

    Skin skin = s.counter_style == 2 ? SkinPlain : t.skin;
    int ry = max_h / 2;
    int rx = (int)(max_w * 0.46f) < (int)(max_h * 0.9f) ? (int)(max_w * 0.46f) : (int)(max_h * 0.9f);
    switch (skin) {
    case SkinMana: {
        // The number briefly takes the color of the change, then settles.
        uint32_t c = color;
        if (prog < 1) {
            c = mix(color, dir < 0 ? kLoss : kGain, (int)((1 - prog) * 200));
        }
        int dot_r = max_h / 26 < 2 ? 2 : max_h / 26;
        draw_rolling_number(f, value, prev, prog, dir, cx, cy - dot_r * 2, max_w, max_h - dot_r * 6, c, shadow);
        draw_mana_dots(f, t, cx, cy + max_h / 2 - dot_r, dot_r, prog, dir);
        break;
    }
    case SkinLore:
        draw_lore_ring(f, t, g, value, cx, cy, rx, ry, prog, dir);
        draw_rolling_number(f, value, prev, prog, dir, cx, cy + 1, (int)(rx * 1.4f), (int)(ry * 1.2f), color,
                            shadow);
        break;
    case SkinShield: {
        int sh = (int)(ry * 1.06f);
        int sw = (int)(sh * 0.85f);
        draw_shield(f, t, g, value, cx, cy, sw, sh, prog, dir);
        draw_rolling_number(f, value, prev, prog, dir, cx, cy - sh / 5, (int)(sw * 1.6f), (int)(sh * 0.95f), color,
                            shadow);
        break;
    }
    default:
        draw_rolling_number(f, value, prev, prog, dir, cx, cy, max_w, max_h, color, shadow);
        break;
    }
}

// Whether counter values are drawn with something around or under them
// (themed frame or d20) rather than as plain digits.
bool framed_value(const Theme &t, const UiState &s)
{
    if (!counter_style_plain(*s.game) || s.counter_style == 2) {
        return false;
    }
    return s.counter_style == 1 || t.skin != SkinPlain;
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
    } else if (facing(s)) {
        // Name on the left, count on the right.
        upper(name, sizeof(name), g.profile->kinds[kind], 10);
        f.setFont(&fonts::Font2);
        f.setTextSize(1);
        f.setTextColor(kc);
        f.setTextDatum(textdatum_t::middle_left);
        f.drawString(name, r.x + 10, r.cy());
        snprintf(buf, sizeof(buf), "%d", g.tally[p][kind]);
        f.setFont(&fonts::FreeSansBold12pt7b);
        f.setTextColor(t.text);
        f.setTextDatum(textdatum_t::middle_right);
        f.drawString(buf, r.x + r.w - 10, r.cy() + 1);
    } else {
        // Small name over the count.
        upper(name, sizeof(name), g.profile->kinds[kind], 7);
        f.setFont(&fonts::Font2);
        f.setTextSize(1);
        f.setTextColor(kc);
        f.drawString(name, r.cx(), r.y + 10);
        snprintf(buf, sizeof(buf), "%d", g.tally[p][kind]);
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextColor(t.text);
        f.drawString(buf, r.cx(), r.y + r.h - 11);
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

void draw_toast(LGFX_Sprite &f, const Theme &t, const char *msg, int cy, int cx = kWidth / 2)
{
    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    int w = f.textWidth(msg) + 24;
    Rect r{cx - w / 2, cy - 10, w, 20};
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
        draw_value(f, t, s, 0, buf, kWidth / 2, 82, 200, 96, vc, shadow);
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
        if (has_kinds(g)) {
            f.drawString(buf, 86, 32);
        } else if (framed_value(t, s)) {
            // The frame (dots, diamonds, shield) sits around and below the
            // number, so put the change in the free top-right corner instead.
            f.setTextDatum(textdatum_t::middle_right);
            f.drawString(buf, kWidth - 14, 46);
        } else {
            f.drawString(buf, kWidth / 2, 136);
        }
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
            draw_value(f, t, s, p, buf, panel.cx(), 92, 140, 80, vc, shadow);
        } else {
            uint32_t hint = mix(t.bg_bottom, t.accent, 110);
            draw_plus_minus(f, panel.x + panel.w - 16, 58, true, hint, 6);
            draw_plus_minus(f, panel.x + panel.w - 16, 128, false, hint, 6);
            draw_value(f, t, s, p, buf, panel.cx() - 6, 92, 124, 82, vc, shadow);
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
        draw_value(f, t, s, p, buf, kWidth / 2, 46, 180, 72, vc, shadow);
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
        draw_value(f, t, s, p, buf, x0 + kQuadW / 2, 48, amount_style(g) ? 136 : 96, 60, vc, shadow);
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

// Menu tiles for each page. Blank slots have a null label.
struct MenuTile {
    const char *label;
    const char *value;
    UiAction::Type action;
    bool enabled;
};

bool counter_style_plain(const bb_game_t &g)
{
    return g.profile->style == BB_STYLE_COUNTER && g.profile->kind_count == 0;
}

void menu_tiles(const UiState &s, MenuTile *out, char *match, char *timer)
{
    static const char *const kCounterNames[] = {"Themed", "d20", "Digits"};
    const bb_game_t &g = *s.game;
    static const char *const kPlayerNames[] = {"Just you", "Two", "Three", "Four"};
    static const char *const kTeamNames[] = {"One team", "Two teams"};
    const char *players = g.profile->teams ? kTeamNames[(g.players - 1) & 1] : kPlayerNames[g.players - 1];
    const char *name = g.profile->short_name ? g.profile->short_name : g.profile->name;
    sprintf(match, s.best_of ? "Bo%d" : "Off", s.best_of);
    sprintf(timer, s.timer_minutes ? "%d min" : "Off", s.timer_minutes);
    bool gauge = gauge_style(g);

    const MenuTile blank = {nullptr, nullptr, UiAction::None, false};
    const MenuTile back = {"Back", "<", UiAction::MenuBack, true};

    if (s.menu_page == 0) {
        bool ending = g.players <= 2; // results can be recorded with or without a match
        bool riftbound = has_kinds(g);
        static char legends[24];
        if (s.match_legend[0] == kNoLegend && s.match_legend[1] == kNoLegend) {
            snprintf(legends, sizeof(legends), "Pick");
        } else {
            char a[24], b[24];
            snprintf(legends, sizeof(legends), "%s v %s", ui_legend_label(s, s.match_legend[0], a, sizeof(a)),
                     ui_legend_label(s, s.match_legend[1], b, sizeof(b)));
        }
        MenuTile tiles[kMenuTiles] = {
            {"Game", name, UiAction::MenuGame, true},
            {"Players", players, UiAction::MenuPlayers, !gauge},
            {"Layout", g.players > 1 && !gauge ? ui_layout_name(g, s.layout) : "-", UiAction::MenuLayout,
             g.players > 1 && !gauge},
            {"Match", g.players <= 2 ? match : "-", UiAction::MenuMatch, g.players <= 2},
            {"Timer", timer, UiAction::MenuTimer, true},
            {ending ? "End game" : "Restart", ending ? "Who won?" : "New",
             ending ? UiAction::MenuEnd : UiAction::MenuRestart, true},
            {"Dice", "Roll", UiAction::MenuDice, true},
            riftbound ? MenuTile{"Legends", legends, UiAction::MenuLegends, true}
                      : MenuTile{"Undo", "Last", UiAction::MenuUndo, true},
            {"More", ">", UiAction::MenuMore, true},
        };
        memcpy(out, tiles, sizeof(tiles));
    } else if (s.menu_page == 2) {
        auto v = [](const char *t) { return t ? t : "-"; };
        MenuTile tiles[kMenuTiles] = {
            {"Brightness", v(s.brightness_text), UiAction::ScreenOpen, true},
            {"Dim after", v(s.dim_after_text), UiAction::ScreenOpen, true},
            {"Dim to", v(s.dim_to_text), UiAction::ScreenOpen, true},
            {"Sleep after", v(s.sleep_after_text), UiAction::ScreenOpen, true},
            blank, blank, blank, blank, back,
        };
        memcpy(out, tiles, sizeof(tiles));
    } else if (s.menu_page == 3) {
        static const char *const kLabels[8] = {"Top press", "Top hold", "Bottom press", "Bottom hold",
                                               "Swipe up", "Swipe down", "Swipe left", "Swipe right"};
        for (int i = 0; i < 8; i++) {
            out[i] = {kLabels[i], s.controls[i] ? s.controls[i] : "-", UiAction::ControlCycle, true};
        }
        out[8] = back;
    } else if (s.menu_page == 4) {
        MenuTile tiles[kMenuTiles] = {
            {"On battery", s.battery_percent < 0 ? "Charging" : s.since_charge ? s.since_charge : "-",
             UiAction::None, false},
            {"Battery test", s.test_text ? s.test_text : "Start", UiAction::MenuBatteryTest, true},
            {"Last test", s.last_test ? s.last_test : "-", UiAction::None, false},
            blank, blank, blank, blank, blank, back,
        };
        memcpy(out, tiles, sizeof(tiles));
    } else {
        MenuTile tiles[kMenuTiles] = {
            {"Look", s.vivid ? "Vivid" : "Subdued", UiAction::MenuLook, true},
            {"Counter", counter_style_plain(g) ? kCounterNames[s.counter_style % 3] : "-", UiAction::MenuCounter,
             counter_style_plain(g)},
            {"Flip", s.flipped ? "On" : "Off", UiAction::MenuFlip, true},
            {"Screen", s.always_on ? "Always on" : "Settings", UiAction::MenuScreen, true},
            {"Controls", "Buttons", UiAction::MenuControls, true},
            {"Battery", s.battery_percent < 0 ? "Charging" : "Info", UiAction::MenuBattery, true},
            {"History", "Games", UiAction::MenuHistory, true},
            {"Turn off", "Sleep", UiAction::MenuOff, true},
            back,
        };
        memcpy(out, tiles, sizeof(tiles));
    }
}

const char *menu_title(int page)
{
    static const char *const kTitles[] = {"MENU", "MORE", "SCREEN", "CONTROLS", "BATTERY"};
    return page >= 0 && page < 5 ? kTitles[page] : "MENU";
}

void draw_menu(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    MenuTile tiles[kMenuTiles];
    char match[8], timer[12];
    menu_tiles(s, tiles, match, timer);
    draw_close_mark(f, t);

    for (int i = 0; i < kMenuTiles; i++) {
        const MenuTile &mt = tiles[i];
        if (!mt.label) {
            continue;
        }
        Rect r = menu_tile(i);
        f.fillRoundRect(r.x, r.y, r.w, r.h, 8, t.surface);
        f.drawRoundRect(r.x, r.y, r.w, r.h, 8, mix(t.surface, t.accent, mt.enabled ? 140 : 40));

        f.setFont(&fonts::Font2);
        f.setTextSize(1);
        f.setTextColor(t.muted);
        f.setTextDatum(textdatum_t::middle_left);
        f.drawString(mt.label, r.x + 8, r.y + 11);

        fit_font(f, mt.value, r.w - 12);
        if (f.getFont() == &fonts::FreeSansBold12pt7b) {
            f.setFont(&fonts::FreeSansBold9pt7b); // keep values to one size in the short tiles
        }
        f.setTextColor(mt.enabled ? t.accent : t.muted);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(mt.value, r.cx(), r.y + 29);
    }
}

// ---- Who won? ------------------------------------------------------------------

Rect winner_tile(int p)
{
    return {6 + p * 158, 32, 150, 94};
}

constexpr Rect kNoResult = {6, 134, 308, 30};

// One counter: you can't be "the other player", so ask Won / Lost / Draw.
Rect solo_result_tile(int i)
{
    return {6 + i * 104, 32, 100, 94};
}

void draw_solo_end_prompt(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    static const char *const kLabels[] = {"Won", "Lost", "Draw"};
    const uint32_t colors[] = {kGain, kLoss, t.muted};
    char buf[24];
    int game_no = s.match ? s.match->games + 1 : 1;
    snprintf(buf, sizeof(buf), "game %d", game_no);
    for (int i = 0; i < 3; i++) {
        Rect r = solo_result_tile(i);
        f.fillRoundRect(r.x, r.y, r.w, r.h, 10, t.surface);
        f.drawRoundRect(r.x, r.y, r.w, r.h, 10, colors[i]);
        f.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 9, mix(t.surface, colors[i], 140));
        f.setFont(&fonts::FreeSansBold12pt7b);
        f.setTextSize(1);
        f.setTextDatum(textdatum_t::middle_center);
        draw_text_glow(f, t, kLabels[i], r.cx(), r.cy() - 10, colors[i]);
        f.setFont(&fonts::Font2);
        f.setTextColor(t.text);
        f.drawString(buf, r.cx(), r.cy() + 18);
    }
}

void draw_end_prompt(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    const bb_game_t &g = *s.game;
    char buf[24];
    draw_close_mark(f, t);
    int game_no = s.match ? s.match->games + 1 : 1;
    if (g.players == 1) {
        draw_solo_end_prompt(f, t, s);
    }
    for (uint8_t p = 0; p < 2 && g.players > 1; p++) {
        Rect r = winner_tile(p);
        uint32_t c = player_color(t, p);
        f.fillRoundRect(r.x, r.y, r.w, r.h, 10, t.surface);
        f.drawRoundRect(r.x, r.y, r.w, r.h, 10, c);
        f.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 9, mix(t.surface, c, 140));
        fit_font(f, ui_player_label(g, p), r.w - 16);
        f.setTextDatum(textdatum_t::middle_center);
        draw_text_glow(f, t, ui_player_label(g, p), r.cx(), r.cy() - 10, c);
        snprintf(buf, sizeof(buf), "won game %d", game_no);
        f.setFont(&fonts::Font2);
        f.setTextSize(1);
        f.setTextColor(t.text);
        f.drawString(buf, r.cx(), r.cy() + 18);
    }
    Rect r = kNoResult;
    f.fillRoundRect(r.x, r.y, r.w, r.h, 8, t.surface);
    f.drawRoundRect(r.x, r.y, r.w, r.h, 8, mix(t.surface, t.accent, 90));
    f.setFont(&fonts::FreeSans9pt7b);
    f.setTextColor(t.muted);
    f.setTextDatum(textdatum_t::middle_center);
    f.drawString("Restart without a result", r.cx(), r.cy());
}

UiAction end_prompt_hit(const UiState &s, int x, int y)
{
    UiAction a;
    if (y < kHeaderH) {
        a.type = UiAction::EndCancel;
        return a;
    }
    if (s.game->players == 1) {
        for (int i = 0; i < 3; i++) {
            if (solo_result_tile(i).contains(x, y)) {
                if (i == 2) {
                    a.type = UiAction::EndDraw;
                } else {
                    a.type = UiAction::DeclareWinner;
                    a.player = (uint8_t)i; // 0 = you won, 1 = they did
                }
                return a;
            }
        }
    }
    for (uint8_t p = 0; p < 2 && s.game->players > 1; p++) {
        if (winner_tile(p).contains(x, y)) {
            a.type = UiAction::DeclareWinner;
            a.player = p;
            return a;
        }
    }
    if (kNoResult.contains(x, y)) {
        a.type = UiAction::EndRestart;
    }
    return a;
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

// Draws a die silhouette rotated by angle and scaled to radius r, then
// stretched by sx / sy (for spins seen edge-on and landing squash).
void draw_die_body(LGFX_Sprite &f, uint8_t sides, int cx, int cy, float r, float angle,
                   uint32_t fill, uint32_t edge, uint32_t facet, float sx = 1, float sy = 1)
{
    DieShape d = die_shape(sides);
    float c = cosf(angle), sn = sinf(angle);
    int x[6], y[6];
    for (int i = 0; i < d.n; i++) {
        x[i] = cx + (int)lroundf((d.px[i] * c - d.py[i] * sn) * r * sx);
        y[i] = cy + (int)lroundf((d.px[i] * sn + d.py[i] * c) * r * sy);
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

// Overshoots a little past 1, then settles: a springy stop.
float ease_out_back(float t)
{
    const float c1 = 1.70158f, c3 = c1 + 1;
    float u = t - 1;
    return 1 + c3 * u * u * u + c1 * u * u;
}

// How each die moves while rolling, given roll progress t (0..1).
struct DieMotion {
    float angle = 0, dx = 0, dy = 0, sx = 1, sy = 1, zoom = 1;
};

DieMotion die_motion(uint8_t sides, float t)
{
    DieMotion m;
    float u = 1 - ease_out_cubic(t); // motion left, 1 -> 0
    switch (sides) {
    case 4: // hops in shrinking bounces, wobbling, squashing as it lands
        m.dy = -fabsf(sinf(t * 3.5f * kPi)) * 46 * u;
        m.angle = sinf(t * 9 * kPi) * 0.45f * u;
        if (m.dy > -3 && u > 0.05f) {
            m.sx = 1.14f;
            m.sy = 0.84f;
        }
        break;
    case 6: // rolls in from the left
        m.dx = -150 * u;
        m.angle = -u * 5 * kPi;
        break;
    case 8: { // spins like a top, seen edge-on as it turns
        float w = fabsf(cosf(u * 9 * kPi));
        m.sx = w < 0.12f ? 0.12f : w;
        break;
    }
    case 10: // swings like a pendulum while drifting side to side
        m.angle = sinf(t * 14 * kPi) * 1.4f * u;
        m.dx = sinf(t * 5 * kPi) * 30 * u;
        break;
    case 12: // tossed in an arc from the right
        m.dx = 150 * u;
        m.dy = -sinf(t * kPi) * 48 * u;
        m.angle = u * 4 * kPi;
        break;
    default: // d20: thrown toward you, spinning as it grows
        m.zoom = 0.35f + 0.65f * ease_out_cubic(t);
        m.angle = u * 6.4f * kPi;
        break;
    }
    return m;
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
        DieMotion m = d.rolling ? die_motion(die->sides, t_roll) : DieMotion();
        float rr = r * m.zoom;
        // Triangles sit lower so they look centered.
        int dy = die->sides == 4 ? (int)(rr * 0.18f) : 0;
        int bx = cx + (int)m.dx, by = cy + dy + (int)m.dy;
        uint32_t edge = d.rolling ? mix(t.surface, t.accent, 150)
                        : max_roll || min_roll ? tier
                                               : t.accent;
        draw_die_body(f, die->sides, bx, by, rr, d.rolling ? m.angle : 0, t.surface, edge,
                      mix(t.surface, t.accent, 45), m.sx, m.sy);
        DieShape shape = die_shape(die->sides);
        float squeeze = (m.sx < m.sy ? m.sx : m.sy) * m.zoom;
        int box = (int)(r * shape.inner * 2 * squeeze);
        if (d.value == 0) {
            f.setFont(&fonts::FreeSansBold12pt7b);
            f.setTextColor(t.text);
            f.setTextDatum(textdatum_t::middle_center);
            bool narrow = die->sides == 4 || die->sides == 8 || die->sides == 10;
            f.drawString(narrow ? "TAP" : "ROLL", bx, by + 2);
        } else if (box > 14) {
            snprintf(buf, sizeof(buf), "%d", d.value);
            uint32_t c = d.rolling ? t.muted : tier;
            draw_number(f, buf, bx, by + 2, box, (int)(box * 0.8f), c, mix(t.surface, 0x000000, 160));
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


// ---- Pop-up screens (overlays) ----------------------------------------------

// Official Riftbound Champion Legends. A champion can have more than one
// Legend card (Master Yi in Proving Grounds and in Unleashed), so each entry is
// a card: name plus set. Stable order: codes saved in the history are indexes
// here, so add new cards at the end only.
// Set codes: OGN Origins, OGS Proving Grounds, SFD Spiritforged, UNL Unleashed,
// VEN Vendetta, RAD Radiance (RAD is ours; no official code found yet).
const char *const kSetCodes[] = {"OGN", "OGS", "SFD", "UNL", "VEN", "RAD"};

struct LegendCard {
    const char *name;
    uint8_t set; // index into kSetCodes
};

const LegendCard kLegends[] = {
    // Origins
    {"Ahri", 0}, {"Darius", 0}, {"Jinx", 0}, {"Kai'Sa", 0}, {"Lee Sin", 0}, {"Leona", 0},
    {"Miss Fortune", 0}, {"Sett", 0}, {"Teemo", 0}, {"Viktor", 0}, {"Volibear", 0}, {"Yasuo", 0},
    // Proving Grounds
    {"Annie", 1}, {"Garen", 1}, {"Lux", 1}, {"Master Yi", 1},
    // Spiritforged
    {"Azir", 2}, {"Draven", 2}, {"Ezreal", 2}, {"Fiora", 2}, {"Irelia", 2}, {"Jax", 2}, {"Lucian", 2},
    {"Ornn", 2}, {"Rek'Sai", 2}, {"Renata Glasc", 2}, {"Rumble", 2}, {"Sivir", 2},
    // Unleashed (its Master Yi is at the end: added later, codes must not move)
    {"Diana", 3}, {"Ivern", 3}, {"Jhin", 3}, {"Kha'Zix", 3}, {"LeBlanc", 3}, {"Lillia", 3}, {"Poppy", 3},
    {"Pyke", 3}, {"Rengar", 3}, {"Vex", 3}, {"Vi", 3},
    // Vendetta
    {"Akali", 4}, {"Ambessa", 4}, {"Jayce", 4}, {"Kennen", 4}, {"Mel", 4}, {"Nasus", 4}, {"Renekton", 4},
    {"Shen", 4}, {"Zed", 4},
    // Radiance
    {"Ekko", 5}, {"Evelynn", 5}, {"Jarvan IV", 5}, {"K'Sante", 5}, {"Mordekaiser", 5}, {"Orianna", 5},
    {"Riven", 5}, {"Seraphine", 5}, {"Ziggs", 5},
    // Added later
    {"Master Yi", 3},
};
constexpr size_t kLegendCount = sizeof(kLegends) / sizeof(kLegends[0]);

// A-Z by name, then by set (oldest first).
bool legend_before(uint8_t a, uint8_t b)
{
    int c = strcasecmp(kLegends[a].name, kLegends[b].name);
    return c < 0 || (c == 0 && kLegends[a].set < kLegends[b].set);
}

// Official legends sorted A-Z for the picker (indexes into kLegends).
const uint8_t *legends_sorted()
{
    static uint8_t order[kLegendCount];
    static bool ready = false;
    if (!ready) {
        for (size_t i = 0; i < kLegendCount; i++) {
            order[i] = (uint8_t)i;
        }
        for (size_t i = 1; i < kLegendCount; i++) { // insertion sort, once
            uint8_t v = order[i];
            size_t j = i;
            while (j > 0 && legend_before(v, order[j - 1])) {
                order[j] = order[j - 1];
                j--;
            }
            order[j] = v;
        }
        ready = true;
    }
    return order;
}

// The picker lists the player's own legends first, then the official ones A-Z.
int picker_legend_count(const UiState &s)
{
    return s.custom_legend_count + (int)kLegendCount;
}

uint8_t picker_legend_code(const UiState &s, int i)
{
    if (i < s.custom_legend_count) {
        return (uint8_t)(kCustomLegend + i);
    }
    return legends_sorted()[i - s.custom_legend_count];
}

void fill_tile(LGFX_Sprite &f, const Theme &t, const Rect &r, uint32_t edge, bool selected = false, int radius = 8)
{
    f.fillRoundRect(r.x, r.y, r.w, r.h, radius, selected ? mix(t.surface, edge, 150) : t.surface);
    f.drawRoundRect(r.x, r.y, r.w, r.h, radius, edge);
}

void tile_text(LGFX_Sprite &f, const Rect &r, const char *text, uint32_t color, int max_w = 0)
{
    fit_font(f, text, max_w ? max_w : r.w - 10);
    if (f.getFont() == &fonts::FreeSansBold12pt7b && r.h < 40) {
        f.setFont(&fonts::FreeSansBold9pt7b);
    }
    f.setTextColor(color);
    f.setTextDatum(textdatum_t::middle_center);
    f.drawString(text, r.cx(), r.cy() + 1);
}

// Quick menu.
Rect quick_tile(int i)
{
    return {6 + (i % 2) * 156, 30 + (i / 2) * 68, 152, 64};
}

void draw_quick(LGFX_Sprite &f, const Theme &t)
{
    static const char *const kLabels[] = {"Undo", "Dice", "Menu", "Turn off"};
    draw_close_mark(f, t);
    for (int i = 0; i < 4; i++) {
        Rect r = quick_tile(i);
        fill_tile(f, t, r, mix(t.surface, t.accent, 150), false, 10);
        f.setFont(&fonts::FreeSansBold12pt7b);
        f.setTextSize(1);
        f.setTextDatum(textdatum_t::middle_center);
        draw_text_glow(f, t, kLabels[i], r.cx(), r.cy(), i == 3 ? kLoss : t.accent);
    }
}

UiAction quick_hit(int x, int y)
{
    UiAction a;
    a.type = UiAction::QuickClose;
    if (y < kHeaderH) {
        return a;
    }
    for (int i = 0; i < 4; i++) {
        if (quick_tile(i).contains(x, y)) {
            a.type = UiAction::QuickPick;
            a.kind = (uint8_t)i;
            return a;
        }
    }
    a.type = UiAction::None;
    return a;
}

// Turn off?
constexpr Rect kOffNo = {6, 96, 150, 64};
constexpr Rect kOffYes = {164, 96, 150, 64};

void draw_off_prompt(LGFX_Sprite &f, const Theme &t)
{
    draw_close_mark(f, t);
    f.setFont(&fonts::FreeSans9pt7b);
    f.setTextSize(1);
    f.setTextColor(t.text);
    f.setTextDatum(textdatum_t::middle_center);
    f.drawString("The game is kept.", kWidth / 2, 46);
    f.setTextColor(t.muted);
    f.drawString("Press the top button to wake it.", kWidth / 2, 70);
    fill_tile(f, t, kOffNo, mix(t.surface, t.accent, 120), false, 10);
    fill_tile(f, t, kOffYes, kLoss, false, 10);
    f.setFont(&fonts::FreeSansBold12pt7b);
    draw_text_glow(f, t, "No", kOffNo.cx(), kOffNo.cy(), t.accent);
    draw_text_glow(f, t, "Turn off", kOffYes.cx(), kOffYes.cy(), kLoss);
}

UiAction off_hit(int x, int y)
{
    UiAction a;
    if (y < kHeaderH || kOffNo.contains(x, y)) {
        a.type = UiAction::OffNo;
    } else if (kOffYes.contains(x, y)) {
        a.type = UiAction::OffYes;
    }
    return a;
}

void draw_sleeping(LGFX_Sprite &f, const Theme &t)
{
    f.fillScreen(0x000000u);
    f.setFont(&fonts::FreeSansBold12pt7b);
    f.setTextSize(1);
    f.setTextDatum(textdatum_t::middle_center);
    f.setTextColor(t.accent);
    f.drawString("Turning off", kWidth / 2, 70);
    f.setFont(&fonts::FreeSans9pt7b);
    f.setTextColor(t.muted);
    f.drawString("Top button wakes it", kWidth / 2, 104);
}

// Setting editor.
constexpr Rect kStepMinus = {14, 34, 64, 44};
constexpr Rect kStepPlus = {242, 34, 64, 44};
constexpr int kSliderX = 24;
constexpr int kSliderW = 272;
constexpr int kSliderY = 98;

Rect preset_chip(int i, int n)
{
    int w = (308 - (n - 1) * 4) / n;
    return {6 + i * (w + 4), 128, w, 34};
}

void draw_editor(LGFX_Sprite &f, const Theme &t, const EditorView &e)
{
    draw_close_mark(f, t);
    f.setFont(&fonts::FreeSansBold18pt7b);
    f.setTextSize(1);
    f.setTextDatum(textdatum_t::middle_center);
    draw_text_glow(f, t, e.value, kWidth / 2, 57, t.text);

    if (e.slider) {
        int knob = kSliderX + e.frac * kSliderW / 1000;
        f.fillRoundRect(kSliderX, kSliderY - 4, kSliderW, 8, 4, mix(t.surface, t.muted, 60));
        f.fillRoundRect(kSliderX, kSliderY - 4, knob - kSliderX + 4, 8, 4, t.accent);
        f.fillCircle(knob, kSliderY, 11, t.accent);
        f.fillCircle(knob, kSliderY, 6, t.surface);
    } else {
        for (int k = 0; k < 2; k++) {
            const Rect &r = k ? kStepPlus : kStepMinus;
            fill_tile(f, t, r, mix(t.surface, t.accent, 140), false, 10);
            f.fillRect(r.cx() - 10, r.cy() - 2, 20, 4, t.accent);
            if (k) {
                f.fillRect(r.cx() - 2, r.cy() - 10, 4, 20, t.accent);
            }
        }
        f.setFont(&fonts::Font2);
        f.setTextColor(t.muted);
        f.drawString("hold to change faster", kWidth / 2, 96);
    }

    for (int i = 0; i < e.preset_count; i++) {
        Rect r = preset_chip(i, e.preset_count);
        bool sel = i == e.preset_sel;
        f.fillRoundRect(r.x, r.y, r.w, r.h, 8, sel ? t.accent : t.surface);
        f.drawRoundRect(r.x, r.y, r.w, r.h, 8, mix(t.surface, t.accent, 140));
        tile_text(f, r, e.presets[i], sel ? kInk : t.text);
    }
}

UiAction editor_hit(const EditorView &e, int x, int y)
{
    UiAction a;
    if (y < kHeaderH) {
        a.type = UiAction::EditorClose;
        return a;
    }
    for (int i = 0; i < e.preset_count; i++) {
        Rect r = preset_chip(i, e.preset_count);
        Rect padded{r.x - 2, r.y - 3, r.w + 4, r.h + 6};
        if (padded.contains(x, y)) {
            a.type = UiAction::EditorPreset;
            a.kind = (uint8_t)i;
            return a;
        }
    }
    if (e.slider) {
        if (y >= kSliderY - 22 && y <= kSliderY + 22) {
            a.type = UiAction::EditorSlide;
            int v = (x - kSliderX) * 1000 / kSliderW;
            a.delta = (int16_t)(v < 0 ? 0 : v > 1000 ? 1000 : v);
        }
        return a;
    }
    Rect minus{kStepMinus.x - 6, kStepMinus.y - 4, kStepMinus.w + 12, kStepMinus.h + 20};
    Rect plus{kStepPlus.x - 6, kStepPlus.y - 4, kStepPlus.w + 12, kStepPlus.h + 20};
    if (minus.contains(x, y) || plus.contains(x, y)) {
        a.type = UiAction::EditorStep;
        a.delta = plus.contains(x, y) ? 1 : -1;
        a.repeat = true;
    }
    return a;
}

// History.
constexpr int kHistoryRows = 3;
constexpr Rect kPagePrev = {6, 147, 64, 21};
constexpr Rect kPageNext = {250, 147, 64, 21};

Rect history_row(int i)
{
    return {6, 28 + i * 40, 308, 37};
}

bool entry_has_kinds(const HistoryEntry &e)
{
    return bb_profile_at(e.profile_index)->kind_count >= 2;
}

// "Won 8-6", "Lost", "Draw", "A won 11-9".
uint32_t history_result(const Theme &t, const HistoryEntry &e, char *buf, size_t n)
{
    const bb_profile_t *p = bb_profile_at(e.profile_index);
    const char *word = e.result == 2 ? "Draw" : e.result == 0 ? "Won" : "Lost";
    if (p->teams && e.result < 2) {
        word = e.result == 0 ? "A won" : "B won";
    }
    if (e.players >= 2) {
        snprintf(buf, n, "%s %d-%d", word, e.value[0], e.value[1]);
    } else {
        snprintf(buf, n, "%s %d", word, e.value[0]);
    }
    return e.result == 2 || p->teams ? t.text : e.result == 0 ? kGain : kLoss;
}

void draw_pager(LGFX_Sprite &f, const Theme &t, int page, int pages)
{
    char buf[12];
    for (int k = 0; k < 2; k++) {
        const Rect &r = k ? kPageNext : kPagePrev;
        bool on = k ? page < pages - 1 : page > 0;
        fill_tile(f, t, r, mix(t.surface, t.accent, on ? 140 : 40), false, 6);
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextColor(on ? t.accent : t.muted);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(k ? ">" : "<", r.cx(), r.cy());
    }
    snprintf(buf, sizeof(buf), "%d / %d", page + 1, pages < 1 ? 1 : pages);
    f.setFont(&fonts::Font2);
    f.setTextColor(t.muted);
    f.drawString(buf, kWidth / 2, kPagePrev.cy());
}

void draw_history(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    draw_close_mark(f, t);
    if (s.history_count == 0) {
        f.setFont(&fonts::FreeSans9pt7b);
        f.setTextSize(1);
        f.setTextColor(t.muted);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString("No games yet", kWidth / 2, 70);
        f.setFont(&fonts::Font2);
        f.drawString("Finish a game (End game > result) to record it", kWidth / 2, 96);
        return;
    }
    char buf[40];
    int pages = (s.history_count + kHistoryRows - 1) / kHistoryRows;
    for (int i = 0; i < kHistoryRows; i++) {
        int idx = s.history_page * kHistoryRows + i;
        if (idx >= s.history_count) {
            break;
        }
        const HistoryEntry &e = s.history[idx];
        Rect r = history_row(i);
        fill_tile(f, t, r, mix(t.surface, t.accent, 90), false, 6);
        const bb_profile_t *p = bb_profile_at(e.profile_index);
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextSize(1);
        f.setTextDatum(textdatum_t::top_left);
        f.setTextColor(t.text);
        snprintf(buf, sizeof(buf), "#%u %s", e.number, p->short_name ? p->short_name : p->name);
        f.drawString(buf, r.x + 8, r.y + 3);
        uint32_t c = history_result(t, e, buf, sizeof(buf));
        f.setTextColor(c);
        f.setTextDatum(textdatum_t::top_right);
        f.drawString(buf, r.x + r.w - 8, r.y + 3);

        f.setFont(&fonts::Font2);
        f.setTextColor(t.muted);
        f.setTextDatum(textdatum_t::bottom_left);
        if (entry_has_kinds(e) && (e.legend[0] != kNoLegend || e.legend[1] != kNoLegend)) {
            char a[24], b[24];
            snprintf(buf, sizeof(buf), "%s v %s", ui_legend_label(s, e.legend[0], a, sizeof(a)),
                     ui_legend_label(s, e.legend[1], b, sizeof(b)));
        } else if (e.best_of > 1) {
            snprintf(buf, sizeof(buf), "Bo%u game %u", e.best_of, e.game_in_match);
        } else {
            buf[0] = 0;
        }
        f.drawString(buf, r.x + 8, r.y + r.h - 1);
        if (e.minutes) {
            snprintf(buf, sizeof(buf), "%u min", e.minutes);
            f.setTextDatum(textdatum_t::bottom_right);
            f.drawString(buf, r.x + r.w - 8, r.y + r.h - 1);
        }
    }
    draw_pager(f, t, s.history_page, pages);
}

UiAction history_hit(const UiState &s, int x, int y)
{
    UiAction a;
    if (y < kHeaderH) {
        a.type = UiAction::HistoryClose;
        return a;
    }
    if (kPagePrev.contains(x, y) || kPageNext.contains(x, y)) {
        a.type = UiAction::HistoryPage;
        a.delta = kPageNext.contains(x, y) ? 1 : -1;
        return a;
    }
    for (int i = 0; i < kHistoryRows; i++) {
        int idx = s.history_page * kHistoryRows + i;
        if (idx < s.history_count && history_row(i).contains(x, y)) {
            a.type = UiAction::HistoryOpen;
            a.kind = (uint8_t)idx;
            return a;
        }
    }
    return a;
}

Rect detail_tile(int i)
{
    return {6 + i * 104, 124, 100, 40};
}

void draw_history_detail(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    draw_close_mark(f, t);
    if (s.history_sel < 0 || s.history_sel >= s.history_count) {
        return;
    }
    const HistoryEntry &e = s.history[s.history_sel];
    const bb_profile_t *p = bb_profile_at(e.profile_index);
    char buf[48];

    f.setFont(&fonts::FreeSans9pt7b);
    f.setTextSize(1);
    f.setTextColor(t.muted);
    f.setTextDatum(textdatum_t::middle_left);
    if (e.best_of > 1) {
        snprintf(buf, sizeof(buf), "%s - Bo%u game %u", p->name, e.best_of, e.game_in_match);
    } else {
        snprintf(buf, sizeof(buf), "%s", p->name);
    }
    f.drawString(buf, 8, 38);
    if (e.minutes) {
        snprintf(buf, sizeof(buf), "%u min", e.minutes);
        f.setTextDatum(textdatum_t::middle_right);
        f.drawString(buf, kWidth - 8, 38);
    }

    uint32_t c = history_result(t, e, buf, sizeof(buf));
    f.setFont(&fonts::FreeSansBold12pt7b);
    f.setTextDatum(textdatum_t::middle_left);
    draw_text_glow(f, t, buf, 8, 62, c);

    if (entry_has_kinds(e)) {
        f.setFont(&fonts::Font2);
        f.setTextColor(t.text);
        f.setTextDatum(textdatum_t::middle_left);
        for (int side = 0; side < (e.players >= 2 ? 2 : 1); side++) {
            snprintf(buf, sizeof(buf), "%s: %d %s, %d %s", side == 0 ? (p->teams ? "Team A" : "You")
                                                                     : (p->teams ? "Team B" : "Opp"),
                     e.tally[side][0], p->kinds[0], e.tally[side][1], p->kinds[1]);
            f.drawString(buf, 8, 88 + side * 17);
        }
        for (int side = 0; side < 2; side++) {
            Rect r = detail_tile(side);
            fill_tile(f, t, r, mix(t.surface, t.accent, 140));
            f.setFont(&fonts::Font2);
            f.setTextColor(t.muted);
            f.setTextDatum(textdatum_t::top_left);
            f.drawString(side == 0 ? "Your legend" : "Their legend", r.x + 6, r.y + 2);
            Rect v{r.x, r.y + 14, r.w, r.h - 14};
            tile_text(f, v, ui_legend_name(s, e.legend[side]), t.accent, r.w - 30);
            f.setFont(&fonts::Font0);
            f.setTextColor(t.muted);
            f.setTextDatum(textdatum_t::top_right);
            f.drawString(ui_legend_set(e.legend[side]), r.x + r.w - 5, r.y + 4);
        }
    }
    Rect r = detail_tile(2);
    fill_tile(f, t, r, s.history_delete_armed ? kLoss : mix(t.surface, kLoss, 120), s.history_delete_armed);
    tile_text(f, r, s.history_delete_armed ? "Sure?" : "Delete", s.history_delete_armed ? t.text : kLoss);
}

UiAction history_detail_hit(const UiState &s, int x, int y)
{
    UiAction a;
    if (y < kHeaderH) {
        a.type = UiAction::DetailBack;
        return a;
    }
    if (s.history_sel < 0 || s.history_sel >= s.history_count) {
        return a;
    }
    const HistoryEntry &e = s.history[s.history_sel];
    for (int i = 0; i < 3; i++) {
        Rect r = detail_tile(i);
        Rect padded{r.x - 2, r.y - 4, r.w + 4, r.h + 6};
        if (!padded.contains(x, y)) {
            continue;
        }
        if (i == 2) {
            a.type = UiAction::HistoryDelete;
        } else if (entry_has_kinds(e)) {
            a.type = UiAction::HistoryLegend;
            a.player = (uint8_t)i;
        }
        return a;
    }
    return a;
}

// Legend picker.
constexpr int kLegendCols = 3;
constexpr int kLegendRowsPerPage = 4;
constexpr int kLegendsPerPage = kLegendCols * kLegendRowsPerPage;
constexpr Rect kLegendNone = {74, 147, 84, 21};
constexpr Rect kLegendAdd = {162, 147, 84, 21};

Rect legend_tile(int i)
{
    return {6 + (i % kLegendCols) * 104, 27 + (i / kLegendCols) * 30, 100, 28};
}

void draw_legends(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    draw_close_mark(f, t);
    int n = picker_legend_count(s);
    int pages = (n + kLegendsPerPage - 1) / kLegendsPerPage;
    for (int i = 0; i < kLegendsPerPage; i++) {
        int idx = s.legend_page * kLegendsPerPage + i;
        if (idx >= n) {
            break;
        }
        uint8_t code = picker_legend_code(s, idx);
        Rect r = legend_tile(i);
        bool sel = code == s.legend_current;
        bool own = code >= kCustomLegend;
        f.fillRoundRect(r.x, r.y, r.w, r.h, 6, sel ? t.accent : t.surface);
        f.drawRoundRect(r.x, r.y, r.w, r.h, 6, mix(t.surface, own ? kWinColor : t.accent, 120));
        const char *name = ui_legend_name(s, code);
        f.setTextSize(1);
        f.setFont(&fonts::FreeSans9pt7b);
        if (f.textWidth(name) > r.w - 8) {
            f.setFont(&fonts::Font2);
        }
        f.setTextColor(sel ? kInk : t.text);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(name, r.cx(), r.cy() - 3);
        f.setFont(&fonts::Font0);
        f.setTextColor(sel ? kInk : t.muted);
        f.setTextDatum(textdatum_t::bottom_center);
        f.drawString(ui_legend_set(code), r.cx(), r.y + r.h - 1);
    }
    // Pager arrows sit at the ends of the bottom row, None and New between them.
    char buf[12];
    for (int k = 0; k < 2; k++) {
        const Rect &r = k ? kPageNext : kPagePrev;
        bool on = k ? s.legend_page < pages - 1 : s.legend_page > 0;
        fill_tile(f, t, r, mix(t.surface, t.accent, on ? 140 : 40), false, 6);
        f.setFont(&fonts::FreeSansBold9pt7b);
        f.setTextColor(on ? t.accent : t.muted);
        f.setTextDatum(textdatum_t::middle_center);
        f.drawString(k ? ">" : "<", r.cx(), r.cy());
    }
    fill_tile(f, t, kLegendNone, mix(t.surface, t.accent, 100), s.legend_current == kNoLegend, 6);
    tile_text(f, kLegendNone, "None", t.text);
    fill_tile(f, t, kLegendAdd, mix(t.surface, kWinColor, 120), false, 6);
    tile_text(f, kLegendAdd, "+ New", kWinColor);
    snprintf(buf, sizeof(buf), "%d/%d", s.legend_page + 1, pages);
    f.setFont(&fonts::Font2);
    f.setTextColor(t.muted);
    f.setTextDatum(textdatum_t::middle_right);
    f.drawString(buf, kWidth - 62, kHeaderH / 2);
}

UiAction legends_hit(const UiState &s, int x, int y)
{
    UiAction a;
    if (y < kHeaderH) {
        a.type = UiAction::LegendClose;
        return a;
    }
    if (kPagePrev.contains(x, y) || kPageNext.contains(x, y)) {
        a.type = UiAction::LegendPage;
        a.delta = kPageNext.contains(x, y) ? 1 : -1;
        return a;
    }
    if (kLegendNone.contains(x, y)) {
        a.type = UiAction::LegendPick;
        a.kind = kNoLegend;
        return a;
    }
    if (kLegendAdd.contains(x, y)) {
        a.type = UiAction::LegendAdd;
        return a;
    }
    int n = picker_legend_count(s);
    for (int i = 0; i < kLegendsPerPage; i++) {
        int idx = s.legend_page * kLegendsPerPage + i;
        Rect r = legend_tile(i);
        Rect padded{r.x - 2, r.y - 1, r.w + 4, r.h + 3};
        if (idx < n && padded.contains(x, y)) {
            a.type = UiAction::LegendPick;
            a.kind = picker_legend_code(s, idx);
            return a;
        }
    }
    return a;
}

// Keyboard for a new legend's name.
const char *const kKeyRows[] = {"QWERTYUIOP", "ASDFGHJKL'", "ZXCVBNM-."};
constexpr Rect kKeySpace = {6, 140, 150, 26};
constexpr Rect kKeyDel = {160, 140, 74, 26};
constexpr Rect kKeyDone = {238, 140, 76, 26};

Rect keyboard_key(int row, int col)
{
    return {6 + col * 31 + (row == 2 ? 15 : 0), 56 + row * 28, 29, 25};
}

void draw_keyboard(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    draw_close_mark(f, t);
    Rect box{6, 27, 308, 25};
    f.fillRoundRect(box.x, box.y, box.w, box.h, 6, t.surface);
    f.drawRoundRect(box.x, box.y, box.w, box.h, 6, t.accent);
    f.setFont(&fonts::FreeSansBold9pt7b);
    f.setTextSize(1);
    f.setTextDatum(textdatum_t::middle_left);
    const char *text = s.keyboard_text ? s.keyboard_text : "";
    f.setTextColor(text[0] ? t.text : t.muted);
    int w = f.drawString(text[0] ? text : "Legend name", box.x + 8, box.cy() + 1);
    if (text[0]) {
        f.fillRect(box.x + 10 + w, box.y + 5, 2, box.h - 10, t.accent);
    }
    for (int row = 0; row < 3; row++) {
        for (int col = 0; kKeyRows[row][col]; col++) {
            Rect r = keyboard_key(row, col);
            f.fillRoundRect(r.x, r.y, r.w, r.h, 5, t.surface);
            f.drawRoundRect(r.x, r.y, r.w, r.h, 5, mix(t.surface, t.accent, 90));
            char label[2] = {kKeyRows[row][col], 0};
            f.setFont(&fonts::FreeSansBold9pt7b);
            f.setTextColor(t.text);
            f.setTextDatum(textdatum_t::middle_center);
            f.drawString(label, r.cx(), r.cy() + 1);
        }
    }
    fill_tile(f, t, kKeySpace, mix(t.surface, t.accent, 90), false, 5);
    tile_text(f, kKeySpace, "space", t.muted);
    fill_tile(f, t, kKeyDel, mix(t.surface, t.accent, 90), false, 5);
    tile_text(f, kKeyDel, "DEL", t.muted);
    fill_tile(f, t, kKeyDone, t.accent, true, 5);
    tile_text(f, kKeyDone, "Done", t.text);
}

UiAction keyboard_hit(int x, int y)
{
    UiAction a;
    if (y < kHeaderH) {
        a.type = UiAction::KeyCancel;
        return a;
    }
    for (int row = 0; row < 3; row++) {
        for (int col = 0; kKeyRows[row][col]; col++) {
            Rect r = keyboard_key(row, col);
            Rect padded{r.x - 1, r.y - 1, r.w + 2, r.h + 3};
            if (padded.contains(x, y)) {
                a.type = UiAction::KeyChar;
                a.kind = (uint8_t)kKeyRows[row][col];
                return a;
            }
        }
    }
    if (kKeySpace.contains(x, y)) {
        a.type = UiAction::KeyChar;
        a.kind = ' ';
    } else if (kKeyDel.contains(x, y)) {
        a.type = UiAction::KeyDelete;
        a.repeat = true;
    } else if (kKeyDone.contains(x, y)) {
        a.type = UiAction::KeyDone;
    }
    return a;
}

const char *overlay_title(const UiState &s)
{
    switch (s.overlay) {
    case Overlay::Quick:
        return "QUICK";
    case Overlay::OffPrompt:
        return "TURN OFF?";
    case Overlay::Editor:
        return s.editor ? s.editor->title : "";
    case Overlay::History:
        return "HISTORY";
    case Overlay::HistoryDetail: {
        static char buf[16];
        snprintf(buf, sizeof(buf), "GAME #%u",
                 s.history_sel >= 0 && s.history_sel < s.history_count ? s.history[s.history_sel].number : 0);
        return buf;
    }
    case Overlay::Legends:
        return s.legend_side == 0 ? "YOUR LEGEND" : "THEIR LEGEND";
    case Overlay::Keyboard:
        return "NEW LEGEND";
    default:
        return "";
    }
}

void draw_overlay(LGFX_Sprite &f, const Theme &t, const UiState &s)
{
    if (s.overlay == Overlay::Sleeping) {
        draw_sleeping(f, t);
        return;
    }
    draw_background(f, t, false);
    UiState hs = s;
    hs.menu = true; // header without the match clock
    draw_header(f, t, hs, overlay_title(s));
    switch (s.overlay) {
    case Overlay::Quick:
        draw_quick(f, t);
        break;
    case Overlay::OffPrompt:
        draw_off_prompt(f, t);
        break;
    case Overlay::Editor:
        if (s.editor) {
            draw_editor(f, t, *s.editor);
        }
        break;
    case Overlay::History:
        draw_history(f, t, s);
        break;
    case Overlay::HistoryDetail:
        draw_history_detail(f, t, s);
        break;
    case Overlay::Legends:
        draw_legends(f, t, s);
        break;
    case Overlay::Keyboard:
        draw_keyboard(f, t, s);
        break;
    default:
        break;
    }
}

UiAction overlay_hit(const UiState &s, int x, int y)
{
    switch (s.overlay) {
    case Overlay::Quick:
        return quick_hit(x, y);
    case Overlay::OffPrompt:
        return off_hit(x, y);
    case Overlay::Editor:
        return s.editor ? editor_hit(*s.editor, x, y) : UiAction();
    case Overlay::History:
        return history_hit(s, x, y);
    case Overlay::HistoryDetail:
        return history_detail_hit(s, x, y);
    case Overlay::Legends:
        return legends_hit(s, x, y);
    case Overlay::Keyboard:
        return keyboard_hit(x, y);
    default:
        return UiAction();
    }
}

} // namespace

size_t ui_legend_count()
{
    return kLegendCount;
}

const char *ui_legend_official(size_t i)
{
    return i < kLegendCount ? kLegends[i].name : "?";
}

const char *ui_legend_label(const UiState &s, uint8_t code, char *buf, size_t n)
{
    const char *name = ui_legend_name(s, code);
    bool shared = false;
    if (code < kLegendCount) {
        for (size_t i = 0; i < kLegendCount; i++) {
            if (i != code && strcmp(kLegends[i].name, name) == 0) {
                shared = true;
            }
        }
    }
    if (shared) {
        snprintf(buf, n, "%s %s", name, kSetCodes[kLegends[code].set]);
    } else {
        snprintf(buf, n, "%s", name);
    }
    return buf;
}

const char *ui_legend_set(uint8_t code)
{
    if (code == kNoLegend) {
        return "";
    }
    if (code >= kCustomLegend) {
        return "OWN";
    }
    return code < kLegendCount ? kSetCodes[kLegends[code].set] : "";
}

const char *ui_legend_name(const UiState &s, uint8_t code)
{
    if (code == kNoLegend) {
        return "-";
    }
    if (code >= kCustomLegend) {
        int i = code - kCustomLegend;
        return s.custom_legends && i < s.custom_legend_count ? s.custom_legends[i] : "?";
    }
    return ui_legend_official(code);
}

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

    if (state.overlay != Overlay::None) {
        draw_overlay(frame, t, state);
        return;
    }

    if (state.end_prompt) {
        draw_background(frame, t, false);
        draw_header(frame, t, state, state.game->players == 1 ? "RESULT" : "WHO WON?");
        draw_end_prompt(frame, t, state);
        return;
    }

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
    // Riftbound-style two-player screens show undo messages in the header,
    // in place of the game name.
    bool toast_in_header = state.toast && !state.menu && has_kinds(*state.game) && state.game->players == 2;
    draw_header(frame, t, state,
                state.menu ? menu_title(state.menu_page) : toast_in_header ? "" : state.game->profile->name,
                !state.menu);

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
        const bb_game_t &g = *state.game;
        if (has_kinds(g) && g.players == 1) {
            // Under the number, so the point track it refers to stays visible.
            draw_toast(frame, t, state.toast, 138, 86);
        } else if (has_kinds(g) && g.players == 2) {
            // In the header, clear of both players' buttons and tracks.
            draw_toast(frame, t, state.toast, kHeaderH / 2, 140);
        } else {
            draw_toast(frame, t, state.toast, (kFooterTop + kHeight) / 2);
        }
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

    if (state.overlay != Overlay::None) {
        return overlay_hit(state, x, y);
    }

    if (state.menu) {
        if (y < kHeaderH) {
            a.type = UiAction::MenuClose;
            return a;
        }
        MenuTile tiles[kMenuTiles];
        char match[8], timer[12];
        menu_tiles(state, tiles, match, timer);
        for (int i = 0; i < kMenuTiles; i++) {
            Rect r = menu_tile(i);
            // Tiles are hit-tested with the gaps included so near-misses still count.
            Rect padded{r.x - 2, r.y - 1, r.w + 4, r.h + 3};
            if (padded.contains(x, y)) {
                if (tiles[i].label && tiles[i].enabled) {
                    a.type = tiles[i].action;
                    a.kind = (uint8_t)i;
                }
                return a;
            }
        }
        return a;
    }

    if (state.end_prompt) {
        return end_prompt_hit(state, x, y);
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
