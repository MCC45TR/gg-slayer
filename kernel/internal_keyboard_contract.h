/* SPDX-License-Identifier: MIT */
#ifndef SLAYER_INTERNAL_KEYBOARD_CONTRACT_H
#define SLAYER_INTERNAL_KEYBOARD_CONTRACT_H
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif
/* The exact R9T's soldered keyboard. Never select by a display name or by
 * eventN: USB, Bluetooth and userspace-created keyboards are excluded. */
static inline int slayer_internal_keyboard(unsigned int bus, const char *phys,
                                           const char *parent, const char *driver)
{
 return bus == 0x11 && phys && parent && driver &&
        !strcmp(phys, "isa0060/serio0/input0") &&
        !strcmp(parent, "serio0") && !strcmp(driver, "atkbd");
}
#endif
