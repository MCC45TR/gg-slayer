/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SLAYER_FIRMWARE_CONTRACT_H
#define SLAYER_FIRMWARE_CONTRACT_H
#define SLAYER_OEM_SIZE 180
#define SLAYER_MUX_OFFSET 0x62
/* Native firmware schema, independently recovered from OemDisplayModeDxe.
 * Unrelated settings are opaque and must survive every MUX operation. */
static inline int slayer_oem_valid(const unsigned char *data, unsigned long size,
                                  unsigned int attributes)
{
	return data && size == SLAYER_OEM_SIZE && attributes == 7 &&
		data[0] == 0x05 && data[1] == 0x1d &&
		data[2] == 0x5c && data[3] == 0x60 &&
		data[6] == 26 && data[7] == 16 && data[SLAYER_MUX_OFFSET] <= 2;
}
static inline unsigned char slayer_mask_bits(unsigned char old,
                                             unsigned char mask,
                                             unsigned char value)
{
	return (old & ~mask) | (value & mask);
}
#endif
