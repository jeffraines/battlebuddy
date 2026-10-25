#include "bb/game.h"

#include <string.h>

void bb_game_init(bb_game_t *game, const bb_profile_t *profile, uint8_t players)
{
    uint8_t max = profile->max_players;
    if (max < 1 || max > BB_MAX_PLAYERS) {
        max = BB_MAX_PLAYERS;
    }
    if (players < 1) {
        players = 1;
    } else if (players > max) {
        players = max;
    }

    memset(game, 0, sizeof(*game));
    game->profile = profile;
    game->players = players;
    for (uint8_t p = 0; p < players; p++) {
        game->value[p] = profile->start;
    }
}

void bb_game_reset(bb_game_t *game)
{
    bb_game_init(game, game->profile, game->players);
}

static void apply(bb_game_t *game, uint8_t player, uint8_t kind, int16_t delta)
{
    game->value[player] = (int16_t)(game->value[player] + delta);
    if (game->profile->kind_count > 0) {
        game->tally[player][kind] = (int16_t)(game->tally[player][kind] + delta);
    }
}

int16_t bb_game_adjust(bb_game_t *game, uint8_t player, uint8_t kind, int16_t delta,
                       uint32_t now_ms)
{
    if (player >= game->players) {
        return 0;
    }
    if (kind >= game->profile->kind_count) {
        kind = 0;
    }

    /* A change to a different player or kind starts a new history entry. */
    if (game->pending.delta != 0 &&
        (game->pending.player != player || game->pending.kind != kind)) {
        bb_game_commit(game);
    }

    int32_t target = (int32_t)game->value[player] + delta;
    if (target < game->profile->min) {
        target = game->profile->min;
    } else if (target > game->profile->max) {
        target = game->profile->max;
    }

    int16_t applied = (int16_t)(target - game->value[player]);
    apply(game, player, kind, applied);
    game->pending.player = player;
    game->pending.kind = kind;
    game->pending.delta = (int16_t)(game->pending.delta + applied);
    game->pending.value = game->value[player];
    game->last_change_ms = now_ms;
    if (!game->profile->group) {
        bb_game_commit(game);
    }
    return applied;
}

bool bb_game_commit(bb_game_t *game)
{
    if (game->pending.delta == 0) {
        return false;
    }

    game->history[game->history_head] = game->pending;
    game->history_head = (uint16_t)((game->history_head + 1) % BB_HISTORY_LEN);
    if (game->history_count < BB_HISTORY_LEN) {
        game->history_count++;
    }
    game->pending.delta = 0;
    return true;
}

bool bb_game_tick(bb_game_t *game, uint32_t now_ms)
{
    if (game->pending.delta == 0 || (uint32_t)(now_ms - game->last_change_ms) < BB_COMMIT_MS) {
        return false;
    }
    return bb_game_commit(game);
}

bool bb_game_undo(bb_game_t *game, bb_entry_t *reverted)
{
    bb_entry_t e;

    if (game->pending.delta != 0) {
        e = game->pending;
        game->pending.delta = 0;
    } else if (game->history_count > 0) {
        game->history_head = (uint16_t)((game->history_head + BB_HISTORY_LEN - 1) % BB_HISTORY_LEN);
        game->history_count--;
        e = game->history[game->history_head];
    } else {
        return false;
    }

    apply(game, e.player, e.kind, (int16_t)-e.delta);
    if (reverted) {
        *reverted = e;
    }
    return true;
}

bb_status_t bb_game_status(const bb_game_t *game, uint8_t player)
{
    const bb_profile_t *p = game->profile;
    int16_t v = game->value[player];

    if (p->win_at != BB_NONE && v >= p->win_at) {
        return BB_STATUS_WON;
    }
    if (p->lose_at != BB_NONE && v <= p->lose_at) {
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

uint8_t bb_gauge_owner(const bb_game_t *game)
{
    if (game->value[0] != 0) {
        return game->value[0] < 0 ? 0 : 1;
    }
    /* The pending change (if any) produced the current 0; look at what came before. */
    for (uint16_t i = 0; i < game->history_count; i++) {
        size_t idx = (game->history_head + BB_HISTORY_LEN - 1 - i) % BB_HISTORY_LEN;
        int16_t v = game->history[idx].value;
        if (v != 0) {
            return v < 0 ? 0 : 1;
        }
    }
    return 0;
}
