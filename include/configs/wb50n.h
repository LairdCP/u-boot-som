/*
 * Configuration settings for the WB50 Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#ifndef __WB50N_CONFIG_H
#define __WB50N_CONFIG_H

#include "wb50n_common.h"

#define CONFIG_EXTRA_ENV_SETTINGS       \
	"autoload=no\0" \
	"autostart=no\0" \
	"cdc_connect_timeout=8\0" \
	"ethaddr=c0:ee:40:00:00:00\0"

#endif
