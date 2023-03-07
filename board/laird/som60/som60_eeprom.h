/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2022 Laird Connectivity
 * Erik Strack <erik.strack@lairdconnect.com>
 *
 * Portions Copyright 2006 Freescale Semiconductor
 * York Sun (yorksun@freescale.com)
 */
#ifndef SOM60_EEPROM_H
#define SOM60_EEPROM_H

#include <common.h>
#include <command.h>

void set_mac_address(const char *string, bool);
bool is_valid_ether_addr(const u8 *addr);
void read_show_mac(bool);
int enetaddr_nvmem_read(u8 *enetaddr, int i);
u16 board_hw_id_nvmem_read(void);
void board_hw_id_nvmem_write(u16 val);

#endif
