/*
 * Configuation settings for the IG60 Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __IG60_CONFIG_H
#define __IG60_CONFIG_H

/* The IG60 inherits most of the SOM60 configuration */
#include "som60x2.h"

/* Customize IG60 settings only */
#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS DEFAULT_ENV_SETTINGS \
	"_setbootvol=if test ${bootside} = a; then " \
		"setenv bootvol 1;" \
	"else " \
		"setenv bootvol 4;" \
	"fi\0" \
	"_formatubi=nand erase.part ubi;" \
		"ubi part ubi;" \
		"for part in a b; do " \
			"ubi create kernel_${part} 800000 dynamic;" \
			"ubi create rootfs_${part} 5200000 dynamic;" \
			"ubi create rootfs_data_${part} 1E00000 dynamic;" \
		"done;" \
		"ubi create rodata 800000 dynamic;" \
		"ubi create ig 8e00000 dynamic;" \
		"ubi create perm 1800000 dynamic\0"

#endif
