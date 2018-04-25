#ifndef __DRIVERS_ATMEL_SHA_H__
#define __DRIVERS_ATMEL_SHA_H__

/* SHA register footprint */

typedef struct atmel_sha {
	u32 cr;
	u32 mr;
	u32 reserved0[2];
	u32 ier;
	u32 idr;
	u32 imr;
	u32 isr;
	u32 reserved1[8];
	u32 idatarx[16];
	u32 iodatarx[16];
	u32 reserved2[16];
} atmel_sha_t;

/* CR */
#define	ATMEL_HASH_CR_MASK 		(0xffff << 0)
#define ATMEL_HASH_CR_START		(1 << 0)
#define ATMEL_HASH_CR_FIRST		(1 << 4)
#define ATMEL_HASH_CR_SWRST		(1 << 8)

/* MR */
#define	ATMEL_HASH_MR_MASK 			(0xffff << 0)
#define ATMEL_HASH_MR_SMOD_MANUAL	(0 << 0)
#define ATMEL_HASH_MR_SMOD_AUTO		(1 << 0)
#define ATMEL_HASH_MR_SMOD_IDATAR0	(2 << 0)
#define ATMEL_HASH_MR_PROCDLY_SHORT	(0 << 4)
#define ATMEL_HASH_MR_PROCDLY_LONG	(1 << 4)
#define ATMEL_HASH_MR_ALGO_SHA1		(0 << 8)
#define ATMEL_HASH_MR_ALGO_SHA256	(1 << 8)
#define ATMEL_HASH_MR_ALGO_SHA384	(2 << 8)
#define ATMEL_HASH_MR_ALGO_SHA512	(3 << 8)
#define ATMEL_HASH_MR_ALGO_SHA224	(4 << 8)
#define ATMEL_HASH_MR_DUALBUFF_INACTIVE (0 << 16)
#define ATMEL_HASH_MR_DUALBUFF_ACTIVE	(1 << 16)

/* ISR */
#define ATMEL_HASH_ISR_MASK 		(1 << 0)
#define ATMEL_HASH_ISR_DATRDY 		(1 << 0)

#endif /* __DRIVERS_ATMEL_SHA_H__ */
