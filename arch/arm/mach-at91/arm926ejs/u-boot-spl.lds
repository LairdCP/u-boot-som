/*
 * Copyright (C) 2015 Atmel Corporation
 *		      Bo Shen <voice.shen@atmel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

MEMORY { .sram : ORIGIN = CONFIG_SPL_TEXT_BASE, \
		LENGTH = CONFIG_SPL_MAX_SIZE }
MEMORY { .sdram : ORIGIN = CONFIG_SPL_BSS_START_ADDR, \
		LENGTH = CONFIG_SPL_BSS_MAX_SIZE }

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
SECTIONS
{
	. = ALIGN(4);
	.text      :
	{
		__image_copy_start = .;
		*(.vectors)
		arch/arm/cpu/arm926ejs/start.o	(.text*)
		*(.text*)
		*(.glue*)
	} >.sram

	. = ALIGN(4);
	.rodata : { *(SORT_BY_ALIGNMENT(.rodata*)) } >.sram

	. = ALIGN(4);
	.data : { *(SORT_BY_ALIGNMENT(.data*)) } >.sram

	. = ALIGN(4);
	.u_boot_list : { KEEP(*(SORT(.u_boot_list*))) } > .sram

	. = ALIGN(4);
	__image_copy_end = .;

	.end :
	{
		*(.__end)
	} >.sram

	_image_binary_end = .;

	.bss (NOLOAD) :
	{
		. = ALIGN(4);
		__bss_start = .;
		*(.bss*)
		. = ALIGN(4);
		__bss_end = .;
	} >.sdram
}
