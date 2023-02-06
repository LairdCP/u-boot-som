/*
 * Copyright (C) 2018 Laird Connectivity
 *	Ben Whitten <ben.whitten@lairdconnect.com>
 *	Boris Krasnovskiy <boris.krasnovskiy@lairdconnect.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <init.h>
#include <env.h>
#include <net.h>
#include <debug_uart.h>
#include <version.h>

#include <asm/arch/at91_common.h>
#include <asm/arch/gpio.h>
#include <asm/arch/clk.h>

#include <spl_menu.h>

#include <asm/arch/sama5d3_smc.h>
#include <asm/arch/atmel_mpddrc.h>
#include <asm/arch/at91_sck.h>

#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/mtd/rawnand.h>

DECLARE_GLOBAL_DATA_PTR;

#define FS_MAX_KEY_SIZE	64
#define FS_KEY_WINDOW	0x31e000

#define MAX_BOARD_HW_ID	0x00000004

#define MAX_NUM_PORTS	2


/* RAM IC's used on boards.  Note JSFBAB3YH3BBG_425 and JSFBAB3Y63GBG_425
 * combo packages have the same DDR silicon internally.
 */
typedef enum {
	MT46H16M32LF                             = 0,  // legacy wb50n (5 & 6)
	MT29C2G24MAAAAKAMD_5                     = 1,  // legacy som60
	MT29C4G48MAYBBAMR_48                     = 2,  // legacy som60x2
	DDR2_JSFBAB3YH3BBG_425_JSFBAB3Y63GBG_425 = 3,  // 2022 som60 redesign
	DDR2_JSFCBB3YH3BBG_425A                  = 4,  // 2022 som60x2 redesign
} laird_ram_ic_t;

typedef enum {
	RAM_TYPE_LPDDR1,
	RAM_TYPE_LPDDR2,
} laird_ram_type;

typedef struct {
	laird_ram_ic_t ram_ic;
} board_hw_description_t;

static const board_hw_description_t board_hw_description[] = {
	/* Legacy WB50n, SOM60, SOM60x2 */
	[0x00000000] = {
		/* 0 board HW ID (legacy boards) must use the type defined in the build */
		.ram_ic = SOM_LEGACY_RAM_IC,
	},
	/* 2022 SOM redesign SOM60 */
	[0x00000001] = {
		.ram_ic = DDR2_JSFBAB3YH3BBG_425_JSFBAB3Y63GBG_425,
	},
	/* 2022 SOM redesign SOM60x2 */
	[0x00000002] = {
		.ram_ic = DDR2_JSFCBB3YH3BBG_425A,
	},
	/* 2022+ manufactured legacy SOM60 */
	[0x00000003] = {
		.ram_ic = MT29C2G24MAAAAKAMD_5,
	},
	/* 2022+ manufactured legacy SOM60x2 */
	[0x00000004] = {
		.ram_ic = MT29C4G48MAYBBAMR_48,
	},
};

typedef struct {
	struct atmel_mpddrc_config ddr_config;
	laird_ram_type type;
	const char *name;
	long sdram_size;
} laird_ram_config_t;


void mem_init_lpddr1(const struct atmel_mpddrc_config *mpddr_value);
void mem_init_lpddr2(const struct atmel_mpddrc_config *mpddr_value);

static const laird_ram_config_t ram_configs[] = {
#if defined(CONFIG_TARGET_WB50N) || defined(CONFIG_TARGET_WB50N_SYSD)
	[MT46H16M32LF] = {
		.type = RAM_TYPE_LPDDR1,
		.name = "MT46H16M32LF",
		.sdram_size = SZ_64M,
		.ddr_config =
			{
			.md = (ATMEL_MPDDRC_MD_DBW_32_BITS |
			       ATMEL_MPDDRC_MD_LPDDR_SDRAM),
			.cr = LPDDR_CR,
			/*
			 * The SDRAM device requires a refresh of all rows at least every 64ms.
			 * ((64ms) / 8192) * 132MHz = 1031 i.e. 0x407
			 */
			.rtr = 0x407,
			.tpr0 = LPDDR_TPR0,
			.tpr1 = LPDDR_TPR1,
			.tpr2 = (4 << ATMEL_MPDDRC_TPR2_TRTP_OFFSET),
			.lpr = (ATMEL_MPDDRC_LPR_LPCB_DISABLED       |
				ATMEL_MPDDRC_LPR_CLK_FR              |
				ATMEL_MPDDRC_LPR_PASR_(0)            |
				ATMEL_MPDDRC_LPR_DS_(1)              |
				ATMEL_MPDDRC_LPR_TIMEOUT_0           |
				ATMEL_MPDDRC_LPR_ADPE_FAST           |
				ATMEL_MPDDRC_LPR_UPD_MR_NO_UPDATE)
		},
	},
#endif

#if defined(CONFIG_TARGET_SOM60) || defined(CONFIG_TARGET_SOM60X2) || defined(CONFIG_TARGET_IG60)
	[MT29C2G24MAAAAKAMD_5] = {
		.type = RAM_TYPE_LPDDR1,
		.name = "MT29C2G24MAAAAKAMD_5",
		.sdram_size = SZ_128M,
		.ddr_config =
			{
			.md = (ATMEL_MPDDRC_MD_DBW_32_BITS |
			       ATMEL_MPDDRC_MD_LPDDR_SDRAM),
			.cr = LPDDR_CR,
			/*
			 * The SDRAM device requires a refresh of all rows at least every 64ms.
			 * ((64ms) / 8192) * 132MHz = 1031 i.e. 0x407
			 */
			.rtr = 0x407,
			.tpr0 = LPDDR_TPR0,
			.tpr1 = LPDDR_TPR1,
			.tpr2 = (4 << ATMEL_MPDDRC_TPR2_TRTP_OFFSET),
			.lpr = (ATMEL_MPDDRC_LPR_LPCB_DISABLED       |
				ATMEL_MPDDRC_LPR_CLK_FR              |
				ATMEL_MPDDRC_LPR_PASR_(0)            |
				ATMEL_MPDDRC_LPR_DS_(1)              |
				ATMEL_MPDDRC_LPR_TIMEOUT_0           |
				ATMEL_MPDDRC_LPR_ADPE_FAST           |
				ATMEL_MPDDRC_LPR_UPD_MR_NO_UPDATE)
		},
	},

	[MT29C4G48MAYBBAMR_48] = {
		.type = RAM_TYPE_LPDDR1,
		.name = "MT29C4G48MAYBBAMR_48",
		.sdram_size = SZ_256M,
		.ddr_config = {
			.md = (ATMEL_MPDDRC_MD_DBW_32_BITS |
			       ATMEL_MPDDRC_MD_LPDDR_SDRAM),
			.cr = LPDDR_CR_SOM60X2,
			/*
			 * The SDRAM device requires a refresh of all rows at least every 64ms.
			 * ((64ms) / 8192) * 132MHz = 1031 i.e. 0x407
			 */
			.rtr = 0x407,
			.tpr0 = LPDDR_TPR0,
			.tpr1 = LPDDR_TPR1,
			.tpr2 = (4 << ATMEL_MPDDRC_TPR2_TRTP_OFFSET),
			.lpr = (ATMEL_MPDDRC_LPR_LPCB_DISABLED       |
				ATMEL_MPDDRC_LPR_CLK_FR              |
				ATMEL_MPDDRC_LPR_PASR_(0)            |
				ATMEL_MPDDRC_LPR_DS_(1)              |
				ATMEL_MPDDRC_LPR_TIMEOUT_0           |
				ATMEL_MPDDRC_LPR_ADPE_FAST           |
				ATMEL_MPDDRC_LPR_UPD_MR_NO_UPDATE)
		},
	},

	[DDR2_JSFBAB3YH3BBG_425_JSFBAB3Y63GBG_425] = {
		.type = RAM_TYPE_LPDDR2,
		.name = "DDR2_JSFBAB3Y[H3B/63G]BG_425",
		.sdram_size = SZ_128M,
		.ddr_config = {
			/* Reference at91bootstrap3/driver/ddramc.c */
			.md = (ATMEL_MPDDRC_MD_DBW_32_BITS |
			       ATMEL_MPDDRC_MD_LPDDR2_SDRAM),

			.cr = LPDDR2_CR,

			/* MPDDRC_LPDDR2_LPR DS: Drive Strength - 3 - DS_48 - 48 ohm typical (reset value is 2, 40 Ohm) */
			.lpddr23_lpr = ATMEL_MPDDRC_LPDDR23_LPR_DS(0x03),
			/* JSFBAB3YH3BBG 90n short calibration (tZQCS) */
			/* at 132 MHz, 12 clocks ~= 90 nsec */
			.tim_cal = ATMEL_MPDDRC_CALR_ZQCS(12),

			.rtr = 0x404,

			/* 0xC852 - Atmel reference count value - see SAMA5D3 29.7.11
			 * Dependent upon expected temperature (T driftrate ) and voltage (V driftrate ) drift rates that the SDRAM is subject to in the application.
			 */
			.cal_mr4 = LPDDR2_CAL_MR4,

			/* tRAS temperature derating += 1.875 ns */
			/* trcd temperature derating += 1.875 ns */
			/* tRAS min 3 tck, 42 ns */
			/* tRAS 44 ns ~= 5.8 clocks */
			.tpr0 = LPDDR2_TPR0,
			.tpr1 = LPDDR2_TPR1,
			.tpr2 = LPDDR2_TPR2,
			.lpr = (ATMEL_MPDDRC_LPR_LPCB_DISABLED       |
				ATMEL_MPDDRC_LPR_CLK_FR              |
				ATMEL_MPDDRC_LPR_PASR_(0)            |
				/* This field is unique to low-power DDR1-SDRAM." */
				ATMEL_MPDDRC_LPR_DS_(1)              |
				ATMEL_MPDDRC_LPR_TIMEOUT_0           |
				ATMEL_MPDDRC_LPR_ADPE_FAST           |
				ATMEL_MPDDRC_LPR_UPD_MR_NO_UPDATE)
		},
	},

	[DDR2_JSFCBB3YH3BBG_425A] = {
		.type = RAM_TYPE_LPDDR2,
		.name = "DDR2_JSFCBB3YH3BBG_425A",
		.sdram_size = SZ_256M,
		.ddr_config = {
			.md = (ATMEL_MPDDRC_MD_DBW_32_BITS |
			       ATMEL_MPDDRC_MD_LPDDR2_SDRAM),
			.cr = LPDDR2_CR_DDR2_JSFCBB3YH3BBG_425A,
			.lpddr23_lpr = ATMEL_MPDDRC_LPDDR23_LPR_DS(0x03),
			.tim_cal = ATMEL_MPDDRC_CALR_ZQCS(12),
			.rtr = 0x202,
			.cal_mr4 = LPDDR2_CAL_MR4,
			.tpr0 = LPDDR2_TPR0,
			.tpr1 = LPDDR2_TPR1,
			.tpr2 = LPDDR2_TPR2,
			.lpr = (ATMEL_MPDDRC_LPR_LPCB_DISABLED       |
				ATMEL_MPDDRC_LPR_CLK_FR              |
				ATMEL_MPDDRC_LPR_PASR_(0)            |
				/* This field is unique to low-power DDR1-SDRAM." */
				ATMEL_MPDDRC_LPR_DS_(1)              |
				ATMEL_MPDDRC_LPR_TIMEOUT_0           |
				ATMEL_MPDDRC_LPR_ADPE_FAST           |
				ATMEL_MPDDRC_LPR_UPD_MR_NO_UPDATE)
		},
	},
#endif
};

static unsigned atmel_encode_ncycles(unsigned int ncycles,
				     unsigned int msbpos,
				     unsigned int msbwidth,
				     unsigned int msbfactor)
{
	unsigned int lsbmask = (1 << msbpos) - 1;
	unsigned int msbmask = (1 << msbwidth) - 1;
	unsigned int msb, lsb;

	msb = ncycles / msbfactor;
	lsb = ncycles % msbfactor;

	if (lsb > lsbmask) {
		lsb = 0;
		msb++;
	}

	/*
	 * Let's just put the maximum we can if the requested setting does
	 * not fit in the register field.
	 */
	if (msb > msbmask) {
		msb = msbmask;
		lsb = lsbmask;
	}

	return (msb << msbpos) | lsb;
}

static unsigned atmel_encode_setup_ncycles(unsigned int ncycles)
{
	return atmel_encode_ncycles(ncycles, 5, 1, 128);
}

static unsigned atmel_encode_pulse_ncycles(unsigned int ncycles)
{
	return atmel_encode_ncycles(ncycles, 6, 1, 256);
}

static unsigned atmel_encode_cycle_ncycles(unsigned int ncycles)
{
	return atmel_encode_ncycles(ncycles, 7, 2, 256);
}

static unsigned atmel_encode_timing_ncycles(unsigned int ncycles)
{
	return atmel_encode_ncycles(ncycles, 3, 1, 64);
}

/* Configures NAND controller from the timing table supplied */
int atmel_setup_data_interface(struct mtd_info *mtd, int chipnr,
			       const struct nand_data_interface *conf)
{
	struct at91_smc *smc = (struct at91_smc *)ATMEL_BASE_SMC;

	u32 ncycles, totalcycles, timeps, mckperiodps;
	u32 setup_reg, pulse_reg, cycle_reg, timings_reg, mode_reg;

	const struct nand_sdr_timings *timings;

	timings = nand_get_sdr_timings(conf);
	if (IS_ERR(timings))
		return PTR_ERR(timings);

	/*
	 * tRC < 30ns implies EDO mode. This controller does not support this
	 * mode.
	 */
	if (timings->tRC_min < 30000)
		return -ENOTSUPP;

	if (chipnr == NAND_DATA_IFACE_CHECK_ONLY)
		return 0;

	setup_reg = 0;
	pulse_reg = 0;
	cycle_reg = 0;
	timings_reg = 0;
	mode_reg = 0;

	/* Clock to period in ps */
	mckperiodps = DIV_ROUND_DOWN_ULL(1000000000000ULL, get_mck_clk_rate());

	/* Truncate frequency to match Linux calculation */
	mckperiodps -= mckperiodps % 1000;

	/*
	 * Set write pulse timing. This one is easy to extract:
	 *
	 * NWE_PULSE = tWP
	 */
	ncycles = DIV_ROUND_UP(timings->tWP_min, mckperiodps);
	totalcycles = ncycles;
	pulse_reg |= AT91_SMC_PULSE_NWE(atmel_encode_pulse_ncycles(ncycles));

	/*
	 * The write setup timing depends on the operation done on the NAND.
	 * All operations goes through the same data bus, but the operation
	 * type depends on the address we are writing to (ALE/CLE address
	 * lines).
	 * Since we have no way to differentiate the different operations at
	 * the SMC level, we must consider the worst case (the biggest setup
	 * time among all operation types):
	 *
	 * NWE_SETUP = max(tCLS, tCS, tALS, tDS) - NWE_PULSE
	 */
	timeps = max3(timings->tCLS_min, timings->tCS_min, timings->tALS_min);
	timeps = max(timeps, timings->tDS_min);
	ncycles = DIV_ROUND_UP(timeps, mckperiodps);
	ncycles = ncycles > totalcycles ? ncycles - totalcycles : 0;
	totalcycles += ncycles;
	setup_reg |= AT91_SMC_SETUP_NWE(atmel_encode_setup_ncycles(ncycles));

	/*
	 * As for the write setup timing, the write hold timing depends on the
	 * operation done on the NAND:
	 *
	 * NWE_HOLD = max(tCLH, tCH, tALH, tDH, tWH)
	 */
	timeps = max3(timings->tCLH_min, timings->tCH_min, timings->tALH_min);
	timeps = max3(timeps, timings->tDH_min, timings->tWH_min);
	ncycles = DIV_ROUND_UP(timeps, mckperiodps);
	totalcycles += ncycles;

	/*
	 * The write cycle timing is directly matching tWC, but is also
	 * dependent on the other timings on the setup and hold timings we
	 * calculated earlier, which gives:
	 *
	 * NWE_CYCLE = max(tWC, NWE_SETUP + NWE_PULSE + NWE_HOLD)
	 */
	ncycles = DIV_ROUND_UP(timings->tWC_min, mckperiodps);
	ncycles = max(totalcycles, ncycles);
	cycle_reg |= AT91_SMC_CYCLE_NWE(atmel_encode_cycle_ncycles(ncycles));

	/*
	 * We don't want the CS line to be toggled between each byte/word
	 * transfer to the NAND. The only way to guarantee that is to have the
	 * NCS_{WR,RD}_{SETUP,HOLD} timings set to 0, which in turn means:
	 *
	 * NCS_WR_PULSE = NWE_CYCLE
	 */
	pulse_reg |= AT91_SMC_PULSE_NCS_WR(atmel_encode_pulse_ncycles(ncycles));

	/*
	 * As for the write setup timing, the read hold timing depends on the
	 * operation done on the NAND:
	 *
	 * NRD_HOLD = max(tREH, tRHOH)
	 */
	timeps = max(timings->tREH_min, timings->tRHOH_min);
	ncycles = DIV_ROUND_UP(timeps, mckperiodps);
	totalcycles = ncycles;

	/*
	 * TDF = tRHZ - NRD_HOLD
	 */
	ncycles = DIV_ROUND_UP(timings->tRHZ_max, mckperiodps);
	ncycles -= totalcycles;

	/*
	 * In ONFI 4.0 specs, tRHZ has been increased to support EDO NANDs and
	 * we might end up with a config that does not fit in the TDF field.
	 * Just take the max value in this case and hope that the NAND is more
	 * tolerant than advertised.
	 */
	if (ncycles > 15)
		ncycles = 15;
	else if (ncycles < 1)
		ncycles = 1;

	mode_reg |= AT91_SMC_MODE_TDF_CYCLE(ncycles) | AT91_SMC_MODE_TDF;

	/*
	 * Read pulse timing directly matches tRP:
	 *
	 * NRD_PULSE = tRP
	 */
	ncycles = DIV_ROUND_UP(timings->tRP_min, mckperiodps);
	totalcycles += ncycles;
	pulse_reg |= AT91_SMC_PULSE_NRD(atmel_encode_pulse_ncycles(ncycles));

	/*
	 * The read cycle timing is directly matching tRC, but is also
	 * dependent on the setup and hold timings we calculated earlier,
	 * which gives:
	 *
	 * NRD_CYCLE = max(tRC, NRD_PULSE + NRD_HOLD)
	 *
	 * NRD_SETUP is always 0.
	 */
	ncycles = DIV_ROUND_UP(timings->tRC_min, mckperiodps);
	ncycles = max(totalcycles, ncycles);
	cycle_reg |= AT91_SMC_CYCLE_NRD(atmel_encode_cycle_ncycles(ncycles));

	/*
	 * We don't want the CS line to be toggled between each byte/word
	 * transfer from the NAND. The only way to guarantee that is to have
	 * the NCS_{WR,RD}_{SETUP,HOLD} timings set to 0, which in turn means:
	 *
	 * NCS_RD_PULSE = NRD_CYCLE
	 */
	pulse_reg |= AT91_SMC_PULSE_NCS_RD(atmel_encode_pulse_ncycles(ncycles));

	/* Txxx timings are directly matching tXXX ones. */
	ncycles = DIV_ROUND_UP(timings->tCLR_min, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TCLR(atmel_encode_timing_ncycles(ncycles));

	ncycles = DIV_ROUND_UP(timings->tADL_min, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TADL(atmel_encode_timing_ncycles(ncycles));

	ncycles = DIV_ROUND_UP(timings->tAR_min, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TAR(atmel_encode_timing_ncycles(ncycles));

	ncycles = DIV_ROUND_UP(timings->tRR_min, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TRR(atmel_encode_timing_ncycles(ncycles));

	ncycles = DIV_ROUND_UP(timings->tWB_max, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TWB(atmel_encode_timing_ncycles(ncycles));

	/* Attach the CS line to the NFC logic. */
	timings_reg |= AT91_SMC_TIMINGS_NFSEL(1) | AT91_SMC_TIMINGS_RBNSEL(3);

	/* Operate in NRD/NWE READ/WRITEMODE. */
	mode_reg |= AT91_SMC_MODE_RM_NRD | AT91_SMC_MODE_WM_NWE;

	writel(setup_reg, &smc->cs[3].setup);
	writel(pulse_reg, &smc->cs[3].pulse);
	writel(cycle_reg, &smc->cs[3].cycle);
	writel(timings_reg, &smc->cs[3].timings);
	writel(mode_reg, &smc->cs[3].mode);

	return 0;
}

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
void board_debug_uart_init(void)
{
#if   CONFIG_DEBUG_UART_BASE == ATMEL_BASE_DBGU   // serial0
	at91_seriald_hw_init();
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_USART0 // serial1
	at91_serial0_hw_init();
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_USART1 // serial2
	at91_serial1_hw_init();
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_USART2 // serial3
	at91_serial2_hw_init();
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_USART3 // serial4
	at91_pio3_set_b_periph(AT91_PIO_PORTE, 19, 0);	/* TXD4 */
	at91_pio3_set_b_periph(AT91_PIO_PORTE, 18, 1);	/* RXD4 */

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_USART3);
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_UART0  // serial5
	at91_pio3_set_a_periph(AT91_PIO_PORTC, 30, 0);	/* TXD5 */
	at91_pio3_set_a_periph(AT91_PIO_PORTC, 29, 1);	/* RXD5 */

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_UART0);
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_UART1  // serial6
	at91_pio3_set_b_periph(AT91_PIO_PORTA, 31, 0);	/* TXD6 */
	at91_pio3_set_b_periph(AT91_PIO_PORTA, 30, 1);	/* RXD6 */

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_UART1);
#else
	#error "Unknown debug port specified"
#endif
}
#endif

#ifndef CONFIG_SPL_BUILD

#ifdef CONFIG_FIT_SIGNATURE
void som60_fs_key_inject(void)
{
	u8	*key = (u8 *)FS_KEY_WINDOW;
	const void *fs_key;
	int fs_key_len;
	int enc_node;

	enc_node = fdt_subnode_offset(gd->fdt_blob, 0, "encryption");
	if (enc_node < 0) {
		debug("No encryption node found\n");
		return;
	}

	fs_key = fdt_getprop(gd->fdt_blob, enc_node, "laird,fs-key", &fs_key_len);
	if (!fs_key) {
		debug("No fs-key property found\n");
		return;
	}

	if (fs_key_len != FS_MAX_KEY_SIZE) {
		debug("Key must be max size\n");
		return;
	}

	memcpy(key, fs_key, fs_key_len);
}
#endif

int board_late_init(void)
{
	char *version;
	int save;

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	char name[32], *p;

	snprintf(name, sizeof(name), "%s%s", get_cpu_name(), CONFIG_LOCALVERSION);

	for (p = name; *p; ++p)
		*p = tolower(*p);

	env_set("lrd_name", name);
#endif

	version = env_get("version");
	save = !version || strcmp(version, PLAIN_VERSION);
	if (save)
		env_set("version", PLAIN_VERSION);

	if ((gd->flags & GD_FLG_ENV_DEFAULT) || save) {
		puts("Saving default environment...\n");
		env_save();
	}

#ifdef CONFIG_USB_ETHER
	usb_ether_init();
#endif

	return 0;
}

void __weak som60_custom_hw_init(void)
{
}

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

#ifdef CONFIG_MTD_RAW_NAND
	at91_periph_clk_enable(ATMEL_ID_SMC);

	/* Disable Flash Write Protect discrete */
	at91_set_pio_output(AT91_PIO_PORTE, 14, 1);
#endif

	som60_custom_hw_init();

#ifdef CONFIG_FIT_SIGNATURE
	som60_fs_key_inject();
#endif

	return 0;
}

void board_quiesce_devices(void)
{
#ifdef CONFIG_MTD_RAW_NAND
#ifndef CONFIG_TARGET_WB50N
	/* Activate Flash Write Protect discrete,
	 * so that flash enter standby if not used in kernel */
	at91_set_pio_output(AT91_PIO_PORTE, 14, 0);
#endif

#ifndef CONFIG_NAND_BOOT
	at91_periph_clk_disable(ATMEL_ID_SMC);
#endif
#endif
}

int dram_init(void)
{
	u16 board_hw_id;
	board_hw_id = 0;
	laird_ram_ic_t ram_ic;
	const laird_ram_config_t* ram_config;

	if (board_hw_id > MAX_BOARD_HW_ID)
		board_hw_id = MAX_BOARD_HW_ID;

	ram_ic = board_hw_description[board_hw_id].ram_ic;
	ram_config = &ram_configs[ram_ic];
	gd->ram_size = ram_config->sdram_size;
	return 0;
}

#else /* SPL */

int board_early_init_f(void)
{
	struct at91_port *at91_port = (struct at91_port *)ATMEL_BASE_PIOE;

	/* pins that could be used as USART3 & USART4 are switched to GPIO here,
	   so allow these USARTs to operate as debug terminal if so configured */
	u32 mask = 0x079fffff ^ (readl(&at91_port->mux.pio3.abcdsr1) & 0x060c0000);

	writel(mask, &at91_port->idr);
	writel(mask, &at91_port->per);

	return 0;
}

void at91_disable_smd_clock(void)
{
	struct at91_pmc *pmc = (struct at91_pmc *)ATMEL_BASE_PMC;

	/*
	 * Set pin DIBP to pull-up and DIBN to pull-down
	 * to save power on VDDIOP0
	 */
	at91_system_clk_enable(AT91_PMC_SMD);
	writel(AT91_PMC_SMDDIV, &pmc->smd);
	at91_periph_clk_enable(ATMEL_ID_SMD);

	writel(0xE, (0x0C + ATMEL_BASE_SMD));

	at91_periph_clk_disable(ATMEL_ID_SMD);
	at91_system_clk_disable(AT91_PMC_SMD);
}

static void at91sama5d3_slowclock_init(void)
{
	/*
	 * On AT91SAMA5D3 CPUs, the slow clock can be based on an
	 * internal imprecise RC oscillator or an external 32 kHz oscillator.
	 * Switch to the latter.
	 */
	static const ulong *reg = (ulong *)ATMEL_BASE_SCKCR;
	unsigned tmp;

	/* Enable the internal 32 kHz RC oscillator for low power by writing a 1 to the RCEN bit. */
	tmp = readl(reg);
	tmp |= AT91SAM9G45_SCKCR_RCEN;
	writel(tmp, reg);

	/* Wait internal 32 kHz RC startup time for clock stabilization (software loop). */
	/* 500 us */
	udelay(500);

	/* Switch from 32768 Hz oscillator to internal RC by writing a 0 to the OSCSEL bit. */
	tmp = readl(reg);
	tmp &= ~AT91SAM9G45_SCKCR_OSCSEL;
	writel(tmp, reg);

	/* Wait 5 slow clock cycles for internal resynchronization. */
	/* 5 slow clock cycles = ~153 us (5 / 32768) */
	udelay(153);

	/* Disable the 32768 Hz oscillator by writing a 0 to the OSC32EN bit. */
	tmp = readl(reg);
	tmp &= ~AT91SAM9G45_SCKCR_OSC32EN;
	writel(tmp, reg);

	/* Wait 5 slow clock cycles for internal resynchronization. */
	/* 5 slow clock cycles = ~153 us (5 / 32768) */
	udelay(153);

	/*
	 * Enable the 32768 Hz oscillator by setting the bit OSC32EN to 1
	 */
	tmp = readl(reg);
	tmp |= AT91SAM9G45_SCKCR_OSC32EN;
	writel(tmp, reg);

	/* Bypass the 32kHz oscillator by using an external clock
	 * Set OSC32BYP=1 and OSC32EN=0 atomically
	 */
	tmp = readl(reg);
	tmp &= ~AT91SAM9G45_SCKCR_OSC32EN;
	tmp |= AT91SAM9G45_SCKCR_OSC32BYP;
	writel(tmp, reg);

	/*
	 * Switching from internal 32kHz RC oscillator to 32768 Hz oscillator
	 * by setting the bit OSCSEL to 1
	 */
	tmp = readl(reg);
	tmp |= AT91SAM9G45_SCKCR_OSCSEL_32;
	writel(tmp, reg);

	/*
	 * Waiting 5 slow clock cycles for internal resynchronization
	 * 5 slow clock cycles = ~153 us (5 / 32768)
	 */
	udelay(153);

	/*
	 * Disable the 32kHz RC oscillator by setting the bit RCEN to 0
	 */
	tmp = readl(reg);
	tmp &= ~AT91SAM9G45_SCKCR_RCEN;
	writel(tmp, reg);

	/* Delay makes boot much more stable
	 * This delay present on boostrap for the case of the oscillator
	 * not in bypass mode, which is all Microchip demo boards
	 * My guess Microchip misattributed the reson for delay need
	 */
	udelay(1300000);
}

void spl_board_init(void)
{
	at91sama5d3_slowclock_init();

	at91_disable_smd_clock();

	/* Configure and disable Radio */
#ifdef CONFIG_TARGET_WB50N
	at91_set_pio_output(AT91_PIO_PORTE, 3, 0);
#endif
	at91_set_pio_output(AT91_PIO_PORTE, 5, 0);

#ifdef CONFIG_NAND_BOOT
	const struct nand_data_interface *conf;

	at91_periph_clk_enable(ATMEL_ID_SMC);

	conf = nand_get_default_data_interface();
	atmel_setup_data_interface(NULL, 1, conf);
#endif
}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	/* Just empty function now - can't decide what to choose */
	debug("%s: %s\n", __func__, name);

	return 0;
}
#endif


// Reference at91bootstrap3/board/sama5d3x_cmp/sama5dx_cmp.c lpddr2_init()
void mem_init_lpddr2(const struct atmel_mpddrc_config *mpddr_value)
{
	const struct atmel_mpddr *mpddr = (struct atmel_mpddr *)ATMEL_BASE_MPDDRC;
	u32 reg;

	// Reference does not open/close input buffers

	/* Enable MPDDR clock */
	at91_periph_clk_enable(ATMEL_ID_MPDDRC);
	at91_system_clk_enable(AT91_PMC_DDR);

	/*
	 * Initialize the special register for the SAMA5D3X_CMP.
	 * MPDDRC DLL Slave Offset Register: DDR2 configuration
	 */
	reg = ATMEL_MPDDRC_SOR_S0OFF(0x04)
		| ATMEL_MPDDRC_SOR_S1OFF(0x03)
		| ATMEL_MPDDRC_SOR_S2OFF(0x04)
		| ATMEL_MPDDRC_SOR_S3OFF(0x04);
	writel(reg, &mpddr->sor);

	/*
	 * MPDDRC DLL Master Offset Register
	 * write master + clk90 offset
	 */
	reg = ATMEL_MPDDRC_MOR_MOFF(7)
		| ATMEL_MPDDRC_MOR_CLK90OFF(0x1F)
		| ATMEL_MPDDRC_MOR_SELOFF_ENABLED | ATMEL_MPDDRC_MOR_KEY;
	writel(reg, &mpddr->mor);

	/*
	 * MPDDRC I/O Calibration Register
	 * DDR2 RZQ = 48 Ohm
	 * TZQIO = 4
	 */
	reg = readl(&mpddr->io_calibr);
	reg &= ~ATMEL_MPDDRC_IO_CALIBR_RDIV;
	reg &= ~ATMEL_MPDDRC_IO_CALIBR_TZQIO;
	// Deviate from reference, use LPDDR2 48-Ohm value.
	// Reference AT91C_MPDDRC_RDIV_DDR2_RZQ_50, 0x4, translates to 60-Ohm for LP-DDR2
	reg |= ATMEL_MPDDRC_IO_CALIBR_LPDDR2_RZQ_48;
	reg |= ATMEL_MPDDRC_IO_CALIBR_TZQIO_(4);
	writel(reg, &mpddr->io_calibr);

	/* LPDDRAM2 Controller initialize */
	lpddr2_init(ATMEL_BASE_MPDDRC, ATMEL_BASE_DDRCS, mpddr_value);

	/* lpr register is not part of the lpddr2 initialization sequence, but
	 * as with lpddr1, we want to set CLK_FR for power-down mode.
	 */
	writel(mpddr_value->lpr, &mpddr->lpr);
}

void mem_init_lpddr1(const struct atmel_mpddrc_config *mpddr_value)
{
	const struct atmel_mpddr *mpddr = (struct atmel_mpddr *)ATMEL_BASE_MPDDRC;

	u32 reg;

	configure_ddrcfg_input_buffers(true);

	/* Enable MPDDR clock */
	at91_periph_clk_enable(ATMEL_ID_MPDDRC);
	at91_system_clk_enable(AT91_PMC_DDR);

	/* Init the special register for sama5d3x */
	/* MPDDRC DLL Slave Offset Register: DDR2 configuration */
	reg = ATMEL_MPDDRC_SOR_S0OFF_1
		| ATMEL_MPDDRC_SOR_S2OFF_1
		| ATMEL_MPDDRC_SOR_S3OFF_1;
	writel(reg, &mpddr->sor);

	/* MPDDRC DLL Master Offset Register */
	/* write master + clk90 offset */
	reg = ATMEL_MPDDRC_MOR_MOFF_7
		| ATMEL_MPDDRC_MOR_CLK90OFF_31
		| ATMEL_MPDDRC_MOR_SELOFF_ENABLED
		| ATMEL_MPDDRC_MOR_KEY;
	writel(reg, &mpddr->mor);

	/* MPDDRC I/O Calibration Register */
	/* LPDDR1 RZQ = 52 Ohm */
	/* TZQIO = (133 * 10^6) * (20 * 10^-9) + 1 = 3.66 == 4 */
	reg = readl(&mpddr->io_calibr);
	reg &= ~ATMEL_MPDDRC_IO_CALIBR_RDIV;
	reg &= ~ATMEL_MPDDRC_IO_CALIBR_TZQIO;
	reg |= ATMEL_MPDDRC_IO_CALIBR_DDR2_RZQ_52;
	reg |= ATMEL_MPDDRC_IO_CALIBR_TZQIO_(4);
	writel(reg, &mpddr->io_calibr);

	reg = readl(&mpddr->hs);
	reg |= ATMEL_MPDDRC_DDR2_EN_CALIB;
	writel(reg, &mpddr->hs);

	/* LPDDRAM1 Controller initialize */
	lpddr1_init(ATMEL_BASE_MPDDRC, ATMEL_BASE_DDRCS, mpddr_value);

	configure_ddrcfg_input_buffers(false);
}

void mem_init(void)
{
#if CONFIG_SYS_SPL_MENU
	mini_spl_menu();
#endif

	u16 board_hw_id;
	board_hw_id = 0;
	laird_ram_ic_t ram_ic;
	const laird_ram_config_t* ram_config;

	if (board_hw_id > MAX_BOARD_HW_ID)
	{
		board_hw_id = MAX_BOARD_HW_ID;
		printf("Board HW id 0x%x exceeds maximum known ID of "
				"0x%x, using SOM_LEGACY_RAM_IC\n",
				board_hw_id, MAX_BOARD_HW_ID);
	}

	ram_ic = board_hw_description[board_hw_id].ram_ic;

	ram_config = &ram_configs[ram_ic];
	puts("Initializing ram module ");
	puts(ram_config->name);
	puts("\r\n");
	if (ram_config->type == RAM_TYPE_LPDDR1)
		mem_init_lpddr1(&ram_config->ddr_config);
	 else
		mem_init_lpddr2(&ram_config->ddr_config);
}

void at91_pmc_init(void)
{
	at91_plla_init(BOARD_PLLA_SETTINGS);

	at91_pllicpr_init(AT91_PMC_IPLL_PLLA(0x3));

	at91_mck_init(BOARD_PRESCALER_PLLA);
}
#endif
