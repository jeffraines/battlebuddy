#ifndef BB_MATCH_H
#define BB_MATCH_H

#include <stdbool.h>
#include <stdint.h>

#include "bb/game.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BB_MATCH_MAX_GAMES 5

/* A best-of-N match between two sides (YOU/OPP or TEAM A/B). */
typedef struct {
    uint8_t best_of; /* 1, 3 or 5 */
    uint8_t wins[2];
    uint8_t games;                         /* games recorded so far */
    uint8_t winners[BB_MATCH_MAX_GAMES]; /* winner of each game, in order */
} bb_match_t;

void bb_match_init(bb_match_t *match, uint8_t best_of);

/* Records the winner (0 or 1) of the current game. Ignored once the match is over. */
void bb_match_record(bb_match_t *match, uint8_t winner);

/* 0 or 1 once a side has won more than half the games, otherwise -1. */
int bb_match_winner(const bb_match_t *match);

/* Who won the game, if it's decided: 0 or 1, otherwise -1. Only two-sided
 * games can be decided. With one counter, it is YOU (0) against an
 * opponent who isn't tracked (1): reaching the win target means you won,
 * reaching the lose threshold means they did. */
int bb_game_winner(const bb_game_t *game);

#ifdef __cplusplus
}
#endif

#endif
