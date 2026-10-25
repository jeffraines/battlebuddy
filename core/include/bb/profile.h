#ifndef BB_PROFILE_H
#define BB_PROFILE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sentinel for "no threshold" in win_at / lose_at. */
#define BB_NONE INT16_MIN

/* Most kinds of score change a profile can name (e.g. Riftbound's Conquer and Hold). */
#define BB_MAX_KINDS 2

/* How a game's counters are changed on screen. */
typedef enum {
    BB_STYLE_COUNTER = 0, /* tap +/- (or kind buttons when the profile has kinds) */
    BB_STYLE_AMOUNT = 1,  /* type an amount on a number pad (e.g. Yu-Gi-Oh! LP) */
    BB_STYLE_GAUGE = 2,   /* one shared marker between two sides (Digimon memory);
                             value[0] < 0 is YOU's side, > 0 is OPP's side */
} bb_style_t;

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
    uint8_t kind_count;              /* 0 = changes have no kind */
    const char *kinds[BB_MAX_KINDS]; /* names of the kinds, e.g. "Conquer" */
    uint8_t group;                   /* 1 = merge rapid changes into one history entry */
    uint8_t max_players;             /* most counters shown at once (1..BB_MAX_PLAYERS) */
    uint8_t teams;                   /* 1 = each counter is a team, not a player */
    const char *family;              /* game the profile belongs to, e.g. "Magic" */
    const char *format;              /* format within the game, or NULL if it's the only one */
    const char *short_name;          /* for tight spots (menu tiles), or NULL to use name */
    uint8_t style;                   /* bb_style_t */
} bb_profile_t;

size_t bb_profile_count(void);
const bb_profile_t *bb_profile_at(size_t index);
const bb_profile_t *bb_profile_find(const char *id);
/* Returns the profile's position in the table, or bb_profile_count() if unknown. */
size_t bb_profile_index(const bb_profile_t *profile);

/* Games, in table order, each with one or more formats (profiles). */
size_t bb_family_count(void);
const char *bb_family_name(size_t family);
/* Fills out with the profile indices of a game's formats; returns how many. */
size_t bb_family_profiles(size_t family, size_t *out, size_t max);
/* The game a profile belongs to. */
size_t bb_family_of(const bb_profile_t *profile);

#ifdef __cplusplus
}
#endif

#endif
