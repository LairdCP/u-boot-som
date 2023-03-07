/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2022 Laird Connectivity
 * Erik Strack <erik.strack@lairdconnect.com>
 *
 * Portions Copyright 2006 Freescale Semiconductor
 * York Sun (yorksun@freescale.com)
 */

#include <common.h>
#include <asm/global_data.h>
#include <console.h>
#include <command.h>
#include <dm.h>
#include <eeprom.h>
#include <dm/uclass.h>
#include <nvmem.h>
#include <linux/if_ether.h>

#include <i2c_eeprom.h>

#define STICKER_PORT_NUM	1
#define MAX_NUM_PORTS		2
#define MAC_STORED_PORT		1

u16 board_hw_id_nvmem_read(void)
{
	u16 hw_id;
	struct nvmem_cell cell;
	int ret;
	struct udevice *eeprom_dev;

	if ((ret = uclass_get_device_by_name(UCLASS_I2C_EEPROM, "i2c_eeprom@50", &eeprom_dev)))
		return -1;
	if ((!nvmem_cell_get_by_name(eeprom_dev, "hw-id", &cell))
		&& !nvmem_cell_read(&cell, &hw_id, sizeof(hw_id)))
		return hw_id;
	else
		return -1;
}

void board_hw_id_nvmem_write(const u16 hw_id)
{
	int ret;
	struct udevice *eeprom_dev;
	struct nvmem_cell cell;

	if ((ret = uclass_get_device_by_name(UCLASS_I2C_EEPROM, "i2c_eeprom@50", &eeprom_dev)))
		return;
	if (!nvmem_cell_get_by_name(eeprom_dev, "hw-id", &cell))
		nvmem_cell_write(&cell, &hw_id, sizeof(hw_id));
}

extern struct udevice *global_eeprom;

/*
 * Unfortunately, due to the patch for device tree nvmem_macaddr_swap not being
 * accepted by Kernel team, we must store the mac addresses in
 * least-significant-byte-first format for consistency with Kernel dts.  This
 * results in the byte order reversed from human expectation when u32 words
 * are printed in hex.
 */
int mac_nvmem_read(u8 *mac, int i)
{
	struct udevice *eeprom_dev;
	int ret;
	struct nvmem_cell cell;

	if ((ret = uclass_get_device_by_name(UCLASS_I2C_EEPROM, "i2c_eeprom@50", &eeprom_dev)))
		return ret;

	/* Check if the device has a valid MAC address in nvmem
	 * We reference from eeprom dts so as not to depend on Ethernet.
	 */
	if ((ret = nvmem_cell_get_by_name(eeprom_dev, i == 0 ? "mac-address0" : "mac-address1", &cell)))
		return ret;

	return nvmem_cell_read(&cell, mac, ETH_ALEN);
}

int mac_nvmem_write(const u8 *mac, int i)
{
	struct udevice *eeprom_dev;
	int ret;
	struct nvmem_cell cell;
	if ((ret = uclass_get_device_by_driver(UCLASS_I2C_EEPROM,
					  DM_DRIVER_GET(i2c_eeprom_std),
					  &eeprom_dev)))
		return ret;

	/* Check if the device has a valid MAC address in nvmem */
	if ((ret = nvmem_cell_get_by_name(eeprom_dev, i == 0 ? "mac-address0" : "mac-address1", &cell)))
		return ret;

	return nvmem_cell_write(&cell, mac, ETH_ALEN);
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

// From include/linux/etherdevice.h
/**
 * is_zero_mac_addr - Determine if given Ethernet address is all zeros.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is all zeroes.
 *
 * Please note: addr must be aligned to u16.
 */
static inline bool is_zero_mac_addr(const u8 *addr)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)
	return ((*(const u32 *)addr) | (*(const u16 *)(addr + 4))) == 0;
#else
	return (*(const u16 *)(addr + 0) |
		*(const u16 *)(addr + 2) |
		*(const u16 *)(addr + 4)) == 0;
#endif
}

/**
 * is_multicast_mac_addr - Determine if the Ethernet address is a multicast.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a multicast address.
 * By definition the broadcast address is also a multicast address.
 */
static inline bool is_multicast_mac_addr(const u8 *addr)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)
	u32 a = *(const u32 *)addr;
#else
	u16 a = *(const u16 *)addr;
#endif
#ifdef __BIG_ENDIAN
	return 0x01 & (a >> ((sizeof(a) * 8) - 8));
#else
	return 0x01 & a;
#endif
}

/**
 * is_local_mac_addr - Determine if the Ethernet address is a locally administered
 *                       address (LAA).
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a local address.
 */
static inline bool is_local_mac_addr(const u8 *addr)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)
	u32 a = *(const u32 *)addr;
#else
	u16 a = *(const u16 *)addr;
#endif
#ifdef __BIG_ENDIAN
	return 0x02 & (a >> ((sizeof(a) * 8) - 8));
#else
	return 0x02 & a;
#endif
}

/**
 * is_valid_mac_addr - Determine if the given Ethernet address is valid
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Check that the Ethernet address (MAC) is not 00:00:00:00:00:00, is not
 * a multicast address, and is not FF:FF:FF:FF:FF:FF.
 *
 * Return true if the address is valid.
 *
 * Please note: addr must be aligned to u16.
 */
bool is_valid_mac_addr(const u8 *addr)
{
	/* FF:FF:FF:FF:FF:FF is a multicast address so we don't need to
	 * explicitly check for it here. */
	return !is_multicast_mac_addr(addr) && !is_local_mac_addr(addr)
		&& !is_zero_mac_addr(addr);
}

/**
 * read_show_mac - display the MAC addresses
 */
void read_show_mac(bool use_dvk)
{
	int i;
	int ret;
	struct udevice *dev;
	u8 mac[ETH_ALEN];

	if (use_dvk) {
		if ((ret = uclass_get_device_by_name(UCLASS_I2C_EEPROM, "eeprom@57", &dev)))
			goto error;

		if ((ret = i2c_eeprom_read(dev, SYS_I2C_MAC_OFFSET, mac, 6)))
			goto error;

		printf("Eth%u: %02x:%02x:%02x:%02x:%02x:%02x\n", MAC_STORED_PORT,
		       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}
	else {
		/* Show MAC addresses  */
		for (i = 0; i < MAX_NUM_PORTS; i++) {
			if (!(ret = mac_nvmem_read(mac, i)))
				printf("Eth%u: %02x:%02x:%02x:%02x:%02x:%02x\n", i,
				       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			else
				goto error;
		}
	}

	return;

error:
	printf("EEPROM Read Error %d\n", ret);
}

/**
 * read_macs_eeproms - read the EEPROMs MAC into memory
 */
static int read_macs_eeproms(u8 mac[MAX_NUM_PORTS][ETH_ALEN])
{
	int ret;
	struct udevice *dev;

	/* Prioritize SOM60v2 EEPROM */
	if ((!mac_nvmem_read(mac[0], 0))
	    && (!mac_nvmem_read(mac[1], 1))
	    && is_valid_mac_addr(mac[0])
	    && is_valid_mac_addr(mac[1]))
		return 0;

	if ((ret = uclass_get_device_by_name(UCLASS_I2C_EEPROM, "eeprom@57", &dev)))
		return ret;

	ret = i2c_eeprom_read(dev, SYS_I2C_MAC_OFFSET, mac[MAC_STORED_PORT], 6);
	if (ret)
		return ret;

	if (is_valid_mac_addr(mac[MAC_STORED_PORT]))
		mac_calc_inc(mac[MAC_STORED_PORT], mac[MAC_STORED_PORT - 1], 1);

	return 0;
}


/**
 * prog_eeprom - write the EEPROM from memory
 */
static int prog_dvk_eeprom(u8 mac[MAX_NUM_PORTS][ETH_ALEN])
{
	struct udevice *dev;
	int ret;
	u8 mac_verify[6];

	if ((ret = uclass_get_device_by_name(UCLASS_I2C_EEPROM, "eeprom@57", &dev))) {
		printf("No EEPROM Found %d\n", ret);
		goto fail;
	}

	ret = i2c_eeprom_write(dev, SYS_I2C_MAC_OFFSET, mac[MAC_STORED_PORT], 6);
	if (ret) {
		if (ret == -EREMOTEIO)
			printf("EEPROM is Write-Protected\n");
		else
			printf("EEPROM Write Error %d\n", ret);
		goto fail;
	}

	ret = i2c_eeprom_read(dev, SYS_I2C_MAC_OFFSET, mac_verify, 6);
	if (ret) {
		printf("EEPROM Read Error %d\n", ret);
		goto fail;
	}

	if (memcmp(mac[MAC_STORED_PORT], mac_verify, 6)) {
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
	u8 mac[MAX_NUM_PORTS][ETH_ALEN];
	unsigned int i;
	bool save = false;

	puts("EEPROM: ");

	if ((i = read_macs_eeproms(mac))) {
		printf("Read failed %d.\n", i);
		return 0;
	}

	for (i = 0; i < MAX_NUM_PORTS; i++) {
		if (is_valid_mac_addr(mac[i])) {
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
			if (!env_get(enetvar)) {
				if (!env_set(enetvar, ethaddr))
					save = true;
			}
		}
	}
	if (save)
		printf("  Saving... %d\n", env_save());

	return 0;
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
void set_mac_address(const char *string, bool use_dvk)
{
	char *p = (char *)string;
	unsigned int i;
	int ret;
	u8 mac[MAX_NUM_PORTS][ETH_ALEN];

	if (!string) {
		printf("Usage: mac <n> XX:XX:XX:XX:XX:XX, where n is ignored\n");
		return;
	}

	for (i = 0; *p && (i < ETH_ALEN); i++) {
		mac[STICKER_PORT_NUM][i] = simple_strtoul(p, &p, 16);
		if (*p == ':')
			p++;
		if ((*p == '\0') && (i < 5)) {
			printf("Usage: mac <n> XX:XX:XX:XX:XX:XX\n");
			return;
		}
	}

	if (is_valid_mac_addr(mac[STICKER_PORT_NUM])) {
		if (use_dvk) {
			prog_dvk_eeprom(mac);
			return;
		}

		mac_calc_inc(mac[STICKER_PORT_NUM], mac[0], 1);
		printf("Eth%u: %02x:%02x:%02x:%02x:%02x:%02x\n",
				STICKER_PORT_NUM, mac[STICKER_PORT_NUM][0],
				mac[STICKER_PORT_NUM][1],
				mac[STICKER_PORT_NUM][2],
				mac[STICKER_PORT_NUM][3],
				mac[STICKER_PORT_NUM][4],
				mac[STICKER_PORT_NUM][5]);

		for (i = 0; i < MAX_NUM_PORTS; i++) {
			if ((ret = mac_nvmem_write(mac[i], i))
				&& i == STICKER_PORT_NUM)
				printf("NVMEM Write Error %d\n", ret);
		}

		printf("You need to reboot before new addresses are activated\n");
	}
	else {
		printf("Usage: mac <n> XX:XX:XX:XX:XX:XX\n");
	}
}

#if CONFIG_IS_ENABLED(ID_EEPROM)
int do_mac(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
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
		set_mac_address(argv[2], true);
		break;
	case 'h':       /* help */
	default:
		return cmd_usage(cmdtp);
	}

	return 0;
}
#endif
