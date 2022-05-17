/*
 * Configuration settings for the SOM60x2 Module.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __SOM60X2_CONFIG_H
#define __SOM60X2_CONFIG_H

#include "som6050_common.h"

#define CONFIG_EXTRA_ENV_SETTINGS DEFAULT_ENV_SETTINGS \
	"_formatubi=nand erase.part ubi;" \
		"ubi part ubi;" \
		"for part in a b; do " \
			"ubi create kernel_${part} C00000 dynamic;" \
			"ubi create rootfs_${part} B000000 dynamic;" \
			"ubi create rootfs_data_${part} 1400000 dynamic;" \
		"done;" \
		"ubi create perm 4335000 dynamic\0"

/* Timing and sizes for MT29C4G48MAYBBAMR-48 */
#define CONFIG_SYS_SDRAM_SIZE           SZ_256M

#define LPDDR_CR \
	(ATMEL_MPDDRC_CR_NC_COL_10         |\
	 ATMEL_MPDDRC_CR_NR_ROW_14         |\
	 ATMEL_MPDDRC_CR_CAS_DDR_CAS3      |\
	 ATMEL_MPDDRC_CR_ENRDM_ON          |\
	 ATMEL_MPDDRC_CR_NDQS_DISABLED     |\
	 ATMEL_MPDDRC_CR_DECOD_INTERLEAVED |\
	 ATMEL_MPDDRC_CR_UNAL_SUPPORTED);

#define LPDDR_TPR0 \
	(6 << ATMEL_MPDDRC_TPR0_TRAS_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TRCD_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TWR_OFFSET   |\
	 8 << ATMEL_MPDDRC_TPR0_TRC_OFFSET   |\
	 2 << ATMEL_MPDDRC_TPR0_TRP_OFFSET   |\
	 2 << ATMEL_MPDDRC_TPR0_TRRD_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TWTR_OFFSET  |\
	 2 << ATMEL_MPDDRC_TPR0_TMRD_OFFSET);

#define LPDDR_TPR1 \
	(2 << ATMEL_MPDDRC_TPR1_TXP_OFFSET   |\
	15 << ATMEL_MPDDRC_TPR1_TXSRD_OFFSET |\
	15 << ATMEL_MPDDRC_TPR1_TXSNR_OFFSET |\
	10 << ATMEL_MPDDRC_TPR1_TRFC_OFFSET);

#endif
