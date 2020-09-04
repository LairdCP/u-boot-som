/*
 * Configuration settings for the WB50N Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#ifndef __WB50N_SYSD_CONFIG_H
#define __WB50N_SYSD_CONFIG_H

#include "wb50n_common.h"

#define CONFIG_EXTRA_ENV_SETTINGS DEFAULT_ENV_SETTINGS \
	"_formatubi=nand erase.part ubi;" \
		"ubi part ubi;" \
		"for part in a b; do " \
			"ubi create kernel_${part}  800000 dynamic;" \
			"ubi create rootfs_${part} 2C50000 dynamic;" \
			"ubi create rootfs_data_${part} 400000 dynamic;" \
		"done;" \
		"ubi create perm 5EF000 dynamic\0"

#endif
