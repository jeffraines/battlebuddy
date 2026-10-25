#include <stdio.h>
#include <stdlib.h>

#include "bb/game.h"
#include "bb/profile.h"

static int failures;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                \
        }                                                              \
    } while (0)

static void test_profiles(void)
{
    CHECK(bb_profile_count() >= 5);
    CHECK(bb_profile_at(bb_profile_count()) == NULL);

    const bb_profile_t *rb = bb_profile_find("riftbound");
    CHECK(rb != NULL);
    CHECK(rb->start == 0 && rb->win_at == 8 && rb->lose_at == BB_NONE);
    CHECK(bb_profile_find("nope") == NULL);
}

static void test_adjust_and_clamp(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("riftbound"));

    CHECK(g.value == 0);
    CHECK(bb_game_adjust(&g, -1, 0) == 0); /* clamped at min */
    CHECK(g.value == 0);
    CHECK(bb_game_adjust(&g, 5, 0) == 5);
    CHECK(bb_game_adjust(&g, 5, 0) == 3); /* clamped at max 8 */
    CHECK(g.value == 8);
    CHECK(g.pending == 8);
}

static void test_grouping_and_tick(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("mtg"));

    bb_game_adjust(&g, -1, 1000);
    bb_game_adjust(&g, -1, 1500);
    bb_game_adjust(&g, -1, 2000);
    CHECK(!bb_game_tick(&g, 2000 + BB_COMMIT_MS - 1));
    CHECK(bb_game_tick(&g, 2000 + BB_COMMIT_MS));
    CHECK(g.pending == 0);
    CHECK(g.history_count == 1);

    bb_entry_t e;
    CHECK(bb_game_history(&g, &e, 1) == 1);
    CHECK(e.delta == -3 && e.value == 17);

    /* Changes that cancel out leave no history entry. */
    bb_game_adjust(&g, 1, 5000);
    bb_game_adjust(&g, -1, 5100);
    CHECK(!bb_game_tick(&g, 10000));
    CHECK(g.history_count == 1);
}

static void test_tick_wraps_clock(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("mtg"));

    bb_game_adjust(&g, -2, UINT32_MAX - 100);
    CHECK(!bb_game_tick(&g, 100));
    CHECK(bb_game_tick(&g, BB_COMMIT_MS));
}

static void test_undo(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("mtg"));

    CHECK(!bb_game_undo(&g));

    bb_game_adjust(&g, -5, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, -2, 10000);
    bb_game_commit(&g);
    bb_game_adjust(&g, 3, 20000); /* still pending */
    CHECK(g.value == 16);

    CHECK(bb_game_undo(&g)); /* pending +3 */
    CHECK(g.value == 13);
    CHECK(bb_game_undo(&g)); /* -2 */
    CHECK(g.value == 15);
    CHECK(bb_game_undo(&g)); /* -5 */
    CHECK(g.value == 20);
    CHECK(!bb_game_undo(&g));
}

static void test_history_ring(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("mtg"));

    for (int i = 0; i < BB_HISTORY_LEN + 10; i++) {
        bb_game_adjust(&g, 1, 0);
        bb_game_commit(&g);
    }
    CHECK(g.history_count == BB_HISTORY_LEN);
    CHECK(g.value == 20 + BB_HISTORY_LEN + 10);

    bb_entry_t latest[2];
    CHECK(bb_game_history(&g, latest, 2) == 2);
    CHECK(latest[0].value == g.value && latest[1].value == g.value - 1);

    int undone = 0;
    while (bb_game_undo(&g)) {
        undone++;
    }
    CHECK(undone == BB_HISTORY_LEN);
    CHECK(g.value == 30); /* the 10 oldest entries fell off the ring */
}

static void test_status(void)
{
    bb_game_t g;

    bb_game_init(&g, bb_profile_find("riftbound"));
    CHECK(bb_game_status(&g) == BB_STATUS_PLAYING);
    bb_game_adjust(&g, 8, 0);
    CHECK(bb_game_status(&g) == BB_STATUS_WON);

    bb_game_init(&g, bb_profile_find("mtg"));
    bb_game_adjust(&g, -20, 0);
    CHECK(bb_game_status(&g) == BB_STATUS_LOST);
    bb_game_adjust(&g, -5, 0);
    CHECK(g.value == -5);
    CHECK(bb_game_status(&g) == BB_STATUS_LOST);

    bb_game_reset(&g);
    CHECK(g.value == 20 && g.history_count == 0 && g.pending == 0);
}

int main(void)
{
    test_profiles();
    test_adjust_and_clamp();
    test_grouping_and_tick();
    test_tick_wraps_clock();
    test_undo();
    test_history_ring();
    test_status();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all core tests passed\n");
    return EXIT_SUCCESS;
}
