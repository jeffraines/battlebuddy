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
    s.vivid = false;
    s.menu = false;
    s.menu_page = 0;
    s.counter_style = 0;
    s.spin = nullptr;
    s.spin_dir = nullptr;
    s.prev = nullptr;
    s.end_prompt = false;
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

    // Look: the same screens subdued and vivid.
    {
        bb_game_init(&g, bb_profile_find("riftbound"), 2);
        const uint8_t seq[][2] = {{0, HOLD}, {1, CONQUER}, {0, CONQUER}, {0, CONQUER}, {1, HOLD}};
        for (auto &e : seq) {
            bb_game_adjust(&g, e[0], e[1], 1, 0);
        }
        UiState s = state_for(g);
        s.vivid = true;
        save(frame, s, "vivid-riftbound");
        PickerState pk;
        s.picker = &pk;
        save(frame, s, "vivid-picker");
        s.picker = nullptr;
        s.menu = true;
        save(frame, s, "vivid-menu");

        bb_game_init(&g, bb_profile_find("digimon"), 2);
        bb_game_adjust(&g, 0, 0, -4, 0);
        UiState d = state_for(g);
        d.vivid = true;
        save(frame, d, "vivid-digimon");
        bb_game_init(&g, bb_profile_find("yugioh"), 2);
        UiState y = state_for(g);
        y.vivid = true;
        save(frame, y, "vivid-yugioh");
    }

    // Dice landings: a celebration (NAT 8 on a d8), a fumble (1 on a d20) and a middle roll.
    {
        bb_game_init(&g, bb_profile_find("riftbound"), 2);
        UiState s = state_for(g);
        DiceState d;
        s.dice = &d;
        struct Landing { uint8_t die, value; const char *name; int pop_ms; };
        const Landing landings[] = {{3, 8, "land-d8-max", 1100}, {6, 1, "land-d20-min", 1100},
                                    {6, 13, "land-d20-mid", 350}};
        for (const Landing &l : landings) {
            d.die = l.die;
            char name[40];
            int n = 0;
            for (int a = 0; a <= 1000; a += 45) {
                d.rolling = true;
                d.anim = (uint16_t)a;
                d.value = (uint8_t)(1 + (a / 90 * 7) % bb_die_at(l.die)->sides);
                snprintf(name, sizeof(name), "%s-%03d", l.name, n++);
                save(frame, s, name);
            }
            d.rolling = false;
            d.value = l.value;
            for (int ms = 0; ms <= l.pop_ms; ms += 40) {
                d.pop = (uint16_t)(ms * 1000 / l.pop_ms);
                snprintf(name, sizeof(name), "%s-%03d", l.name, n++);
                save(frame, s, name);
            }
            d.pop = 1000;
            snprintf(name, sizeof(name), "%s-%03d", l.name, n++);
            save(frame, s, name);
        }
    }

    // Menu pages and the who-won screen during a Bo3 of Digimon.
    {
        bb_match_t m;
        bb_match_init(&m, 3);
        bb_match_record(&m, 1);
        bb_game_init(&g, bb_profile_find("digimon"), 2);
        UiState s = state_for(g);
        s.match = &m;
        s.best_of = 3;
        s.menu = true;
        save(frame, s, "menu-page1");
        s.menu_page = 1;
        save(frame, s, "menu-page2");
        s.menu = false;
        s.end_prompt = true;
        save(frame, s, "end-prompt");
    }

    // Themed counters: Magic, Commander, Lorcana, FaB, at rest and mid-change.
    {
        uint16_t spin[4] = {1000, 1000, 1000, 1000};
        int8_t dir[4] = {-1, -1, 1, -1};
        int16_t prev[4] = {20, 20, 40, 40};
        struct Case { const char *id; uint8_t players; int16_t d0, d1; const char *name; };
        const Case cases[] = {{"mtg", 1, -3, 0, "theme-mtg"},        {"mtg", 2, -6, -13, "theme-mtg-2p"},
                              {"mtg-commander", 4, -11, -4, "theme-cmd-4p"}, {"lorcana", 1, 7, 0, "theme-lorcana"},
                              {"lorcana", 2, 12, 5, "theme-lorcana-2p"}, {"fab", 1, -9, 0, "theme-fab"},
                              {"fab", 2, -16, -31, "theme-fab-2p"}};
        for (const Case &c : cases) {
            bb_game_init(&g, bb_profile_find(c.id), c.players);
            bb_game_adjust(&g, 0, 0, c.d0, 0);
            bb_game_commit(&g);
            if (c.players > 1) {
                bb_game_adjust(&g, 1, 0, c.d1, 0);
                bb_game_commit(&g);
            }
            UiState s = state_for(g);
            s.spin = spin;
            s.spin_dir = dir;
            s.prev = prev;
            for (int p = 0; p < 4; p++) {
                spin[p] = 1000;
            }
            save(frame, s, c.name);
            // Mid-change frames for the first player: animation strips.
            char name[48];
            dir[0] = c.d0 < 0 ? -1 : 1;
            prev[0] = (int16_t)(g.value[0] + 1 * (c.d0 < 0 ? 1 : -1));
            for (int i = 0; i <= 10; i++) {
                spin[0] = (uint16_t)(i * 100);
                snprintf(name, sizeof(name), "%s-anim-%02d", c.name, i);
                save(frame, s, name);
            }
            spin[0] = 1000;
        }
        // Yu-Gi-Oh! LP counting down after 1800 damage.
        bb_game_init(&g, bb_profile_find("yugioh"), 2);
        bb_game_adjust(&g, 0, 0, -1800, 0);
        bb_game_commit(&g);
        UiState s = state_for(g);
        prev[0] = 8000;
        dir[0] = -1;
        s.spin = spin;
        s.spin_dir = dir;
        s.prev = prev;
        spin[0] = 350;
        save(frame, s, "yugioh-countdown");
        spin[0] = 1000;
    }

    // Each die's own roll: frames for d4, d6, d8, d10, d12, d20.
    {
        bb_game_init(&g, bb_profile_find("riftbound"), 2);
        UiState s = state_for(g);
        DiceState d;
        s.dice = &d;
        const uint8_t finals[] = {0, 3, 5, 6, 9, 11, 17};
        for (int die = 1; die <= 6; die++) {
            d.die = (uint8_t)die;
            char name[32];
            int n = 0;
            for (int a = 0; a <= 1000; a += 40) {
                d.rolling = true;
                d.anim = (uint16_t)a;
                d.value = (uint8_t)(1 + (a / 90 * 5) % bb_die_at(die)->sides);
                snprintf(name, sizeof(name), "shape%d-%03d", die, n++);
                save(frame, s, name);
            }
            d.rolling = false;
            d.value = finals[die];
            d.pop = 1000;
            for (int k = 0; k < 6; k++) {
                snprintf(name, sizeof(name), "shape%d-%03d", die, n++);
                save(frame, s, name);
            }
        }
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
