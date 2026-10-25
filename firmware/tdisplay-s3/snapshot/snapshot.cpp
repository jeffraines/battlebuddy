// Renders the counter UI for a few game states to PPM files, using the same
// ui_draw() as the firmware. Lets the layout be checked without hardware.

#include <stdio.h>

#include "../src/ui.h"
#include "bb/game.h"
#include "bb/profile.h"

static void save_ppm(LGFX_Sprite &frame, const char *path)
{
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

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char path[256];
    LGFX_Sprite frame;
    frame.setColorDepth(16);
    frame.createSprite(kWidth, kHeight);

    bb_game_t game;

    // Magic mid-game: history plus a pending change, step 5.
    bb_game_init(&game, bb_profile_find("mtg"));
    bb_game_adjust(&game, -3, 0);
    bb_game_commit(&game);
    bb_game_adjust(&game, 2, 0);
    bb_game_commit(&game);
    bb_game_adjust(&game, -5, 0);
    bb_game_commit(&game);
    bb_game_adjust(&game, -2, 0);
    ui_draw(frame, game, 5, 76);
    snprintf(path, sizeof(path), "%s/mtg.ppm", dir);
    save_ppm(frame, path);

    // Commander start, on USB.
    bb_game_init(&game, bb_profile_find("mtg-commander"));
    ui_draw(frame, game, 1, -1);
    snprintf(path, sizeof(path), "%s/commander.ppm", dir);
    save_ppm(frame, path);

    // Riftbound win.
    bb_game_init(&game, bb_profile_find("riftbound"));
    for (int i = 0; i < 8; i++) {
        bb_game_adjust(&game, 1, 0);
        bb_game_commit(&game);
    }
    ui_draw(frame, game, 1, 42);
    snprintf(path, sizeof(path), "%s/riftbound-win.ppm", dir);
    save_ppm(frame, path);

    // Magic loss with a negative total.
    bb_game_init(&game, bb_profile_find("mtg"));
    bb_game_adjust(&game, -27, 0);
    bb_game_commit(&game);
    ui_draw(frame, game, 1, 15);
    snprintf(path, sizeof(path), "%s/mtg-lose.ppm", dir);
    save_ppm(frame, path);

    return 0;
}
