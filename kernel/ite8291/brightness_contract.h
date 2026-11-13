/* SPDX-License-Identifier: MIT */
#ifndef SLAYER_BRIGHTNESS_CONTRACT_H
#define SLAYER_BRIGHTNESS_CONTRACT_H
/* Logical off stays off. Every positive logical level has a visible minimum. */
static inline unsigned int slayer_effective_brightness(unsigned int logical, unsigned int scale)
{
    unsigned int value;
    if (!logical) return 0;
    value = (logical * scale + 500) / 1000;
    return value ? value : 1;
}
#endif
