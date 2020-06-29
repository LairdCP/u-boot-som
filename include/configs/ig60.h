/*
 * Configuation settings for the IG60 Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __IG60_CONFIG_H
#define __IG60_CONFIG_H

/* The IG60 inherits most of the SOM60 configuration */
#include "som60x2.h"

#ifdef CONFIG_NAND_BOOT
#undef CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_TEXT_BASE            0x26f00000
#endif

/*
 * DO NOT CHANGE - The IG60 must use the legacy load address
 * since the boot command does not specify an address in bootm
 */
#undef CONFIG_SYS_LOAD_ADDR
#define CONFIG_SYS_LOAD_ADDR		0x21000000

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

/* BOOTCOUNT */
#define CONFIG_BOOTCOUNT_ENV
#define CONFIG_BOOTCOUNT_LIMIT

/* u-boot env in nand flash */
#undef CONFIG_ENV_OFFSET
#undef CONFIG_ENV_OFFSET_REDUND
#define CONFIG_ENV_OFFSET               0xA0000
#define CONFIG_ENV_OFFSET_REDUND        0xC0000

#endif
