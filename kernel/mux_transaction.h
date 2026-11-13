/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SLAYER_MUX_TRANSACTION_H
#define SLAYER_MUX_TRANSACTION_H
#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/string.h>
#else
#include <errno.h>
#include <string.h>
#endif
#include "firmware_contract.h"

/* Callbacks operate on the fixed, validated native schema. The kernel caller
 * holds both its device mutex and the global efivar lock for this transaction.
 * Shared with host fault-injection tests; no raw variable/address input. */
struct slayer_mux_ops {
	int (*read)(void *context, unsigned char *data);
	int (*write)(void *context, unsigned char *data);
};
static inline int slayer_mux_update(unsigned int mode,
				   const struct slayer_mux_ops *ops, void *context)
{
	unsigned char desired[SLAYER_OEM_SIZE], live[SLAYER_OEM_SIZE], old;
	int ret, write_error, read_error;

	if (mode > 2)
		return -EINVAL;
	ret = ops->read(context, desired);
	if (ret)
		return ret;
	old = desired[SLAYER_MUX_OFFSET];
	if (old == mode)
		return 0;
	desired[SLAYER_MUX_OFFSET] = mode;
	write_error = ops->write(context, desired);
	read_error = ops->read(context, live);
	if (!write_error && !read_error && !memcmp(desired, live, sizeof(live)))
		return 0;
	ret = write_error ? write_error : (read_error ? read_error : -EIO);

	/* A failed store can already have committed. Read before rollback and
	 * never write a stale whole object or a redundant old-mode value. */
	if (read_error && ops->read(context, live))
		return -EUCLEAN;
	if (live[SLAYER_MUX_OFFSET] == old)
		return ret;
	live[SLAYER_MUX_OFFSET] = old;
	ops->write(context, live);
	if (ops->read(context, live) || live[SLAYER_MUX_OFFSET] != old)
		return -EUCLEAN;
	return ret;
}
#endif
