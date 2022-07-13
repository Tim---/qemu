#ifndef ZEN_CPUID_H
#define ZEN_CPUID_H

/* https://github.com/leogx9r/ryzen_smu */

typedef enum {
    CODENAME_SUMMIT_RIDGE,
    CODENAME_PINNACLE_RIDGE,
    CODENAME_RAVEN_RIDGE,
    CODENAME_PICASSO,
    CODENAME_MATISSE,
    CODENAME_VERMEER,
    CODENAME_LUCIENNE,
    CODENAME_RENOIR,
    CODENAME_CEZANNE,
} zen_codename;

uint32_t zen_get_cpuid(zen_codename codename);
uint32_t zen_get_ram_size(zen_codename codename);
uint32_t zen_get_bootrom_revid(zen_codename codename);
uint32_t zen_get_num_ccp_queues(zen_codename codename);

#endif
