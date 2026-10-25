#include "bb/profile.h"

#include <string.h>

static const bb_profile_t profiles[] = {
    {"riftbound", "Riftbound", 0, 0, 8, 8, BB_NONE},
    {"mtg", "Magic", 20, -99, 999, BB_NONE, 0},
    {"mtg-commander", "Commander", 40, -99, 999, BB_NONE, 0},
    {"lorcana", "Lorcana", 0, 0, 99, 20, BB_NONE},
    {"fab", "Flesh and Blood", 40, 0, 99, BB_NONE, 0},
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
