/*
 * Copyright (c) 2011 Peter Korsgaard <jacmet@sunsite.dk>
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/at91_pmc.h>

#define TRNG_CR		0x00
#define TRNG_ISR	0x1c
#define TRNG_ODATA	0x50

#define TRNG_KEY	0x524e4700 /* RNG */

static u32 atmel_trng_read(void)
{
	u32 data;

	/* data ready? */
	while ((readl(ATMEL_BASE_TRNG + TRNG_ISR) & 1) == 0) ;

	data = readl(ATMEL_BASE_TRNG + TRNG_ODATA);
	/*
	  ensure data ready is only set again AFTER the next data
	  word is ready in case it got set between checking ISR
	  and reading ODATA, so we don't risk re-reading the
	  same word
	*/
	readl(ATMEL_BASE_TRNG + TRNG_ISR);

	return data;
}

static void inline atmel_trng_enable(void)
{
	writel(TRNG_KEY | 1, ATMEL_BASE_TRNG + TRNG_CR);
}

static void inline  atmel_trng_disable(void)
{
	writel(TRNG_KEY, ATMEL_BASE_TRNG + TRNG_CR);
}

void atmel_trng_init(void)
{
	at91_periph_clk_enable(ATMEL_ID_TRNG);
	atmel_trng_enable();
}

void atmel_trng_remove(void)
{
	atmel_trng_disable();
	at91_periph_clk_disable(ATMEL_ID_TRNG);
}

unsigned int rand(void)
{
	return atmel_trng_read();
}

unsigned int rand_r(unsigned int *seedp)
{
	return atmel_trng_read();
}

void srand(unsigned int seed)
{
}

