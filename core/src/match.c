#include "bb/match.h"

#include <string.h>

void bb_match_init(bb_match_t *match, uint8_t best_of)
{
    memset(match, 0, sizeof(*match));
    if (best_of < 1) {
        best_of = 1;
    } else if (best_of > BB_MATCH_MAX_GAMES) {
        best_of = BB_MATCH_MAX_GAMES;
    }
    match->best_of = best_of;
}

int bb_match_winner(const bb_match_t *match)
{
    uint8_t needed = (uint8_t)(match->best_of / 2 + 1);
    if (match->wins[0] >= needed) {
        return 0;
    }
    if (match->wins[1] >= needed) {
        return 1;
    }
    return -1;
}

void bb_match_record(bb_match_t *match, uint8_t winner)
{
    if (winner > 1 || bb_match_winner(match) >= 0 || match->games >= BB_MATCH_MAX_GAMES) {
        return;
    }
    match->winners[match->games++] = winner;
    match->wins[winner]++;
}

int bb_game_winner(const bb_game_t *game)
{
    if (game->players > 2) {
        return -1;
    }
    for (uint8_t p = 0; p < game->players; p++) {
        if (bb_game_status(game, p) == BB_STATUS_WON) {
            return p;
        }
    }
    for (uint8_t p = 0; p < game->players; p++) {
        if (bb_game_status(game, p) == BB_STATUS_LOST) {
            return 1 - p;
        }
    }
    return -1;
}
