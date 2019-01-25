/*
 * Copyright (C) 2013 Atmel Corporation
 *		      Bo Shen <voice.shen@atmel.com>
 *
 * Copyright (C) 2015 Atmel Corporation
 *		      Wenyou Yang <wenyou.yang@atmel.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/atmel_mpddrc.h>
#include <asm/arch/sama5_sfr.h>

#define SAMA5D3_MPDDRC_VERSION		0x140

static inline void atmel_mpddr_op(const struct atmel_mpddr *mpddr,
	      int mode,
	      u32 ram_address)
{
	writel(mode, &mpddr->mr);
	dmb();
	writel(0, ram_address);
}

static int ddr2_decodtype_is_seq(const unsigned int base, u32 cr)
{
	struct atmel_mpddr *mpddr = (struct atmel_mpddr *)base;
	u16 version = readl(&mpddr->version) & 0xffff;

	if ((version >= SAMA5D3_MPDDRC_VERSION) &&
	    (cr & ATMEL_MPDDRC_CR_DECOD_INTERLEAVED))
		return 0;

	return 1;
}


int ddr2_init(const unsigned int base,
	      const unsigned int ram_address,
	      const struct atmel_mpddrc_config *mpddr_value)
{
	const struct atmel_mpddr *mpddr = (struct atmel_mpddr *)base;

	u32 ba_off, cr;

	/* Compute bank offset according to NC in configuration register */
	ba_off = (mpddr_value->cr & ATMEL_MPDDRC_CR_NC_MASK) + 9;
	if (ddr2_decodtype_is_seq(base, mpddr_value->cr))
		ba_off += ((mpddr_value->cr & ATMEL_MPDDRC_CR_NR_MASK) >> 2) + 11;

	ba_off += (mpddr_value->md & ATMEL_MPDDRC_MD_DBW_MASK) ? 1 : 2;

	/* Program the memory device type into the memory device register */
	writel(mpddr_value->md, &mpddr->md);

	/* Program the configuration register */
	writel(mpddr_value->cr, &mpddr->cr);

	/* Program the timing register */
	writel(mpddr_value->tpr0, &mpddr->tpr0);
	writel(mpddr_value->tpr1, &mpddr->tpr1);
	writel(mpddr_value->tpr2, &mpddr->tpr2);

	/* Issue a NOP command */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NOP_CMD, ram_address);

	/* A 200 us is provided to precede any signal toggle */
	udelay(200);

	/* Issue a NOP command */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NOP_CMD, ram_address);

	/* Issue an all banks precharge command */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_PRCGALL_CMD, ram_address);

	/* Issue an extended mode register set(EMRS2) to choose operation */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_EXT_LMR_CMD,
		       ram_address + (0x2 << ba_off));

	/* Issue an extended mode register set(EMRS3) to set EMSR to 0 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_EXT_LMR_CMD,
		       ram_address + (0x3 << ba_off));

	/*
	 * Issue an extended mode register set(EMRS1) to enable DLL and
	 * program D.I.C (output driver impedance control)
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_EXT_LMR_CMD,
		       ram_address + (0x1 << ba_off));

	/* Enable DLL reset */
	cr = readl(&mpddr->cr);
	writel(cr | ATMEL_MPDDRC_CR_DLL_RESET_ENABLED, &mpddr->cr);

	/* A mode register set(MRS) cycle is issued to reset DLL */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_LMR_CMD, ram_address);

	/* Issue an all banks precharge command */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_PRCGALL_CMD, ram_address);

	/* Two auto-refresh (CBR) cycles are provided */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_RFSH_CMD, ram_address);
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_RFSH_CMD, ram_address);

	/* Disable DLL reset */
	cr = readl(&mpddr->cr);
	writel(cr & (~ATMEL_MPDDRC_CR_DLL_RESET_ENABLED), &mpddr->cr);

	/* A mode register set (MRS) cycle is issued to disable DLL reset */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_LMR_CMD, ram_address);

	/* Set OCD calibration in default state */
	cr = readl(&mpddr->cr);
	writel(cr | ATMEL_MPDDRC_CR_OCD_DEFAULT, &mpddr->cr);

	/*
	 * An extended mode register set (EMRS1) cycle is issued
	 * to OCD default value
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_EXT_LMR_CMD,
		       ram_address + (0x1 << ba_off));

	 /* OCD calibration mode exit */
	cr = readl(&mpddr->cr);
	writel(cr & (~ATMEL_MPDDRC_CR_OCD_DEFAULT), &mpddr->cr);

	/*
	 * An extended mode register set (EMRS1) cycle is issued
	 * to enable OCD exit
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_EXT_LMR_CMD,
		       ram_address + (0x1 << ba_off));

	/* A nornal mode command is provided */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NORMAL_CMD, ram_address);

	/* Perform a write access to any DDR2-SDRAM address */
	writel(0, ram_address);

	/* Write the refresh rate */
	writel(mpddr_value->rtr, &mpddr->rtr);

	return 0;
}

int ddr3_init(const unsigned int base,
	      const unsigned int ram_address,
	      const struct atmel_mpddrc_config *mpddr_value)
{
	struct atmel_mpddr *mpddr = (struct atmel_mpddr *)base;
	u32 ba_off;

	/* Compute bank offset according to NC in configuration register */
	ba_off = (mpddr_value->cr & ATMEL_MPDDRC_CR_NC_MASK) + 9;
	if (ddr2_decodtype_is_seq(base, mpddr_value->cr))
		ba_off += ((mpddr_value->cr &
			   ATMEL_MPDDRC_CR_NR_MASK) >> 2) + 11;

	ba_off += (mpddr_value->md & ATMEL_MPDDRC_MD_DBW_MASK) ? 1 : 2;

	/* Program the memory device type */
	writel(mpddr_value->md, &mpddr->md);

	/*
	 * Program features of the DDR3-SDRAM device and timing parameters
	 */
	writel(mpddr_value->cr, &mpddr->cr);

	writel(mpddr_value->tpr0, &mpddr->tpr0);
	writel(mpddr_value->tpr1, &mpddr->tpr1);
	writel(mpddr_value->tpr2, &mpddr->tpr2);

	/* A NOP command is issued to the DDR3-SRAM */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NOP_CMD, ram_address);

	/* A pause of at least 500us must be observed before a single toggle. */
	udelay(500);

	/* A NOP command is issued to the DDR3-SDRAM */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NOP_CMD, ram_address);

	/*
	 * An Extended Mode Register Set (EMRS2) cycle is issued to choose
	 * between commercial or high temperature operations.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_EXT_LMR_CMD,
		       ram_address + (0x2 << ba_off));
	/*
	 * Step 7: An Extended Mode Register Set (EMRS3) cycle is issued to set
	 * the Extended Mode Register to 0.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_EXT_LMR_CMD,
		       ram_address + (0x3 << ba_off));
	/*
	 * An Extended Mode Register Set (EMRS1) cycle is issued to disable and
	 * to program O.D.S. (Output Driver Strength).
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_EXT_LMR_CMD,
		       ram_address + (0x1 << ba_off));

	/*
	 * Write a one to the DLL bit (enable DLL reset) in the MPDDRC
	 * Configuration Register.
	 */

	/* A Mode Register Set (MRS) cycle is issued to reset DLL. */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_LMR_CMD, ram_address);

	udelay(50);

	/*
	 * A Calibration command (MRS) is issued to calibrate RTT and RON
	 * values for the Process Voltage Temperature (PVT).
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_DEEP_CMD, ram_address);

	/* A Normal Mode command is provided. */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NORMAL_CMD, ram_address);

	/* Perform a write access to any DDR3-SDRAM address. */
	writel(0, ram_address);

	/*
	 * Write the refresh rate into the COUNT field in the MPDDRC
	 * Refresh Timer Register (MPDDRC_RTR):
	 */
	writel(mpddr_value->rtr, &mpddr->rtr);

	return 0;
}

int lpddr1_init(const unsigned int base,
	      const unsigned int ram_address,
	      const struct atmel_mpddrc_config *mpddr_value)
{
	const struct atmel_mpddr *mpddr = (struct atmel_mpddr *)base;

	u32 ba_off;

	/* Compute BA[] offset according to CR configuration */
	ba_off = (mpddr_value->cr & ATMEL_MPDDRC_CR_NC_MASK) + 8;
	if (ddr2_decodtype_is_seq(base, mpddr_value->cr))
		ba_off += ((mpddr_value->cr & ATMEL_MPDDRC_CR_NR_MASK) >> 2) + 11;

	ba_off += (mpddr_value->md & ATMEL_MPDDRC_MD_DBW_MASK) ? 1 : 2;

	/*
	 * Step 1: Program the memory device type in the MPDDRC Memory Device Register
	 */
	writel(mpddr_value->md, &mpddr->md);

	/*
	 * Step 2: Program the features of the low-power DDR1-SDRAM device
	 * in the MPDDRC Configuration Register and in the MPDDRC Timing
	 * Parameter 0 Register/MPDDRC Timing Parameter 1 Register.
	 */
	writel(mpddr_value->cr, &mpddr->cr);

	writel(mpddr_value->tpr0, &mpddr->tpr0);
	writel(mpddr_value->tpr1, &mpddr->tpr1);
	writel(mpddr_value->tpr2, &mpddr->tpr2);

	/*
	 * Step 3: Program Temperature Compensated Self-refresh (TCR),
	 * Partial Array Self-refresh (PASR) and Drive Strength (DS) parameters
	 * in the MPDDRC Low-power Register.
	 */
	writel(mpddr_value->lpr, &mpddr->lpr);

	/*
	 * Step 4: A NOP command is issued to the low-power DDR1-SDRAM.
	 * Program the NOP command in the MPDDRC Mode Register (MPDDRC_MR).
	 * The clocks which drive the low-power DDR1-SDRAM device
	 * are now enabled.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NOP_CMD, ram_address);

	/*
	 * Step 5: A pause of at least 200 us must be observed before
	 * a signal toggle.
	 */
	 udelay(200);

	/*
	 * Step 6: A NOP command is issued to the low-power DDR1-SDRAM.
	 * Program the NOP command in the MPDDRC_MR. calibration request is
	 * now made to the I/O pad.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NOP_CMD, ram_address);

	/*
	 * Step 7: An All Banks Precharge command is issued
	 * to the low-power DDR1-SDRAM.
	 * Program All Banks Precharge command in the MPDDRC_MR.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_PRCGALL_CMD, ram_address);

	/*
	 * Step 8: Two auto-refresh (CBR) cycles are provided.
	 * Program the Auto Refresh command (CBR) in the MPDDRC_MR.
	 * The application must write a four to the MODE field
	 * in the MPDDRC_MR. Perform a write access to any low-power
	 * DDR1-SDRAM location twice to acknowledge these commands.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_RFSH_CMD, ram_address);
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_RFSH_CMD, ram_address);

	/*
	 * Step 9: An Extended Mode Register Set (EMRS) cycle is issued to
	 * program the low-power DDR1-SDRAM parameters (TCSR, PASR, DS).
	 * The application must write a five to the MODE field in the MPDDRC_MR
	 * and perform a write access to the SDRAM to acknowledge this command.
	 * The write address must be chosen so that signal BA[1] is set to 1
	 * and BA[0] is set to 0.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_EXT_LMR_CMD,
		       ram_address + (0x2 << ba_off));

	/*
	 * Step 10: A Mode Register Set (MRS) cycle is issued to program
	 * parameters of the low-power DDR1-SDRAM devices, in particular
	 * CAS latency.
	 * The application must write a three to the MODE field in the MPDDRC_MR
	 * and perform a write access to the SDRAM to acknowledge this command.
	 * The write address must be chosen so that signals BA[1:0] are set to 0.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_LMR_CMD,
		       ram_address + (0x0 << ba_off));

	/*
	 * Step 11: The application must enter Normal mode, write a zero
	 * to the MODE field in the MPDDRC_MR and perform a write access
	 * at any location in the SDRAM to acknowledge this command.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NORMAL_CMD, ram_address);

	/*
	 * Step 12: Perform a write access to any low-power DDR1-SDRAM address.
	 */
	writel(0, ram_address);

	/*
	 * Step 14: Write the refresh rate into the COUNT field in the MPDDRC
	 * Refresh Timer Register (MPDDRC_RTR):
	 */
	writel(mpddr_value->rtr, &mpddr->rtr);

	return 0;
}

#ifdef CONFIG_SAMA5D2

int lpddr2_init(const unsigned int base,
	      const unsigned int ram_address,
	      const struct atmel_mpddrc_config *mpddr_value)
{
	const struct atmel_mpddr *mpddr = (struct atmel_mpddr *)base;
	const struct atmel_sfr *sfr = (struct atmel_sfr *)ATMEL_BASE_SFR;

	u32 reg;

	writel(mpddr_value->lpr, &mpddr->lpddr23_lpr);

	writel(mpddr_value->tim_cal, &mpddr->tim_cal);

	/*
	 * Step 1: Program the memory device type.
	 */
	writel(mpddr_value->md, &mpddr->md);

	/*
	 * Step 2: Program the feature of the low-power DDR2-SDRAM device.
	 */
	writel(mpddr_value->cr, &mpddr->cr);

	writel(mpddr_value->tpr0, &mpddr->tpr0);
	writel(mpddr_value->tpr1, &mpddr->tpr1);
	writel(mpddr_value->tpr2, &mpddr->tpr2);

	/*
	 * Step 3: A NOP command is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NOP_CMD, ram_address);

	/*
	 * Step 4: A pause of at least 100 ns must be observed before
	 * a single toggle.
	 */
	udelay(1);

	/*
	 * Step 5: A NOP command is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NOP_CMD, ram_address);

	/*
	 * Step 6: A pause of at least 200 us must be observed before a Reset
	 * Command.
	 */
	udelay(200);

	/*
	 * Step 7: A Reset command is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(63) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 8: A pause of at least tINIT5 must be observed before issuing
	 * any commands.
	 */
	udelay(1);

	/*
	 * Step 9: A Calibration command is issued to the low-power DDR2-SDRAM.
	 */
	reg = readl(&mpddr->cr);
	reg &= ~ATMEL_MPDDRC_CR_ZQ_MASK;
	reg |= ATMEL_MPDDRC_CR_ZQ_RESET;
	writel(reg, &mpddr->cr);

	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(10) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 9bis: The ZQ Calibration command is now issued.
	 * Program the type of calibration in the MPDDRC_CR: set the
	 * ZQ field to the SHORT value.
	 */
	reg = readl(&mpddr->cr);
	reg &= ~ATMEL_MPDDRC_CR_ZQ_MASK;
	reg |= ATMEL_MPDDRC_CR_ZQ_SHORT;
	writel(reg, &mpddr->cr);

	/*
	 * Step 10: A Mode Register Write command with 1 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(1) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 11: A Mode Register Write command with 2 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(2) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 12: A Mode Register Write command with 3 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(3) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 13: A Mode Register Write command with 16 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(16) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 14: In the DDR Configuration Register, open the input buffers.
	 */
	reg = readl(&sfr->ddrcfg);
	reg |= (ATMEL_SFR_DDRCFG_FDQIEN | ATMEL_SFR_DDRCFG_FDQSIEN);
	writel(reg, &sfr->ddrcfg);

	/*
	 * Step 15: A NOP command is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NOP_CMD, ram_address);

	/*
	 * Step 16: A Mode Register Read command with 5 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(5) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 17: A Mode Register Read command with 6 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(6) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 18: A Mode Register Read command with 8 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(8) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 19: A Mode Register Read command with 0 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(0) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 20: A Normal Mode command is provided.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NORMAL_CMD, ram_address);

	/*
	 * Step 21: In the DDR Configuration Register, close the input buffers.
	 */
	reg = readl(&sfr->ddrcfg);
	reg &= ~(ATMEL_SFR_DDRCFG_FDQIEN | ATMEL_SFR_DDRCFG_FDQSIEN);
	writel(reg, &sfr->ddrcfg);

	/*
	 * Step 22: Write the refresh rate into the COUNT field in the MPDDRC
	 * Refresh Timer Register.
	 */
	writel(mpddr_value->rtr, &mpddr->rtr);

	/*
	 * Now configure the CAL MR4 register.
	 */
	writel(mpddr_value->cal_mr4, &mpddr->cal_mr4);

	return 0;
}

#else

int lpddr2_init(const unsigned int base,
	      const unsigned int ram_address,
	      const struct atmel_mpddrc_config *mpddr_value)
{
	const struct atmel_mpddr *mpddr = (struct atmel_mpddr *)base;

	u32 reg;

	writel(mpddr_value->lpr, &mpddr->lpddr23_lpr);

	writel(mpddr_value->tim_cal, &mpddr->tim_cal);

	/*
	 * Step 1: Program the memory device type.
	 */
	writel(mpddr_value->md, &mpddr->md);

	/*
	 * Step 2: Program the feature of the low-power DDR2-SDRAM device.
	 */
	writel(mpddr_value->cr, &mpddr->cr);

	writel(mpddr_value->tpr0, &mpddr->tpr0);
	writel(mpddr_value->tpr1, &mpddr->tpr1);
	writel(mpddr_value->tpr2, &mpddr->tpr2);

	/*
	 * Step 3: A NOP command is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NOP_CMD, ram_address);

	/*
	 * Step 4: A pause of at least 100 ns must be observed before
	 * a single toggle.
	 */
	udelay(1);

	/*
	 * Step 5: A NOP command is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NOP_CMD, ram_address);

	/*
	 * Step 6: A pause of at least 200 us must be observed before a Reset
	 * Command.
	 */
	udelay(200);

	/*
	 * Step 7: A Reset command is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(63) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 8: A pause of at least tINIT5 must be observed before issuing
	 * any commands.
	 */
	udelay(1);

	/*
	 * Step 9: A Calibration command is issued to the low-power DDR2-SDRAM.
	 */
	reg = readl(&mpddr->cr);
	reg &= ~ATMEL_MPDDRC_CR_ZQ_MASK;
	reg |= ATMEL_MPDDRC_CR_ZQ_RESET;
	writel(reg, &mpddr->cr);

	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(10) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 9bis: The ZQ Calibration command is now issued.
	 * Program the type of calibration in the MPDDRC_CR: set the
	 * ZQ field to the SHORT value.
	 */
	reg = readl(&mpddr->cr);
	reg &= ~ATMEL_MPDDRC_CR_ZQ_MASK;
	reg |= ATMEL_MPDDRC_CR_ZQ_SHORT;
	writel(reg, &mpddr->cr);

	/*
	 * Step 10: A Mode Register Write command with 1 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(1) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 11: A Mode Register Write command with 2 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(2) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 12: A Mode Register Write command with 3 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(3) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 13: A Mode Register Write command with 16 to the MRS field
	 * is issued to the low-power DDR2-SDRAM.
	 */
	atmel_mpddr_op(mpddr,
		ATMEL_MPDDRC_MR_MODE_MRS(16) | ATMEL_MPDDRC_MR_MODE_LPDDR2_CMD,
		ram_address);

	/*
	 * Step 14: A Normal Mode command is provided.
	 */
	atmel_mpddr_op(mpddr, ATMEL_MPDDRC_MR_MODE_NORMAL_CMD, ram_address);

	/*
	 * Step 15: close the input buffers: error in documentation: no need.
	 */

	/*
	 * Step 16: Write the refresh rate into the COUNT field in the MPDDRC
	 * Refresh Timer Register.
	 */
	writel(mpddr_value->rtr, &mpddr->rtr);

	/*
	 * Now configure the CAL MR4 register.
	 */
	writel(mpddr_value->cal_mr4, &mpddr->cal_mr4);

	return 0;
}

#endif
