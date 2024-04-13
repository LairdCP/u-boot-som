// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Boris Krasnovskiy <boris.krasnovskiy@ezurio.com>
 */

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <rng.h>
#include <asm/io.h>

struct atmel_rng_plat {
	fdt_addr_t base;
	struct clk clk;
};

#define TRNG_CR		0x00
#define TRNG_MR		0x04
#define TRNG_ISR	0x1c
#define TRNG_ODATA	0x50

#define TRNG_KEY	0x524e4700 /* RNG */

#define TRNG_HALFR	BIT(0) /* generate RN every 168 cycles */

struct atmel_rng_data {
	bool has_half_rate;
};

/**
 * atmel_rng_read() - fill buffer with random bytes
 *
 * @buffer:	buffer to receive data
 * @size:	size of buffer
 *
 * Return:	0
 */
static int atmel_rng_read(struct udevice *dev, void *data, size_t len)
{
	struct atmel_rng_plat *pdata = dev_get_plat(dev);
	size_t step;
	u32 reg;

	while (len > 0) {
		if (!(readl(pdata->base + TRNG_ISR) & 1)) {
			schedule();
			continue;
		}

		reg = readl(pdata->base + TRNG_ODATA);

		/* Clear ready flag again in case it have changed */
		readl(pdata->base + TRNG_ISR);

		step = min(len, sizeof(u32));
		memcpy(data, &reg, step);
		data += step;
		len -= step;
	}

	return (int) len;
}

/**
 * atmel_rng_probe() - probe rng device
 *
 * @dev:	device
 * Return:	0 if ok
 */
static int atmel_rng_probe(struct udevice *dev)
{
	struct atmel_rng_plat *pdata = dev_get_plat(dev);
	struct atmel_rng_data *data;
	int err;

	data = (void*)dev_get_driver_data(dev);
	if (!data)
		return -ENODEV;

	err = clk_enable(&pdata->clk);
	if (err)
		return err;

	if (data->has_half_rate) {
		unsigned long rate = clk_get_rate(&pdata->clk);

		/* if peripheral clk is above 100MHz, set HALFR */
		if (rate > 100000000)
			writel(TRNG_HALFR, pdata->base + TRNG_MR);
	}


	writel(TRNG_KEY | 1, pdata->base + TRNG_CR);

	return 0;
}

/**
 * atmel_rng_remove() - deinitialize rng device
 *
 * @dev:	device
 * Return:	0 if ok
 */
static int atmel_rng_remove(struct udevice *dev)
{
	struct atmel_rng_plat *pdata = dev_get_plat(dev);

	writel(TRNG_KEY, pdata->base + TRNG_CR);

	return clk_disable(&pdata->clk);
}

/**
 * atmel_rng_of_to_plat() - transfer device tree data to plaform data
 *
 * @dev:	device
 * Return:	0 if ok
 */
static int atmel_rng_of_to_plat(struct udevice *dev)
{
	struct atmel_rng_plat *pdata = dev_get_plat(dev);
	int err;

	pdata->base = dev_read_addr(dev);
	if (!pdata->base)
		return -ENODEV;

	/* Get optional "core" clock */
	err = clk_get_by_index(dev, 0, &pdata->clk);
	if (err)
		return err;

	return 0;
}

static const struct dm_rng_ops atmel_rng_ops = {
	.read = atmel_rng_read,
};

static const struct atmel_rng_data at91sam9g45_config = {
	.has_half_rate = false,
};

static const struct atmel_rng_data sam9x60_config = {
	.has_half_rate = true,
};

static const struct udevice_id atmel_rng_match[] = {
	{
		.compatible = "atmel,at91sam9g45-trng",
		.data = (ulong)&at91sam9g45_config,
	},
	{
		.compatible = "microchip,sam9x60-trng",
		.data = (ulong)&sam9x60_config,
	},
	{},
};

U_BOOT_DRIVER(atmel_rng) = {
	.name = "atmel-rng",
	.id = UCLASS_RNG,
	.of_match = atmel_rng_match,
	.ops = &atmel_rng_ops,
	.probe = atmel_rng_probe,
	.remove = atmel_rng_remove,
	.plat_auto	= sizeof(struct atmel_rng_plat),
	.of_to_plat = atmel_rng_of_to_plat,
};