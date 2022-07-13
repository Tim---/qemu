#ifndef CCP_INTERNAL_H
#define CCP_INTERNAL_H

enum ccp_engine {
    CCP_ENGINE_AES = 0,
    CCP_ENGINE_XTS_AES_128,
    CCP_ENGINE_DES3,
    CCP_ENGINE_SHA,
    CCP_ENGINE_RSA,
    CCP_ENGINE_PASSTHRU,
    CCP_ENGINE_ZLIB_DECOMPRESS,
    CCP_ENGINE_ECC,
    CCP_ENGINE__LAST,
};

enum ccp_aes_type {
    CCP_AES_TYPE_128 = 0,
    CCP_AES_TYPE_192,
    CCP_AES_TYPE_256,
    CCP_AES_TYPE__LAST,
};

enum ccp_aes_mode {
    CCP_AES_MODE_ECB = 0,
    CCP_AES_MODE_CBC,
    CCP_AES_MODE_OFB,
    CCP_AES_MODE_CFB,
    CCP_AES_MODE_CTR,
    CCP_AES_MODE_CMAC,
    CCP_AES_MODE_GHASH,
    CCP_AES_MODE_GCTR,
    CCP_AES_MODE_GCM,
    CCP_AES_MODE_GMAC,
    CCP_AES_MODE__LAST,
};

enum ccp_aes_action {
    CCP_AES_ACTION_DECRYPT = 0,
    CCP_AES_ACTION_ENCRYPT,
    CCP_AES_ACTION__LAST,
};

enum ccp_memtype {
    CCP_MEMTYPE_SYSTEM = 0,
    CCP_MEMTYPE_SB,
    CCP_MEMTYPE_LOCAL,
    CCP_MEMTYPE__LAST,
};

enum ccp_passthru_bitwise {
    CCP_PASSTHRU_BITWISE_NOOP = 0,
    CCP_PASSTHRU_BITWISE_AND,
    CCP_PASSTHRU_BITWISE_OR,
    CCP_PASSTHRU_BITWISE_XOR,
    CCP_PASSTHRU_BITWISE_MASK,
    CCP_PASSTHRU_BITWISE__LAST,
};

enum ccp_passthru_byteswap {
    CCP_PASSTHRU_BYTESWAP_NOOP = 0,
    CCP_PASSTHRU_BYTESWAP_32BIT,
    CCP_PASSTHRU_BYTESWAP_256BIT,
    CCP_PASSTHRU_BYTESWAP__LAST,
};

enum ccp_sha_type {
    CCP_SHA_TYPE_1 = 1,
    CCP_SHA_TYPE_224,
    CCP_SHA_TYPE_256,
    CCP_SHA_TYPE_384,
    CCP_SHA_TYPE_512,
    CCP_SHA_TYPE__LAST,
};



typedef uint32_t u32;
typedef uint16_t u16;

struct dword0 {
    unsigned int soc:1;
    unsigned int ioc:1;
    unsigned int rsvd1:1;
    unsigned int init:1;
    unsigned int eom:1;     /* AES/SHA only */
    unsigned int function:15;
    unsigned int engine:4;
    unsigned int prot:1;
    unsigned int rsvd2:7;
};

struct dword3 {
    unsigned int  src_hi:16;
    unsigned int  src_mem:2;
    unsigned int  lsb_cxt_id:8;
    unsigned int  rsvd1:5;
    unsigned int  fixed:1;
};

union dword4 {
    u32 dst_lo;     /* NON-SHA  */
    u32 sha_len_lo;     /* SHA      */
};

union dword5 {
    struct {
        unsigned int  dst_hi:16;
        unsigned int  dst_mem:2;
        unsigned int  rsvd1:13;
        unsigned int  fixed:1;
    } fields;
    u32 sha_len_hi;
};

struct dword7 {
    unsigned int  key_hi:16;
    unsigned int  key_mem:2;
    unsigned int  rsvd1:14;
};

struct ccp5_desc {
    struct dword0 dw0;
    u32 length;
    u32 src_lo;
    struct dword3 dw3;
    union dword4 dw4;
    union dword5 dw5;
    u32 key_lo;
    struct dword7 dw7;
};

union ccp_function {
    struct {
        u16 size:7;
        u16 encrypt:1;
        u16 mode:5;
        u16 type:2;
    } aes;
    struct {
        u16 size:7;
        u16 encrypt:1;
        u16 rsvd:5;
        u16 type:2;
    } aes_xts;
    struct {
        u16 size:7;
        u16 encrypt:1;
        u16 mode:5;
        u16 type:2;
    } des3;
    struct {
        u16 rsvd1:10;
        u16 type:4;
        u16 rsvd2:1;
    } sha;
    struct {
        u16 mode:3;
        u16 size:12;
    } rsa;
    struct {
        u16 byteswap:2;
        u16 bitwise:3;
        u16 reflect:2;
        u16 rsvd:8;
    } pt;
    struct  {
        u16 rsvd:13;
    } zlib;
    struct {
        u16 size:10;
        u16 type:2;
        u16 mode:3;
    } ecc;
    u16 raw;
};

#endif
