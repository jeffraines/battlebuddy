#ifndef BB_DICE_H
#define BB_DICE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name; /* "Coin", "d6", ... */
    uint8_t sides;    /* a coin has 2: 1 = heads, 2 = tails */
} bb_die_t;

size_t bb_dice_count(void);
const bb_die_t *bb_die_at(size_t index);

/* Returns a uniform roll in 1..sides using rand32 as the source of random
 * 32-bit values. Uses rejection sampling, so no face is favoured. */
uint8_t bb_dice_roll(uint8_t sides, uint32_t (*rand32)(void *ctx), void *ctx);

#ifdef __cplusplus
}
#endif

#endif
