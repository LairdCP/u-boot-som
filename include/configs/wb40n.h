/*
 * Configuation settings for the WB40N CPU Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <linux/sizes.h>
#include <asm/hardware.h>
#include <version.h>

/* ARM asynchronous clock */
#define CFG_SYS_AT91_SLOW_CLOCK      32768
#define CFG_SYS_AT91_MAIN_CLOCK      18432000 /* from 18.432 MHz crystal */

/* serial console */
#define CFG_USART_BASE               ATMEL_BASE_DBGU
#define CFG_USART_ID                 ATMEL_ID_SYS

/* SDRAM */
#define CFG_SYS_SDRAM_BASE           ATMEL_BASE_CS1
#define CFG_SYS_SDRAM_SIZE           SZ_32M

#define CFG_SYS_INIT_RAM_ADDR        ATMEL_BASE_SRAM1
#define CFG_SYS_INIT_RAM_SIZE        SZ_16K

/* NAND flash */
#define CFG_SYS_NAND_BASE            ATMEL_BASE_CS3
/* our ALE is AD21 */
#define CFG_SYS_NAND_MASK_ALE        (1 << 21)
/* our CLE is AD22 */
#define CFG_SYS_NAND_MASK_CLE        (1 << 22)
#define CFG_SYS_NAND_ENABLE_PIN      AT91_PIN_PC14
#define CFG_SYS_NAND_READY_PIN       AT91_PIN_PC13

#define CFG_EXTRA_ENV_SETTINGS       \
	"autoload=no\0" \
	"autostart=no\0" \
	"cdc_connect_timeout=15\0" \
	"version=" PLAIN_VERSION "\0" \
	"ethaddr=c0:ee:40:00:00:00\0"

#endif
