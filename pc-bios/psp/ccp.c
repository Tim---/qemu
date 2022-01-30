#include <stdint.h>
#include <string.h>
#include "loader-internal.h"
#include "ccp-internal.h"
#include "utils.h"


struct ccp5_desc __attribute__ ((aligned (32))) ccp_descs[1];

void ccp_execute(struct ccp5_desc *desc)
{
    memcpy(ccp_descs, desc, sizeof(struct ccp5_desc));

    *(uint32_t *)(0x03000000 + 0x1000 + 8) = (uint32_t)ccp_descs;
    *(uint32_t *)(0x03000000 + 0x1000 + 4) = (uint32_t)(ccp_descs + 1);
    *(uint32_t *)(0x03000000 + 0x1000 + 0) = 1;
}

void ccp_aes_ecb(void *src, void *dst, void *key, uint32_t length)
{
    struct ccp5_desc desc = {0};

    union ccp_function func = {
        .aes = {
            .size = 0,
            .encrypt = CCP_AES_ACTION_DECRYPT,
            .mode = CCP_AES_MODE_ECB,
            .type = CCP_AES_TYPE_128,
        }
    };

    desc.dw0.soc = 1;
    desc.dw0.engine = CCP_ENGINE_AES;
    desc.dw0.function = func.raw;
    desc.length = length;

    desc.dw3.src_mem = CCP_MEMTYPE_SYSTEM;
    desc.src_lo = (uint32_t)src;

    desc.dw5.fields.dst_mem = CCP_MEMTYPE_SYSTEM;
    desc.dw4.dst_lo = (uint32_t)dst;

    desc.dw7.key_mem = CCP_MEMTYPE_SYSTEM;
    desc.key_lo = (uint32_t)key;

    ccp_execute(&desc);
}

void ccp_passthrough_to_sb(void *src, uint32_t lsb_ctx_id, uint32_t length)
{
    struct ccp5_desc desc = {0};

    desc.dw0.soc = 1;
    desc.dw0.eom = 1;
    desc.dw0.engine = CCP_ENGINE_PASSTHRU;
    desc.dw0.function = 0;
    desc.length = length;

    desc.dw3.src_mem = CCP_MEMTYPE_SYSTEM;
    desc.src_lo = (uint32_t)src;

    desc.dw5.fields.dst_mem = CCP_MEMTYPE_SB;
    desc.dw4.dst_lo = lsb_ctx_id * 0x20;

    ccp_execute(&desc);
}

void ccp_aes_cbc(void *src, void *dst, void *key, uint32_t length, void *iv)
{
    ccp_passthrough_to_sb(iv, 0, 16);

    struct ccp5_desc desc = {0};

    union ccp_function func = {
        .aes = {
            .size = 0,
            .encrypt = CCP_AES_ACTION_DECRYPT,
            .mode = CCP_AES_MODE_CBC,
            .type = CCP_AES_TYPE_128,
        }
    };

    desc.dw0.soc = 1;
    desc.dw0.engine = CCP_ENGINE_AES;
    desc.dw0.function = func.raw;
    desc.length = length;

    desc.dw3.src_mem = CCP_MEMTYPE_SYSTEM;
    desc.src_lo = (uint32_t)src;

    desc.dw5.fields.dst_mem = CCP_MEMTYPE_SYSTEM;
    desc.dw4.dst_lo = (uint32_t)dst;

    desc.dw7.key_mem = CCP_MEMTYPE_SYSTEM;
    desc.key_lo = (uint32_t)key;

    desc.dw3.lsb_cxt_id = 0;

    ccp_execute(&desc);
}

uint8_t wikek[16] = {0x2e, 0xf9, 0xa1, 0x7d, 0x93, 0xae, 0x1b, 0x73, 0x07, 0x84, 0x5b, 0x22, 0xb2, 0x88, 0x3d, 0xc2};
uint8_t ikek[16] = {0x4c, 0x77, 0x63, 0x65, 0x32, 0xfe, 0x4c, 0x6f, 0xd6, 0xb9, 0xd6, 0xd7, 0xb5, 0x1e, 0xde, 0x59};

void decrypt_hw(fw_hdr_t *fw_hdr, void *dst)
{
    uint8_t key[16] = {0};
    uint8_t tmp[16] = {0};

    memcpy(tmp, ikek, 16);
    reverse_bytes(tmp, 16);

    ccp_aes_ecb(fw_hdr->wrapped_key, key, tmp, 16);
    reverse_bytes(key, 16);

    memcpy(tmp, fw_hdr->iv, 16);
    reverse_bytes(tmp, 16);
    uint32_t length = fw_hdr->size_signed;
    ccp_aes_cbc(fw_hdr + 1, dst, key, length, tmp);
}

