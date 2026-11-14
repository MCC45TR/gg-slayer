/* SPDX-License-Identifier: MIT */
#ifndef SLAYER_BRIGHTNESS_CONTRACT_H
#define SLAYER_BRIGHTNESS_CONTRACT_H
/* The target ITE controller's level 1 is optically off. Its first visible
 * level is 2/50 (4%), confirmed on hardware. Interpolate the full fade range
 * between that level and the selected level; logical zero stays off. */
static inline unsigned int slayer_effective_brightness(unsigned int logical, unsigned int scale)
{
    unsigned int upper;
    if (!logical) return 0;
    if (scale < 1) scale = 1;
    if (scale > 1000) scale = 1000;
    upper = logical < 2 ? 2 : logical;
    return 2 + ((upper - 2) * (scale - 1) + 499) / 999;
}
#endif
