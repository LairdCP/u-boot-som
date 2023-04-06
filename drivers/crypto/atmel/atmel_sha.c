/*
 * Atmel SHA engine
 */

#include <common.h>
#include <dm.h>
#include <malloc.h>
#include <watchdog.h>
#include <hw_sha.h>

#include <u-boot/sha512.h>
#include <u-boot/sha256.h>
#include <u-boot/sha1.h>
#include <u-boot/hash.h>

#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/at91_pmc.h>

#include "atmel_sha.h"

enum atmel_hash_algos {
	ATMEL_HASH_SHA1,
	ATMEL_HASH_SHA256,
	ATMEL_HASH_SHA384,
	ATMEL_HASH_SHA512,
};

#define HASH_MAX_BLOCK_SIZE (2 * HASH_MAX_DIGEST_SIZE)

struct sha_ctx {
	struct atmel_sha *sha;
	enum atmel_hash_algos algo;
	size_t length;
	uint8_t buffer[HASH_MAX_BLOCK_SIZE];
};

#if CONFIG_IS_ENABLED(SHA1)
const uint8_t sha1_der_prefix[SHA1_DER_LEN] = {
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
	0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14
};
#endif

#if CONFIG_IS_ENABLED(SHA256)
const uint8_t sha256_der_prefix[SHA256_DER_LEN] = {
	0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
	0x00, 0x04, 0x20
};
#endif

#if CONFIG_IS_ENABLED(SHA384)
const uint8_t sha384_der_prefix[SHA384_DER_LEN] = {
	0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05,
	0x00, 0x04, 0x30
};
#endif

#if CONFIG_IS_ENABLED(SHA512)
const uint8_t sha512_der_prefix[SHA512_DER_LEN] = {
	0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05,
	0x00, 0x04, 0x40
};
#endif

static void atmel_sha_init(struct sha_ctx *ctx)
{
	struct atmel_sha *sha = ctx->sha;
	u32 reg_mr;

	switch (ctx->algo) {
	case ATMEL_HASH_SHA1:
		reg_mr = ATMEL_HASH_MR_ALGO_SHA1;
		break;
	case ATMEL_HASH_SHA256:
		reg_mr = ATMEL_HASH_MR_ALGO_SHA256;
		break;
	case ATMEL_HASH_SHA384:
		reg_mr = ATMEL_HASH_MR_ALGO_SHA384;
		break;
	case ATMEL_HASH_SHA512:
		reg_mr = ATMEL_HASH_MR_ALGO_SHA512;
		break;
	}

	debug("Atmel sha init\n");
	at91_periph_clk_enable(ATMEL_ID_SHA);

	/* Reset the SHA engine */
	writel(ATMEL_HASH_CR_SWRST, &sha->cr);

	/* Set AUTO mode and fastest operation */
	reg_mr |= ATMEL_HASH_MR_SMOD_AUTO | ATMEL_HASH_MR_PROCDLY_SHORT;
	writel(reg_mr, &sha->mr);

	/* Set ready to recieve first */
	writel(ATMEL_HASH_CR_FIRST, &sha->cr);
}

static void atmel_sha_process(struct atmel_sha *sha, const void *in_addr, size_t size)
{
	const u32 *addr_buf = (u32 *)in_addr;
	unsigned i;

	size /= sizeof(u32);

	/* Copy data in */
	for (i = 0; i < size; i++)
		sha->idatarx[i] = addr_buf[i];
	debug("Atmel sha, engine is loaded\n");

	/* Wait for hash to complete */
	while (!(readl(&sha->isr) & ATMEL_HASH_ISR_DATRDY));
	debug("Atmel sha, engine signaled completion\n");
}

static void atmel_sha_update(struct sha_ctx *ctx, const void *buf, size_t size, size_t chunk_sz)
{
	const unsigned block_size = ctx->algo >= ATMEL_HASH_SHA384 ? 128 : 64;

	/* Chunk to block_size byte blocks */
	unsigned remaining = ctx->length & (block_size - 1);
	unsigned fill = block_size - remaining;
	size_t chunk = 0;

	debug("Atmel sha, adding %d bytes to chunks\n", size);

	/* If we have things in the buffer transfer the remaining into it */
	if (remaining && size >= fill) {
		memcpy(ctx->buffer + remaining, buf, fill);

		debug("Atmel sha, handling remainder (%d bytes) in buffer with %d bytes additional\n",
		      remaining, fill);

		/* Process 1 block chunk */
		atmel_sha_process(ctx->sha, ctx->buffer, block_size);

		size -= fill;
		buf += fill;
		ctx->length += fill;
		remaining = 0;
		chunk += block_size;
	}

	/* We are alligned take from source for any additional */
	while (size >= block_size) {
		debug("Atmel sha, sending full %u byte chunk\n", block_size);

		/* Process 1 block chunk */
		if (fill & 3) {
			memcpy(ctx->buffer, buf, block_size);
			atmel_sha_process(ctx->sha, ctx->buffer, block_size);
		} else
			atmel_sha_process(ctx->sha, buf, block_size);

		size -= block_size;
		buf += block_size;
		ctx->length += block_size;

#if CONFIG_IS_ENABLED(HW_WATCHDOG) || CONFIG_IS_ENABLED(WATCHDOG)
		chunk += block_size;
		if (chunk >= chunk_sz) {
			chunk -= chunk_sz;
			schedule();
		}
#endif
	}

	if (size) {
		debug("Atmel sha, storing leftovers of %d bytes\n", size);
		memcpy(ctx->buffer + remaining, buf, size);
		ctx->length += size;
	}
}

static void atmel_sha_fill_padding(struct sha_ctx *ctx)
{
	const unsigned block_size = ctx->algo >= ATMEL_HASH_SHA384 ? 128 : 64;
	const unsigned break_point = block_size * 7 / 8;

	const u64 bits = cpu_to_be64(((u64)ctx->length) << 3);

	/* 64 byte, 512 bit block size */
	const unsigned remaining = ctx->length & (block_size - 1);

	const bool block1 = remaining < break_point;
	const unsigned padlen = block1 ? (block_size - remaining) :
			  (2 * block_size - remaining);

	debug("Atmel sha, padding with %d bytes of size\n", padlen);

	/* zero remainder of the buffer */
	memset(ctx->buffer + remaining, 0, block_size - remaining);

	/* set last entry to be 0x80 then 0's*/
	*(ctx->buffer + remaining) = 0x80;

	if (!block1) {
		atmel_sha_process(ctx->sha, ctx->buffer, block_size);

		/* zero beginning of the buffer, end was zeroed above */
		memset(ctx->buffer, 0, remaining + 1);
	}

	/* Bolt number of bits to the end */
	memcpy(ctx->buffer + block_size - sizeof(u64), &bits, sizeof(u64));

	atmel_sha_process(ctx->sha, ctx->buffer, block_size);

	ctx->length += padlen;
}

static void atmel_sha_finish(struct sha_ctx *ctx, void *dest_buf)
{
	struct atmel_sha *sha = ctx->sha;
	size_t len, len32, i;

	/* Copy data back */
	switch (ctx->algo) {
	case ATMEL_HASH_SHA1:
		len = SHA1_SUM_LEN;
		break;
	case ATMEL_HASH_SHA256:
		len = SHA256_SUM_LEN;
		break;
	case ATMEL_HASH_SHA384:
		len = SHA384_SUM_LEN;
		break;
	case ATMEL_HASH_SHA512:
		len = SHA512_SUM_LEN;
		break;
	}

	len32 = len / sizeof(u32);

	if ((long)dest_buf & 3) {
		u32 buf[HASH_MAX_DIGEST_SIZE / sizeof(u32)];
		for (i = 0; i < len32; i++)
			buf[i] = sha->iodatarx[i];
		memcpy(dest_buf, buf, len);
	} else {
		for (i = 0; i < len32; i++)
			((u32*)dest_buf)[i] = sha->iodatarx[i];
	}

	/* Reset the SHA engine */
	writel(ATMEL_HASH_CR_SWRST, &sha->cr);

	at91_periph_clk_disable(ATMEL_ID_SHA);
}


static void atmel_sha_digest(enum atmel_hash_algos algo,
			     const uchar * in_addr, uint buflen,
			     uchar * out_addr, uint chunk_sz)
{
	struct sha_ctx ctx =
	{
		.sha	= (struct atmel_sha *)ATMEL_BASE_SHA,
		.algo	= algo,
		.length = 0,
	};

	atmel_sha_init(&ctx);
	atmel_sha_update(&ctx, in_addr, buflen, chunk_sz);
	atmel_sha_fill_padding(&ctx);
	atmel_sha_finish(&ctx, out_addr);
}

static struct sha_ctx *atmel_sha_alloc(enum atmel_hash_algos algo)
{
	struct sha_ctx *ctx = malloc(sizeof(struct sha_ctx));

	if (!ctx)
		return NULL;

	ctx->sha = (struct atmel_sha *)ATMEL_BASE_SHA;
	ctx->algo = algo;
	ctx->length = 0;

	atmel_sha_init(ctx);

	return ctx;
}

/**
 * Computes hash value of input pbuf using h/w acceleration
 *
 * @param in_addr	A pointer to the input buffer
 * @param buflen	Byte length of input buffer
 * @param out_addr	A pointer to the output buffer. When complete
 *			32 or 64 bytes are copied to pout[0]...pout[31]. Thus, a user
 *			should allocate at least 32 bytes at pOut in advance.
 * @param chunk_size	chunk size for sha256
 */
#if CONFIG_IS_ENABLED(SHA1)
void hw_sha1(const uchar *in_addr, uint buflen, uchar *out_addr, uint chunk_size)
{
	atmel_sha_digest(ATMEL_HASH_SHA1, in_addr, buflen, out_addr, chunk_size);
}
#endif

#if CONFIG_IS_ENABLED(SHA256)
void hw_sha256(const uchar *in_addr, uint buflen, uchar *out_addr, uint chunk_size)
{
	atmel_sha_digest(ATMEL_HASH_SHA256, in_addr, buflen, out_addr, chunk_size);
}
#endif

#if CONFIG_IS_ENABLED(SHA384)
void hw_sha384(const uchar *in_addr, uint buflen, uchar *out_addr, uint chunk_size)
{
	atmel_sha_digest(ATMEL_HASH_SHA384, in_addr, buflen, out_addr, chunk_size);
}
#endif

#if CONFIG_IS_ENABLED(SHA512)
void hw_sha512(const uchar *in_addr, uint buflen, uchar *out_addr, uint chunk_size)
{
	atmel_sha_digest(ATMEL_HASH_SHA512, in_addr, buflen, out_addr, chunk_size);
}
#endif

/*
 * Create the context for sha progressive hashing using h/w acceleration
 *
 * @algo: Pointer to the hash_algo struct
 * @ctxp: Pointer to the pointer of the context for hashing
 * @return 0 if ok, -ve on error
 */
int hw_sha_init(struct hash_algo *algo, void **ctxp)
{
	enum atmel_hash_algos halgo;

#if CONFIG_IS_ENABLED(SHA1)
	if (!strcmp(algo->name, "sha1"))
		halgo = ATMEL_HASH_SHA1;
	else
#endif
#if CONFIG_IS_ENABLED(SHA256)
	if (!strcmp(algo->name, "sha256"))
		halgo = ATMEL_HASH_SHA256;
	else
#endif
#if CONFIG_IS_ENABLED(SHA384)
	if (!strcmp(algo->name, "sha384"))
		halgo = ATMEL_HASH_SHA384;
	else
#endif
#if CONFIG_IS_ENABLED(SHA512)
	if (!strcmp(algo->name, "sha512"))
		halgo = ATMEL_HASH_SHA512;
	else
#endif
	return -EINVAL;

	*ctxp = atmel_sha_alloc(halgo);
	if (!*ctxp) {
		debug("Failed to allocate context\n");
		return -ENOMEM;
	}

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
	atmel_sha_update(sha_ctx, buf, size, size + 1);

	if (is_last)
		atmel_sha_fill_padding(sha_ctx);

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
	if (size < algo->digest_size)
		return -EINVAL;

	atmel_sha_finish(ctx, dest_buf);

	free(ctx);

	return 0;
}

#if CONFIG_IS_ENABLED(DM_HASH)

static enum atmel_hash_algos dm_hash_algo_decode(enum HASH_ALGO algo)
{
	switch (algo) {
#if CONFIG_IS_ENABLED(SHA1)
	case HASH_ALGO_SHA1:
		return ATMEL_HASH_SHA1;
#endif
#if CONFIG_IS_ENABLED(SHA256)
	case HASH_ALGO_SHA256:
		return ATMEL_HASH_SHA256;
#endif
#if CONFIG_IS_ENABLED(SHA384)
	case HASH_ALGO_SHA384:
		return ATMEL_HASH_SHA384;
#endif
#if CONFIG_IS_ENABLED(SHA512)
	case HASH_ALGO_SHA512:
		return ATMEL_HASH_SHA512;
#endif
	default:
		return -1;
	}
}

static int dm_hash_init(struct udevice *dev, enum HASH_ALGO algo, void **ctxp)
{
	enum atmel_hash_algos halgo = dm_hash_algo_decode(algo);

	if (halgo == -1)
		return -ENOTSUPP;

	*ctxp = atmel_sha_alloc(halgo);
	if (!*ctxp) {
		debug("Failed to allocate context\n");
		return -ENOMEM;
	}

	return 0;
}

static int dm_hash_update(struct udevice *dev, void *ctx, const void *ibuf, uint32_t ilen)
{
	atmel_sha_update(ctx, ibuf, ilen, ilen + 1);

	return 0;
}

static int dm_hash_finish(struct udevice *dev, void *ctx, void *obuf)
{
	atmel_sha_fill_padding(ctx);
	atmel_sha_finish(ctx, obuf);

	free(ctx);

	return 0;
}

static int dm_hash_digest_wd(struct udevice *dev, enum HASH_ALGO algo,
			  const void *ibuf, const uint32_t ilen,
			  void *obuf, uint32_t chunk_sz)
{
	enum atmel_hash_algos halgo = dm_hash_algo_decode(algo);

	if (halgo == -1)
		return -ENOTSUPP;

	atmel_sha_digest(halgo, ibuf, ilen, obuf, chunk_sz);

	return 0;
}

static int dm_hash_digest(struct udevice *dev, enum HASH_ALGO algo,
			  const void *ibuf, const uint32_t ilen,
			  void *obuf)
{
	enum atmel_hash_algos halgo = dm_hash_algo_decode(algo);

	if (halgo == -1)
		return -ENOTSUPP;

	atmel_sha_digest(halgo, ibuf, ilen, obuf, ilen + 1);

	return 0;
}

static const struct hash_ops hash_ops_atmel = {
	.hash_init	= dm_hash_init,
	.hash_update	= dm_hash_update,
	.hash_finish	= dm_hash_finish,
	.hash_digest_wd = dm_hash_digest_wd,
	.hash_digest	= dm_hash_digest,
};

static const struct udevice_id atmel_sha_ids[] = {
	{ .compatible = "atmel,at91sam9g46-sha" },
	{ }
};

U_BOOT_DRIVER(hash_atmel) = {
	.name		= "hash_atmel",
	.id		= UCLASS_HASH,
	.of_match	= atmel_sha_ids,
	.ops		= &hash_ops_atmel,
};

#endif
