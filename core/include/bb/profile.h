#ifndef BB_PROFILE_H
#define BB_PROFILE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sentinel for "no threshold" in win_at / lose_at. */
#define BB_NONE INT16_MIN

/* One game's counter rules. Profiles are plain data so new games can be
 * added (and later loaded over BLE) without changing the counter logic. */
typedef struct {
    const char *id;
    const char *name;
    int16_t start;
    int16_t min;
    int16_t max;
    int16_t win_at;  /* reaching this value (from below) wins, or BB_NONE */
    int16_t lose_at; /* reaching this value (from above) loses, or BB_NONE */
} bb_profile_t;

size_t bb_profile_count(void);
const bb_profile_t *bb_profile_at(size_t index);
const bb_profile_t *bb_profile_find(const char *id);

#ifdef __cplusplus
}
#endif

#endif
