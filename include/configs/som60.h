/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Configuration settings for the SOM60 Module.
 */

#ifndef __SOM60_CONFIG_H
#define __SOM60_CONFIG_H

#include "som6050_common.h"
#include "som60_common.h"

#define CONFIG_EXTRA_ENV_SETTINGS DEFAULT_ENV_SETTINGS \
	"bootside=a\0"

#define SOM_LEGACY_RAM_IC               MT29C2G24MAAAAKAMD_5

#endif // __SOM60_CONFIG_H
