// SPDX-License-Identifier: LicenseRef-Ezurio-Clause
/*
 * Copyright (C) 2022 Ezurio
 */

#ifndef SOM60_EEPROM_H
#define SOM60_EEPROM_H

int set_mac_address(const char *buf, bool use_dvk);
int read_show_mac(bool use_dvk);

int board_hw_id_nvmem_read(void);
int board_hw_id_nvmem_write(uint16_t val);

#endif
