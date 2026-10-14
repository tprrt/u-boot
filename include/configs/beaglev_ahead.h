/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2023 Yixun Lan <dlan@gentoo.org>
 * Copyright (c) 2026 Thomas Perrot <thomas.perrot@bootlin.com>
 */

#ifndef __BEAGLEV_AHEAD_H
#define __BEAGLEV_AHEAD_H

#include <linux/sizes.h>

#define CFG_SYS_NS16550_CLK		100000000
#define CFG_SYS_SDRAM_BASE		0x00000000

/* Environment options */

#define CFG_EXTRA_ENV_SETTINGS \
	"PS1=[BeagleV-Ahead]# \0"

#endif /* __BEAGLEV_AHEAD_H */
