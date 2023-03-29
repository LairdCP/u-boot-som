/*
 * Configuration settings for the SOM60 Module
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __SOM60_H
#define __SOM60_H

#include "som6050_common.h"

#define LPDDR_CR \
	(ATMEL_MPDDRC_CR_NC_COL_10         |\
	 ATMEL_MPDDRC_CR_NR_ROW_13         |\
	 ATMEL_MPDDRC_CR_CAS_DDR_CAS3      |\
	 ATMEL_MPDDRC_CR_ENRDM_ON          |\
	 ATMEL_MPDDRC_CR_NDQS_DISABLED     |\
	 ATMEL_MPDDRC_CR_DECOD_INTERLEAVED |\
	 ATMEL_MPDDRC_CR_UNAL_SUPPORTED)

#define LPDDR_CR_SOM60X2 \
	(ATMEL_MPDDRC_CR_NC_COL_10         |\
	 ATMEL_MPDDRC_CR_NR_ROW_14         |\
	 ATMEL_MPDDRC_CR_CAS_DDR_CAS3      |\
	 ATMEL_MPDDRC_CR_ENRDM_ON          |\
	 ATMEL_MPDDRC_CR_NDQS_DISABLED     |\
	 ATMEL_MPDDRC_CR_DECOD_INTERLEAVED |\
	 ATMEL_MPDDRC_CR_UNAL_SUPPORTED)

#define LPDDR_TPR0 \
	(6 << ATMEL_MPDDRC_TPR0_TRAS_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TRCD_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TWR_OFFSET   |\
	 8 << ATMEL_MPDDRC_TPR0_TRC_OFFSET   |\
	 2 << ATMEL_MPDDRC_TPR0_TRP_OFFSET   |\
	 2 << ATMEL_MPDDRC_TPR0_TRRD_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TWTR_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TMRD_OFFSET)

#define LPDDR_TPR1 \
	(2 << ATMEL_MPDDRC_TPR1_TXP_OFFSET   |\
	15 << ATMEL_MPDDRC_TPR1_TXSRD_OFFSET |\
	15 << ATMEL_MPDDRC_TPR1_TXSNR_OFFSET |\
	10 << ATMEL_MPDDRC_TPR1_TRFC_OFFSET)

/* JSFBAB3YH3BBG: 1Gb+ densities have 8 banks
 * 1GB/2GB LPDDRII: The row address R0 through R14 is used to determine which row to activate in the selected bank.
 * JSL22Gxx8WA (embedded in JSFCBx3YH3xBx): Row addresses R0-R13,
 *              X16: Column addresses C0-C9  (JSF**[A|C])
 *              X32: Column addresses C0-C8  (JSF**[B|D])
 */
#define LPDDR2_CR \
	(ATMEL_MPDDRC_CR_NC_COL_9     | \
	 ATMEL_MPDDRC_CR_NR_ROW_13    | \
	 ATMEL_MPDDRC_CR_CAS_DDR_CAS3 | \
	 ATMEL_MPDDRC_CR_ZQ_SHORT     | \
	 ATMEL_MPDDRC_CR_NB_8BANKS    | \
	 ATMEL_MPDDRC_CR_UNAL_SUPPORTED)

#define LPDDR2_CR_DDR2_JSFCBB3YH3BBG_425A \
	(ATMEL_MPDDRC_CR_NC_COL_9     | \
	 ATMEL_MPDDRC_CR_NR_ROW_14    | \
	 ATMEL_MPDDRC_CR_CAS_DDR_CAS3 | \
	 ATMEL_MPDDRC_CR_ZQ_SHORT     | \
	 ATMEL_MPDDRC_CR_NB_8BANKS    | \
	 ATMEL_MPDDRC_CR_UNAL_SUPPORTED)

/*
 * The JSFBAB3YH3BBG refresh window: 32ms ( temp <= 85C )
 * See Table 50 and section 3.9.1 of data sheet
 * Required number of REFRESH commands(MIN): 4096 (1Gb)
 * Required number of REFRESH commands(MIN): 8192 (2Gb+)
 * (32ms / 8192) * 132MHz = 514 i.e. 0x202
 */

/* 0xC852 - Atmel reference count value - see SAMA5D3 29.7.11
 * Dependent upon expected temperature (T driftrate ) and voltage (V driftrate ) drift rates that the SDRAM is subject to in the application.
 */
#define LPDDR2_CAL_MR4 ATMEL_MPDDRC_CAL_MR4_COUNT_CAL(0xC852)

/* tRAS temperature derating += 1.875 ns */
/* trcd temperature derating += 1.875 ns */
/* tRAS min 3 tck, 42 ns */
/* tRAS 44 ns ~= 5.8 clocks */
#define LPDDR2_TPR0 \
	((6 << ATMEL_MPDDRC_TPR0_TRAS_OFFSET) |\
	 (3 << ATMEL_MPDDRC_TPR0_TRCD_OFFSET) |\
	 (3 << ATMEL_MPDDRC_TPR0_TWR_OFFSET)  |\
	 (9 << ATMEL_MPDDRC_TPR0_TRC_OFFSET)  |\
	 (3 << ATMEL_MPDDRC_TPR0_TRP_OFFSET)  |\
	 (3 << ATMEL_MPDDRC_TPR0_TRRD_OFFSET) |\
	 (2 << ATMEL_MPDDRC_TPR0_TWTR_OFFSET) |\
	 (5 << ATMEL_MPDDRC_TPR0_TMRD_OFFSET))

#define LPDDR2_TPR1 \
	((2 << ATMEL_MPDDRC_TPR1_TXP_OFFSET)    |\
	 (19 << ATMEL_MPDDRC_TPR1_TXSRD_OFFSET) |\
	 (19 << ATMEL_MPDDRC_TPR1_TXSNR_OFFSET) |\
	 (18 << ATMEL_MPDDRC_TPR1_TRFC_OFFSET))

#define LPDDR2_TPR2 \
	((8 << ATMEL_MPDDRC_TPR2_TFAW_OFFSET)   |\
	 (2 << ATMEL_MPDDRC_TPR2_TRTP_OFFSET)   |\
	 (3 << ATMEL_MPDDRC_TPR2_TRPA_OFFSET)   |\
	 (2 << ATMEL_MPDDRC_TPR2_TXARDS_OFFSET) |\
	 (2 << ATMEL_MPDDRC_TPR2_TXARD_OFFSET))

#define CONFIG_EXTRA_ENV_SETTINGS DEFAULT_ENV_SETTINGS \
	"bootside=a\0"

#define CONFIG_SYS_SDRAM_SIZE SZ_256M

#endif // __SOM60_COMMON_H
