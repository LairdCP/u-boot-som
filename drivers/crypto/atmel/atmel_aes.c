// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 Boris Krasnovskiy <boris.krasnovskiy@ezurio.com>
 */

#include <common.h>

#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/at91_pmc.h>

#include <uboot_aes.h>

#include "atmel-aes-regs.h"

#define SIZE_IN_WORDS(x)        ((x) >> 2)

static inline u32 atmel_aes_read(u32 offset)
{
	return readl(ATMEL_BASE_AES + offset);
}

static inline void atmel_aes_write(u32 offset, u32 value)
{
	writel(value, ATMEL_BASE_AES + offset);
}

static void atmel_aes_read_n(u32 offset, u32 *value, int count)
{
	for (; count--; value++, offset += 4)
		*value = atmel_aes_read(offset);
}

static void atmel_aes_write_n(u32 offset, const u32 *value, int count)
{
	for (; count--; value++, offset += 4)
		atmel_aes_write(offset, *value);
}

static inline void atmel_aes_read_block(u32 offset, u8 *value)
{
	atmel_aes_read_n(offset, (u32*)value, SIZE_IN_WORDS(AES_BLOCK_LENGTH));
}

static inline void atmel_aes_write_block(u32 offset, const u8 *value)
{
	atmel_aes_write_n(offset, (u32*)value, SIZE_IN_WORDS(AES_BLOCK_LENGTH));
}

static void atmel_aes_write_ctrl_key(u32 flags, const u8 *iv,
				     const u8 *key, u32 keylen)
{
	u32 valmr = AES_MR_SMOD_AUTO | flags;

	at91_periph_clk_enable(ATMEL_ID_AES);

	/* MR register must be set before IV registers */
	switch (keylen) {
	case AES128_KEY_LENGTH:
		valmr |= AES_MR_KEYSIZE_128;
		break;
	case AES192_KEY_LENGTH:
		valmr |= AES_MR_KEYSIZE_192;
		break;
	case AES256_KEY_LENGTH:
		valmr |= AES_MR_KEYSIZE_256;
		break;
	default:
		return;
	}

	atmel_aes_write(AES_CR, AES_CR_SWRST);
	atmel_aes_write(AES_MR, 0xE << AES_MR_CKEY_OFFSET);

	atmel_aes_write(AES_MR, valmr);

	atmel_aes_write_n(AES_KEYWR(0), (u32*)key, SIZE_IN_WORDS(keylen));

	if (iv)
		atmel_aes_write_block(AES_IVR(0), iv);
	else if (flags & AES_MR_OPMOD_CBC) {
		u32 ivt[ AES_BLOCK_LENGTH / sizeof(u32) ] = { 0 };
		atmel_aes_write_block(AES_IVR(0), (u8*)ivt);
	}
}

static void atmel_aes_crypt(u8 *src, u8 *dst, u32 num_aes_blocks)
{
	u32 i;

	for (i = 0; i < num_aes_blocks; ++i) {
		atmel_aes_write_block(AES_IDATAR(0), src);

		while (!(atmel_aes_read(AES_ISR) & AES_INT_DATARDY)) {
		}

		atmel_aes_read_block(AES_ODATAR(0), dst);

		src += AES_BLOCK_LENGTH;
		dst += AES_BLOCK_LENGTH;
	}

	atmel_aes_write(AES_CR, AES_CR_SWRST);

	at91_periph_clk_disable(ATMEL_ID_AES);
}

void aes_encrypt(u32 key_size, u8 *in, u8 *expkey, u8 *out)
{
	atmel_aes_write_ctrl_key(AES_MR_CYPHER_ENC | AES_MR_OPMOD_ECB, NULL,
				 expkey, key_size);

	atmel_aes_crypt(in, out, 1);
}

void aes_decrypt(u32 key_size, u8 *in, u8 *expkey, u8 *out)
{
	atmel_aes_write_ctrl_key(AES_MR_OPMOD_ECB, NULL, expkey, key_size);

	atmel_aes_crypt(in, out, 1);
}

void aes_cbc_encrypt_blocks(u32 key_size, u8 *key_exp, u8 *iv, u8 *src, u8 *dst,
			    u32 num_aes_blocks)
{
	atmel_aes_write_ctrl_key(AES_MR_CYPHER_ENC | AES_MR_OPMOD_CBC, iv,
				 key_exp, key_size);

	atmel_aes_crypt(src, dst, num_aes_blocks);
}

void aes_cbc_decrypt_blocks(u32 key_size, u8 *key_exp, u8 *iv, u8 *src,
			    u8 *dst, u32 num_aes_blocks)
{
	atmel_aes_write_ctrl_key(AES_MR_OPMOD_CBC, iv, key_exp, key_size);

	atmel_aes_crypt(src, dst, num_aes_blocks);
}

void aes_expand_key(u8 *key, u32 key_size, u8 *expkey)
{
	memcpy(expkey, key, key_size);
}
