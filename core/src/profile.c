#include "bb/profile.h"

#include <string.h>

static const bb_profile_t profiles[] = {
    {.id = "riftbound", .name = "Riftbound", .family = "Riftbound", .format = "1v1",
     .start = 0, .min = 0, .max = 8, .win_at = 8, .lose_at = BB_NONE,
     .kind_count = 2, .kinds = {"Conquer", "Hold"}, .group = 0, .max_players = 4},
    /* Team play: one shared score per team. Target of 11 is unconfirmed. */
    {.id = "riftbound-2v2", .name = "Riftbound 2v2", .family = "Riftbound", .format = "2v2",
     .short_name = "Rift 2v2",
     .start = 0, .min = 0, .max = 11, .win_at = 11, .lose_at = BB_NONE,
     .kind_count = 2, .kinds = {"Conquer", "Hold"}, .group = 0, .max_players = 2, .teams = 1},
    {.id = "mtg", .name = "Magic", .family = "Magic", .format = "Standard",
     .start = 20, .min = -99, .max = 999, .win_at = BB_NONE, .lose_at = 0,
     .group = 1, .max_players = 4},
    {.id = "mtg-commander", .name = "Commander", .family = "Magic", .format = "Commander",
     .start = 40, .min = -99, .max = 999, .win_at = BB_NONE, .lose_at = 0,
     .group = 1, .max_players = 4},
    /* Memory gauge: 10 on YOU's side (-10) to 10 on OPP's side (+10). */
    {.id = "digimon", .name = "Digimon", .family = "Digimon",
     .start = 0, .min = -10, .max = 10, .win_at = BB_NONE, .lose_at = BB_NONE,
     .group = 1, .max_players = 2, .style = BB_STYLE_GAUGE},
    {.id = "yugioh", .name = "Yu-Gi-Oh!", .family = "Yu-Gi-Oh!",
     .start = 8000, .min = 0, .max = 32000, .win_at = BB_NONE, .lose_at = 0,
     .group = 1, .max_players = 4, .style = BB_STYLE_AMOUNT},
    {.id = "lorcana", .name = "Lorcana", .family = "Lorcana",
     .start = 0, .min = 0, .max = 99, .win_at = 20, .lose_at = BB_NONE,
     .group = 1, .max_players = 4},
    {.id = "fab", .name = "Flesh and Blood", .family = "Flesh and Blood", .short_name = "FaB",
     .start = 40, .min = 0, .max = 99, .win_at = BB_NONE, .lose_at = 0,
     .group = 1, .max_players = 4},
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

size_t bb_family_count(void)
{
    size_t n = 0;
    for (size_t i = 0; i < bb_profile_count(); i++) {
        if (bb_family_of(&profiles[i]) == n) {
            n++;
        }
    }
    return n;
}

/* Family index = number of distinct families before the profile's first appearance. */
size_t bb_family_of(const bb_profile_t *profile)
{
    size_t family = 0;
    for (size_t i = 0; i < bb_profile_count(); i++) {
        const char *f = profiles[i].family;
        if (strcmp(f, profile->family) == 0) {
            return family;
        }
        /* Count i as a new family only at its first appearance. */
        size_t j = 0;
        while (j < i && strcmp(profiles[j].family, f) != 0) {
            j++;
        }
        if (j == i) {
            family++;
        }
    }
    return family;
}

const char *bb_family_name(size_t family)
{
    for (size_t i = 0; i < bb_profile_count(); i++) {
        if (bb_family_of(&profiles[i]) == family) {
            return profiles[i].family;
        }
    }
    return NULL;
}

size_t bb_family_profiles(size_t family, size_t *out, size_t max)
{
    size_t n = 0;
    for (size_t i = 0; i < bb_profile_count() && n < max; i++) {
        if (bb_family_of(&profiles[i]) == family) {
            out[n++] = i;
        }
    }
    return n;
}
