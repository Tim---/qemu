/*
 * PSP CCP (cryptographic processor) operations.
 *
 * Copyright (C) 2021 Timothée Cocault <timothee.cocault@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "qemu/osdep.h"
#include <nettle/aes.h>
#include "hw/sysbus.h"
#include "hw/pci/pci.h"
#include "crypto/cipher.h"
#include "crypto/hash.h"
#include "qemu-common.h"
#include "qapi/error.h"
#include "qemu/qemu-print.h"
#include <openssl/bn.h>
#include <nettle/sha1.h>
#include <nettle/sha2.h>
#include <zlib.h>


#include "hw/misc/ccp.h"
#include "ccp-internal.h"
#include "hw/misc/ccp-ops.h"
#include "trace.h"

#define NOASSERT(f)

struct addr_t {
    enum ccp_memtype type;
    hwaddr addr;
};
typedef struct addr_t addr_t;

static inline addr_t get_src(struct ccp5_desc * desc)
{
    assert(desc->dw3.fixed == 0);
    addr_t res = {
        .type=desc->dw3.src_mem,
        .addr=((hwaddr)desc->dw3.src_hi << 0x20) | desc->src_lo,
    };
    return res;
}

static inline addr_t get_dst(struct ccp5_desc * desc)
{
    assert(desc->dw5.fields.fixed == 0);
    addr_t res = {
        .type=desc->dw5.fields.dst_mem,
        .addr=((hwaddr)desc->dw5.fields.dst_hi << 0x20) | desc->dw4.dst_lo,
    };
    return res;
}

static inline addr_t get_key(struct ccp5_desc * desc)
{
    addr_t res = {
        .type=desc->dw7.key_mem,
        .addr=((hwaddr)desc->dw7.key_hi << 0x20) | desc->key_lo,
    };
    return res;
}

static inline void assert_no_key(struct ccp5_desc * desc)
{
    assert(desc->dw7.key_mem == 0);
    assert(desc->dw7.key_hi == 0);
    assert(desc->key_lo == 0);
}

static inline addr_t get_lsb_cxt(struct ccp5_desc * desc)
{
    addr_t res = {
        .type=CCP_MEMTYPE_SB,
        .addr=desc->dw3.lsb_cxt_id * 0x20,
    };
    return res;
}

static void read_from_system(CcpState *s, hwaddr addr, void *buf, hwaddr len)
{
    address_space_read(&address_space_memory, addr, MEMTXATTRS_UNSPECIFIED, buf, len);
}

static void read_from_sb(CcpState *s, hwaddr addr, void *buf, hwaddr len)
{
    memcpy(buf, &s->sb[addr], len); // TODO: yeah, that's safe !
}


static void write_to_system(CcpState *s, hwaddr addr, void *buf, hwaddr len)
{
    address_space_write(&address_space_memory, addr, MEMTXATTRS_UNSPECIFIED, buf, len);
}

static void write_to_sb(CcpState *s, hwaddr addr, void *buf, hwaddr len)
{
    memcpy(&s->sb[addr], buf, len); // TODO: yeah, that's safe !
}


static void do_read(CcpState *s, void *buf, addr_t *addr, hwaddr len)
{
    switch(addr->type)
    {
    case CCP_MEMTYPE_LOCAL:
    case CCP_MEMTYPE_SYSTEM:
        read_from_system(s, addr->addr, buf, len);
        break;
    case CCP_MEMTYPE_SB:
        read_from_sb(s, addr->addr, buf, len);
        break;
    default:
        g_assert_not_reached();
    }
}

static void do_write(CcpState *s, void *buf, addr_t *addr, hwaddr len)
{
    switch(addr->type)
    {
    case CCP_MEMTYPE_LOCAL:
    case CCP_MEMTYPE_SYSTEM:
        write_to_system(s, addr->addr, buf, len);
        break;
    case CCP_MEMTYPE_SB:
        write_to_sb(s, addr->addr, buf, len);
        break;
    default:
        g_assert_not_reached();
    }
}

static const char *memtype_to_str(enum ccp_memtype memtype) {
    switch(memtype) {
    case CCP_MEMTYPE_SYSTEM:
        return "SYS";
    case CCP_MEMTYPE_SB:
        return "SB";
    case CCP_MEMTYPE_LOCAL:
        return "LOC";
    default:
        g_assert_not_reached();
    }
}

static void reverse_bytes(void *buf, int len)
{
    uint8_t *p = buf;
    uint8_t tmp;
    for(int i = 0; i < len / 2; i++)
    {
        tmp = p[i];
        p[i] = p[len - i - 1];
        p[len - i - 1] = tmp;
    }
}

static void reverse_u32(uint32_t *p, int len)
{
    uint32_t tmp;
    for(int i = 0; i < len / 2; i++)
    {
        tmp = p[i];
        p[i] = p[len - i - 1];
        p[len - i - 1] = tmp;
    }
}

static void reverse_u64(uint64_t *p, int len)
{
    uint64_t tmp;
    for(int i = 0; i < len / 2; i++)
    {
        tmp = p[i];
        p[i] = p[len - i - 1];
        p[len - i - 1] = tmp;
    }
}

union ccp_aes_ctx {
    struct aes128_ctx aes128_ctx;
    struct aes192_ctx aes192_ctx;
    struct aes256_ctx aes256_ctx;
};

// AES128

static void ccp_aes128_set_encrypt_key(union ccp_aes_ctx *ctx, const uint8_t *key)
{
    aes128_set_encrypt_key(&ctx->aes128_ctx, key);
}

static void ccp_aes128_set_decrypt_key(union ccp_aes_ctx *ctx, const uint8_t *key)
{
    aes128_set_decrypt_key(&ctx->aes128_ctx, key);
}

static void ccp_aes128_encrypt(const union ccp_aes_ctx *ctx, uint8_t *dst, const uint8_t *src)
{
    aes128_encrypt(&ctx->aes128_ctx, AES_BLOCK_SIZE, dst, src);
}

static void ccp_aes128_decrypt(const union ccp_aes_ctx *ctx, uint8_t *dst, const uint8_t *src)
{
    aes128_decrypt(&ctx->aes128_ctx, AES_BLOCK_SIZE, dst, src);
}

// AES192

static void ccp_aes192_set_encrypt_key(union ccp_aes_ctx *ctx, const uint8_t *key)
{
    aes192_set_encrypt_key(&ctx->aes192_ctx, key);
}

static void ccp_aes192_set_decrypt_key(union ccp_aes_ctx *ctx, const uint8_t *key)
{
    aes192_set_decrypt_key(&ctx->aes192_ctx, key);
}

static void ccp_aes192_encrypt(const union ccp_aes_ctx *ctx, uint8_t *dst, const uint8_t *src)
{
    aes192_encrypt(&ctx->aes192_ctx, AES_BLOCK_SIZE, dst, src);
}

static void ccp_aes192_decrypt(const union ccp_aes_ctx *ctx, uint8_t *dst, const uint8_t *src)
{
    aes192_decrypt(&ctx->aes192_ctx, AES_BLOCK_SIZE, dst, src);
}

// AES256

static void ccp_aes256_set_encrypt_key(union ccp_aes_ctx *ctx, const uint8_t *key)
{
    aes256_set_encrypt_key(&ctx->aes256_ctx, key);
}

static void ccp_aes256_set_decrypt_key(union ccp_aes_ctx *ctx, const uint8_t *key)
{
    aes256_set_decrypt_key(&ctx->aes256_ctx, key);
}

static void ccp_aes256_encrypt(const union ccp_aes_ctx *ctx, uint8_t *dst, const uint8_t *src)
{
    aes256_encrypt(&ctx->aes256_ctx, AES_BLOCK_SIZE, dst, src);
}

static void ccp_aes256_decrypt(const union ccp_aes_ctx *ctx, uint8_t *dst, const uint8_t *src)
{
    aes256_decrypt(&ctx->aes256_ctx, AES_BLOCK_SIZE, dst, src);
}

struct ccp_op_aes_type {
    const char *name;
    int key_size;
    void (*set_encrypt_key)(union ccp_aes_ctx *ctx, const uint8_t *key);
    void (*set_decrypt_key)(union ccp_aes_ctx *ctx, const uint8_t *key);
    void (*encrypt)(const union ccp_aes_ctx *ctx, uint8_t *dst, const uint8_t *src);
    void (*decrypt)(const union ccp_aes_ctx *ctx, uint8_t *dst, const uint8_t *src);
} ccp_op_aes_types[] = {
    [CCP_AES_TYPE_128] = {
        .name = "AES128",
        .key_size = AES128_KEY_SIZE,
        .set_encrypt_key = ccp_aes128_set_encrypt_key,
        .set_decrypt_key = ccp_aes128_set_decrypt_key,
        .encrypt = ccp_aes128_encrypt,
        .decrypt = ccp_aes128_decrypt,
    },
    [CCP_AES_TYPE_192] = {
        .name = "AES192",
        .key_size = AES192_KEY_SIZE,
        .set_encrypt_key = ccp_aes192_set_encrypt_key,
        .set_decrypt_key = ccp_aes192_set_decrypt_key,
        .encrypt = ccp_aes192_encrypt,
        .decrypt = ccp_aes192_decrypt,
    },
    [CCP_AES_TYPE_256] = {
        .name = "AES256",
        .key_size = AES256_KEY_SIZE,
        .set_encrypt_key = ccp_aes256_set_encrypt_key,
        .set_decrypt_key = ccp_aes256_set_decrypt_key,
        .encrypt = ccp_aes256_encrypt,
        .decrypt = ccp_aes256_decrypt,
    },
};


static void xor_block(uint8_t *dst, const uint8_t *src) {
    for(int i = 0; i < AES_BLOCK_SIZE; i++)
        dst[i] ^= src[i];
}


static void ccp_aes_ecb_encrypt(union ccp_aes_ctx *ctx, struct ccp_op_aes_type *type, uint8_t *buf, int blocks, uint8_t *iv, const uint8_t *key)
{
    type->set_encrypt_key(ctx, key);

    for(int i = 0; i < blocks; i++) {
        uint8_t *p = buf + i * AES_BLOCK_SIZE;
        type->encrypt(ctx, p, p);
    }
}

static void ccp_aes_ecb_decrypt(union ccp_aes_ctx *ctx, struct ccp_op_aes_type *type, uint8_t *buf, int blocks, uint8_t *iv, const uint8_t *key)
{
    type->set_decrypt_key(ctx, key);

    for(int i = 0; i < blocks; i++) {
        uint8_t *p = buf + i * AES_BLOCK_SIZE;
        type->decrypt(ctx, p, p);
    }
}

static void ccp_aes_cbc_encrypt(union ccp_aes_ctx *ctx, struct ccp_op_aes_type *type, uint8_t *buf, int blocks, uint8_t *iv, const uint8_t *key)
{
    type->set_encrypt_key(ctx, key);

    for(int i = 0; i < blocks; i++) {
        uint8_t *p = buf + i * AES_BLOCK_SIZE;

        xor_block(p, iv);
        type->encrypt(ctx, p, p);

        memcpy(iv, p, AES_BLOCK_SIZE);
    }
}

static void ccp_aes_cbc_decrypt(union ccp_aes_ctx *ctx, struct ccp_op_aes_type *type, uint8_t *buf, int blocks, uint8_t *iv, const uint8_t *key)
{
    uint8_t tmp[AES_BLOCK_SIZE];

    type->set_decrypt_key(ctx, key);

    for(int i = 0; i < blocks; i++) {
        uint8_t *p = buf + i * AES_BLOCK_SIZE;

        memcpy(tmp, p, AES_BLOCK_SIZE);
        type->decrypt(ctx, p, p);
        xor_block(p, iv);
        memcpy(iv, tmp, AES_BLOCK_SIZE);
    }
}

static void ccp_aes_cfb_encrypt(union ccp_aes_ctx *ctx, struct ccp_op_aes_type *type, uint8_t *buf, int blocks, uint8_t *iv, const uint8_t *key)
{
    type->set_encrypt_key(ctx, key);

    for(int i = 0; i < blocks; i++) {
        uint8_t *p = buf + i * AES_BLOCK_SIZE;

        type->encrypt(ctx, iv, iv);
        xor_block(p, iv);
        memcpy(iv, p, AES_BLOCK_SIZE);
    }
}

static void ccp_aes_cfb_decrypt(union ccp_aes_ctx *ctx, struct ccp_op_aes_type *type, uint8_t *buf, int blocks, uint8_t *iv, const uint8_t *key)
{
    uint8_t tmp[AES_BLOCK_SIZE];

    type->set_encrypt_key(ctx, key);

    for(int i = 0; i < blocks; i++) {
        uint8_t *p = buf + i * AES_BLOCK_SIZE;

        type->encrypt(ctx, iv, iv);

        memcpy(tmp, p, AES_BLOCK_SIZE);
        xor_block(p, iv);
        memcpy(iv, tmp, AES_BLOCK_SIZE);
    }
}

static void ccp_aes_ofb_crypt(union ccp_aes_ctx *ctx, struct ccp_op_aes_type *type, uint8_t *buf, int blocks, uint8_t *iv, const uint8_t *key)
{
    type->set_encrypt_key(ctx, key);

    for(int i = 0; i < blocks; i++) {
        uint8_t *p = buf + i * AES_BLOCK_SIZE;

        type->encrypt(ctx, iv, iv);
        xor_block(p, iv);
    }
}

struct ccp_op_aes_mode {
    const char *name;
    bool has_iv;
    void (*encrypt)(union ccp_aes_ctx *ctx, struct ccp_op_aes_type *type, uint8_t *buf, int blocks, uint8_t *iv, const uint8_t *key);
    void (*decrypt)(union ccp_aes_ctx *ctx, struct ccp_op_aes_type *type, uint8_t *buf, int blocks, uint8_t *iv, const uint8_t *key);
} ccp_op_aes_modes[] = {
    [CCP_AES_MODE_ECB] = {
        .name = "ECB",
        .has_iv = false,
        .encrypt = ccp_aes_ecb_encrypt,
        .decrypt = ccp_aes_ecb_decrypt,
    },
    [CCP_AES_MODE_CBC] = {
        .name = "CBC",
        .has_iv = true,
        .encrypt = ccp_aes_cbc_encrypt,
        .decrypt = ccp_aes_cbc_decrypt,
    },
    [CCP_AES_MODE_CFB] = {
        .name = "CFB",
        .has_iv = true,
        .encrypt = ccp_aes_cfb_encrypt,
        .decrypt = ccp_aes_cfb_decrypt,
    },
    [CCP_AES_MODE_OFB] = {
        .name = "OFB",
        .has_iv = true,
        .encrypt = ccp_aes_ofb_crypt,
        .decrypt = ccp_aes_ofb_crypt,
    },
};

int ccp_op_aes(CcpState *s, struct ccp5_desc * desc)
{
    /* First, some assertions */

    // Flags
    //assert(desc->dw0.init == 1); // TODO
    //assert(desc->dw0.eom == 1); // TODO
    assert(desc->dw0.prot == 0);

    // Engine / function
    assert(desc->dw0.engine == CCP_ENGINE_AES);
    union ccp_function func = {
        .raw = desc->dw0.function
    };
    //assert(func.aes.size == 0);
    NOASSERT(func.aes.encrypt);
    assert(func.aes.mode == CCP_AES_MODE_ECB || func.aes.mode == CCP_AES_MODE_CBC || func.aes.mode == CCP_AES_MODE_CFB || func.aes.mode == CCP_AES_MODE_OFB);
    assert(func.aes.type == CCP_AES_TYPE_128 || func.aes.type == CCP_AES_TYPE_192 || func.aes.type == CCP_AES_TYPE_256);

    // Misc
    NOASSERT(desc->length);
    // assert(desc->dw3.lsb_cxt_id == 0); // TODO

    // Reserved
    assert(desc->dw0.rsvd1 == 0);
    NOASSERT(desc->dw0.rsvd2);
    assert(desc->dw3.rsvd1 == 0);
    assert(desc->dw5.fields.rsvd1 == 0);
    assert(desc->dw7.rsvd1 == 0);

    /* Now, let's do things */

    addr_t src_addr = get_src(desc);
    addr_t dst_addr = get_dst(desc);
    addr_t key_addr = get_key(desc);

    int length = desc->length;

    assert(length % AES_BLOCK_SIZE == 0);

    struct ccp_op_aes_type *aes_type = &ccp_op_aes_types[func.aes.type];
    struct ccp_op_aes_mode *aes_mode = &ccp_op_aes_modes[func.aes.mode];
    union ccp_aes_ctx ctx;

    trace_ccp_aes(aes_type->name, aes_mode->name, src_addr.addr, memtype_to_str(src_addr.type), dst_addr.addr, memtype_to_str(dst_addr.type), key_addr.addr, memtype_to_str(key_addr.type), length);

    uint8_t key[aes_type->key_size];
    do_read(s, key, &key_addr, aes_type->key_size);
    reverse_bytes(key, aes_type->key_size);

    g_autofree uint8_t * buf = g_malloc(length);
    do_read(s, buf, &src_addr, length);

    uint8_t iv[AES_BLOCK_SIZE];
    addr_t lsb_addr = get_lsb_cxt(desc);

    if(aes_mode->has_iv) {
        do_read(s, iv, &lsb_addr, AES_BLOCK_SIZE);
        reverse_bytes(iv, AES_BLOCK_SIZE);
    }

    switch(func.aes.encrypt) {
    case CCP_AES_ACTION_ENCRYPT:
        aes_mode->encrypt(&ctx, aes_type, buf, length / AES_BLOCK_SIZE, iv, key);
        break;
    case CCP_AES_ACTION_DECRYPT:
        aes_mode->decrypt(&ctx, aes_type, buf, length / AES_BLOCK_SIZE, iv, key);
        break;
    default:
        g_assert_not_reached();
    }

    if(aes_mode->has_iv) {
        reverse_bytes(iv, AES_BLOCK_SIZE);
        do_write(s, iv, &lsb_addr, AES_BLOCK_SIZE);
    }


    //qemu_hexdump(stderr, "out", buf, length);
    do_write(s, buf, &dst_addr, length);

    return 0;
}

int ccp_op_passthru(CcpState *s, struct ccp5_desc * desc)
{
    /* First, some assertions */

    // Flags
    NOASSERT(desc->dw0.init);
    assert(desc->dw0.eom == 1);
    assert(desc->dw0.prot == 0);

    // Engine / function
    assert(desc->dw0.engine == CCP_ENGINE_PASSTHRU);
    union ccp_function func = {
        .raw = desc->dw0.function
    };
    assert(func.pt.bitwise == CCP_PASSTHRU_BITWISE_NOOP);
    assert(func.pt.byteswap == CCP_PASSTHRU_BYTESWAP_NOOP || func.pt.byteswap == CCP_PASSTHRU_BYTESWAP_256BIT);
    assert(func.pt.reflect == 0);
    assert(func.pt.rsvd == 0);

    // Misc
    NOASSERT(desc->length);
    assert(desc->dw3.lsb_cxt_id == 0);

    // Src
    NOASSERT(desc->dw3.src_mem);
    NOASSERT(desc->dw3.src_hi);
    NOASSERT(desc->src_lo);
    assert(desc->dw3.fixed == 0);

    // Dst
    NOASSERT(desc->dw5.fields.dst_mem);
    NOASSERT(desc->dw5.fields.dst_hi);
    NOASSERT(desc->dw4.dst_lo);
    assert(desc->dw5.fields.fixed == 0);

    // Key
    assert(desc->dw7.key_mem == 0);
    assert(desc->dw7.key_hi == 0);
    assert(desc->key_lo == 0);

    // Reserved
    assert(desc->dw0.rsvd1 == 0);
    assert(desc->dw0.rsvd2 == 0);
    assert(desc->dw3.rsvd1 == 0);
    assert(desc->dw5.fields.rsvd1 == 0);
    assert(desc->dw7.rsvd1 == 0);

    /* Now, let's do things */

    addr_t src_addr = get_src(desc);
    addr_t dst_addr = get_dst(desc);
    int length = desc->length;

    trace_ccp_passthru(src_addr.addr, memtype_to_str(src_addr.type), dst_addr.addr, memtype_to_str(dst_addr.type), length);

    g_autofree uint8_t * buf = g_malloc(length);

    do_read(s, buf, &src_addr, length);

    if(func.pt.byteswap == CCP_PASSTHRU_BYTESWAP_256BIT) {
        assert(length % 32 == 0);
        for(int i = 0; i < length; i += 32) {
            reverse_bytes(buf + i, 32);
        }
    }

    do_write(s, buf, &dst_addr, length);

    return 0;
}


union sha_ctx {
    struct sha1_ctx sha1_ctx;
    struct sha256_ctx sha256_ctx;
    struct sha512_ctx sha512_ctx;
};

// SHA1

static void ccp_sha1_read_ctx(union sha_ctx *ctx, uint8_t *data, uint64_t count) {
    memcpy(ctx->sha1_ctx.state, data, SHA1_DIGEST_SIZE);
    reverse_u32(ctx->sha1_ctx.state, _SHA1_DIGEST_LENGTH);
    ctx->sha1_ctx.count = count / SHA1_BLOCK_SIZE;
    ctx->sha1_ctx.index = 0;
}

static void ccp_sha1_write_ctx(union sha_ctx *ctx, uint8_t *data) {
    // If the length is not a multiple of the block length, we need to remember it ?
    // Where is it stored ? For now, we just die...
    assert(ctx->sha1_ctx.index == 0);

    reverse_u32(ctx->sha1_ctx.state, _SHA1_DIGEST_LENGTH);
    memcpy(data, ctx->sha1_ctx.state, SHA1_DIGEST_SIZE);
}

static void ccp_sha1_update(union sha_ctx *ctx, size_t length, const uint8_t *data) {
    sha1_update(&ctx->sha1_ctx, length, data);
}
static void ccp_sha1_digest(union sha_ctx *ctx, size_t length, uint8_t *digest) {
    sha1_digest(&ctx->sha1_ctx, length, digest);
}

// SHA256

static void ccp_sha256_read_ctx(union sha_ctx *ctx, uint8_t *data, uint64_t count) {
    memcpy(ctx->sha256_ctx.state, data, SHA256_DIGEST_SIZE);
    reverse_u32(ctx->sha256_ctx.state, _SHA256_DIGEST_LENGTH);
    ctx->sha256_ctx.count = count / SHA256_BLOCK_SIZE;
    ctx->sha256_ctx.index = 0;
}

static void ccp_sha256_write_ctx(union sha_ctx *ctx, uint8_t *data) {
    // If the length is not a multiple of the block length, we need to remember it ?
    // Where is it stored ? For now, we just die...
    assert(ctx->sha256_ctx.index == 0);

    reverse_u32(ctx->sha256_ctx.state, _SHA256_DIGEST_LENGTH);
    memcpy(data, ctx->sha256_ctx.state, SHA256_DIGEST_SIZE);
}

static void ccp_sha256_update(union sha_ctx *ctx, size_t length, const uint8_t *data) {
    sha256_update(&ctx->sha256_ctx, length, data);
}
static void ccp_sha256_digest(union sha_ctx *ctx, size_t length, uint8_t *digest) {
    sha256_digest(&ctx->sha256_ctx, length, digest);
}

// SHA512

static void ccp_sha512_read_ctx(union sha_ctx *ctx, uint8_t *data, uint64_t count) {
    memcpy(ctx->sha512_ctx.state, data, SHA512_DIGEST_SIZE);
    reverse_u64(ctx->sha512_ctx.state, _SHA512_DIGEST_LENGTH);
    ctx->sha512_ctx.count_low = count / SHA512_BLOCK_SIZE;
    ctx->sha512_ctx.index = 0;
}

static void ccp_sha512_write_ctx(union sha_ctx *ctx, uint8_t *data) {
    // If the length is not a multiple of the block length, we need to remember it ?
    // Where is it stored ? For now, we just die...
    assert(ctx->sha512_ctx.index == 0);

    reverse_u64(ctx->sha512_ctx.state, _SHA512_DIGEST_LENGTH);
    memcpy(data, ctx->sha512_ctx.state, SHA512_DIGEST_SIZE);
}

static void ccp_sha512_update(union sha_ctx *ctx, size_t length, const uint8_t *data) {
    sha512_update(&ctx->sha512_ctx, length, data);
}
static void ccp_sha512_digest(union sha_ctx *ctx, size_t length, uint8_t *digest) {
    sha512_digest(&ctx->sha512_ctx, length, digest);
}

struct sha_algo {
    const char *name;
    int digest_size;
    int state_size;
    int block_size;
    void (*read_ctx)(union sha_ctx *ctx, uint8_t *data, uint64_t count);
    void (*write_ctx)(union sha_ctx *ctx, uint8_t *data);
    void (*update)(union sha_ctx *ctx, size_t length, const uint8_t *data);
    void (*digest)(union sha_ctx *ctx, size_t length, uint8_t *digest);
} sha_algos[] = {
    [CCP_SHA_TYPE_1] = {
        .name = "SHA1",
        .digest_size = SHA1_DIGEST_SIZE,
        .state_size = SHA1_DIGEST_SIZE,
        .block_size = SHA1_BLOCK_SIZE,
        .read_ctx = ccp_sha1_read_ctx,
        .write_ctx = ccp_sha1_write_ctx,
        .update = ccp_sha1_update,
        .digest = ccp_sha1_digest,
    },
    [CCP_SHA_TYPE_224] = {
        .name = "SHA224",
        .digest_size = SHA224_DIGEST_SIZE,
        .state_size = SHA256_DIGEST_SIZE,
        .block_size = SHA224_BLOCK_SIZE,
        .read_ctx = ccp_sha256_read_ctx,
        .write_ctx = ccp_sha256_write_ctx,
        .update = ccp_sha256_update,
        .digest = ccp_sha256_digest,
    },
    [CCP_SHA_TYPE_256] = {
        .name = "SHA256",
        .digest_size = SHA256_DIGEST_SIZE,
        .state_size = SHA256_DIGEST_SIZE,
        .block_size = SHA256_BLOCK_SIZE,
        .read_ctx = ccp_sha256_read_ctx,
        .write_ctx = ccp_sha256_write_ctx,
        .update = ccp_sha256_update,
        .digest = ccp_sha256_digest,
    },
    [CCP_SHA_TYPE_384] = {
        .name = "SHA384",
        .digest_size = SHA384_DIGEST_SIZE,
        .state_size = SHA512_DIGEST_SIZE,
        .block_size = SHA384_BLOCK_SIZE,
        .read_ctx = ccp_sha512_read_ctx,
        .write_ctx = ccp_sha512_write_ctx,
        .update = ccp_sha512_update,
        .digest = ccp_sha512_digest,
    },
    [CCP_SHA_TYPE_512] = {
        .name = "SHA512",
        .digest_size = SHA512_DIGEST_SIZE,
        .state_size = SHA512_DIGEST_SIZE,
        .block_size = SHA512_BLOCK_SIZE,
        .read_ctx = ccp_sha512_read_ctx,
        .write_ctx = ccp_sha512_write_ctx,
        .update = ccp_sha512_update,
        .digest = ccp_sha512_digest,
    },
};

int ccp_op_sha(CcpState *s, struct ccp5_desc * desc)
{
    /* First, some assertions */

    // Flags
    NOASSERT(desc->dw0.init);
    NOASSERT(desc->dw0.eom);
    assert(desc->dw0.prot == 0);

    // Engine / function
    assert(desc->dw0.engine == CCP_ENGINE_SHA);
    union ccp_function func = {
        .raw = desc->dw0.function
    };
    assert(func.sha.rsvd1 == 0);
    NOASSERT(func.sha.type);
    NOASSERT(func.sha.rsvd2); // ?

    // Misc
    NOASSERT(desc->length);
    NOASSERT(desc->dw3.lsb_cxt_id);

    // Sha specific
    NOASSERT(desc->dw5.sha_len_hi);
    NOASSERT(desc->dw4.sha_len_lo);

    // Reserved
    assert(desc->dw0.rsvd1 == 0);
    assert(desc->dw0.rsvd2 == 0);
    assert(desc->dw3.rsvd1 == 0);
    NOASSERT(desc->dw7.rsvd1);

    /* Now, let's do things */

    addr_t src_addr = get_src(desc);
    assert_no_key(desc);
    hwaddr length = desc->length;
    uint64_t sha_len = ((uint64_t)desc->dw5.sha_len_hi << 0x20) | desc->dw4.sha_len_lo;

    addr_t lsb_addr = get_lsb_cxt(desc);

    struct sha_algo *algo = &sha_algos[func.sha.type];
    union sha_ctx ctx;

    g_autofree uint8_t * hash_buf = g_malloc(algo->state_size);

    do_read(s, hash_buf, &lsb_addr, algo->state_size);
    algo->read_ctx(&ctx, hash_buf, sha_len / 8 - length);

    // Update
    g_autofree uint8_t * data_buf = g_malloc(length);
    do_read(s, data_buf, &src_addr, length);
    algo->update(&ctx, length, data_buf);

    // Write result
    if(desc->dw0.eom)
    {
        algo->digest(&ctx, algo->digest_size, hash_buf);
        reverse_bytes(hash_buf, algo->digest_size);
        do_write(s, hash_buf, &lsb_addr, algo->digest_size);
    }
    else
    {
        algo->write_ctx(&ctx, hash_buf);
        do_write(s, hash_buf, &lsb_addr, algo->state_size);
    }

    trace_ccp_sha(algo->name, src_addr.addr, memtype_to_str(src_addr.type), lsb_addr.addr, memtype_to_str(lsb_addr.type), length);

    return 0;
}

int ccp_op_rsa(CcpState *s, struct ccp5_desc * desc)
{
    /* First, some assertions */

    // Flags
    //assert(desc->dw0.init == 0);
    //assert(desc->dw0.eom == 1);
    assert(desc->dw0.prot == 0);

    // Engine / function
    assert(desc->dw0.engine == CCP_ENGINE_RSA);
    union ccp_function func = {
        .raw = desc->dw0.function
    };
    assert(func.rsa.mode == 0);
    //assert(func.rsa.size == 0x100); // 2048 bits

    // Misc
    NOASSERT(desc->length);
    assert(desc->dw3.lsb_cxt_id == 0);

    // Reserved
    assert(desc->dw0.rsvd1 == 0);
    assert(desc->dw0.rsvd2 == 0);
    assert(desc->dw3.rsvd1 == 0);
    assert(desc->dw5.fields.rsvd1 == 0);
    assert(desc->dw7.rsvd1 == 0);

    /* Now, let's do things */

    int rsa_size_bytes = func.rsa.size;


    addr_t src = get_src(desc);
    addr_t dst = get_dst(desc);
    addr_t key = get_key(desc);

    hwaddr length = desc->length;
    assert(length == rsa_size_bytes * 2);


    trace_ccp_rsa(src.addr, memtype_to_str(src.type), dst.addr, memtype_to_str(dst.type), key.addr, memtype_to_str(key.type), length);

    /*
    fprintf(stderr, "CCP  src: %lx (%s)\n", src.addr, memtype_to_str(src.type));
    fprintf(stderr, "CCP  dst: %lx (%s)\n", dst.addr, memtype_to_str(dst.type));
    fprintf(stderr, "CCP  key: %lx (%s)\n", key.addr, memtype_to_str(key.type));
    fprintf(stderr, "CCP  length: %lx\n", length);
    */

    uint8_t mod_sig_buf[rsa_size_bytes * 2];
    do_read(s, mod_sig_buf, &src, rsa_size_bytes * 2);

    uint8_t exp_buf[rsa_size_bytes];
    do_read(s, exp_buf, &key, rsa_size_bytes);

    uint8_t *pmod = mod_sig_buf;
    uint8_t *pexp = exp_buf;
    uint8_t *psig = mod_sig_buf + rsa_size_bytes;

    /*
    qemu_hexdump(stderr, "CCP  mod", pmod, 0x100);
    qemu_hexdump(stderr, "CCP  exp", pexp, 0x100);
    qemu_hexdump(stderr, "CCP  sig", psig, 0x100);
    */

    BN_CTX *ctx = BN_CTX_new();

    BIGNUM *mod = BN_lebin2bn(pmod, rsa_size_bytes, NULL);
    BIGNUM *exp = BN_lebin2bn(pexp, rsa_size_bytes, NULL);
    BIGNUM *sig = BN_lebin2bn(psig, rsa_size_bytes, NULL);

    BIGNUM *r = BN_new();
    BN_mod_exp(r, sig, exp, mod, ctx);

    //fprintf(stderr, "CCP  res: %s\n", BN_bn2hex(r));

    uint8_t outbuf[rsa_size_bytes];
    BN_bn2lebinpad(r, outbuf, rsa_size_bytes);
    do_write(s, outbuf, &dst, rsa_size_bytes);


    BN_free(r);
    BN_free(sig);
    BN_free(exp);
    BN_free(mod);

    BN_CTX_free(ctx);

    return 0;
}

int ccp_op_zlib(CcpState *s, struct ccp5_desc * desc)
{
    /* First, some assertions */

    // Flags
    assert(desc->dw0.soc == 1);
    assert(desc->dw0.ioc == 0);
    assert(desc->dw0.init == 1);
    assert(desc->dw0.eom == 1);
    assert(desc->dw0.prot == 0);

    // Engine / function
    assert(desc->dw0.engine == CCP_ENGINE_ZLIB_DECOMPRESS);
    union ccp_function func = {
        .raw = desc->dw0.function
    };
    assert(func.zlib.rsvd == 0);

    // Misc
    NOASSERT(desc->length);
    assert(desc->dw3.lsb_cxt_id == 0);

    // Reserved
    assert(desc->dw0.rsvd1 == 0);
    assert(desc->dw0.rsvd2 == 0);
    assert(desc->dw3.rsvd1 == 0);
    assert(desc->dw5.fields.rsvd1 == 0);
    assert(desc->dw7.rsvd1 == 0);

    /* Now, let's do things */


    addr_t src = get_src(desc);
    addr_t dst = get_dst(desc);
    assert_no_key(desc);
    hwaddr length = desc->length;

    trace_ccp_zlib(src.addr, memtype_to_str(src.type), dst.addr, memtype_to_str(dst.type), length);

    g_autofree uint8_t * buf = g_malloc(length);
    do_read(s, buf, &src, length);

    uint8_t outbuf[4096];

    z_stream strm;
    int ret;
    memset(&strm, 0, sizeof(strm));

    strm.next_in = (uint8_t *)buf;
    strm.avail_in = length;
    strm.next_out = outbuf;
    strm.avail_out = sizeof(outbuf);

    ret = inflateInit(&strm);
    assert(ret == Z_OK);

    //qemu_hexdump(stderr, "CCP zbuf", buf, length);
    ret = Z_OK;
    while(ret != Z_STREAM_END)
    {
        strm.next_out = outbuf;
        strm.avail_out = sizeof(outbuf);

        ret = inflate(&strm, Z_FINISH);

        assert(ret == Z_OK || ret == Z_BUF_ERROR || ret == Z_STREAM_END);

        int out_len = strm.next_out - outbuf;
        do_write(s, outbuf, &dst, out_len);
        dst.addr += out_len;
    }

    ret = inflateEnd(&strm);
    assert(ret == Z_OK);

    return 0;
}

