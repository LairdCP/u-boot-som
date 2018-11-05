/*
 * Copyright (C) 2018 Laird
 * Ben Whitten <ben.whitten@lairdtech.com
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>

#include <asm/arch/gpio.h>
#include <asm/arch/clk.h>

#include <asm/arch/sama5_sfr.h>
#include <asm/arch/sama5d3_smc.h>
#include <asm/arch/at91_common.h>
#include <asm/arch/atmel_mpddrc.h>
#include <asm/arch/atmel_usba_udc.h>
#include <asm/arch/at91_sck.h>

#include <linux/ctype.h>
#include <linux/mtd/rawnand.h>

#include <debug_uart.h>
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

static void at91sama5d3_slowclock_init(void)
{
	/*
	 * On AT91SAMA5D3 CPUs, the slow clock can be based on an
	 * internal imprecise RC oscillator or an external 32 kHz oscillator.
	 * Switch to the latter.
	 */
	unsigned tmp;
	ulong *reg = (ulong *)ATMEL_BASE_SCKCR;

	tmp = readl(reg);
	if ((tmp & AT91SAM9G45_SCKCR_OSCSEL) == AT91SAM9G45_SCKCR_OSCSEL_RC) {
		tmp &= ~AT91SAM9G45_SCKCR_OSC32EN;
		tmp |= AT91SAM9G45_SCKCR_OSC32BYP;
		writel(tmp, reg);
		udelay(200);
		tmp |= AT91SAM9G45_SCKCR_OSCSEL_32;
		writel(tmp, reg);
		udelay(200);
		tmp &= ~AT91SAM9G45_SCKCR_RCEN;
		writel(tmp, reg);
	}
}

/* Configures NAND controller from the timing table supplied */
static void atmel_smc_nand_prepare(const struct nand_sdr_timings *timings)
{
	struct at91_smc *smc = (struct at91_smc *)ATMEL_BASE_SMC;

	u32 ncycles, totalcycles, timeps, mckperiodps;
	u32 setup_reg, pulse_reg, cycle_reg, timings_reg, mode_reg;

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
}

void som60_nand_hw_init(void)
{
	/* Atmel SAMA5D3 NAND controller does not support EDO,
	   so get fastest non-EDO timing mode - mode 3
	   Any flash compatible with ONFI timing mode 3 will work now
	*/
	const struct nand_sdr_timings *timings =
		onfi_async_timing_mode_to_sdr_timings(3);

	at91_periph_clk_enable(ATMEL_ID_SMC);

	atmel_smc_nand_prepare(timings);

	/* Disable Flash Write Protect Line */
	at91_set_pio_output(AT91_PIO_PORTE, 14, 1);
}

static void som60_usb_hw_init(void)
{
    at91_udp_hw_init();

    usba_udc_probe(&pdata);
}

#ifdef CONFIG_FIT_SIGNATURE
void board_fit_image_post_process(void **p_image, size_t *p_size)
{
	u8 *image = (u8 *)*p_image;
	int enc_node, aes_blocks;
	struct key_prop prop;
	u8 key_exp[AES_EXPAND_KEY_LENGTH];
	u8 padding;

	enc_node = fdt_subnode_offset(gd->fdt_blob, 0, "encryption");
	if (enc_node < 0) {
		debug("No encryption node found\n");
		return;
	}

	prop.cipher_key = fdt_getprop(gd->fdt_blob, enc_node, "aes,cipher-key", &prop.cipher_key_len);
	prop.cmac_key = fdt_getprop(gd->fdt_blob, enc_node, "aes,cmac-key", &prop.cmac_key_len);
	prop.iv = fdt_getprop(gd->fdt_blob, enc_node, "aes,iv", NULL);

	aes_expand_key((u8 *)prop.cipher_key, key_exp);

	puts("   Decrypting ... ");
	/* got to be 128bit */
	aes_blocks = DIV_ROUND_UP(*p_size, prop.cipher_key_len);
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

	return;
}
#endif

void board_debug_uart_init(void)
{
	at91_seriald_hw_init();
}

int board_late_init(void)
{
	const char *LAIRD_NAME = "lrd_name";
	char name[32], *p;

	strcpy(name, get_cpu_name());
	for (p = name; *p != '\0'; *p = tolower(*p), p++)
		;
	strcat(name, "-som60");
	env_set(LAIRD_NAME, name);

    return 0;
}

int board_early_init_f(void)
{
#ifdef CONFIG_DEBUG_UART
	debug_uart_init();
#endif

    return 0;
}

int board_init(void)
{
	/* adress of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	at91sama5d3_slowclock_init();

#ifndef CONFIG_NAND_BOOT
	som60_nand_hw_init();
#endif

	som60_usb_hw_init();

	atmel_trng_init();

#ifdef CONFIG_FIT_SIGNATURE
	som60_fs_key_inject();
#endif

	return 0;
}

int dram_init(void)
{
	gd->ram_size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
				    CONFIG_SYS_SDRAM_SIZE);

	return 0;
}

/* SPL */
#ifdef CONFIG_SPL_BUILD
void spl_board_init(void)
{
#ifdef CONFIG_NAND_BOOT
	som60_nand_hw_init();
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

static void ddr2_conf(struct atmel_mpddrc_config *ddr2)
{
	ddr2->md = (ATMEL_MPDDRC_MD_DBW_32_BITS | ATMEL_MPDDRC_MD_LPDDR_SDRAM);

	ddr2->cr = (ATMEL_MPDDRC_CR_NC_COL_10 | //or 11 from DS
	            ATMEL_MPDDRC_CR_NR_ROW_13 |
	            ATMEL_MPDDRC_CR_CAS_DDR_CAS3 |
	            ATMEL_MPDDRC_CR_ENRDM_ON |
	            ATMEL_MPDDRC_CR_NDQS_DISABLED |
	            ATMEL_MPDDRC_CR_DECOD_INTERLEAVED |
	            ATMEL_MPDDRC_CR_UNAL_SUPPORTED);

	ddr2->rtr = 0x408;

	ddr2->tpr0 = (6 << ATMEL_MPDDRC_TPR0_TRAS_OFFSET |
	              2 << ATMEL_MPDDRC_TPR0_TRCD_OFFSET |
	              2 << ATMEL_MPDDRC_TPR0_TWR_OFFSET |
	              8 << ATMEL_MPDDRC_TPR0_TRC_OFFSET |
	              2 << ATMEL_MPDDRC_TPR0_TRP_OFFSET |
	              2 << ATMEL_MPDDRC_TPR0_TRRD_OFFSET |
	              2 << ATMEL_MPDDRC_TPR0_TWTR_OFFSET |
	              2 << ATMEL_MPDDRC_TPR0_TMRD_OFFSET);

	ddr2->tpr1 = (2 << ATMEL_MPDDRC_TPR1_TXP_OFFSET |
	              200 << ATMEL_MPDDRC_TPR1_TXSRD_OFFSET |
	              19 << ATMEL_MPDDRC_TPR1_TXSNR_OFFSET |
	              18 << ATMEL_MPDDRC_TPR1_TRFC_OFFSET);

	ddr2->tpr2 = (7 << ATMEL_MPDDRC_TPR2_TFAW_OFFSET |
	              2 << ATMEL_MPDDRC_TPR2_TRTP_OFFSET |
	              3 << ATMEL_MPDDRC_TPR2_TRPA_OFFSET |
	              7 << ATMEL_MPDDRC_TPR2_TXARDS_OFFSET |
	              2 << ATMEL_MPDDRC_TPR2_TXARD_OFFSET);
}

void mem_init(void)
{
	struct atmel_sfr *sfr = (struct atmel_sfr *)ATMEL_BASE_SFR;
	struct atmel_mpddrc_config ddr2;

	ddr2_conf(&ddr2);

	writel(ATMEL_SFR_DDRCFG_FDQIEN | ATMEL_SFR_DDRCFG_FDQSIEN,
	       &sfr->ddrcfg);

	/* Enable MPDDR clock */
	at91_periph_clk_enable(ATMEL_ID_MPDDRC);
	at91_system_clk_enable(AT91_PMC_DDR);

	/* DDRAM2 Controller initialize */
	ddr2_init(ATMEL_BASE_MPDDRC, ATMEL_BASE_DDRCS, &ddr2);
}

void at91_pmc_init(void)
{
	u32 tmp;

	tmp = AT91_PMC_PLLAR_29 |
	      AT91_PMC_PLLXR_PLLCOUNT(0x3f) |
	      AT91_PMC_PLLXR_MUL(43) |
	      AT91_PMC_PLLXR_DIV(1);
	at91_plla_init(tmp);

	at91_pllicpr_init(AT91_PMC_IPLL_PLLA(0x3));

	tmp = AT91_PMC_MCKR_MDIV_4 |
	      AT91_PMC_MCKR_CSS_PLLA;
	at91_mck_init(tmp);
}
#endif
