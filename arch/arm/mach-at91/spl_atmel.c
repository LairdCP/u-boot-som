// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2013 Atmel Corporation
 *		      Bo Shen <voice.shen@atmel.com>
 */

#include <common.h>
#include <hang.h>
#include <init.h>
#include <log.h>
#include <asm/io.h>
#include <asm/arch/at91_common.h>
#include <asm/arch/at91_pit.h>
#include <asm/arch/at91_pmc.h>
#include <asm/arch/at91_rstc.h>
#include <asm/arch/at91_wdt.h>
#include <asm/arch/clk.h>
#include <spl.h>

static void switch_to_main_crystal_osc(void)
{
	struct at91_pmc *pmc = (struct at91_pmc *)ATMEL_BASE_PMC;
	u32 tmp;

	/*
	 * Enable the Main Crystal Oscillator
	 * tST_max = 2ms
	 * Startup Time: 32768 * 2ms / 8 = 8
	 */
	tmp = readl(&pmc->mor);
	tmp &= ~AT91_PMC_MOR_OSCOUNT(0xff);
	tmp &= ~AT91_PMC_MOR_KEY(0xff);
	tmp |= AT91_PMC_MOR_MOSCEN;
	tmp |= AT91_PMC_MOR_OSCOUNT(8);
	tmp |= AT91_PMC_MOR_KEY(0x37);
	writel(tmp, &pmc->mor);
	while (!(readl(&pmc->sr) & AT91_PMC_IXR_MOSCS))
		;

#if defined(CONFIG_SAMA5D2)
	/* Enable a measurement of the external oscillator */
	tmp = readl(&pmc->mcfr);
	tmp |= AT91_PMC_MCFR_CCSS_XTAL_OSC;
	tmp |= AT91_PMC_MCFR_RCMEAS;
	writel(tmp, &pmc->mcfr);

	while (!(readl(&pmc->mcfr) & AT91_PMC_MCFR_MAINRDY))
		;

	if (!(readl(&pmc->mcfr) & AT91_PMC_MCFR_MAINF_MASK))
		hang();
#endif

	/* Switch from internal 12MHz RC to the Main Crystal Oscillator */
	tmp = readl(&pmc->mor);
/*
 * some boards have an external oscillator with driving.
 * in this case we need to disable the internal SoC driving (bypass mode)
 */
#if defined(CONFIG_SPL_AT91_MCK_BYPASS)
	tmp |= AT91_PMC_MOR_OSCBYPASS;
#else
	tmp &= ~AT91_PMC_MOR_OSCBYPASS;
#endif
	tmp &= ~AT91_PMC_MOR_KEY(0xff);
	tmp |= AT91_PMC_MOR_KEY(0x37);
	writel(tmp, &pmc->mor);

	tmp = readl(&pmc->mor);
	tmp |= AT91_PMC_MOR_MOSCSEL;
	tmp &= ~AT91_PMC_MOR_KEY(0xff);
	tmp |= AT91_PMC_MOR_KEY(0x37);
	writel(tmp, &pmc->mor);

	while (!(readl(&pmc->sr) & AT91_PMC_IXR_MOSCSELS))
		;

#if 0
#if !defined(CONFIG_SAMA5D2)
	/* Wait until MAINRDY field is set to make sure main clock is stable */
	while (!(readl(&pmc->mcfr) & AT91_PMC_MAINRDY))
		;
#endif
#endif

#if !defined(CONFIG_SAMA5D4) && !defined(CONFIG_SAMA5D2)
	/* Disable the 12MHz RC Oscillator */
	tmp = readl(&pmc->mor);
	tmp &= ~AT91_PMC_MOR_MOSCRCEN;
	tmp &= ~AT91_PMC_MOR_KEY(0xff);
	tmp |= AT91_PMC_MOR_KEY(0x37);
	writel(tmp, &pmc->mor);
#endif
}

__weak void matrix_init(void)
{
	/* This only be used for sama5d4 soc now */
}

__weak void redirect_int_from_saic_to_aic(void)
{
	/* This only be used for sama5d4 soc now */
}

void s_init(void)
{
	/* Setup cpu and peripheral clock here for debug uart baud rate */
	switch_to_main_crystal_osc();

#ifdef CONFIG_SAMA5D2
	configure_2nd_sram_as_l2_cache();
#endif

#if !defined(CONFIG_WDT_AT91)
	/* disable watchdog */
	at91_disable_wdt();
#endif

	/* PMC configuration */
	at91_pmc_init();

	matrix_init();

	redirect_int_from_saic_to_aic();
}

void board_init_f(ulong dummy)
{
	int ret;

	at91_clock_init(CFG_SYS_AT91_MAIN_CLOCK);

#if CONFIG_IS_ENABLED(OF_CONTROL)
	ret = spl_early_init();
	if (ret) {
		debug("spl_early_init() failed: %d\n", ret);
		hang();
	}
#endif

	timer_init();

	board_early_init_f();

	mem_init();

	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}

	preloader_console_init();

}
