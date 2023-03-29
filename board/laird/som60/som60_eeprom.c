/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2022 Laird Connectivity
 * Erik Strack <erik.strack@lairdconnect.com>
 *
 * Portions Copyright 2006 Freescale Semiconductor
 * York Sun (yorksun@freescale.com)
 */

#include <common.h>
#include <command.h>
#include <dm/uclass.h>
#include <nvmem.h>
#include <net.h>

extern int save_env;

static int nvmem_cell_rw(const char *name, bool use_dvk, bool write, void *p, size_t size)
{
	const char *device = use_dvk ? "eeprom@57" : "eeprom@50";
	struct udevice *eeprom_dev;
	struct nvmem_cell cell;
	int ret;

	ret = uclass_get_device_by_name(UCLASS_I2C_EEPROM, device, &eeprom_dev);
	if (ret)
		return ret;

	/* Check if the device has a valid MAC address in nvmem */
	ret = nvmem_cell_get_by_name(eeprom_dev, name, &cell);
	if (ret)
		return ret;

	if (write)
		return nvmem_cell_write(&cell, p, size);
	else
		return nvmem_cell_read(&cell, p, size);
}

int board_hw_id_nvmem_read(void)
{
	int ret;
	u16 hw_id;

	ret = nvmem_cell_rw("hw-id", false, false, &hw_id, sizeof(hw_id));
	if (ret)
		return ret;

	return hw_id;
}

int board_hw_id_nvmem_write(u16 hw_id)
{
	return nvmem_cell_rw("hw-id", false, true, &hw_id, sizeof(hw_id));
}

static int mac_nvmem_rw(u8 *mac, const char *name, bool write)
{
	bool use_dvk = !strcmp(name, "mac-address-dvk");

	return nvmem_cell_rw(name, use_dvk, write, mac, ETH_ALEN);
}

static inline int mac_nvmem_read(u8 *mac, const char *name)
{
	return mac_nvmem_rw(mac, name, false);
}

static inline int mac_nvmem_write(u8 *mac, const char *name)
{
	return mac_nvmem_rw(mac, name, true);
}

static void mac_calc_inc(u8 *src, u8 *dst, int inc)
{
	u64 m;
	memcpy(&m, src, ETH_ALEN);
	m = be64_to_cpu(m);
	m += inc << 16;
	m = cpu_to_be64(m);
	memcpy(dst, &m, ETH_ALEN);
}

static void do_read_show_mac(const char *name, unsigned i)
{
	u8 mac[ETH_ALEN];

	int ret = mac_nvmem_read(mac, name);
	if (ret) {
		printf("EEPROM Read Error %d\n", ret);
		return;
	}

	printf("Eth%u: %02x:%02x:%02x:%02x:%02x:%02x\n", i,
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * read_show_mac - display the MAC addresses
 */
void read_show_mac(bool use_dvk)
{
	if (use_dvk) {
		do_read_show_mac("mac-address-dvk", 1);
		return;
	}

	do_read_show_mac("mac-address0", 0);
	do_read_show_mac("mac-address1", 1);
}

/**
 * set_mac_address - stores a MAC address
 *
 * This function takes a pointer to MAC address string
 * (i.e."XX:XX:XX:XX:XX:XX", where "XX" is a two-digit hex number) and
 * stores it as "mac1", storing "mac0" as +1 from it.
 * So "mac1" is written as specified, and "mac0" is +1 from it.
 *
 */
int set_mac_address(const char *buf, bool use_dvk)
{
	char *p = (char*)buf;
	unsigned long val;
	int i, ret;
	u8 mac[ETH_ALEN] = { 0,0,0,0,0,0 };

	if (!buf)
		return -ERANGE;

	for (i = 0; *p && i < ETH_ALEN; i++) {
			val = simple_strtoul(p, &p, 16);
			if (val > 0xff)
				break;

			mac[i] = val & 0xff;

			if (*p == ':')
				p++;
			else if (*p)
				break;
	}

	if (i != ETH_ALEN || !is_valid_ethaddr(mac))
		return -ERANGE;

	if (use_dvk) {
		ret = mac_nvmem_write(mac, "mac-address-dvk");
		if (ret)
			return ret;
	} else {
		ret = mac_nvmem_write(mac, "mac-address1");
		if (ret)
			return ret;

		mac_calc_inc(mac, mac, 1);

		ret = mac_nvmem_write(mac, "mac-address0");
		if (ret)
			return ret;
	}

	return 0;
}

#if CONFIG_IS_ENABLED(ID_EEPROM)
/**
 * read_macs_eeproms - read the EEPROMs MAC into memory
 */
static int read_macs_eeproms(u8 *mac0, u8 *mac1)
{
	int ret = -1;

	/* Prioritize SOM60v2 EEPROM */
	ret = mac_nvmem_read(mac0, "mac-address0");
	if (ret)
		goto read_dvk;

	ret = mac_nvmem_read(mac1, "mac-address1");
	if (ret)
		goto read_dvk;

	return is_valid_ethaddr(mac0) && is_valid_ethaddr(mac1) ?
		0 : -ERANGE;

read_dvk:
#ifndef CONFIG_TARGET_IG60
	ret = mac_nvmem_read(mac1, "mac-address-dvk");
	if (ret)
		return ret;

	if (!is_valid_ethaddr(mac1))
		return -ERANGE;

	mac_calc_inc(mac1, mac0, 1);
#endif

	return ret;
}

static int eth_env_set_mac(const char *name, const u8 *mac)
{
	char buf[ARP_HLEN_ASCII + 1];

	if (eth_env_get_enetaddr(name, (uint8_t *)buf))
		return -EEXIST;

	snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	return env_set(name, buf);
}

/**
 * mac_read_from_eeprom - read the MAC addresses from EEPROM
 *
 * This function reads the MAC addresses from EEPROM and sets the
 * appropriate environment variables for each one read.
 *
 * The environment variables are only set if they haven't been set already.
 * This ensures that any user-saved variables are never overwritten.
 *
 * This function must be called after relocation.
 */
int mac_read_from_eeprom(void)
{
	u8 mac0[ETH_ALEN] = { 0,0,0,0,0,0 };
	u8 mac1[ETH_ALEN] = { 0,0,0,0,0,0 };
	int ret;

	puts("EEPROM: ");

	if (eth_env_get_enetaddr("ethaddr",  mac0) &&
	    eth_env_get_enetaddr("eth1addr", mac1)) {
		puts("Skipped\n");
		return 0;
	}

	ret = read_macs_eeproms(mac0, mac1);
	if (ret) {
		puts("No MACs found\n");
		return 0;
	}

	eth_env_set_mac("ethaddr",  mac0);
	eth_env_set_mac("eth1addr", mac1);

	save_env = 1;
	puts("MACs set\n");

	return 0;
}

int do_mac(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret;
	char cmd;

	if (argc == 1) {
		read_show_mac(true);
		return 0;
	}

	cmd = argv[1][0];

	if (cmd == 'r') {
		read_show_mac(true);
		return 0;
	}

	if (cmd == 'i')
		return 0;

	/* We know we have at least one parameter  */

	switch (cmd) {
	case '1':       /* "mac 1" */
		/* we ignore the first parameter, specified as "1" on command line for historic reasons */
		ret = set_mac_address(argv[2], board_hw_id_nvmem_read() < 0);
		switch (ret) {
		case 0:
			printf("Reboot required before new addresses are activated\n");
			break;

		case -ERANGE:
			printf("Invalid MAC address entered\n");
			printf("Usage: mac 1 XX:XX:XX:XX:XX:XX\n");
			break;

		default:
			printf("EEPROM Write Error %d\n", ret);
			break;
		}
		break;
	case 'h':       /* help */
	default:
		return cmd_usage(cmdtp);
	}

	return 0;
}
#endif
