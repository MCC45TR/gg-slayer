/* SPDX-License-Identifier: MIT */
#ifndef SLAYER_COLOR_CONTRACT_H
#define SLAYER_COLOR_CONTRACT_H
/* The measured white point is the maximum of each LED channel. All color
 * inputs retain their RGB meaning; round the per-channel product once. */
static inline unsigned char slayer_color_channel(unsigned char value, unsigned char white)
{
    return ((unsigned int)value * white + 127) / 255;
}
#endif
