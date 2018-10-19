/*
 * Configuation settings for the IG60 Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

/* The IG60 inherits most of the SOM60 configuration */
#include "som60.h"

/* Customize IG60 settings only */
#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS \
	"bootside=a\0"						\
	"_setbootvol=if test ${bootside} = a;then "	\
			"setenv bootvol 1;"			\
		"else "							\
			"setenv bootvol 4;"			\
		"fi\0"							\
	"_formatubi=nand erase.part ubi;"							\
		"ubi part ubi;"											\
		"for part in a b; do "									\
			"ubi create kernel_${part} 800000 static;"			\
			"ubi create rootfs_${part} 5200000 static;"			\
			"ubi create rootfs_data_${part} 1E00000 dynamic;"	\
		"done;" \
		"ubi create rodata 1400000 dynamic;" \
		"ubi create ig 8e00000 dynamic\0"

/*Enable Watchdog in UBoot*/
#define CONFIG_AT91SAM9_WATCHDOG
#define CONFIG_AT91_HW_WDT_TIMEOUT	16
#if !defined(CONFIG_SPL_BUILD)
#define CONFIG_HW_WATCHDOG
#endif

/* BOOTCOUNT */
#define CONFIG_BOOTCOUNT_ENV
#define CONFIG_BOOTCOUNT_LIMIT

