/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Configuration settings for the WB50n Module.
 */
#ifndef __WB50_CONFIG_H
#define __WB50_CONFIG_H

#include "som6050_common.h"

/* Timing and sizes for W949D2DB and MT46H16M32LF (5 & 6) */
#define LPDDR_CR \
	(ATMEL_MPDDRC_CR_NC_COL_9          |\
	 ATMEL_MPDDRC_CR_NR_ROW_13         |\
	 ATMEL_MPDDRC_CR_CAS_DDR_CAS3      |\
	 ATMEL_MPDDRC_CR_ENRDM_ON          |\
	 ATMEL_MPDDRC_CR_NDQS_DISABLED     |\
	 ATMEL_MPDDRC_CR_DECOD_INTERLEAVED |\
	 ATMEL_MPDDRC_CR_UNAL_SUPPORTED)

#define LPDDR_TPR0 \
	(6 << ATMEL_MPDDRC_TPR0_TRAS_OFFSET  |\
	 3 << ATMEL_MPDDRC_TPR0_TRCD_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TWR_OFFSET   |\
	 8 << ATMEL_MPDDRC_TPR0_TRC_OFFSET   |\
	 3 << ATMEL_MPDDRC_TPR0_TRP_OFFSET   |\
	 2 << ATMEL_MPDDRC_TPR0_TRRD_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TWTR_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TMRD_OFFSET)

#define LPDDR_TPR1 \
	(2 << ATMEL_MPDDRC_TPR1_TXP_OFFSET   |\
	16 << ATMEL_MPDDRC_TPR1_TXSRD_OFFSET |\
	16 << ATMEL_MPDDRC_TPR1_TXSNR_OFFSET |\
	10 << ATMEL_MPDDRC_TPR1_TRFC_OFFSET)

#define CFG_SYS_SDRAM_SIZE SZ_64M

#endif
