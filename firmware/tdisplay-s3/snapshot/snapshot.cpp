// Renders the UI for a few game states to PPM files, using the same ui_draw()
// as the firmware. Lets the layout be checked without hardware.

#include <stdio.h>

#include "../src/ui.h"
#include "bb/game.h"
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
        save(frame, s, "menu");
    }

    return 0;
}
