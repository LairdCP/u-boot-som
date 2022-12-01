/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2022 Laird Connectivity
 * Erik Strack <erik.strack@lairdconnect.com>
 *
 * Portions Copyright 2006 Freescale Semiconductor
 * York Sun (yorksun@freescale.com)
 */

#include <common.h>
#include <command.h>

extern int do_mac(struct cmd_tbl *cmdtp, int flag, int argc,
		  char *const argv[]);

U_BOOT_CMD(
	mac, 3, 1,  do_mac,
	"display and program the MAC addresses in FUSE",
	"[read|id]\n"
	"mac read\n"
	"    - print MAC addresses as colon separated string\n"
	"mac X string\n"
	"    - program MAC addr for port X [X=0,1..] to colon separated string"
);
