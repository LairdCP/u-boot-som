/*
 * Copyright (C) 2018 Laird
 *      Boris Krasnovskiy <boris.krasnovskiy@lairdtech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/unaligned.h>
#include <dm.h>
#include <i2c_eeprom.h>

#define MAX_NUM_PORTS   2
#define MAC_STORED_PORT 1

static struct udevice *dev;
static u8 mac[MAX_NUM_PORTS][8];

static void mac_calc_inc(u8 *src, u8 *dst, int inc)
{
	u32 m = (src[3] << 16) | (src[4] << 8) | src[5];

	m += inc;

	memcpy(dst, src, 3);

	dst[3] = (m >> 16) & 0xff;
	dst[4] = (m >> 8) & 0xff;
	dst[5] = m & 0xff;
}

static bool mac_valid(u8 *m)
{
	static const u8 bad_mac[2][6] =
	{
		{ 0,	0,    0,    0,	  0,	0    },
		{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
	};

	return memcmp(m, bad_mac[0], 6) &&
	       memcmp(m, bad_mac[1], 6);
}

/**
 * show_eeprom - display the contents of the EEPROM
 */
static void show_eeprom(void)
{
	int i;

	/* Show MAC addresses  */
	for (i = 0; i < MAX_NUM_PORTS; i++) {
		u8 *p = mac[i];

		printf("Eth%u: %02x:%02x:%02x:%02x:%02x:%02x\n", i,
		       p[0], p[1], p[2], p[3], p[4], p[5]);
	}
}

/**
 * read_eeprom - read the EEPROM into memory
 */
static int read_eeprom(void)
{
	int ret, i;

	if (!dev) {
		ret = uclass_first_device_err(UCLASS_I2C_EEPROM, &dev);
		if (ret)
			return ret;
	}

	ret = i2c_eeprom_read(dev, CONFIG_SYS_I2C_MAC_OFFSET, mac[MAC_STORED_PORT], 6);
	if (ret)
		return ret;

	if (mac_valid(mac[MAC_STORED_PORT]))
		mac_calc_inc(mac[MAC_STORED_PORT], mac[MAC_STORED_PORT - 1], 1);

	return 0;
}

/**
 * prog_eeprom - write the EEPROM from memory
 */
static int prog_eeprom(void)
{
	int ret;
	u8 mac2[6];

	if (!dev) {
		ret = uclass_first_device_err(UCLASS_I2C_EEPROM, &dev);
		if (ret) {
			printf("No EEPROM Found %d\n", ret);
			goto fail;
		}
	}

	ret = i2c_eeprom_write(dev, CONFIG_SYS_I2C_MAC_OFFSET, mac[MAC_STORED_PORT], 6);
	if (ret) {
		if (ret == -EREMOTEIO)
			printf("EEPROM is Write-Protected\n");
		else
			printf("EEPROM Write Error %d\n", ret);
		goto fail;
	}

	ret = i2c_eeprom_read(dev, CONFIG_SYS_I2C_MAC_OFFSET, mac2, 6);
	if (ret) {
		printf("EEPROM Read Error %d\n", ret);
		goto fail;
	}

	if (memcmp(mac[MAC_STORED_PORT], mac2, 6)) {
		ret = -1;
		printf("EEPROM Compare Error %d\n", ret);
		goto fail;
	}

	printf("Programming passed.\n");
	return 0;

fail:
	printf("Programming failed.\n");

	return ret;
}

/**
 * set_mac_address - stores a MAC address into the EEPROM
 *
 * This function takes a pointer to MAC address string
 * (i.e."XX:XX:XX:XX:XX:XX", where "XX" is a two-digit hex number) and
 * stores it in one of the MAC address fields of the EEPROM local copy.
 */
static void set_mac_address(unsigned int index, const char *string)
{
	char *p = (char *)string;
	unsigned int i;

	if (index != MAC_STORED_PORT || !string) {
		printf("Usage: mac <n> XX:XX:XX:XX:XX:XX\n");
		return;
	}

	for (i = 0; *p && (i < 6); i++) {
		mac[index][i] = simple_strtoul(p, &p, 16);
		if (*p == ':')
			p++;
	}

	if (mac_valid(mac[MAC_STORED_PORT]))
		mac_calc_inc(mac[MAC_STORED_PORT], mac[MAC_STORED_PORT - 1], 1);

	printf("You need to save address and reboot before new address is activated\n");
}

int do_mac(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	char cmd;

	if (argc == 1) {
		show_eeprom();
		return 0;
	}

	cmd = argv[1][0];

	if (cmd == 'r') {
		read_eeprom();
		return 0;
	}

	if (cmd == 'i')
		return 0;

	if (argc == 2) {
		switch (cmd) {
		case 's':       /* save */
			prog_eeprom();
			break;
		default:
			return cmd_usage(cmdtp);
		}

		return 0;
	}

	/* We know we have at least one parameter  */

	switch (cmd) {
	case '1':       /* "mac 1" */
		set_mac_address(simple_strtoul(argv[1], NULL, 10), argv[2]);
		break;
	case 'h':       /* help */
	default:
		return cmd_usage(cmdtp);
	}

	return 0;
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
	unsigned int i;

	puts("EEPROM: ");

	if (read_eeprom()) {
		printf("Read failed.\n");
		return 0;
	}

	for (i = 0; i < MAX_NUM_PORTS; i++) {
		if (mac_valid(mac[i])) {
			char ethaddr[18];
			char enetvar[9];

			sprintf(ethaddr, "%02X:%02X:%02X:%02X:%02X:%02X",
				mac[i][0],
				mac[i][1],
				mac[i][2],
				mac[i][3],
				mac[i][4],
				mac[i][5]);
			sprintf(enetvar, i ? "eth%daddr" : "ethaddr", i);
			/* Only initialize environment variables that are blank
			 * (i.e. have not yet been set)
			 */
			if (!env_get(enetvar))
				env_set(enetvar, ethaddr);
		}
	}

	return 0;
}
