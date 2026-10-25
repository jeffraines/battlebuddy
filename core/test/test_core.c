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

enum { CONQUER = 0, HOLD = 1 };

static void test_profiles(void)
{
    CHECK(bb_profile_count() >= 5);
    CHECK(bb_profile_at(bb_profile_count()) == NULL);

    const bb_profile_t *rb = bb_profile_find("riftbound");
    CHECK(rb != NULL);
    CHECK(rb->start == 0 && rb->win_at == 8 && rb->lose_at == BB_NONE);
    CHECK(rb->kind_count == 2);
    CHECK(bb_profile_find("nope") == NULL);
    CHECK(bb_profile_at(bb_profile_index(rb)) == rb);
    CHECK(bb_profile_index(NULL) == bb_profile_count());
}

static void test_init_players(void)
{
    bb_game_t g;

    bb_game_init(&g, bb_profile_find("mtg"), 0);
    CHECK(g.players == 1);
    bb_game_init(&g, bb_profile_find("mtg"), 5);
    CHECK(g.players == BB_MAX_PLAYERS);
    CHECK(g.value[0] == 20 && g.value[1] == 20);
    CHECK(bb_game_adjust(&g, BB_MAX_PLAYERS, 0, -1, 0) == 0); /* no such player */
}

static void test_adjust_and_clamp(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("riftbound"), 1);

    CHECK(g.value[0] == 0);
    CHECK(bb_game_adjust(&g, 0, CONQUER, -1, 0) == 0); /* clamped at min */
    CHECK(bb_game_adjust(&g, 0, CONQUER, 5, 0) == 5);
    CHECK(bb_game_adjust(&g, 0, CONQUER, 5, 0) == 3); /* clamped at max 8 */
    CHECK(g.value[0] == 8);
    CHECK(g.pending.delta == 8);
}

static void test_grouping_and_tick(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("mtg"), 1);

    bb_game_adjust(&g, 0, 0, -1, 1000);
    bb_game_adjust(&g, 0, 0, -1, 1500);
    bb_game_adjust(&g, 0, 0, -1, 2000);
    CHECK(!bb_game_tick(&g, 2000 + BB_COMMIT_MS - 1));
    CHECK(bb_game_tick(&g, 2000 + BB_COMMIT_MS));
    CHECK(g.pending.delta == 0);
    CHECK(g.history_count == 1);

    bb_entry_t e;
    CHECK(bb_game_history(&g, &e, 1) == 1);
    CHECK(e.delta == -3 && e.value == 17 && e.player == 0);

    /* Changes that cancel out leave no history entry. */
    bb_game_adjust(&g, 0, 0, 1, 5000);
    bb_game_adjust(&g, 0, 0, -1, 5100);
    CHECK(!bb_game_tick(&g, 10000));
    CHECK(g.history_count == 1);
}

static void test_tick_wraps_clock(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("mtg"), 1);

    bb_game_adjust(&g, 0, 0, -2, UINT32_MAX - 100);
    CHECK(!bb_game_tick(&g, 100));
    CHECK(bb_game_tick(&g, BB_COMMIT_MS));
}

static void test_undo(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("mtg"), 1);

    CHECK(!bb_game_undo(&g, NULL));

    bb_game_adjust(&g, 0, 0, -5, 0);
    bb_game_commit(&g);
    bb_game_adjust(&g, 0, 0, -2, 10000);
    bb_game_commit(&g);
    bb_game_adjust(&g, 0, 0, 3, 20000); /* still pending */
    CHECK(g.value[0] == 16);

    bb_entry_t r;
    CHECK(bb_game_undo(&g, &r)); /* pending +3 */
    CHECK(r.delta == 3 && g.value[0] == 13);
    CHECK(bb_game_undo(&g, &r)); /* -2 */
    CHECK(r.delta == -2 && g.value[0] == 15);
    CHECK(bb_game_undo(&g, NULL)); /* -5 */
    CHECK(g.value[0] == 20);
    CHECK(!bb_game_undo(&g, &r));
}

static void test_history_ring(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("mtg"), 1);

    for (int i = 0; i < BB_HISTORY_LEN + 10; i++) {
        bb_game_adjust(&g, 0, 0, 1, 0);
        bb_game_commit(&g);
    }
    CHECK(g.history_count == BB_HISTORY_LEN);
    CHECK(g.value[0] == 20 + BB_HISTORY_LEN + 10);

    bb_entry_t latest[2];
    CHECK(bb_game_history(&g, latest, 2) == 2);
    CHECK(latest[0].value == g.value[0] && latest[1].value == g.value[0] - 1);

    int undone = 0;
    while (bb_game_undo(&g, NULL)) {
        undone++;
    }
    CHECK(undone == BB_HISTORY_LEN);
    CHECK(g.value[0] == 30); /* the 10 oldest entries fell off the ring */
}

static void test_two_players(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("mtg"), 2);

    /* Switching player commits the other player's pending change. */
    bb_game_adjust(&g, 0, 0, -3, 0);
    bb_game_adjust(&g, 1, 0, -2, 100);
    CHECK(g.history_count == 1);
    CHECK(g.pending.player == 1 && g.pending.delta == -2);
    CHECK(g.value[0] == 17 && g.value[1] == 18);

    /* Undo walks back across both players in order. */
    bb_entry_t r;
    CHECK(bb_game_undo(&g, &r) && r.player == 1);
    CHECK(g.value[1] == 20);
    CHECK(bb_game_undo(&g, &r) && r.player == 0);
    CHECK(g.value[0] == 20);

    bb_game_adjust(&g, 1, 0, -20, 0);
    CHECK(bb_game_status(&g, 0) == BB_STATUS_PLAYING);
    CHECK(bb_game_status(&g, 1) == BB_STATUS_LOST);
}

static void test_kinds_and_tally(void)
{
    bb_game_t g;
    bb_game_init(&g, bb_profile_find("riftbound"), 2);

    bb_game_adjust(&g, 0, CONQUER, 1, 0);
    bb_game_adjust(&g, 0, HOLD, 1, 10); /* different kind: separate entry */
    bb_game_adjust(&g, 0, HOLD, 1, 20);
    bb_game_adjust(&g, 1, CONQUER, 1, 30);
    bb_game_commit(&g);

    CHECK(g.value[0] == 3 && g.value[1] == 1);
    CHECK(g.tally[0][CONQUER] == 1 && g.tally[0][HOLD] == 2);
    CHECK(g.tally[1][CONQUER] == 1 && g.tally[1][HOLD] == 0);
    CHECK(g.history_count == 3);

    bb_entry_t h[3];
    CHECK(bb_game_history(&g, h, 3) == 3);
    CHECK(h[0].player == 1 && h[0].kind == CONQUER && h[0].delta == 1);
    CHECK(h[1].player == 0 && h[1].kind == HOLD && h[1].delta == 2);
    CHECK(h[2].player == 0 && h[2].kind == CONQUER && h[2].delta == 1);

    /* Undo reverts the tally too. */
    bb_game_undo(&g, NULL);
    bb_game_undo(&g, NULL);
    CHECK(g.tally[0][HOLD] == 0 && g.tally[1][CONQUER] == 0);
    CHECK(g.value[0] == 1 && g.value[1] == 0);

    /* Kind is ignored for profiles without kinds. */
    bb_game_init(&g, bb_profile_find("mtg"), 1);
    bb_game_adjust(&g, 0, 1, -1, 0);
    CHECK(g.pending.kind == 0);
}

static void test_status(void)
{
    bb_game_t g;

    bb_game_init(&g, bb_profile_find("riftbound"), 1);
    CHECK(bb_game_status(&g, 0) == BB_STATUS_PLAYING);
    bb_game_adjust(&g, 0, CONQUER, 8, 0);
    CHECK(bb_game_status(&g, 0) == BB_STATUS_WON);

    bb_game_init(&g, bb_profile_find("mtg"), 1);
    bb_game_adjust(&g, 0, 0, -25, 0);
    CHECK(g.value[0] == -5);
    CHECK(bb_game_status(&g, 0) == BB_STATUS_LOST);

    bb_game_reset(&g);
    CHECK(g.value[0] == 20 && g.history_count == 0 && g.pending.delta == 0);
    CHECK(g.players == 1);
}

int main(void)
{
    test_profiles();
    test_init_players();
    test_adjust_and_clamp();
    test_grouping_and_tick();
    test_tick_wraps_clock();
    test_undo();
    test_history_ring();
    test_two_players();
    test_kinds_and_tally();
    test_status();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all core tests passed\n");
    return EXIT_SUCCESS;
}
