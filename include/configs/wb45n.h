/*
 * Configuation settings for the WB45N CPU Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <linux/sizes.h>
#include <asm/hardware.h>
#include <version.h>


/* ARM asynchronous clock */
#define CONFIG_SYS_AT91_SLOW_CLOCK      32768
#define CONFIG_SYS_AT91_MAIN_CLOCK      12000000 /* from 12 MHz crystal */

/* serial console */
#define CONFIG_USART_BASE               ATMEL_BASE_DBGU
#define CONFIG_USART_ID                 ATMEL_ID_SYS

/* SDRAM */
#define CONFIG_SYS_SDRAM_BASE           ATMEL_BASE_CS1
#define CONFIG_SYS_SDRAM_SIZE           SZ_64M

#define CONFIG_SYS_INIT_RAM_ADDR        ATMEL_BASE_SRAM
#define CONFIG_SYS_INIT_RAM_SIZE        SZ_32K

/* NAND flash */
#define CONFIG_SYS_MAX_NAND_DEVICE      1
#define CONFIG_SYS_NAND_BASE            ATMEL_BASE_CS3
#define CONFIG_SYS_NAND_DBW_8
/* our ALE is AD21 */
#define CONFIG_SYS_NAND_MASK_ALE        (1 << 21)
/* our CLE is AD22 */
#define CONFIG_SYS_NAND_MASK_CLE        (1 << 22)
#define CONFIG_SYS_NAND_ENABLE_PIN      AT91_PIN_PD4
#define CONFIG_SYS_NAND_READY_PIN       AT91_PIN_PD5

/* System */
#define CONFIG_SYS_MONITOR_LEN          SZ_512K /* max u-boot size */

#define CONFIG_EXTRA_ENV_SETTINGS       \
	"autoload=no\0" \
	"autostart=no\0" \
	"cdc_connect_timeout=15\0" \
	"version=" PLAIN_VERSION "\0" \
	"ethaddr=c0:ee:40:00:00:00\0"

#endif
