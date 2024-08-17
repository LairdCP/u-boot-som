#include <common.h>
#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/sama5d3_smc.h>
#include <linux/delay.h>

#ifndef CONFIG_SYS_NAND_MAX_CHIPS
#define CONFIG_SYS_NAND_MAX_CHIPS 0
#endif

#include <linux/mtd/rawnand.h>

#if CONFIG_IS_ENABLED(NAND_IDENT)
static unsigned atmel_encode_ncycles(unsigned int ncycles,
				     unsigned int msbpos,
				     unsigned int msbwidth,
				     unsigned int msbfactor)
{
	unsigned int lsbmask = (1 << msbpos) - 1;
	unsigned int msbmask = (1 << msbwidth) - 1;
	unsigned int msb, lsb;

	msb = ncycles / msbfactor;
	lsb = ncycles % msbfactor;

	if (lsb > lsbmask) {
		lsb = 0;
		msb++;
	}

	/*
	 * Let's just put the maximum we can if the requested setting does
	 * not fit in the register field.
	 */
	if (msb > msbmask) {
		msb = msbmask;
		lsb = lsbmask;
	}

	return (msb << msbpos) | lsb;
}

static unsigned atmel_encode_setup_ncycles(unsigned int ncycles)
{
	return atmel_encode_ncycles(ncycles, 5, 1, 128);
}

static unsigned atmel_encode_pulse_ncycles(unsigned int ncycles)
{
	return atmel_encode_ncycles(ncycles, 6, 1, 256);
}

static unsigned atmel_encode_cycle_ncycles(unsigned int ncycles)
{
	return atmel_encode_ncycles(ncycles, 7, 2, 256);
}

static unsigned atmel_encode_timing_ncycles(unsigned int ncycles)
{
	return atmel_encode_ncycles(ncycles, 3, 1, 64);
}

/* Configures NAND controller from the timing table supplied */
int atmel_setup_data_interface(struct mtd_info *mtd, int chipnr,
			       const struct nand_data_interface *conf)
{
	struct at91_smc *smc = (struct at91_smc *)ATMEL_BASE_SMC;

	u32 ncycles, totalcycles, timeps, mckperiodps;
	u32 setup_reg, pulse_reg, cycle_reg, timings_reg, mode_reg;

	const struct nand_sdr_timings *timings;

	if (!conf)
		conf = nand_get_default_data_interface();

	timings = nand_get_sdr_timings(conf);
	if (IS_ERR(timings))
		return PTR_ERR(timings);

	/*
	 * tRC < 30ns implies EDO mode. This controller does not support this
	 * mode.
	 */
	if (timings->tRC_min < 30000)
		return -ENOTSUPP;

	if (chipnr == NAND_DATA_IFACE_CHECK_ONLY)
		return 0;

	setup_reg = 0;
	pulse_reg = 0;
	cycle_reg = 0;
	timings_reg = 0;
	mode_reg = 0;

	/* Clock to period in ps */
	mckperiodps = DIV_ROUND_DOWN_ULL(1000000000000ULL, get_mck_clk_rate());

	/* Truncate frequency to match Linux calculation */
	mckperiodps -= mckperiodps % 1000;

	/*
	 * Set write pulse timing. This one is easy to extract:
	 *
	 * NWE_PULSE = tWP
	 */
	ncycles = DIV_ROUND_UP(timings->tWP_min, mckperiodps);
	totalcycles = ncycles;
	pulse_reg |= AT91_SMC_PULSE_NWE(atmel_encode_pulse_ncycles(ncycles));

	/*
	 * The write setup timing depends on the operation done on the NAND.
	 * All operations goes through the same data bus, but the operation
	 * type depends on the address we are writing to (ALE/CLE address
	 * lines).
	 * Since we have no way to differentiate the different operations at
	 * the SMC level, we must consider the worst case (the biggest setup
	 * time among all operation types):
	 *
	 * NWE_SETUP = max(tCLS, tCS, tALS, tDS) - NWE_PULSE
	 */
	timeps = max3(timings->tCLS_min, timings->tCS_min, timings->tALS_min);
	timeps = max(timeps, timings->tDS_min);
	ncycles = DIV_ROUND_UP(timeps, mckperiodps);
	ncycles = ncycles > totalcycles ? ncycles - totalcycles : 0;
	totalcycles += ncycles;
	setup_reg |= AT91_SMC_SETUP_NWE(atmel_encode_setup_ncycles(ncycles));

	/*
	 * As for the write setup timing, the write hold timing depends on the
	 * operation done on the NAND:
	 *
	 * NWE_HOLD = max(tCLH, tCH, tALH, tDH, tWH)
	 */
	timeps = max3(timings->tCLH_min, timings->tCH_min, timings->tALH_min);
	timeps = max3(timeps, timings->tDH_min, timings->tWH_min);
	ncycles = DIV_ROUND_UP(timeps, mckperiodps);
	totalcycles += ncycles;

	/*
	 * The write cycle timing is directly matching tWC, but is also
	 * dependent on the other timings on the setup and hold timings we
	 * calculated earlier, which gives:
	 *
	 * NWE_CYCLE = max(tWC, NWE_SETUP + NWE_PULSE + NWE_HOLD)
	 */
	ncycles = DIV_ROUND_UP(timings->tWC_min, mckperiodps);
	ncycles = max(totalcycles, ncycles);
	cycle_reg |= AT91_SMC_CYCLE_NWE(atmel_encode_cycle_ncycles(ncycles));

	/*
	 * We don't want the CS line to be toggled between each byte/word
	 * transfer to the NAND. The only way to guarantee that is to have the
	 * NCS_{WR,RD}_{SETUP,HOLD} timings set to 0, which in turn means:
	 *
	 * NCS_WR_PULSE = NWE_CYCLE
	 */
	pulse_reg |= AT91_SMC_PULSE_NCS_WR(atmel_encode_pulse_ncycles(ncycles));

	/*
	 * As for the write setup timing, the read hold timing depends on the
	 * operation done on the NAND:
	 *
	 * NRD_HOLD = max(tREH, tRHOH)
	 */
	timeps = max(timings->tREH_min, timings->tRHOH_min);
	ncycles = DIV_ROUND_UP(timeps, mckperiodps);
	totalcycles = ncycles;

	/*
	 * TDF = tRHZ - NRD_HOLD
	 */
	ncycles = DIV_ROUND_UP(timings->tRHZ_max, mckperiodps);
	ncycles -= totalcycles;

	/*
	 * In ONFI 4.0 specs, tRHZ has been increased to support EDO NANDs and
	 * we might end up with a config that does not fit in the TDF field.
	 * Just take the max value in this case and hope that the NAND is more
	 * tolerant than advertised.
	 */
	if (ncycles > 15)
		ncycles = 15;
	else if (ncycles < 1)
		ncycles = 1;

	mode_reg |= AT91_SMC_MODE_TDF_CYCLE(ncycles) | AT91_SMC_MODE_TDF;

	/*
	 * Read pulse timing directly matches tRP:
	 *
	 * NRD_PULSE = tRP
	 */
	ncycles = DIV_ROUND_UP(timings->tRP_min, mckperiodps);
	totalcycles += ncycles;
	pulse_reg |= AT91_SMC_PULSE_NRD(atmel_encode_pulse_ncycles(ncycles));

	/*
	 * The read cycle timing is directly matching tRC, but is also
	 * dependent on the setup and hold timings we calculated earlier,
	 * which gives:
	 *
	 * NRD_CYCLE = max(tRC, NRD_PULSE + NRD_HOLD)
	 *
	 * NRD_SETUP is always 0.
	 */
	ncycles = DIV_ROUND_UP(timings->tRC_min, mckperiodps);
	ncycles = max(totalcycles, ncycles);
	cycle_reg |= AT91_SMC_CYCLE_NRD(atmel_encode_cycle_ncycles(ncycles));

	/*
	 * We don't want the CS line to be toggled between each byte/word
	 * transfer from the NAND. The only way to guarantee that is to have
	 * the NCS_{WR,RD}_{SETUP,HOLD} timings set to 0, which in turn means:
	 *
	 * NCS_RD_PULSE = NRD_CYCLE
	 */
	pulse_reg |= AT91_SMC_PULSE_NCS_RD(atmel_encode_pulse_ncycles(ncycles));

	/* Txxx timings are directly matching tXXX ones. */
	ncycles = DIV_ROUND_UP(timings->tCLR_min, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TCLR(atmel_encode_timing_ncycles(ncycles));

	ncycles = DIV_ROUND_UP(timings->tADL_min, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TADL(atmel_encode_timing_ncycles(ncycles));

	ncycles = DIV_ROUND_UP(timings->tAR_min, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TAR(atmel_encode_timing_ncycles(ncycles));

	ncycles = DIV_ROUND_UP(timings->tRR_min, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TRR(atmel_encode_timing_ncycles(ncycles));

	ncycles = DIV_ROUND_UP(timings->tWB_max, mckperiodps);
	timings_reg |= AT91_SMC_TIMINGS_TWB(atmel_encode_timing_ncycles(ncycles));

	/* Attach the CS line to the NFC logic. */
	timings_reg |= AT91_SMC_TIMINGS_NFSEL(1) | AT91_SMC_TIMINGS_RBNSEL(3);

	/* Operate in NRD/NWE READ/WRITEMODE. */
	mode_reg |= AT91_SMC_MODE_RM_NRD | AT91_SMC_MODE_WM_NWE;

	writel(setup_reg,   &smc->cs[3].setup);
	writel(pulse_reg,   &smc->cs[3].pulse);
	writel(cycle_reg,   &smc->cs[3].cycle);
	writel(timings_reg, &smc->cs[3].timings);
	writel(mode_reg,    &smc->cs[3].mode);

	debug("setup: %x, pulse: %x, cycle: %x, timings: %x, mode: %x\n",
	       setup_reg, pulse_reg, cycle_reg, timings_reg, mode_reg);

	return 0;
}
#else
int atmel_setup_data_interface(struct mtd_info *mtd, int chipnr,
			       const struct nand_data_interface *conf)
{
	struct at91_smc *smc = (struct at91_smc *)ATMEL_BASE_SMC;

	/* Set timing to Mode 0, calculated by above */
	writel(0x00000002, &smc->cs[3].setup);
	writel(0x0f080f08, &smc->cs[3].pulse);
	writel(0x000f000f, &smc->cs[3].cycle);
	writel(0xb8060483, &smc->cs[3].timings);
	writel(0x001f0003, &smc->cs[3].mode);

	return 0;
}
#endif

#ifdef CONFIG_SPL_BUILD
#ifndef CONFIG_SPL_NAND_SUPPORT
static void __iomem *IO_ADDR_R, *IO_ADDR_W;
static unsigned long chipsize;
static unsigned jedec_id;

static void at91_nand_hwcontrol(int cmd, unsigned int ctrl)
{
	if (ctrl & NAND_CTRL_CHANGE) {
		ulong IO_ADDRT_W = (ulong) IO_ADDR_W;
		IO_ADDRT_W &= ~(CFG_SYS_NAND_MASK_ALE
			     | CFG_SYS_NAND_MASK_CLE);

		if (ctrl & NAND_CLE)
			IO_ADDRT_W |= CFG_SYS_NAND_MASK_CLE;
		if (ctrl & NAND_ALE)
			IO_ADDRT_W |= CFG_SYS_NAND_MASK_ALE;

		IO_ADDR_W = (void *) IO_ADDRT_W;
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, IO_ADDR_W);
}

static int nand_command(uint32_t offs, u8 cmd)
{
	at91_nand_hwcontrol(cmd, NAND_CTRL_CLE | NAND_CTRL_CHANGE);

#ifdef CONFIG_SYS_NAND_DBW_16
	if (!nand_opcode_8bits(cmd))
		offs >>= 1;
#endif

	at91_nand_hwcontrol(offs & 0xff, NAND_CTRL_ALE | NAND_CTRL_CHANGE);
	at91_nand_hwcontrol((offs >> 8) & 0xff, NAND_CTRL_ALE);

	at91_nand_hwcontrol(NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

	if (cmd != NAND_CMD_READID)
		udelay(20);

	return 0;
}

static int at91_nand_read_buf(uint8_t *buf, int len)
{
#ifdef CONFIG_SYS_NAND_DBW_16
	readsw(IO_ADDR_R, buf, len);
#else
	readsb(IO_ADDR_R, buf, len);
#endif

	return 0;
}

static u16 onfi_crc16(u16 crc, u8 const *p, size_t len)
{
	int i;
	while (len--) {
		crc ^= *p++ << 8;
		for (i = 0; i < 8; i++)
			crc = (crc << 1) ^ ((crc & 0x8000) ? 0x8005 : 0);
	}

	return crc;
}

static int nandflash_detect_onfi(void)
{
	unsigned char onfi_ind[4];
	struct nand_onfi_params p;
	int i;

	nand_command(0x20, NAND_CMD_READID);
	at91_nand_read_buf(onfi_ind, sizeof(onfi_ind));

	if (memcmp(onfi_ind, "ONFI", 4)) {
		printf("NAND: ONFI not supported\n");
		return -1;
	}

	printf("NAND: ONFI flash detected\n");

	nand_command(0, NAND_CMD_PARAM);

	for (i = 0; i < 3; i++) {
		at91_nand_read_buf((u8*)&p, sizeof(p));

		if (onfi_crc16(ONFI_CRC_BASE, (unsigned char *)&p, 254) == p.crc)
			break;
	}

	if (i == 3) {
		printf("NAND: ONFI para CRC error!\n");
		return -1;
	}

	jedec_id = p.jedec_id;

	chipsize  = le32_to_cpu(p.byte_per_page);
	chipsize *= le32_to_cpu(p.pages_per_block);
	chipsize *= le32_to_cpu(p.blocks_per_lun);

	return 0;
}

void nand_init(void)
{
#ifndef CONFIG_TARGET_WB50N_SYSD
	at91_periph_clk_enable(ATMEL_ID_SMC);

	atmel_setup_data_interface(NULL, 1, NULL);

	IO_ADDR_R = (void __iomem *)CFG_SYS_NAND_BASE;
	IO_ADDR_W = (void __iomem *)CFG_SYS_NAND_BASE;

	nandflash_detect_onfi();

	at91_periph_clk_disable(ATMEL_ID_SMC);
#endif
}

unsigned long nand_size(void)
{
	return chipsize;
}

static unsigned nand_jedec_id(void)
{
	return jedec_id;
}
#else
unsigned nand_jedec_id(void);
#endif

int is_micron(void)
{
    return nand_jedec_id() == NAND_MFR_MICRON;
}
#endif
