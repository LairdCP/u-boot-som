/*
 * Configuration settings for the WB50 Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#ifndef __WB50_CONFIG_H
#define __WB50_CONFIG_H

#include "som6050_common.h"

#define CONFIG_EXTRA_ENV_SETTINGS DEFAULT_ENV_SETTINGS \
	"_formatubi=nand erase.part ubi;" \
		"ubi part ubi;" \
		"for part in a b; do " \
			"ubi create kernel_${part}  800000 dynamic;" \
			"ubi create rootfs_${part} 2C50000 dynamic;" \
			"ubi create rootfs_data_${part} 400000 dynamic;" \
		"done;" \
		"ubi create perm 5EF000 dynamic\0"

/* Timing and sizes for MT46H16M32LF (5 & 6) */
#define CONFIG_SYS_SDRAM_SIZE           0x04000000

#define LPDDR_CR \
	(ATMEL_MPDDRC_CR_NC_COL_9          |\
	 ATMEL_MPDDRC_CR_NR_ROW_13         |\
	 ATMEL_MPDDRC_CR_CAS_DDR_CAS3      |\
	 ATMEL_MPDDRC_CR_ENRDM_ON          |\
	 ATMEL_MPDDRC_CR_NDQS_DISABLED     |\
	 ATMEL_MPDDRC_CR_DECOD_INTERLEAVED |\
	 ATMEL_MPDDRC_CR_UNAL_SUPPORTED);

#define LPDDR_TPR0 \
	(6 << ATMEL_MPDDRC_TPR0_TRAS_OFFSET  |\
	 3 << ATMEL_MPDDRC_TPR0_TRCD_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TWR_OFFSET   |\
	 8 << ATMEL_MPDDRC_TPR0_TRC_OFFSET   |\
	 3 << ATMEL_MPDDRC_TPR0_TRP_OFFSET   |\
	 2 << ATMEL_MPDDRC_TPR0_TRRD_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TWTR_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TMRD_OFFSET);

#define LPDDR_TPR1 \
	(1 << ATMEL_MPDDRC_TPR1_TXP_OFFSET   |\
	 0 << ATMEL_MPDDRC_TPR1_TXSRD_OFFSET |\
	15 << ATMEL_MPDDRC_TPR1_TXSNR_OFFSET |\
	10 << ATMEL_MPDDRC_TPR1_TRFC_OFFSET);

#endif
