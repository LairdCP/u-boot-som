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
#if   CONFIG_DEBUG_UART_BASE == ATMEL_BASE_DBGU   // serial0
	at91_seriald_hw_init();
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_USART0 // serial1
	at91_serial0_hw_init();
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_USART1 // serial2
	at91_serial1_hw_init();
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_USART2 // serial3
	at91_serial2_hw_init();
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_USART3 // serial4
	at91_pio3_set_b_periph(AT91_PIO_PORTC, 22, 0);	/* TXD4 */
	at91_pio3_set_b_periph(AT91_PIO_PORTC, 23, 1);	/* RXD4 */

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_USART3);
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_UART0  // serial5
	at91_pio3_set_c_periph(AT91_PIO_PORTC, 8, 0);	/* TXD5 */
	at91_pio3_set_c_periph(AT91_PIO_PORTC, 9, 1);	/* RXD5 */

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_UART0);
#elif CONFIG_DEBUG_UART_BASE == ATMEL_BASE_UART1  // serial6
	at91_pio3_set_c_periph(AT91_PIO_PORTC, 16, 0);	/* TXD6 */
	at91_pio3_set_c_periph(AT91_PIO_PORTC, 17, 1);	/* RXD6 */

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_UART1);
#else
	#error "Unknown debug port specified"
#endif
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

#ifdef CONFIG_MTD_RAW_NAND
	/* Disable Flash Write Protect Line */
	at91_set_pio_output(AT91_PIO_PORTD, 10, 1);
#endif

	return 0;
}

int dram_init(void)
{
	gd->ram_size = get_ram_size((void *) CONFIG_SYS_SDRAM_BASE,
					CONFIG_SYS_SDRAM_SIZE);

	return 0;
}
