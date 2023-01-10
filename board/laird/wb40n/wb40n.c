/*
 * Copyright (C) 2020 Laird Connectivity
 * Boris Krasnovskiy <boris.krasnovskiy@lairdconnect.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <init.h>
#include <env.h>
#include <net.h>
#include <debug_uart.h>

#include <asm/arch/at91_common.h>
#include <asm/arch/gpio.h>

DECLARE_GLOBAL_DATA_PTR;

/* ------------------------------------------------------------------------- */
/*
 * Miscelaneous platform dependent initializations
 */

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

	if (gd->flags & GD_FLG_ENV_DEFAULT) {
		puts("Saving default environment...\n");
		env_save();
	}

#ifdef CONFIG_USB_ETHER
	usb_ether_init();
#endif

	return 0;
}

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	return 0;
}

int dram_init(void)
{
	gd->ram_size = get_ram_size((void *) CONFIG_SYS_SDRAM_BASE,
					CONFIG_SYS_SDRAM_SIZE);
	return 0;
}
