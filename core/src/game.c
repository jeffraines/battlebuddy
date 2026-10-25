#include "bb/game.h"

#include <string.h>

void bb_game_init(bb_game_t *game, const bb_profile_t *profile)
{
    memset(game, 0, sizeof(*game));
    game->profile = profile;
    game->value = profile->start;
}

void bb_game_reset(bb_game_t *game)
{
    bb_game_init(game, game->profile);
}

int16_t bb_game_adjust(bb_game_t *game, int16_t delta, uint32_t now_ms)
{
    int32_t target = (int32_t)game->value + delta;

    if (target < game->profile->min) {
        target = game->profile->min;
    } else if (target > game->profile->max) {
        target = game->profile->max;
    }

    int16_t applied = (int16_t)(target - game->value);
    game->value = (int16_t)target;
    game->pending = (int16_t)(game->pending + applied);
    game->last_change_ms = now_ms;
    return applied;
}

bool bb_game_commit(bb_game_t *game)
{
    if (game->pending == 0) {
        return false;
    }

    game->history[game->history_head] = (bb_entry_t){game->pending, game->value};
    game->history_head = (uint16_t)((game->history_head + 1) % BB_HISTORY_LEN);
    if (game->history_count < BB_HISTORY_LEN) {
        game->history_count++;
    }
    game->pending = 0;
    return true;
}

bool bb_game_tick(bb_game_t *game, uint32_t now_ms)
{
    if (game->pending == 0 || (uint32_t)(now_ms - game->last_change_ms) < BB_COMMIT_MS) {
        return false;
    }
    return bb_game_commit(game);
}

bool bb_game_undo(bb_game_t *game)
{
    if (game->pending != 0) {
        game->value = (int16_t)(game->value - game->pending);
        game->pending = 0;
        return true;
    }
    if (game->history_count == 0) {
        return false;
    }

    game->history_head = (uint16_t)((game->history_head + BB_HISTORY_LEN - 1) % BB_HISTORY_LEN);
    game->history_count--;
    game->value = (int16_t)(game->value - game->history[game->history_head].delta);
    return true;
}

bb_status_t bb_game_status(const bb_game_t *game)
{
    const bb_profile_t *p = game->profile;

    if (p->win_at != BB_NONE && game->value >= p->win_at) {
        return BB_STATUS_WON;
    }
    if (p->lose_at != BB_NONE && game->value <= p->lose_at) {
        return BB_STATUS_LOST;
    }
    return BB_STATUS_PLAYING;
}

size_t bb_game_history(const bb_game_t *game, bb_entry_t *out, size_t max)
{
    size_t n = game->history_count < max ? game->history_count : max;

    for (size_t i = 0; i < n; i++) {
        size_t idx = (game->history_head + BB_HISTORY_LEN - 1 - i) % BB_HISTORY_LEN;
        out[i] = game->history[idx];
    }
    return n;
}
