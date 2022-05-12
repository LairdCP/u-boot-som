// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019, Linaro Limited
 */

#define LOG_CATEGORY UCLASS_RNG

#include <common.h>
#include <dm.h>
#include <rng.h>

int dm_rng_read(struct udevice *dev, void *buffer, size_t size)
{
	const struct dm_rng_ops *ops = device_get_ops(dev);

	if (!ops->read)
		return -ENOSYS;

	return ops->read(dev, buffer, size);
}

#ifdef CONFIG_LIB_HW_RAND
unsigned int rand(void)
{
	struct udevice *dev;
	unsigned int val = 0;

	int ret = uclass_get_device(UCLASS_RNG, 0, &dev);
	if (!ret)
		ret = dm_rng_read(dev, &val, 4);

	return val;
}

unsigned int rand_r(unsigned int *seedp)
{
	return rand();
}

void srand(unsigned int seed)
{
}
#endif

UCLASS_DRIVER(rng) = {
	.name = "rng",
	.id = UCLASS_RNG,
};
