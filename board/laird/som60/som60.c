/*
 * Copyright (C) 2018 Laird
 * Ben Whitten <ben.whitten@lairdtech.com
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/sama5_sfr.h>
#include <asm/arch/sama5d3_smc.h>
#include <asm/arch/at91_common.h>
#include <asm/arch/at91_rstc.h>
#include <asm/arch/gpio.h>
#include <asm/arch/clk.h>
#include <debug_uart.h>
#include <spl.h>
#include <asm/arch/atmel_mpddrc.h>
#include <asm/arch/at91_wdt.h>
#include <asm/arch/atmel_usba_udc.h>
#include <linux/ctype.h>
#include <uboot_aes.h>

DECLARE_GLOBAL_DATA_PTR;

struct key_prop {
	const void *cipher_key;
	int cipher_key_len;
	const void *cmac_key;
	int cmac_key_len;
	const void *iv;
};

void som60_nand_hw_init(void)
{
	struct at91_smc *smc = (struct at91_smc *)ATMEL_BASE_SMC;

	at91_periph_clk_enable(ATMEL_ID_SMC);

	/* Configure SMC CS3 for NAND/SmartMedia */
	writel(AT91_SMC_SETUP_NWE(2) | AT91_SMC_SETUP_NCS_WR(1) |
	       AT91_SMC_SETUP_NRD(2) | AT91_SMC_SETUP_NCS_RD(1),
	       &smc->cs[3].setup);
	writel(AT91_SMC_PULSE_NWE(2) | AT91_SMC_PULSE_NCS_WR(4) |
	       AT91_SMC_PULSE_NRD(2) | AT91_SMC_PULSE_NCS_RD(4),
	       &smc->cs[3].pulse);
	writel(AT91_SMC_CYCLE_NWE(4) | AT91_SMC_CYCLE_NRD(4),
	       &smc->cs[3].cycle);
	writel(AT91_SMC_TIMINGS_TCLR(2) | AT91_SMC_TIMINGS_TADL(11) |
	       AT91_SMC_TIMINGS_TAR(2)  | AT91_SMC_TIMINGS_TRR(3)   |
	       AT91_SMC_TIMINGS_TWB(5)  | AT91_SMC_TIMINGS_RBNSEL(3)|
	       AT91_SMC_TIMINGS_NFSEL(1), &smc->cs[3].timings);
	writel(AT91_SMC_MODE_RM_NRD | AT91_SMC_MODE_WM_NWE |
	       AT91_SMC_MODE_EXNW_DISABLE |
	       AT91_SMC_MODE_DBW_8 |
	       AT91_SMC_MODE_TDF_CYCLE(9), /* 65ns, 7.5757ns clock = 8.58 */
	       &smc->cs[3].mode);

	/* Disable Flash Write Protect Line */
	at91_set_pio_output(AT91_PIO_PORTE, 14, 1);
}

static void som60_usb_hw_init(void)
{
    at91_udp_hw_init();

    usba_udc_probe(&pdata);
}

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
	debug_uart_init();

    return 0;
}

int board_init(void)
{
	/* adress of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	som60_nand_hw_init();
	som60_usb_hw_init();

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
	som60_nand_hw_init();
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
	/*
	 * As the DDR2-SDRAm device requires a refresh time is 7.8125us
	 * when DDR run at 133MHz, so it needs (7.8125us * 133MHz / 10^9) clocks
	 */
    /* TODO DDR running at 132MHz, 7.5757ns <--- NOT 133Mhz */

	ddr2->rtr = 0x411;

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
