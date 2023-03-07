/*
 * Copyright (C) 2022 Laird Connectivity
 * Erik Strack <erik.strack@lairdconnect.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <init.h>
#include <env.h>
#include <net.h>
#include <debug_uart.h>
#include <watchdog.h>
#include <wdt.h>
#include <linux/delay.h>
#include <serial.h>

#include <../drivers/serial/atmel_usart.h>

#include <som60_eeprom.h>

#include <asm/arch/at91_common.h>
#include <asm/arch/gpio.h>

#include <linux/ctype.h>

#define MINI_MENU_INPUT_SIZE    20

DECLARE_GLOBAL_DATA_PTR;

/* Because console code expects console infrastructure fully initialized, but
 * SPL by default has only uart support, don't utilize console for a simple
 * menu in the SPL.  Ugly, but will keep things cleaner and not require
 * initialization of the full console from the SPL, which is at least
 * non-trivial.  (simply invoking console_init is not appropriate)
 */
static void spl_gets(char* chr, size_t sz)
{
	size_t i = 0;
	for(;;) {
		*chr = serial_getc();
		if (*chr == '\b' || (*chr == 127)) {
			if (i == 0)
				continue;
			putc('\b');
			chr--;
			i--;
		}
		else {
			putc(*chr);
			if ((*chr == '\r') || (*chr == '\n')) {
				*chr = '\0';
				break;
			}
			chr++;
			i++;
			if (i >= sz) {
				chr--;
				*chr = '\0';
				break;
			}
		}
	}

	puts("\r\n");
	return;
}

void mini_spl_menu(void)
{
	u32 val;
	char chr;
	char buf[MINI_MENU_INPUT_SIZE];

#if defined(CONFIG_SPL_WATCHDOG) && CONFIG_IS_ENABLED(WDT)
	initr_watchdog();
#endif
	for(;;) {
		puts("\nmini spl menu\n");
		puts("0. hw id set\n");
		puts("1. hw id read\n");
		puts("2. mac set\n");
		puts("3. mac read\n");
		puts("4. DVK mac set\n");
		puts("5. DVK mac read\n");
		puts("a. continue boot\n");
		chr = tolower(serial_getc());
		switch(chr) {
		case '0':
			puts("hw id set\n");
			val = board_hw_id_nvmem_read();
			if (val != 0) {
				puts("hw id already programmed - write prohibited\n");
				break;
			}
			puts("enter hw id in hexadecimal\n");
			spl_gets(buf, MINI_MENU_INPUT_SIZE);
			val = simple_strtoul(buf, NULL, 16) & 0xff;
			printf("write HW ID as 0x%x\n", val);
			board_hw_id_nvmem_write(val);
			break;
		case '1':
			puts("hw id read\n");
			val = board_hw_id_nvmem_read();
			printf("hw id: 0x%x\n", val);
			break;
		case '2':
			puts("set mac addresses som60v2\n");
			puts("enter mac address 1\n");
			spl_gets(buf, MINI_MENU_INPUT_SIZE);
			set_mac_address(buf, false);
			break;
		case '3':
			puts("mac read\n");
			read_show_mac(false);
			break;
		case '4':
			puts("set mac addresses DVK\n");
			puts("enter mac address 1\n");
			spl_gets(buf, MINI_MENU_INPUT_SIZE);
			set_mac_address(buf, true);
			break;
		case '5':
			puts("mac read DVK\n");
			read_show_mac(true);
			break;
		case 'a':
			return;
		default:
			puts("Unknown option ");
			putc(chr);
			puts("\n");
			break;
		}
	}
}
