/*
 * Copyright (C) 2022 Laird Connectivity
 * Erik Strack <erik.strack@lairdconnect.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <console.h>
#include <errno.h>
#include <linux/delay.h>
#include <fuse.h>
#include <clk.h>
#include <dm.h>
#include <misc.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/at91_fuse.h>

DECLARE_GLOBAL_DATA_PTR;

struct at91_fuse_priv {
	u32 base;
	const struct at91_fuse_params *params;
};

struct at91_fuse_params {
	unsigned int nregs;
};

/* Legacy fuse API methods */
int fuse_read(u32 bank, u32 word, u32 *val)
{
	int ret;
	struct udevice *fuse_dev;
	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_DRIVER_GET(at91_fuse),
					  &fuse_dev);
	if (ret)
		return ret;

	if (bank > 0) {
		printf("Invalid bank argument, only bank 0 supported\r\n");
		return -EINVAL;
	}
	if (word > 7) {
		printf("Invalid word argument, only words 0-7 supported\r\n");
		return -EINVAL;
	}
	if (misc_read(fuse_dev, word * 4, val, 4) != 4)
		return -EIO;

	return 0;
}

int fuse_sense(u32 bank, u32 word, u32 *val)
{
	return fuse_read(bank, word, val);
}

int fuse_prog(u32 bank, u32 word, u32 val)
{
	int ret;
	struct udevice *fuse_dev;
	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_DRIVER_GET(at91_fuse),
					  &fuse_dev);
	if (ret)
		return ret;

	if (bank > 0) {
		printf("Invalid bank argument, only bank 0 supported\r\n");
		return -EINVAL;
	}
	if (word > 7) {
		printf("Invalid word argument, only words 0-7 supported\r\n");
		return -EINVAL;
	}
	printf("writing...\r\n");
	if (misc_write(fuse_dev, word * 4, &val, 4) != 4)
		return -EIO;

	return 0;
}

int fuse_override(u32 bank, u32 word, u32 val)
{
	return -EFAULT;
}

/* nvmem API methods */

static void busy_wait(struct at91_fuse_reg *fuse)
{
	do udelay(100000); while ((readl(&fuse->fir)
		& (AT91_FUSE_FIR_WS | AT91_FUSE_FIR_RS)) !=
		(AT91_FUSE_FIR_WS | AT91_FUSE_FIR_RS));
}

static int at91_fuse_read(struct udevice *dev, int offset,
			     void *val, int bytes)
{
	struct at91_fuse_priv *priv = dev_get_priv(dev);
	struct at91_fuse_reg *fuse = (struct at91_fuse_reg *) priv->base;
	unsigned int count;
	u8 *buf;
	int i;
	u32 index, num_bytes, bytes_remaining, bytes_copy;
	u32 temp;
	u8 *cpy_start;

	if (!priv)
		return -ENODEV;

	index = offset >> 2;
	num_bytes = round_up((offset % 4) + bytes, 4);
	count = num_bytes >> 2;

	if (count > (priv->params->nregs - index))
		count = priv->params->nregs - index;

	buf = val;

	cpy_start = ((u8 *) &temp) + (offset % 4);
	bytes_remaining = bytes;
	bytes_copy = min_t(u32, bytes_remaining, 4 - (offset % 4));
	for (i = index; i < (index + count); i++) {
		temp = readl(&fuse->fsr[i]);
		memcpy(buf, cpy_start, bytes_copy);
		buf += bytes_copy;
		bytes_remaining -= bytes_copy;
		cpy_start = (u8 *) &temp;
		bytes_copy = min_t(u32, bytes_remaining, 4);
	}

	return bytes - bytes_remaining;
}

static void write_word(struct at91_fuse_reg *fuse, int word, int val)
{
	writel(word << AT91_FUSE_FIR_WSEL_OFFSET, &fuse->fir);
	writel(val, &fuse->fdr);

	busy_wait(fuse);
	writel(AT91_FUSE_FCR_WRQ | AT91_FUSE_FCR_VALID_KEY_CODE, &fuse->fcr);
	busy_wait(fuse);
}

static int at91_fuse_write(struct udevice *dev, int offset,
			      const void *val, int bytes)
{
	struct at91_fuse_priv *priv = dev_get_priv(dev);
	struct at91_fuse_reg *fuse = (struct at91_fuse_reg *) priv->base;
	struct clk_bulk clk_bulk;
	unsigned int count;
	const u8 *buf;
	int i;
	u32 index, num_bytes, bytes_remaining, bytes_copy;
	u32 temp;
	u8 *cpy_start;
	u32 w_check;
	int ret;

	if (!priv)
		return -ENODEV;

	if ((ret = clk_get_bulk(dev, &clk_bulk)))
	{
		printf("clk_get_bulk failed with %d\n", ret);
		return ret;
	}

	if ((ret = clk_enable_bulk(&clk_bulk)))
	{
		printf("enable clocks failed with %d\n", ret);
		return ret;
	}

	fuse_read(0, AT91_FUSE_W_WORD, &w_check);
	if (w_check & AT91_FUSE_FSR5_W)
		printf("Warning - bit 160 - \"W\" is set, FUSE writing may be disabled\r\n");

	index = offset >> 2;
	num_bytes = round_up((offset % 4) + bytes, 4);
	count = num_bytes >> 2;

	if (count > (priv->params->nregs - index))
		count = priv->params->nregs - index;

	buf = val;

	cpy_start = ((u8 *) &temp) + (offset % 4);
	bytes_remaining = bytes;
	bytes_copy = min_t(u32, bytes_remaining, 4 - (offset % 4));
	for (i = index; i < (index + count); i++) {
		temp = 0;
		memcpy(cpy_start, buf, bytes_copy);
		write_word(fuse, i, temp);
		buf += bytes_copy;
		bytes_remaining -= bytes_copy;
		cpy_start = (u8 *) &temp;
		bytes_copy = min_t(u32, bytes_remaining, 4);
	}

	/* Update cache by hitting RRQ in FUSE_CR, wait for RS & WS of FUSE_IR to be at level 1 */
	writel(AT91_FUSE_FCR_RRQ | AT91_FUSE_FCR_VALID_KEY_CODE, &fuse->fcr);
	/* Do not turn clocks off until not busy */
	busy_wait(fuse);
	clk_release_bulk(&clk_bulk);
	return bytes - bytes_remaining;
}

static const struct misc_ops at91_fuse_ops = {
	.read = at91_fuse_read,
	.write = at91_fuse_write,
};

static int at91_fuse_probe(struct udevice *dev)
{
	struct at91_fuse_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr(dev);
	if (!priv->base)
		return -ENODEV;
	priv->params = (struct at91_fuse_params *) dev->driver_data;
	if (!priv->params)
		return -ENODEV;

	return 0;
}

static const struct at91_fuse_params atsama5d3_params = {
	.nregs = 8, /* fuse registers 0 through 7 */
};

static const struct udevice_id at91_fuse_ids[] = {
	{ .compatible = "atmel,sama5d3-fuse", .data = (ulong) &atsama5d3_params },
	{}
};

U_BOOT_DRIVER(at91_fuse) = {
	.name = "at91_fuse",
	.id = UCLASS_MISC,
	.of_match = at91_fuse_ids,
	.priv_auto = sizeof(struct at91_fuse_priv),
	.ops = &at91_fuse_ops,
	.probe = at91_fuse_probe,
};

