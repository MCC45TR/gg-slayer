/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SLAYER_THERMAL_PROFILE_H
#define SLAYER_THERMAL_PROFILE_H
/* Target DSDT PMSC: 0751[4]=TBME, [7]=UFME. Bit 5 is a separate fan-table
 * flag: live firmware hotkey produced Office=a0 -> Performance=30. Preserve
 * that flag, Fan Boost (bit 6), and every other unrelated bit on all writes. */
#define SLAYER_THERMAL_MASK 0x90
static inline int slayer_thermal_decode(unsigned char value)
{
    switch (value & SLAYER_THERMAL_MASK) {
    case 0x80: return 0;
    case 0x00: return 1;
    case 0x10: return 2;
    default: return -1;
    }
}
static inline int slayer_thermal_encode(unsigned char previous, unsigned int mode,
                                        unsigned char *next)
{
    static const unsigned char bits[] = {0x80, 0x00, 0x10};
    if (mode > 2 || !next) return -1;
    *next = (previous & ~SLAYER_THERMAL_MASK) | bits[mode];
    return 0;
}
#endif
