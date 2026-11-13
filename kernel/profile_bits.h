/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SLAYER_PROFILE_BITS_H
#define SLAYER_PROFILE_BITS_H
/* Shared with the exhaustive user-space contract test. This is the complete
 * write-value construction: six unrelated EC flag bits are never altered. */
static inline int slayer_profile_bits(unsigned char old, unsigned int profile, unsigned char *next)
{
	if (profile > 2 || ((old >> 4) & 3) > 2)
		return -1;
	*next = (old & 0xcf) | (profile << 4);
	return 0;
}
#endif
