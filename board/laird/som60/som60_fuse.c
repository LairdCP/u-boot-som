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
#include <fuse.h>
#include <dm/uclass.h>
#include <nvmem.h>
#include <linux/if_ether.h>

#include <asm/arch/at91_fuse.h>

#define STICKER_PORT_NUM	1
#define MAX_NUM_PORTS		2

u16 board_hw_id_fuse_read(void)
{
	u16 hw_id;
	struct nvmem_cell cell;
	int ret;
	struct udevice *fuse_dev;
	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_DRIVER_GET(at91_fuse),
					  &fuse_dev);
	if (ret)
		return -1;
	if ((!nvmem_cell_get_by_name(fuse_dev, "hw-id", &cell))
		&& !nvmem_cell_read(&cell, &hw_id, sizeof(hw_id)))
		return hw_id;
	else
		return -1;
}

void board_hw_id_fuse_write(const u16 hw_id)
{
	int ret;
	struct udevice *fuse_dev;
	struct nvmem_cell cell;
	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_DRIVER_GET(at91_fuse),
					  &fuse_dev);
	if (ret)
		return;
	if (!nvmem_cell_get_by_name(fuse_dev, "hw-id", &cell))
		nvmem_cell_write(&cell, &hw_id, sizeof(hw_id));
}

/*
 * Unfortunately, due to the patch for device tree nvmem_macaddr_swap not being
 * accepted by Kernel team, we must store the mac addresses in
 * least-significant-byte-first format for consistency with Kernel.  This
 * results in the byte order reversed from human expectation when u32 words
 * are printed in hex.
 */
int enetaddr_fuse_read(uint8_t *enetaddr, int i)
{
	struct udevice *fuse_dev;
	int ret;
	struct nvmem_cell cell;
	if ((ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_DRIVER_GET(at91_fuse),
					  &fuse_dev)))
		return ret;

	/* Check if the device has a valid MAC address in nvmem
	 * We reference in fuse driver so as not to depend on Ethernet.
	 */
	if ((ret = nvmem_cell_get_by_name(fuse_dev, i == 0 ? "mac-address0" : "mac-address1", &cell)))
		return ret;

	return nvmem_cell_read(&cell, enetaddr, ETH_ALEN);
}

int enetaddr_fuse_write(const uint8_t *enetaddr, int i)
{
	struct udevice *fuse_dev;
	int ret;
	struct nvmem_cell cell;
	if ((ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_DRIVER_GET(at91_fuse),
					  &fuse_dev)))
		return ret;

	/* Check if the device has a valid MAC address in nvmem */
	if ((ret = nvmem_cell_get_by_name(fuse_dev, i == 0 ? "mac-address0" : "mac-address1", &cell)))
		return ret;

	return nvmem_cell_write(&cell, enetaddr, ETH_ALEN);
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
 * is_zero_ether_addr - Determine if given Ethernet address is all zeros.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is all zeroes.
 *
 * Please note: addr must be aligned to u16.
 */
static inline bool is_zero_ether_addr(const u8 *addr)
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
 * is_multicast_ether_addr - Determine if the Ethernet address is a multicast.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a multicast address.
 * By definition the broadcast address is also a multicast address.
 */
static inline bool is_multicast_ether_addr(const u8 *addr)
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
 * is_local_ether_addr - Determine if the Ethernet address is a locally administered
 *                       address (LAA).
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a local address.
 */
static inline bool is_local_ether_addr(const u8 *addr)
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
 * is_valid_ether_addr - Determine if the given Ethernet address is valid
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Check that the Ethernet address (MAC) is not 00:00:00:00:00:00, is not
 * a multicast address, and is not FF:FF:FF:FF:FF:FF.
 *
 * Return true if the address is valid.
 *
 * Please note: addr must be aligned to u16.
 */
bool is_valid_ether_addr(const u8 *addr)
{
	/* FF:FF:FF:FF:FF:FF is a multicast address so we don't need to
	 * explicitly check for it here. */
	return !is_multicast_ether_addr(addr) && !is_local_ether_addr(addr)
		&& !is_zero_ether_addr(addr);
}

/**
 * read_show_mac - display the MAC addresses
 */
void read_show_mac(void)
{
	int i;
	int ret;
	u8 mac[ETH_ALEN];

	/* Show MAC addresses  */
	for (i = 0; i < MAX_NUM_PORTS; i++) {
		if (!(ret = enetaddr_fuse_read(mac, i)))
			printf("Eth%u: %02x:%02x:%02x:%02x:%02x:%02x\n", i,
			       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		else
			printf("Error %d\n", ret);
	}
}

static int confirm_prog(void)
{
	puts("Warning: Programming fuses is an irreversible operation!\n"
			"         This may brick your system.\n"
			"         Use this command only if you are sure of "
					"what you are doing!\n"
			"\nReally perform this fuse programming? <y/N>\n");

	if (confirm_yesno())
		return 1;

	puts("Fuse programming aborted\n");
	return 0;
}

static bool valid_mac_programmed(void)
{

	int i;
	u8 mac[ETH_ALEN];

	for (i = 0; i < MAX_NUM_PORTS; i++) {
		if ((!enetaddr_fuse_read(mac, i))
			&& is_valid_ether_addr(mac))
				return true;
	}
	return false;
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
void set_mac_address(const char *string)
{
	char *p = (char *)string;
	unsigned int i;
	int ret;
	u8 mac[MAX_NUM_PORTS][ETH_ALEN];

	if (!string) {
		printf("Usage: mac <n> XX:XX:XX:XX:XX:XX, where n is ignored\n");
		return;
	}

	if (valid_mac_programmed()) {
		printf("Valid MAC address already programmed, will not over-write, aborting.\n");
		return;
	}

	for (i = 0; *p && (i < ETH_ALEN); i++) {
		mac[STICKER_PORT_NUM][i] = simple_strtoul(p, &p, 16);
		if (*p == ':')
			p++;
		if ((*p == '\0') && (i < 5))
		{
			printf("Usage: mac <n> XX:XX:XX:XX:XX:XX\n");
			return;
		}
	}

	if (is_valid_ether_addr(mac[STICKER_PORT_NUM]))
	{
		mac_calc_inc(mac[STICKER_PORT_NUM], mac[0], 1);
		printf("Eth%u: %02x:%02x:%02x:%02x:%02x:%02x\n", STICKER_PORT_NUM,
		       mac[1][0], mac[1][1], mac[1][2], mac[1][3], mac[1][4], mac[1][5]);

		if ((!gd->have_console) || confirm_prog())
		{
			for (i = 0; i < MAX_NUM_PORTS; i++)
			{
				if ((ret = enetaddr_fuse_write(mac[i], i))
					&& i == STICKER_PORT_NUM)
					printf("Error %d\n", ret);
			}

			printf("You need to reboot before new addresses are activated\n");
		}
	}
	else
	{
		printf("Usage: mac <n> XX:XX:XX:XX:XX:XX\n");
	}
}

#if CONFIG_IS_ENABLED(CMD_MAC_FUSE)
int do_mac(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	char cmd;

	if (argc == 1) {
		read_show_mac();
		return 0;
	}

	cmd = argv[1][0];

	if (cmd == 'r') {
		read_show_mac();
		return 0;
	}

	if (cmd == 'i')
		return 0;

	/* We know we have at least one parameter  */

	switch (cmd) {
	case '1':       /* "mac 1" */
		/* we ignore the first parameter, specified as "1" on command line for historic reasons */
		set_mac_address(argv[2]);
		break;
	case 'h':       /* help */
	default:
		return cmd_usage(cmdtp);
	}

	return 0;
}
#endif
