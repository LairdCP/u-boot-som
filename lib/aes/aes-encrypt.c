/*
 * Copyright (c) 2018, Laird.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include "mkimage.h"
#include <stdio.h>
#include <string.h>
#include <image.h>
#include <time.h>


int set_hex(const char *in, unsigned char *out, int size)
{
    int i, n;
    unsigned char j;

    n = strlen(in);
    if (n > (size * 2)) {
        printf("hex string is too long\n");
        return (0);
    }
    memset(out, 0, size);
    for (i = 0; i < n; i++) {
        j = (unsigned char)*in++;
        if (j == 0)
            break;
        if ((j >= '0') && (j <= '9'))
            j -= '0';
        else if ((j >= 'A') && (j <= 'F'))
            j = j - 'A' + 10;
        else if ((j >= 'a') && (j <= 'f'))
            j = j - 'a' + 10;
        else {
            printf("non-hex digit\n");
            return (0);
        }
        if (i & 1)
            out[i / 2] |= j;
        else
            out[i / 2] = (j << 4);
    }
    return (1);
}

int aes_add_encryption_data(struct image_encrypt_info *info, void *keydest)
{
	int parent;
	int ret = 0;
	unsigned char key[16];
	unsigned char iv[16];
	unsigned char cmac[16];

	debug("%s: Getting encryption data\n", __func__);

	parent = fdt_subnode_offset(keydest, 0, "encryption");
	if (parent == -FDT_ERR_NOTFOUND) {
		parent = fdt_add_subnode(keydest, 0, "encryption");
		if (parent < 0) {
			ret = parent;
			if (ret != -FDT_ERR_NOSPACE) {
				fprintf(stderr, "Couldn't create encryption node: %s\n",
					fdt_strerror(parent));
			}
		}
	}
	if (ret) {
		printf("Had a problem making the encryption node\n");
		return ret;
	}

	if (info->cipher_key) {
		if (set_hex(info->cipher_key, key, sizeof(key)))
			ret = fdt_setprop(keydest, parent, "aes,cipher-key", key, sizeof(key));
		else
			return -1;
	}

	if (info->iv) {
		if (set_hex(info->iv, iv, sizeof(iv)))
			ret = fdt_setprop(keydest, parent, "aes,iv", iv, sizeof(iv));
		else
			return -1;
	}

	if (info->cmac_key) {
		if (set_hex(info->cmac_key, cmac, sizeof(cmac)))
			ret = fdt_setprop(keydest, parent, "aes,cmac-key", cmac, sizeof(cmac)); /* Always 128bit */
		else
			return -1;
	}

	return ret;
}
