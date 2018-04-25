/*
 * Atmel SHA engine
 */

#include <common.h>
#include <malloc.h>
#include "atmel_sha.h"

#ifdef CONFIG_SHA_HW_ACCEL
#include <u-boot/sha256.h>
#include <u-boot/sha1.h>
#include <hw_sha.h>

#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/at91_pmc.h>


enum atmel_hash_algos {
	ATMEL_HASH_SHA1,
	ATMEL_HASH_SHA256
};

struct sha_ctx {
	enum atmel_hash_algos algo;
	uint32_t	length;
	uint8_t 	buffer[64];
};

const uint8_t sha256_der_prefix[SHA256_DER_LEN] = {
	0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
	0x00, 0x04, 0x20
};

const uint8_t sha1_der_prefix[SHA1_DER_LEN] = {
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
	0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14
};


static enum atmel_hash_algos get_hash_type(struct hash_algo *algo)
{
	if (!strcmp(algo->name, "sha1"))
		return ATMEL_HASH_SHA1;
	else
		return ATMEL_HASH_SHA256;
};

static int atmel_sha_process(const uint8_t * in_addr, uint8_t buflen)
{
	struct atmel_sha *sha = (struct atmel_sha *)ATMEL_BASE_SHA;
	int i;
	u32 *addr_buf;

	/* Copy data in */
	addr_buf = (u32 *)in_addr;
	for (i = 0; i < (buflen / 4); i++)
		sha->idatarx[i] = addr_buf[i];
	debug("Atmel sha, engine is loaded\n");

	/* Wait for hash to complete */
	while ((readl(&sha->isr) & ATMEL_HASH_ISR_MASK) != ATMEL_HASH_ISR_DATRDY);
	debug("Atmel sha, engine signaled completion\n");

	return 0;
}

static int atmel_sha_chunk(struct sha_ctx *ctx, const uint8_t *buf, unsigned int size)
{
	uint8_t remaining, fill;

	debug("Atmel sha, adding %d bytes to chunks\n", size);

	/* Chunk to 64 byte blocks */
	remaining = ctx->length & 0x3F;
	fill = 64 - remaining;

	/* If we have things in the buffer transfer the remaining into it */
	if (remaining && size >= fill) {
		memcpy(ctx->buffer + remaining, buf, fill);

		debug("Atmel sha, handling remainder (%d bytes) in buffer with %d bytes additional\n", remaining, fill);

		/* Process 64 byte chunk */
		atmel_sha_process(ctx->buffer, 64);

		size -= fill;
		buf += fill;
		ctx->length += fill;
		remaining = 0;
	}

	/* We are alligned take from source for any additional */
	while (size >= 64) {
		debug("Atmel sha, sending full 64 byte chunk\n");
		/* Process 64 byte chunk */
		atmel_sha_process(buf, 64);

		size -= 64;
		buf += 64;
		ctx->length += 64;
	}

	if (size) {
		debug("Atmel sha, storing leftovers of %d bytes\n", size);
		memcpy(ctx->buffer + remaining, buf, size);
		ctx->length += size;
	}



	return 0;
}

static int atmel_sha_fill_padding(struct sha_ctx *ctx)
{
	unsigned int index, padlen;
	u64 size, bits;
	uint8_t sha256_padding[64] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	size = ctx->length;

	bits = cpu_to_be64(size << 3);

	/* 64 byte, 512 bit block size */
	index = ctx->length & 0x3F;
	padlen = (index < 56) ? (56 - index) : ((64+56) - index);

	debug("Atmel sha, padding with %d bytes + 8 of size\n", padlen);
	/* set last entry to be 0x80 then 0's*/
	atmel_sha_chunk(ctx, sha256_padding, padlen);
	/* Bolt number of bits to the end */
	atmel_sha_chunk(ctx, (uint8_t *)&bits, 8);

	if (ctx->length & 0x3F) {
		debug("ERROR, Remainder after PADDING");
	}

	return 0;
}


/**
 * Computes hash value of input pbuf using h/w acceleration
 *
 * @param in_addr	A pointer to the input buffer
 * @param buflen	Byte length of input buffer
 * @param out_addr	A pointer to the output buffer. When complete
 *			32 bytes are copied to pout[0]...pout[31]. Thus, a user
 *			should allocate at least 32 bytes at pOut in advance.
 * @param chunk_size	chunk size for sha256
 */
void hw_sha256(const uchar * in_addr, uint buflen,
			uchar * out_addr, uint chunk_size)
{
	struct hash_algo *algo;
	struct sha_ctx *ctx;

	hash_lookup_algo("sha256", &algo);
	hw_sha_init(algo, (void *)&ctx);
	atmel_sha_chunk((void *)ctx, in_addr, buflen);
	atmel_sha_fill_padding(ctx);
	hw_sha_finish(algo, (void *)ctx, out_addr, buflen);
}

/**
 * Computes hash value of input pbuf using h/w acceleration
 *
 * @param in_addr	A pointer to the input buffer
 * @param buflen	Byte length of input buffer
 * @param out_addr	A pointer to the output buffer. When complete
 *			32 bytes are copied to pout[0]...pout[31]. Thus, a user
 *			should allocate at least 32 bytes at pOut in advance.
 * @param chunk_size	chunk_size for sha1
 */
void hw_sha1(const uchar * in_addr, uint buflen,
			uchar * out_addr, uint chunk_size)
{
	struct hash_algo *algo;
	struct sha_ctx *ctx;

	hash_lookup_algo("sha1", &algo);
	hw_sha_init(algo, (void *)&ctx);
	atmel_sha_chunk((void *)ctx, in_addr, buflen);
	atmel_sha_fill_padding(ctx);
	hw_sha_finish(algo, (void *)ctx, out_addr, buflen);
}

/*
 * Create the context for sha progressive hashing using h/w acceleration
 *
 * @algo: Pointer to the hash_algo struct
 * @ctxp: Pointer to the pointer of the context for hashing
 * @return 0 if ok, -ve on error
 */
int hw_sha_init(struct hash_algo *algo, void **ctxp)
{
	struct atmel_sha *sha = (struct atmel_sha *)ATMEL_BASE_SHA;
	struct sha_ctx *ctx;
	u32 reg;

	ctx = malloc(sizeof(struct sha_ctx));
	if (ctx == NULL) {
		debug("Failed to allocate context\n");
		return -ENOMEM;
	}
	*ctxp = ctx;

	ctx->algo = get_hash_type(algo);
	ctx->length = 0;

	debug("Atmel sha init\n");
	at91_periph_clk_enable(ATMEL_ID_SHA);

	/* Reset the SHA engine */
	writel(ATMEL_HASH_CR_SWRST, &sha->cr);

	/* Set AUTO mode and fastest operation */
	reg = ATMEL_HASH_MR_SMOD_AUTO | ATMEL_HASH_MR_PROCDLY_SHORT;
	if (ctx->algo == ATMEL_HASH_SHA1)
		reg |= ATMEL_HASH_MR_ALGO_SHA1;
	else
		reg |= ATMEL_HASH_MR_ALGO_SHA256;
	writel(reg, &sha->mr);

	/* Set ready to recieve first */
	writel(ATMEL_HASH_CR_FIRST, &sha->cr);

	/* Ready to roll */
	return 0;
}

/*
 * Update buffer for sha progressive hashing using h/w acceleration
 *
 * The context is freed by this function if an error occurs.
 *
 * @algo: Pointer to the hash_algo struct
 * @ctx: Pointer to the context for hashing
 * @buf: Pointer to the buffer being hashed
 * @size: Size of the buffer being hashed
 * @is_last: 1 if this is the last update; 0 otherwise
 * @return 0 if ok, -ve on error
 */
int hw_sha_update(struct hash_algo *algo, void *ctx, const void *buf,
		     unsigned int size, int is_last)
{
	struct sha_ctx *sha_ctx = ctx;

	debug("Atmel sha update: %d bytes\n", size);

	/* Send down in chunks */
	atmel_sha_chunk(sha_ctx, buf, size);

	if (is_last) {
		atmel_sha_fill_padding(sha_ctx);
	}

	return 0;
}

/*
 * Copy sha hash result at destination location
 *
 * The context is freed after completion of hash operation or after an error.
 *
 * @algo: Pointer to the hash_algo struct
 * @ctx: Pointer to the context for hashing
 * @dest_buf: Pointer to the destination buffer where hash is to be copied
 * @size: Size of the buffer being hashed
 * @return 0 if ok, -ve on error
 */
int hw_sha_finish(struct hash_algo *algo, void *ctx, void *dest_buf,
		     int size)
{
	struct atmel_sha *sha = (struct atmel_sha *)ATMEL_BASE_SHA;
	struct sha_ctx *sha_ctx = ctx;
	unsigned int len, i;
	u32 *addr_buf;

	/* Copy data back */
	len = (sha_ctx->algo == ATMEL_HASH_SHA1) ? SHA1_SUM_LEN : SHA256_SUM_LEN;
	addr_buf = (u32 *)dest_buf;
	for (i = 0; i < (len / 4); i++)
		addr_buf[i] = sha->iodatarx[i];

	free(ctx);

	return 0;
}

#endif /* CONFIG_SHA_HW_ACCEL */
