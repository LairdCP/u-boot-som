/*
 * Configuation settings for the WB40N CPU Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <linux/sizes.h>

#include <asm/hardware.h>

/* ARM asynchronous clock */
#define CONFIG_SYS_AT91_SLOW_CLOCK      32768
#define CONFIG_SYS_AT91_MAIN_CLOCK      18432000 /* from 18.432 MHz crystal */

#define CONFIG_CMDLINE_TAG	/* enable passing of ATAGs */
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG

#define CONFIG_SKIP_LOWLEVEL_INIT

/* general purpose I/O */
#define CONFIG_ATMEL_LEGACY	/* required until (g)pio is fixed */

/* serial console */
#define CONFIG_USART_BASE               ATMEL_BASE_DBGU
#define CONFIG_USART_ID                 ATMEL_ID_SYS

/* BOOTP options */
#define CONFIG_BOOTP_BOOTFILESIZE
#define CONFIG_BOOTP_BOOTPATH
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME

/* u-boot env in nand flash */
#define CONFIG_ENV_OFFSET               0xa0000
#define CONFIG_ENV_OFFSET_REDUND        0xc0000
#define CONFIG_ENV_SIZE                 SZ_128K
#define CONFIG_ENV_OVERWRITE

/* Command line configuration. */
#define CONFIG_SYS_LONGHELP
#define CONFIG_CMDLINE_EDITING
#define CONFIG_AUTO_COMPLETE

/* SDRAM */
#define CONFIG_NR_DRAM_BANKS            1
#define CONFIG_SYS_SDRAM_BASE           ATMEL_BASE_CS1
#define CONFIG_SYS_SDRAM_SIZE           SZ_32M

#define CONFIG_SYS_INIT_SP_ADDR \
    (ATMEL_BASE_SRAM1 + SZ_16K - GENERATED_GBL_DATA_SIZE)

/* NAND flash */
#define CONFIG_NAND_ATMEL
#define CONFIG_SYS_MAX_NAND_DEVICE      1
#define CONFIG_SYS_NAND_BASE            ATMEL_BASE_CS3
#define CONFIG_SYS_NAND_DBW_8
/* our ALE is AD21 */
#define CONFIG_SYS_NAND_MASK_ALE        (1 << 21)
/* our CLE is AD22 */
#define CONFIG_SYS_NAND_MASK_CLE        (1 << 22)
#define CONFIG_SYS_NAND_ENABLE_PIN      AT91_PIN_PC14
#define CONFIG_SYS_NAND_READY_PIN       AT91_PIN_PC13
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define CONFIG_SYS_NAND_USE_FLASH_BBT

#define CONFIG_MTD_DEVICE
#define CONFIG_MTD_PARTITIONS

/* System */
#define CONFIG_SYS_TEXT_BASE            0x21000000
#define CONFIG_SYS_LOAD_ADDR            (CONFIG_SYS_SDRAM_BASE + SZ_4K)

#define CONFIG_SYS_MALLOC_LEN           SZ_8M   /* Size of malloc() pool */
#define CONFIG_SYS_MONITOR_LEN          SZ_512K /* max u-boot size */

#ifdef CONFIG_BOOTCOUNT
#define CONFIG_BOOTCOUNT_ENV
#define CONFIG_BOOTCOUNT_LIMIT
#endif

/* Enable Watchdog in U-Boot */
#define CONFIG_AT91SAM9_WATCHDOG
#define CONFIG_AT91_HW_WDT_TIMEOUT	16
#define CONFIG_HW_WATCHDOG

#define CONFIG_EXTRA_ENV_SETTINGS       \
	"autoload=no\0" \
	"autostart=no\0" \
	"ethaddr=c0:ee:40:00:00:00\0"

#endif
