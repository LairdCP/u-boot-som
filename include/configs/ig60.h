/*
 * Configuation settings for the IG60 Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __IG60_CONFIG_H
#define __IG60_CONFIG_H

/* The IG60 inherits most of the SOM60 configuration */
#include "som60.h"

/* Customize IG60 settings only */
#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS DEFAULT_ENV_SETTINGS \
	"_setbootvol=if test ${bootside} = a; then " \
		"setenv bootvol 1;" \
	"else " \
		"setenv bootvol 4;" \
	"fi\0" \

#endif
