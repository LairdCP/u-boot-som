/*
 * Copyright (C) 2022 Laird Connectivity
 * Erik Strack <erik.strack@lairdconnect.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef AT91_FUSE_H
#define AT91_FUSE_H

/* Fuse control register (fcr, write-only) */
/* WRQ: Write Request */
#define AT91_FUSE_FCR_WRQ_OFFSET	(0)
#define AT91_FUSE_FCR_WRQ		(1 << AT91_FUSE_FCR_WRQ_OFFSET)
/* RRQ: Read Request */
#define AT91_FUSE_FCR_RRQ_OFFSET	(1)
#define AT91_FUSE_FCR_RRQ		(1 << AT91_FUSE_FCR_RRQ_OFFSET)
/* Valid code used to unlock write in fcr */
#define AT91_FUSE_FCR_KEY_OFFSET	(8)
#define AT91_FUSE_FCR_VALID_KEY_CODE	((0xfb) << AT91_FUSE_FCR_KEY_OFFSET)
/* FMR: Fuse Mode Register */
#define AT91_FUSE_FMR_MSK_OFFSET	(0)
#define AT91_FUSE_FMR_MSK		(1 << AT91_FUSE_FMR_MSK_OFFSET)
/* FIR: Fuse Index Register */
#define AT91_FUSE_FIR_WS_OFFSET		(0)
#define AT91_FUSE_FIR_WS		(1 << AT91_FUSE_FIR_WS_OFFSET)
#define AT91_FUSE_FIR_RS_OFFSET		(1)
#define AT91_FUSE_FIR_RS		(1 << AT91_FUSE_FIR_RS_OFFSET)
/* WSEL: Word Selection (0-15: Selects the word to write) */
#define AT91_FUSE_FIR_WSEL_OFFSET	(8)
/* Fuse status register 5 */
#define AT91_FUSE_W_WORD		(5)
#define AT91_FUSE_FSR5_W_OFFSET		(0)
#define AT91_FUSE_FSR5_W		(1 << AT91_FUSE_FSR5_W_OFFSET)
#define AT91_FUSE_FSR5_J_OFFSET		(1)
#define AT91_FUSE_FSR5_B_OFFSET		(2)

struct at91_fuse_reg {
	u32	fcr;		/* 0x00 Fuse Control Register */
	u32	fmr;		/* 0x04 Fuse Mode Register */
	u32	fir;		/* 0x08 Fuse Index Register */
	u32	fdr;		/* 0x0C Fuse Data Register */
	u32	fsr[8];		/* 0x10 Fuse Status Register 0 */
	u32	reserved1[44];	/* 0x30 ~ 0xDC */
	u32	reserved2[8];	/* 0xE0 ~ 0xFC */
};

#endif
