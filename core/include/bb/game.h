#ifndef BB_GAME_H
#define BB_GAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bb/profile.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BB_HISTORY_LEN 64

/* Rapid taps are grouped: changes made within this window of each other
 * become one history entry (e.g. five taps of -1 are recorded as -5). */
#define BB_COMMIT_MS 1500

typedef struct {
    int16_t delta; /* change applied */
    int16_t value; /* value after the change */
} bb_entry_t;

typedef enum {
    BB_STATUS_PLAYING,
    BB_STATUS_WON,
    BB_STATUS_LOST,
} bb_status_t;

typedef struct {
    const bb_profile_t *profile;
    int16_t value;
    int16_t pending; /* change not yet committed to history */
    uint32_t last_change_ms;
    bb_entry_t history[BB_HISTORY_LEN]; /* ring buffer */
    uint16_t history_count;
    uint16_t history_head; /* index of the next write */
} bb_game_t;

void bb_game_init(bb_game_t *game, const bb_profile_t *profile);
void bb_game_reset(bb_game_t *game);

/* Applies delta, clamped to the profile's range. Returns the change that was
 * actually applied. */
int16_t bb_game_adjust(bb_game_t *game, int16_t delta, uint32_t now_ms);

/* Commits the pending change once BB_COMMIT_MS has passed since the last
 * adjustment. Call regularly. Returns true if an entry was committed. */
bool bb_game_tick(bb_game_t *game, uint32_t now_ms);

/* Commits the pending change immediately. Returns true if one was committed. */
bool bb_game_commit(bb_game_t *game);

/* Reverts the pending change, or else the latest history entry. Returns false
 * if there was nothing to undo. */
bool bb_game_undo(bb_game_t *game);

bb_status_t bb_game_status(const bb_game_t *game);

/* Copies up to max entries into out, newest first. Returns the number copied. */
size_t bb_game_history(const bb_game_t *game, bb_entry_t *out, size_t max);

#ifdef __cplusplus
}
#endif

#endif
