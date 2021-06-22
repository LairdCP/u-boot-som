/*
 * Copyright (C) 2018 Laird Connectivity
 * 	Ben Whitten <ben.whitten@lairdconnect.com>
 * 	Boris Krasnovskiy <boris.krasnovskiy@lairdconnect.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <environment.h>
#include <debug_uart.h>

#include <asm/arch/at91_common.h>
#include <asm/arch/gpio.h>
#include <asm/arch/clk.h>

#include <asm/arch/sama5_sfr.h>
#include <asm/arch/sama5d3_smc.h>
#include <asm/arch/atmel_mpddrc.h>
#include <asm/arch/at91_sck.h>

#include <linux/ctype.h>
#include <linux/mtd/rawnand.h>

#include <uboot_aes.h>

#include <../drivers/crypto/atmel_trng.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_FIT_SIGNATURE
#define FS_MAX_KEY_SIZE	64
#define FS_KEY_WINDOW	0x31e000

struct key_prop {
	const void *cipher_key;
	int cipher_key_len;
	const void *cmac_key;
	int cmac_key_len;
	const void *iv;
};
#endif

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

	/*
	 * Set write pulse timing. This one is easy to extract:
	 *
	 * NWE_PULSE = tWP
	 */
	ncycles = DIV_ROUND_UP(timings->tWP_min, mckperiodps);
	totalcycles = ncycles;
	pulse_reg |= AT91_SMC_PULSE_NWE(ncycles);

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
	setup_reg |= AT91_SMC_SETUP_NWE(ncycles);

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
	cycle_reg |= AT91_SMC_CYCLE_NWE(ncycles);

	/*
	 * We don't want the CS line to be toggled between each byte/word
	 * transfer to the NAND. The only way to guarantee that is to have the
	 * NCS_{WR,RD}_{SETUP,HOLD} timings set to 0, which in turn means:
	 *
	 * NCS_WR_PULSE = NWE_CYCLE
	 */
	pulse_reg |= AT91_SMC_PULSE_NCS_WR(ncycles);

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
	pulse_reg |= AT91_SMC_PULSE_NRD(ncycles);

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
	cycle_reg |= AT91_SMC_CYCLE_NRD(ncycles);

	/*
	 * We don't want the CS line to be toggled between each byte/word
	 * transfer from the NAND. The only way to guarantee that is to have
	 * the NCS_{WR,RD}_{SETUP,HOLD} timings set to 0, which in turn means:
	 *
	 * NCS_RD_PULSE = NRD_CYCLE
	 */
	pulse_reg |= AT91_SMC_PULSE_NCS_RD(ncycles);

	/* Txxx timings are directly matching tXXX ones. */
	ncycles = DIV_ROUND_UP(timings->tCLR_min, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TCLR(ncycles);

	ncycles = DIV_ROUND_UP(timings->tADL_min, mckperiodps);

	/*
	 * Version 4 of the ONFI spec mandates that tADL be at least 400
	 * nanoseconds, but, depending on the master clock rate, 400 ns may not
	 * fit in the tADL field of the SMC reg.
	 *
	 * Note that previous versions of the ONFI spec had a lower tADL_min
	 * (100 or 200 ns). It's not clear why this timing constraint got
	 * increased but it seems most NANDs are fine with values lower than
	 * 400ns, so we should be safe.
	 */
	if (ncycles > 15)
		ncycles = 15;

	timings_reg |= AT91_SMC_TIMINGS_TADL(ncycles);

	ncycles = DIV_ROUND_UP(timings->tAR_min, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TAR(ncycles);

	ncycles = DIV_ROUND_UP(timings->tRR_min, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TRR(ncycles);

	ncycles = DIV_ROUND_UP(timings->tWB_max, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TWB(ncycles);

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

#ifdef CONFIG_FIT_SIGNATURE
void board_fit_image_post_process(void **p_image, size_t *p_size)
{
	u8 *image = (u8 *)*p_image;
	int enc_node, aes_blocks;
	struct key_prop prop;
	u8 key_exp[AES_EXPAND_KEY_LENGTH] __attribute__ ((aligned(4)));
	u8 padding;

	enc_node = fdt_subnode_offset(gd->fdt_blob, 0, "encryption");
	if (enc_node < 0) {
		debug("No encryption node found\n");
		return;
	}

	prop.cipher_key = fdt_getprop(gd->fdt_blob, enc_node, "aes,cipher-key",
		&prop.cipher_key_len);
	if (!prop.cipher_key)
		return;

	prop.cmac_key = fdt_getprop(gd->fdt_blob, enc_node, "aes,cmac-key",
		&prop.cmac_key_len);
	prop.iv = fdt_getprop(gd->fdt_blob, enc_node, "aes,iv", NULL);

	aes_expand_key((u8 *)prop.cipher_key, key_exp);

	puts("   Decrypting ... ");
	/* got to be 128bit */
	aes_blocks = DIV_ROUND_UP(*p_size, AES_KEY_LENGTH);
	aes_cbc_decrypt_blocks(key_exp, (u8 *)prop.iv, image, image, aes_blocks);
	puts("OK\n");

	/* Reduce PKCS7 padded length */
	padding = *(image + (*p_size - 1));
	*p_size = *p_size - padding;
}

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

#ifdef CONFIG_DEBUG_UART_BOARD_INIT
void board_debug_uart_init(void)
{
	at91_seriald_hw_init();
}
#endif

int board_late_init(void)
{
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	char name[32], *p;

	snprintf(name, sizeof(name), "%s%s", get_cpu_name(), CONFIG_LOCALVERSION);

	for (p = name; *p; ++p)
		*p = tolower(*p);

	env_set("lrd_name", name);
#endif

#ifndef CONFIG_TARGET_IG60
	if (gd->flags & GD_FLG_ENV_DEFAULT) {
		puts("Saving default environment...\n");
		env_save();
	}
#endif

#ifdef CONFIG_USB_ETHER
	usb_ether_init();
#endif

    return 0;
}

int board_early_init_f(void)
{
#ifdef CONFIG_DEBUG_UART
	debug_uart_init();
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

#ifdef CONFIG_NAND
	at91_periph_clk_enable(ATMEL_ID_SMC);

	/* Disable Flash Write Protect discrete */
	at91_set_pio_output(AT91_PIO_PORTE, 14, 1);
#endif

	atmel_trng_init();

	som60_custom_hw_init();

#ifdef CONFIG_FIT_SIGNATURE
	som60_fs_key_inject();
#endif

	return 0;
}

void board_quiesce_devices(void)
{
	atmel_trng_remove();

#ifdef CONFIG_NAND
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
	gd->ram_size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
				    CONFIG_SYS_SDRAM_SIZE);

	return 0;
}

/* SPL */
#ifdef CONFIG_SPL_BUILD
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

static void lpddr1_conf(struct atmel_mpddrc_config *lpddr1)
{
	lpddr1->md = (ATMEL_MPDDRC_MD_DBW_32_BITS | ATMEL_MPDDRC_MD_LPDDR_SDRAM);

	lpddr1->cr = LPDDR_CR;

	/*
	 * The SDRAM device requires a refresh of all rows at least every 64ms.
	 * ((64ms) / 8192) * 132MHz = 1031 i.e. 0x407
	 */
	lpddr1->rtr = 0x407;

	lpddr1->tpr0 = LPDDR_TPR0;
	lpddr1->tpr1 = LPDDR_TPR1;

	lpddr1->tpr2 = (4 << ATMEL_MPDDRC_TPR2_TRTP_OFFSET);

	lpddr1->lpr = (ATMEL_MPDDRC_LPR_LPCB_DISABLED       |
	               ATMEL_MPDDRC_LPR_CLK_FR              |
	               ATMEL_MPDDRC_LPR_PASR_(0)            |
	               ATMEL_MPDDRC_LPR_DS_(1)              |
	               ATMEL_MPDDRC_LPR_TIMEOUT_0           |
	               ATMEL_MPDDRC_LPR_ADPE_FAST           |
	               ATMEL_MPDDRC_LPR_UPD_MR_NO_UPDATE);
}

void mem_init(void)
{
	struct atmel_sfr *sfr = (struct atmel_sfr *)ATMEL_BASE_SFR;
	const struct atmel_mpddr *mpddr = (struct atmel_mpddr *)ATMEL_BASE_MPDDRC;

	struct atmel_mpddrc_config lpddr1;
	u32 reg;

	lpddr1_conf(&lpddr1);

	reg = readl(&sfr->ddrcfg);
	reg |= (ATMEL_SFR_DDRCFG_FDQIEN | ATMEL_SFR_DDRCFG_FDQSIEN);
	writel(reg, &sfr->ddrcfg);

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
		| ATMEL_MPDDRC_MOR_SELOFF_ENABLED;
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
	lpddr1_init(ATMEL_BASE_MPDDRC, ATMEL_BASE_DDRCS, &lpddr1);

	reg = readl(&sfr->ddrcfg);
	reg &= ~(ATMEL_SFR_DDRCFG_FDQIEN | ATMEL_SFR_DDRCFG_FDQSIEN);
	writel(reg, &sfr->ddrcfg);
}

void at91_pmc_init(void)
{
	at91_plla_init(BOARD_PLLA_SETTINGS);

	at91_pllicpr_init(AT91_PMC_IPLL_PLLA(0x3));

	at91_mck_init(BOARD_PRESCALER_PLLA);
}
#endif
