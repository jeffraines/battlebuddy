#ifndef BB_GAME_H
#define BB_GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bb/profile.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BB_MAX_PLAYERS 2
#define BB_HISTORY_LEN 64

/* Rapid taps are grouped: changes to the same player and kind made within
 * this window of each other become one history entry (five taps of -1 are
 * recorded as -5). */
#define BB_COMMIT_MS 1500

typedef struct {
    uint8_t player;
    uint8_t kind;  /* index into profile->kinds; 0 when the profile has none */
    int16_t delta; /* change applied */
    int16_t value; /* player's value after the change */
} bb_entry_t;

typedef enum {
    BB_STATUS_PLAYING,
    BB_STATUS_WON,
    BB_STATUS_LOST,
} bb_status_t;

typedef struct {
    const bb_profile_t *profile;
    uint8_t players;
    int16_t value[BB_MAX_PLAYERS];
    int16_t tally[BB_MAX_PLAYERS][BB_MAX_KINDS]; /* net change per kind */
    bb_entry_t pending; /* not yet in history; pending.delta == 0 means none */
    uint32_t last_change_ms;
    bb_entry_t history[BB_HISTORY_LEN]; /* ring buffer */
    uint16_t history_count;
    uint16_t history_head; /* index of the next write */
} bb_game_t;

/* players is clamped to 1..BB_MAX_PLAYERS. */
void bb_game_init(bb_game_t *game, const bb_profile_t *profile, uint8_t players);
void bb_game_reset(bb_game_t *game);

/* Applies delta to a player's value, clamped to the profile's range. kind is
 * ignored when the profile has no kinds. Returns the change actually applied. */
int16_t bb_game_adjust(bb_game_t *game, uint8_t player, uint8_t kind, int16_t delta,
                       uint32_t now_ms);

/* Commits the pending change once BB_COMMIT_MS has passed since the last
 * adjustment. Call regularly. Returns true if an entry was committed. */
bool bb_game_tick(bb_game_t *game, uint32_t now_ms);

/* Commits the pending change immediately. Returns true if one was committed. */
bool bb_game_commit(bb_game_t *game);

/* Reverts the pending change, or else the latest history entry. If reverted
 * is not NULL it receives what was undone. Returns false if there was
 * nothing to undo. */
bool bb_game_undo(bb_game_t *game, bb_entry_t *reverted);

bb_status_t bb_game_status(const bb_game_t *game, uint8_t player);

/* Copies up to max entries into out, newest first. Returns the number copied. */
size_t bb_game_history(const bb_game_t *game, bb_entry_t *out, size_t max);

#ifdef __cplusplus
}
#endif

#endif
