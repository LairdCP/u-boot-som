/*
 * Configuration settings for the SOM60, WB50 Modules.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __SOM6050_CONFIG_H
#define __SOM6050_CONFIG_H

#include <linux/sizes.h>

#include <asm/arch/sama5d3.h>

/* ARM asynchronous clock */
#define CONFIG_SYS_AT91_SLOW_CLOCK      32768
#define CONFIG_SYS_AT91_MAIN_CLOCK      12000000 /* from 12 MHz crystal */

/* PCK = 528MHz, MCK = 132MHz */
#define BOARD_PLLA_SETTINGS (\
	AT91_PMC_PLLAR_29 |\
	AT91_PMC_PLLXR_PLLCOUNT(0x3f) |\
	AT91_PMC_PLLXR_MUL(43) |\
	AT91_PMC_PLLXR_DIV(1))

#define BOARD_PRESCALER_PLLA \
	(AT91_PMC_MCKR_MDIV_4 | AT91_PMC_MCKR_CSS_PLLA)

/* serial console */
#define CONFIG_USART_BASE               ATMEL_BASE_DBGU
#define CONFIG_USART_ID                 ATMEL_ID_SYS

/* SDRAM */
#define CONFIG_NR_DRAM_BANKS            1
#define CONFIG_SYS_SDRAM_BASE           ATMEL_BASE_DDRCS

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SYS_INIT_SP_ADDR         (ATMEL_BASE_SRAM1 + SZ_32K - 4)
#else
#define CONFIG_SYS_INIT_SP_ADDR \
	(ATMEL_BASE_SRAM1 + SZ_32K - GENERATED_GBL_DATA_SIZE)
#endif

/* NAND flash */
#define CONFIG_SYS_MAX_NAND_DEVICE      1
#define CONFIG_SYS_NAND_BASE            ATMEL_BASE_CS3
#define CONFIG_SYS_NAND_DBW_8
/* our ALE is AD21 */
#define CONFIG_SYS_NAND_MASK_ALE        (1 << 21)
/* our CLE is AD22 */
#define CONFIG_SYS_NAND_MASK_CLE        (1 << 22)

/* System */
#define CONFIG_SYS_CBSIZE               1024
#define CONFIG_SYS_BARGSIZE             CONFIG_SYS_CBSIZE
#define CONFIG_SYS_MAXARGS              32

#define CONFIG_SYS_MONITOR_LEN          SZ_512K /* max u-boot size */
#ifndef CONFIG_SPL_BUILD
#define CONFIG_SYS_BOOTM_LEN	        SZ_16M
#endif

#ifdef CONFIG_I2C_EEPROM
#define SYS_I2C_MAC_OFFSET		0
#endif

/* SPL */
#define CONFIG_SPL_MAX_SIZE             0x10000

#define CONFIG_SPL_BSS_START_ADDR       ATMEL_BASE_SRAM1 + SZ_32K
#define CONFIG_SPL_BSS_MAX_SIZE         SZ_32K

#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define CONFIG_SPL_FS_LOAD_PAYLOAD_NAME	"u-boot.itb"

#define DEFAULT_ENV_SETTINGS \
	"autoload=no\0" \
	"autostart=no\0" \
	"cdc_connect_timeout=15\0" \
	"bootside=a\0"

#ifndef __ASSEMBLY__
/* Define hook for custom board initialization */
void som60_custom_hw_init(void);
#endif

#endif
