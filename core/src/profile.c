#include "bb/profile.h"

#include <string.h>

static const bb_profile_t profiles[] = {
    /* id, name, start, min, max, win_at, lose_at, kinds, group, max_players, teams */
    {"riftbound", "Riftbound", 0, 0, 8, 8, BB_NONE, 2, {"Conquer", "Hold"}, 0, 4, 0},
    /* Team play: one shared score per team. Target of 11 is unconfirmed. */
    {"riftbound-2v2", "Riftbound 2v2", 0, 0, 11, 11, BB_NONE, 2, {"Conquer", "Hold"}, 0, 2, 1},
    {"mtg", "Magic", 20, -99, 999, BB_NONE, 0, 0, {NULL}, 1, 4, 0},
    {"mtg-commander", "Commander", 40, -99, 999, BB_NONE, 0, 0, {NULL}, 1, 4, 0},
    {"lorcana", "Lorcana", 0, 0, 99, 20, BB_NONE, 0, {NULL}, 1, 4, 0},
    {"fab", "Flesh and Blood", 40, 0, 99, BB_NONE, 0, 0, {NULL}, 1, 4, 0},
};

size_t bb_profile_count(void)
{
    return sizeof(profiles) / sizeof(profiles[0]);
}

const bb_profile_t *bb_profile_at(size_t index)
{
    return index < bb_profile_count() ? &profiles[index] : NULL;
}

const bb_profile_t *bb_profile_find(const char *id)
{
    for (size_t i = 0; i < bb_profile_count(); i++) {
        if (strcmp(profiles[i].id, id) == 0) {
            return &profiles[i];
        }
    }
    return NULL;
}

size_t bb_profile_index(const bb_profile_t *profile)
{
    for (size_t i = 0; i < bb_profile_count(); i++) {
        if (&profiles[i] == profile) {
            return i;
        }
    }
    return bb_profile_count();
}
