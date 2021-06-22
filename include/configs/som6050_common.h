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

#define CONFIG_ARCH_CPU_INIT

#define CONFIG_CMDLINE_TAG	/* enable passing of ATAGs */
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG

#ifndef CONFIG_SPL_BUILD
#define CONFIG_SKIP_LOWLEVEL_INIT
#endif
#define CONFIG_DISABLE_IMAGE_LEGACY

/* serial console */
#define CONFIG_USART_BASE               ATMEL_BASE_DBGU
#define CONFIG_USART_ID                 ATMEL_ID_SYS

/* BOOTP options */
#define CONFIG_BOOTP_BOOTFILESIZE
#define CONFIG_BOOTP_BOOTPATH
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME

/* u-boot env in nand flash */
#define CONFIG_ENV_OFFSET               0x120000
#define CONFIG_ENV_OFFSET_REDUND        0x140000
#define CONFIG_ENV_SIZE                 SZ_128K
#define CONFIG_ENV_OVERWRITE

/* Command line configuration. */
#define CONFIG_SYS_LONGHELP
#define CONFIG_CMDLINE_EDITING
#define CONFIG_AUTO_COMPLETE

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
#define CONFIG_NAND_ATMEL
#define CONFIG_SYS_MAX_NAND_DEVICE      1
#define CONFIG_SYS_NAND_BASE            ATMEL_BASE_CS3
#define CONFIG_SYS_NAND_DBW_8
/* our ALE is AD21 */
#define CONFIG_SYS_NAND_MASK_ALE        (1 << 21)
/* our CLE is AD22 */
#define CONFIG_SYS_NAND_MASK_CLE        (1 << 22)
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define CONFIG_SYS_NAND_USE_FLASH_BBT

/* PMECC & PMERRLOC */
#define CONFIG_ATMEL_NAND_HWECC
#define CONFIG_ATMEL_NAND_HW_PMECC
#define CONFIG_PMECC_CAP                8
#define CONFIG_PMECC_SECTOR_SIZE        512

#define CONFIG_MTD_DEVICE
#define CONFIG_MTD_PARTITIONS

/* System */
#define CONFIG_SYS_TEXT_BASE            0x21000000 /* (CONFIG_SYS_SDRAM_BASE + SZ_16M) */
#define CONFIG_SYS_LOAD_ADDR            (CONFIG_SYS_SDRAM_BASE + SZ_2M)

#define CONFIG_SYS_CBSIZE               1024
#define CONFIG_SYS_BARGSIZE             CONFIG_SYS_CBSIZE
#define CONFIG_SYS_MAXARGS              32

#define CONFIG_SYS_MALLOC_LEN           SZ_8M   /* Size of malloc() pool */
#define CONFIG_SYS_MONITOR_LEN          SZ_512K /* max u-boot size */
#ifndef CONFIG_SPL_BUILD
#define CONFIG_SYS_BOOTM_LEN	        SZ_16M
#endif

#ifdef CONFIG_I2C_EEPROM
#define CONFIG_ID_EEPROM
#define CONFIG_SYS_I2C_MAC_OFFSET       0
#endif

/* SPL */
#define CONFIG_SPL_FRAMEWORK
#define CONFIG_SPL_TEXT_BASE            0x00300000 /* ATMEL_BASE_SRAM0 */
#define CONFIG_SPL_MAX_SIZE             SZ_64K

#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_PAGE_SIZE       2048
#define CONFIG_SYS_NAND_PAGE_COUNT      64
#define CONFIG_SYS_NAND_OOBSIZE         64
#define CONFIG_SYS_NAND_BLOCK_SIZE      SZ_128K
#define CONFIG_SYS_NAND_BAD_BLOCK_POS   0x0

#ifdef CONFIG_SD_BOOT

#define CONFIG_SPL_BSS_START_ADDR       CONFIG_SYS_SDRAM_BASE
#define CONFIG_SPL_BSS_MAX_SIZE         SZ_512K
#define CONFIG_SYS_SPL_MALLOC_START     (CONFIG_SPL_BSS_START_ADDR + CONFIG_SPL_BSS_MAX_SIZE)
#define CONFIG_SYS_SPL_MALLOC_SIZE      SZ_512K

#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define CONFIG_SPL_FS_LOAD_PAYLOAD_NAME	"u-boot.itb"

#elif CONFIG_NAND_BOOT

#define CONFIG_SPL_BSS_START_ADDR       ATMEL_BASE_SRAM1 + SZ_32K
#define CONFIG_SPL_BSS_MAX_SIZE         SZ_32K
#define CONFIG_SYS_SPL_MALLOC_START     CONFIG_SYS_SDRAM_BASE
#define CONFIG_SYS_SPL_MALLOC_SIZE      SZ_512K

#define CONFIG_SPL_NAND_DRIVERS
#define CONFIG_SPL_NAND_BASE
#define CONFIG_SPL_NAND_IDENT
#define CONFIG_SPL_GENERATE_ATMEL_PMECC_HEADER

#ifdef CONFIG_BOOTCOUNT
#define CONFIG_BOOTCOUNT_ENV
#define CONFIG_BOOTCOUNT_LIMIT
#endif

#endif

/* Enable Watchdog in U-Boot */
#define CONFIG_AT91SAM9_WATCHDOG
#define CONFIG_AT91_HW_WDT_TIMEOUT	16
#define CONFIG_HW_WATCHDOG

#define CONFIG_WD_PERIOD  (CONFIG_AT91_HW_WDT_TIMEOUT * 1000000 / 2)

#define DEFAULT_ENV_SETTINGS \
	"autoload=no\0" \
	"autostart=no\0" \
	"bootside=a\0"

#ifndef __ASSEMBLY__
/* Define hook for custom board initialization */
void som60_custom_hw_init(void);
#endif

#endif
