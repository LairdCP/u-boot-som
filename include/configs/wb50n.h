/*
 * Configuration settings for the WB50 Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#ifndef __WB50N_CONFIG_H
#define __WB50N_CONFIG_H

#include "wb50n_common.h"

#undef  CONFIG_ENV_OFFSET
#define CONFIG_ENV_OFFSET               0xa0000

#undef  CONFIG_ENV_OFFSET_REDUND
#define CONFIG_ENV_OFFSET_REDUND        0xc0000

#undef  CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_TEXT_BASE            0x23f00000

#undef  CONFIG_SYS_LOAD_ADDR
#define CONFIG_SYS_LOAD_ADDR            (CONFIG_SYS_SDRAM_BASE + SZ_32M)

#define CONFIG_EXTRA_ENV_SETTINGS       \
	"autoload=no\0" \
	"autostart=no\0" \
	"ethaddr=c0:ee:40:00:00:00\0"

#endif
