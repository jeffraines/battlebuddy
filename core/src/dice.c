#include "bb/dice.h"

static const bb_die_t dice[] = {
    {"Coin", 2}, {"d4", 4}, {"d6", 6}, {"d8", 8}, {"d10", 10}, {"d12", 12}, {"d20", 20},
};

size_t bb_dice_count(void)
{
    return sizeof(dice) / sizeof(dice[0]);
}

const bb_die_t *bb_die_at(size_t index)
{
    return index < bb_dice_count() ? &dice[index] : NULL;
}

uint8_t bb_dice_roll(uint8_t sides, uint32_t (*rand32)(void *ctx), void *ctx)
{
    if (sides < 2) {
        return 1;
    }
    /* Largest multiple of sides that fits in 32 bits; values at or above it
     * would make the low faces slightly more likely, so draw again. */
    uint32_t limit = UINT32_MAX - (UINT32_MAX % sides);
    uint32_t r;
    do {
        r = rand32(ctx);
    } while (r >= limit);
    return (uint8_t)(r % sides + 1);
}
