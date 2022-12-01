/*
 * Configuration settings for the SOM60x2 Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __SOM60X2_CONFIG_H
#define __SOM60X2_CONFIG_H

#include "som6050_common.h"

#include "som60_common.h"

#define CONFIG_EXTRA_ENV_SETTINGS DEFAULT_ENV_SETTINGS \
	"_formatubi=nand erase.part ubi;" \
		"ubi part ubi;" \
		"for part in a b; do " \
			"ubi create kernel_${part} C00000 dynamic;" \
			"ubi create rootfs_${part} B000000 dynamic;" \
			"ubi create rootfs_data_${part} 1400000 dynamic;" \
		"done;" \
		"ubi create perm 4335000 dynamic\0"

#define SOM_LEGACY_RAM_IC               MT29C4G48MAYBBAMR_48

#endif // __SOM60X2_CONFIG_H
