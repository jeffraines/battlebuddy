// Renders the UI for a few game states to PPM files, using the same ui_draw()
// as the firmware. Lets the layout be checked without hardware.

#include <stdio.h>

#include "../src/ui.h"
#include "bb/game.h"
#include "bb/match.h"
#include "bb/profile.h"

static const char *out_dir = ".";

static void save(LGFX_Sprite &frame, const UiState &state, const char *name)
{
    ui_draw(frame, state);

    char path[256];
    snprintf(path, sizeof(path), "%s/%s.ppm", out_dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", kWidth, kHeight);
    for (int y = 0; y < kHeight; y++) {
        for (int x = 0; x < kWidth; x++) {
            auto c = frame.readPixelRGB(x, y);
            unsigned char px[3] = {c.r, c.g, c.b};
            fwrite(px, 1, 3, f);
        }
    }
    fclose(f);
    printf("wrote %s\n", path);
}

static UiState state_for(const bb_game_t &g)
{
    UiState s;
    s.game = &g;
    s.step = 1;
    s.battery_percent = 76;
    s.layout = Layout::SideYouLeft;
    s.flipped = false;
    s.menu = false;
    s.toast = nullptr;
    s.match = nullptr;
    s.best_of = 0;
    s.timer_minutes = 0;
    s.timer_running = false;
    s.timer_left_s = 0;
    s.dice = nullptr;
    s.picker = nullptr;
    s.entry = nullptr;
    return s;
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        out_dir = argv[1];
    }
    LGFX_Sprite frame;
    frame.setColorDepth(16);
    frame.createSprite(kWidth, kHeight);

    bb_game_t g;
    enum { CONQUER = 0, HOLD = 1 };

    // Riftbound, just yourself: Conquer, Conquer, Hold, Conquer, Hold.
    bb_game_init(&g, bb_profile_find("riftbound"), 1);
    const uint8_t mine[] = {CONQUER, CONQUER, HOLD, CONQUER, HOLD};
    for (uint8_t k : mine) {
        bb_game_adjust(&g, 0, k, 1, 0);
    }
    save(frame, state_for(g), "riftbound-1p");

    // Riftbound, both players side by side.
    bb_game_init(&g, bb_profile_find("riftbound"), 2);
    const uint8_t seq[][2] = {{0, HOLD}, {1, CONQUER}, {0, CONQUER}, {0, CONQUER}, {1, HOLD},
                              {1, HOLD}, {0, HOLD}, {1, CONQUER}, {0, CONQUER}, {0, CONQUER}};
    for (auto &e : seq) {
        bb_game_adjust(&g, e[0], e[1], 1, 0);
    }
    save(frame, state_for(g), "riftbound-2p");

    // Same game, facing each other across the table.
    {
        UiState s = state_for(g);
        s.layout = Layout::Facing;
        save(frame, s, "riftbound-facing");
    }

    // Riftbound, you win (8 points).
    bb_game_init(&g, bb_profile_find("riftbound"), 2);
    for (int i = 0; i < 8; i++) {
        bb_game_adjust(&g, 0, i % 3 == 0 ? HOLD : CONQUER, 1, 0);
    }
    for (int i = 0; i < 6; i++) {
        bb_game_adjust(&g, 1, i % 2 ? HOLD : CONQUER, 1, 0);
    }
    {
        UiState s = state_for(g);
        s.layout = Layout::SideYouRight;
        save(frame, s, "riftbound-win-you-right");
    }

    // Magic facing, opponent's change pending, undo toast.
    bb_game_init(&g, bb_profile_find("mtg"), 2);
    bb_game_adjust(&g, 0, 0, -6, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 1, 0, -13, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 1, 0, -2, 0);
    {
        UiState s = state_for(g);
        s.layout = Layout::Facing;
        save(frame, s, "mtg-facing");
        s.layout = Layout::SideYouLeft;
        s.toast = "Undo OPP -2";
        save(frame, s, "mtg-2p-toast");
    }

    // Commander solo on USB power.
    bb_game_init(&g, bb_profile_find("mtg-commander"), 1);
    bb_game_adjust(&g, 0, 0, -9, 0);
    bb_game_commit(&g);
    {
        UiState s = state_for(g);
        s.battery_percent = -1;
        save(frame, s, "commander");
    }

    // Riftbound, four separate scores.
    bb_game_init(&g, bb_profile_find("riftbound"), 4);
    const uint8_t four[][2] = {{0, HOLD}, {1, CONQUER}, {2, CONQUER}, {3, HOLD}, {0, CONQUER},
                               {1, HOLD}, {2, CONQUER}, {0, CONQUER}, {3, CONQUER}, {1, CONQUER},
                               {0, HOLD}, {2, HOLD}};
    for (auto &e : four) {
        bb_game_adjust(&g, e[0], e[1], 1, 0);
    }
    save(frame, state_for(g), "riftbound-4p");
    {
        UiState s = state_for(g);
        s.layout = Layout::Facing;
        save(frame, s, "riftbound-4p-facing");
    }

    // Riftbound 2v2, one score per team.
    bb_game_init(&g, bb_profile_find("riftbound-2v2"), 2);
    const uint8_t team[][2] = {{0, HOLD}, {1, CONQUER}, {0, CONQUER}, {0, CONQUER}, {1, HOLD},
                               {0, HOLD}, {1, CONQUER}, {0, CONQUER}, {1, HOLD}, {0, CONQUER},
                               {1, CONQUER}, {0, HOLD}};
    for (auto &e : team) {
        bb_game_adjust(&g, e[0], e[1], 1, 0);
    }
    save(frame, state_for(g), "riftbound-2v2");
    {
        UiState s = state_for(g);
        s.layout = Layout::Facing;
        save(frame, s, "riftbound-2v2-facing");
    }

    // Commander, three players: the spare quarter shows the game and battery.
    bb_game_init(&g, bb_profile_find("mtg-commander"), 3);
    bb_game_adjust(&g, 0, 0, -12, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 1, 0, -40, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 2, 0, -3, 0);
    save(frame, state_for(g), "commander-3p");

    // Best of 3, 1-0 up, timer running: header shows score and clock.
    bb_match_t match;
    bb_match_init(&match, 3);
    bb_match_record(&match, 0);
    bb_game_init(&g, bb_profile_find("riftbound"), 2);
    const uint8_t g2[][2] = {{0, HOLD}, {1, CONQUER}, {0, CONQUER}, {1, CONQUER}, {0, CONQUER}};
    for (auto &e : g2) {
        bb_game_adjust(&g, e[0], e[1], 1, 0);
    }
    {
        UiState s = state_for(g);
        s.match = &match;
        s.best_of = 3;
        s.timer_minutes = 50;
        s.timer_running = true;
        s.timer_left_s = 23 * 60 + 41;
        save(frame, s, "match-timer");

        // Same, facing: score and clock in a pill on the divider, over time.
        s.layout = Layout::Facing;
        s.timer_left_s = -(2 * 60 + 10);
        save(frame, s, "match-facing-overtime");
    }

    // YOU reach 8: game-over banner, winning the match 2-0.
    for (int i = 0; i < 6; i++) {
        bb_game_adjust(&g, 0, i % 2 ? HOLD : CONQUER, 1, 0);
    }
    {
        UiState s = state_for(g);
        s.match = &match;
        s.best_of = 3;
        s.timer_minutes = 50;
        s.timer_running = true;
        s.timer_left_s = 4 * 60 + 2;
        save(frame, s, "match-banner");
    }

    // Dice. Stills of each die, then numbered frames of whole rolls for GIFs.
    {
        DiceState d;
        const uint8_t hist[][2] = {{20, 6}, {7, 6}, {2, 0}, {4, 2}, {11, 6}, {1, 0}, {3, 1}};
        for (auto &h : hist) {
            d.history[d.history_count] = h[0];
            d.history_die[d.history_count] = h[1];
            d.history_count++;
        }
        bb_game_init(&g, bb_profile_find("riftbound"), 2);
        UiState s = state_for(g);
        s.dice = &d;

        const uint8_t stills[][2] = {{6, 20}, {1, 3}, {2, 1}, {3, 0}, {4, 7}, {5, 12}, {0, 1}, {0, 0}};
        const char *names[] = {"dice-d20", "dice-d4", "dice-d6", "dice-d8",
                               "dice-d10", "dice-d12", "dice-coin", "dice-coin-new"};
        for (int i = 0; i < 8; i++) {
            d.die = stills[i][0];
            d.value = stills[i][1];
            save(frame, s, names[i]);
        }

        // A d20 roll that lands on 20, and a coin flip.
        const uint8_t faces[] = {7, 13, 2, 18, 9, 15, 4, 11, 16, 6, 19, 14};
        for (int roll = 0; roll < 2; roll++) {
            d.die = roll == 0 ? 6 : 0;
            char name[32];
            int frame_no = 0;
            for (int a = 0; a <= 1000; a += 40) {
                d.rolling = true;
                d.anim = (uint16_t)a;
                d.value = roll == 0 ? faces[(a / 90) % 12] : (uint8_t)(1 + (a / 120) % 2);
                snprintf(name, sizeof(name), "%s-%03d", roll == 0 ? "roll-d20" : "roll-coin", frame_no++);
                save(frame, s, name);
            }
            d.rolling = false;
            d.value = roll == 0 ? 20 : 2;
            for (int p = 0; p <= 1000; p += 125) {
                d.pop = (uint16_t)p;
                snprintf(name, sizeof(name), "%s-%03d", roll == 0 ? "roll-d20" : "roll-coin", frame_no++);
                save(frame, s, name);
            }
            d.pop = 1000;
        }
    }

    // Game picker: the games, then Magic's formats bursting out.
    {
        bb_game_init(&g, bb_profile_find("riftbound"), 2);
        UiState s = state_for(g);
        PickerState pk;
        s.picker = &pk;
        save(frame, s, "picker");
        pk.family = 1;
        pk.origin = 1;
        pk.anim = 350;
        save(frame, s, "picker-burst");
        pk.anim = 1000;
        save(frame, s, "picker-formats");
        pk.family = 0;
        pk.origin = 0;
        save(frame, s, "picker-riftbound");
    }

    // Digimon: YOU have 3 memory; then OPP's turn with 5.
    bb_game_init(&g, bb_profile_find("digimon"), 2);
    bb_game_adjust(&g, 0, 0, -3, 0);
    bb_game_commit(&g);
    save(frame, state_for(g), "digimon-you");
    bb_game_adjust(&g, 0, 0, 8, 0);
    bb_game_commit(&g);
    save(frame, state_for(g), "digimon-opp");

    // Yu-Gi-Oh!: a fresh game shows how to start; then two players, then the number pad.
    bb_game_init(&g, bb_profile_find("yugioh"), 2);
    save(frame, state_for(g), "yugioh-new");
    bb_game_adjust(&g, 1, 0, -2500, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 0, 0, -1800, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 1, 0, 1000, 0);
    bb_game_commit(&g);
    save(frame, state_for(g), "yugioh-2p");
    {
        UiState s = state_for(g);
        EntryState e;
        e.player = 1;
        e.amount = 2400;
        s.entry = &e;
        save(frame, s, "yugioh-entry");
    }

    // Menu with four players.
    bb_game_init(&g, bb_profile_find("riftbound"), 4);
    {
        UiState s = state_for(g);
        s.menu = true;
        save(frame, s, "menu-4p");
    }

    // Menu, Riftbound theme, two players facing.
    bb_game_init(&g, bb_profile_find("riftbound"), 2);
    {
        UiState s = state_for(g);
        s.menu = true;
        s.layout = Layout::Facing;
        s.best_of = 3;
        s.timer_minutes = 50;
        save(frame, s, "menu");
    }

    return 0;
}
