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

    // Riftbound, just yourself: two Conquers and a Hold, one Hold pending.
    bb_game_init(&g, bb_profile_find("riftbound"), 1);
    bb_game_adjust(&g, 0, CONQUER, 1, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 0, HOLD, 1, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 0, CONQUER, 1, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 0, HOLD, 1, 0);
    save(frame, state_for(g), "riftbound-1p");

    // Riftbound, both players, you have won.
    bb_game_init(&g, bb_profile_find("riftbound"), 2);
    bb_game_adjust(&g, 1, HOLD, 2, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 0, CONQUER, 5, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 1, CONQUER, 3, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 0, HOLD, 3, 0);
    bb_game_commit(&g);
    save(frame, state_for(g), "riftbound-2p");

    // Magic, both players, opponent's change pending.
    bb_game_init(&g, bb_profile_find("mtg"), 2);
    bb_game_adjust(&g, 0, 0, -6, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 1, 0, -13, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 0, 0, 3, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 1, 0, -2, 0);
    save(frame, state_for(g), "mtg-2p");

    // Lorcana solo, step 5, showing an undo toast.
    bb_game_init(&g, bb_profile_find("lorcana"), 1);
    bb_game_adjust(&g, 0, 0, 7, 0);
    bb_game_commit(&g);
    {
        UiState s = state_for(g);
        s.step = 5;
        s.toast = "Undo +2";
        s.battery_percent = 15;
        save(frame, s, "lorcana-toast");
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

    // Menu, Riftbound theme.
    bb_game_init(&g, bb_profile_find("riftbound"), 2);
    {
        UiState s = state_for(g);
        s.menu = true;
        save(frame, s, "menu");
    }

    return 0;
}
