/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2022 Laird Connectivity
 * Erik Strack <erik.strack@lairdconnect.com>
 *
 * Portions Copyright 2006 Freescale Semiconductor
 * York Sun (yorksun@freescale.com)
 */
#ifndef SOM60_FUSE_H

#include <common.h>
#include <command.h>

void set_mac_address(const char *string);
bool is_valid_ether_addr(const u8 *addr);
void read_show_mac(void);
int enetaddr_fuse_read(uint8_t *enetaddr, int i);
u16 board_hw_id_fuse_read(void);
void board_hw_id_fuse_write(u16 val);
int do_mac(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]);

#endif
