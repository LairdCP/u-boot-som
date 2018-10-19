/*
 * Copyright (C) 2018 Laird
 * Don Ferencz <donald.ferencz@lairdtech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/at91_common.h>
#include <asm/arch/gpio.h>
#include <spl.h>
#include <linux/ctype.h>

#include "som60_def.h"

void som60_custom_hw_init()
{
	/* Turn off all LEDs, except power red/green to indicate U-Boot */
	at91_set_pio_output(AT91_PIO_PORTA, 1, 1);
	at91_set_pio_output(AT91_PIO_PORTA, 3, 1);
	at91_set_pio_output(AT91_PIO_PORTA, 6, 1);
	at91_set_pio_output(AT91_PIO_PORTA, 13, 1);
	at91_set_pio_output(AT91_PIO_PORTA, 15, 1);
	at91_set_pio_output(AT91_PIO_PORTA, 16, 1);
	at91_set_pio_output(AT91_PIO_PORTA, 17, 0); //ENABLE POWER_GREEN_LED
	at91_set_pio_output(AT91_PIO_PORTA, 18, 1);
	at91_set_pio_output(AT91_PIO_PORTA, 25, 1);
	at91_set_pio_output(AT91_PIO_PORTA, 27, 1);
	at91_set_pio_output(AT91_PIO_PORTA, 29, 0); //ENABLE POWER_RED_LED}
}
