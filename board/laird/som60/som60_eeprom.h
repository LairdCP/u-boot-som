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

int set_mac_address(const char *buf, bool);
int read_show_mac(bool);

int board_hw_id_nvmem_read(void);
int board_hw_id_nvmem_write(u16 val);

#endif
