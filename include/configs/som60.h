/*
 * Configuation settings for the SOM60 Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define CONFIG_SYS_TEXT_BASE            0x26f00000

/* ARM asynchronous clock */
#define CONFIG_SYS_AT91_SLOW_CLOCK      32768
#define CONFIG_SYS_AT91_MAIN_CLOCK      12000000 /* from 12 MHz crystal */

#define CONFIG_ARCH_CPU_INIT

#define CONFIG_CMDLINE_TAG	/* enable passing of ATAGs */

#ifndef CONFIG_SPL_BUILD
#define CONFIG_SKIP_LOWLEVEL_INIT
#endif

#ifdef CONFIG_TARGET_WB50N

#define CONFIG_EXTRA_ENV_SETTINGS \
	"autoload=no\0" \
	"autostart=no\0" \
	"bootside=a\0" \
	"_setbootvol=if test ${bootside} = a;then "	\
		"setenv bootvol 1;" \
	"else " \
		"setenv bootvol 4;"	\
	"fi\0" \
	"_formatubi=nand erase.part ubi;" \
		"ubi part ubi;" \
		"for part in a b; do " \
			"ubi create kernel_${part}  800000 static;" \
			"ubi create rootfs_${part} 2520000 static;" \
			"ubi create rootfs_data_${part} ec0000 dynamic;" \
		"done\0"

#else

#define CONFIG_EXTRA_ENV_SETTINGS \
	"autoload=no\0" \
	"autostart=no\0" \
	"bootside=a\0" \
	"_setbootvol=if test ${bootside} = a; then " \
		"setenv bootvol 1;" \
	"else " \
		"setenv bootvol 4;" \
	"fi\0" \
	"_formatubi=nand erase.part ubi;" \
		"ubi part ubi;" \
		"for part in a b; do " \
			"ubi create kernel_${part}  800000 static;" \
			"ubi create rootfs_${part} 5200000 static;" \
			"ubi create rootfs_data_${part} 1E20000 dynamic;" \
		"done\0"

#endif

#define CONFIG_ENV_VARS_UBOOT_CONFIG

/*
 * BOOTP options
 */
#define CONFIG_BOOTP_BOOTFILESIZE
#define CONFIG_BOOTP_BOOTPATH
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME

/*
 * Command line configuration.
 */

/* u-boot env in nand flash */
#define CONFIG_ENV_OFFSET               0x140000
#define CONFIG_ENV_OFFSET_REDUND        0x160000
#define CONFIG_ENV_SIZE                 0x20000

#define CONFIG_SYS_LONGHELP
#define CONFIG_CMDLINE_EDITING
#define CONFIG_AUTO_COMPLETE

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN           (8 * 1024 * 1024)

/* SDRAM */
#define CONFIG_NR_DRAM_BANKS            1
#define CONFIG_SYS_SDRAM_BASE           0x20000000
#ifdef CONFIG_TARGET_WB50N
#define CONFIG_SYS_SDRAM_SIZE           0x04000000
#else
#define CONFIG_SYS_SDRAM_SIZE           0x10000000
#endif


#ifdef CONFIG_SPL_BUILD
#define CONFIG_SYS_INIT_SP_ADDR         0x318000
#else
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_SDRAM_BASE + 16 * 1024 - GENERATED_GBL_DATA_SIZE)
#endif

/* NAND flash */
#define CONFIG_NAND_ATMEL
#define CONFIG_SYS_MAX_NAND_DEVICE      1
#define CONFIG_SYS_NAND_BASE            0x60000000
/* our ALE is AD21 */
#define CONFIG_SYS_NAND_MASK_ALE        (1 << 21)
/* our CLE is AD22 */
#define CONFIG_SYS_NAND_MASK_CLE        (1 << 22)
#define CONFIG_SYS_NAND_ONFI_DETECTION

#define CONFIG_MTD_DEVICE
#define CONFIG_MTD_PARTITIONS

/* PMECC & PMERRLOC */
#define CONFIG_ATMEL_NAND_HWECC
#define CONFIG_ATMEL_NAND_HW_PMECC
#define CONFIG_PMECC_CAP                8
#define CONFIG_PMECC_SECTOR_SIZE        512

#define CONFIG_SYS_LOAD_ADDR            0x21000000 /* load address */

/* SPL */
#define CONFIG_SPL_FRAMEWORK
#define CONFIG_SPL_TEXT_BASE            0x300000
#define CONFIG_SPL_MAX_SIZE             0x10000
#define CONFIG_SPL_BSS_START_ADDR       0x20000000
#define CONFIG_SPL_BSS_MAX_SIZE         0x80000
#define CONFIG_SYS_SPL_MALLOC_START     0x20080000
#define CONFIG_SYS_SPL_MALLOC_SIZE      0x80000

#ifdef CONFIG_I2C_EEPROM

#define CONFIG_ID_EEPROM
#define CONFIG_SYS_I2C_MAC_OFFSET 0

#endif

#ifdef CONFIG_SD_BOOT

#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define CONFIG_SPL_FS_LOAD_PAYLOAD_NAME	"u-boot.itb"
#define CONFIG_SYS_MONITOR_LEN          (512 << 10)

#elif CONFIG_NAND_BOOT

#define CONFIG_SPL_NAND_DRIVERS
#define CONFIG_SPL_NAND_BASE
#define CONFIG_SPL_NAND_IDENT

#define CONFIG_SYS_MONITOR_LEN          (1152 << 10)

#endif

#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_PAGE_SIZE       0x800
#define CONFIG_SYS_NAND_PAGE_COUNT      64
#define CONFIG_SYS_NAND_OOBSIZE         64
#define CONFIG_SYS_NAND_BLOCK_SIZE      0x20000
#define CONFIG_SYS_NAND_BAD_BLOCK_POS   0x0
#define CONFIG_SPL_GENERATE_ATMEL_PMECC_HEADER

/* Enable Watchdog in U-Boot */
#define CONFIG_AT91SAM9_WATCHDOG
#define CONFIG_AT91_HW_WDT_TIMEOUT	16
#if !defined(CONFIG_SPL_BUILD)
#define CONFIG_HW_WATCHDOG
#endif

#endif
