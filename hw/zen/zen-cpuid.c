#include "qemu/osdep.h"
#include "hw/zen/zen-cpuid.h"

const char *zen_get_name(zen_codename codename)
{
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
        return "summit-ridge";
    case CODENAME_PINNACLE_RIDGE:
        return "pinnacle-ridge";
    case CODENAME_RAVEN_RIDGE:
        return "raven-ridge";
    case CODENAME_PICASSO:
        return "picasso";
    case CODENAME_MATISSE:
        return "matisse";
    case CODENAME_VERMEER:
        return "vermeer";
    case CODENAME_LUCIENNE:
        return "lucienne";
    case CODENAME_RENOIR:
        return "renoir";
    case CODENAME_CEZANNE:
        return "cezanne";
    default:
        g_assert_not_reached();
    }
}

zen_codename zen_get_codename(const char *name)
{
    if(!strcmp(name, "summit-ridge")) {
        return CODENAME_SUMMIT_RIDGE;
    } else if(!strcmp(name, "pinnacle-ridge")) {
        return CODENAME_PINNACLE_RIDGE;
    } else if(!strcmp(name, "raven-ridge")) {
        return CODENAME_RAVEN_RIDGE;
    } else if(!strcmp(name, "picasso")) {
        return CODENAME_PICASSO;
    } else if(!strcmp(name, "matisse")) {
        return CODENAME_MATISSE;
    } else if(!strcmp(name, "vermeer")) {
        return CODENAME_VERMEER;
    } else if(!strcmp(name, "lucienne")) {
        return CODENAME_LUCIENNE;
    } else if(!strcmp(name, "renoir")) {
        return CODENAME_RENOIR;
    } else if(!strcmp(name, "cezanne")) {
        return CODENAME_CEZANNE;
    }
    g_assert_not_reached();
}


uint32_t zen_get_cpuid(zen_codename codename)
{
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
        return 0x00800f11;
    case CODENAME_PINNACLE_RIDGE:
        return 0x00800f82;
    case CODENAME_RAVEN_RIDGE:
        return 0x00810f10;
    case CODENAME_PICASSO:
        return 0x00810f81;
    case CODENAME_MATISSE:
        return 0x00870f10;
    case CODENAME_VERMEER:
        return 0x00a20f10;
    case CODENAME_LUCIENNE:
        return 0x00860f81;
    case CODENAME_RENOIR:
        return 0x00860f01;
    case CODENAME_CEZANNE:
        return 0x00a50f00;
    default:
        g_assert_not_reached();
    }
}

uint32_t zen_get_bootrom_revid(zen_codename codename)
{
    /*
    Note: we are probably missing the "revision" part (low byte) of this.
    I guess the top 3 bytes are used to select the bootloader, and the low byte
    is used inside the bootloader to target different models.
    */
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
        return 0xbc090000;
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        return 0xbc0a0000;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        return 0xbc0b0500;
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
        return 0xbc0c0000;
    case CODENAME_CEZANNE:
        return 0xbc0c0140;
    default:
        g_assert_not_reached();
    }
}

uint32_t zen_get_ram_size(zen_codename codename)
{
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
        return 0x40000;
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        return 0x50000;
    default:
        g_assert_not_reached();
    }
}

uint32_t zen_get_num_ccp_queues(zen_codename codename)
{
    switch(codename) {
    case CODENAME_SUMMIT_RIDGE:
    case CODENAME_PINNACLE_RIDGE:
    case CODENAME_RAVEN_RIDGE:
    case CODENAME_PICASSO:
    case CODENAME_MATISSE:
    case CODENAME_VERMEER:
        return 5;
    case CODENAME_LUCIENNE:
    case CODENAME_RENOIR:
    case CODENAME_CEZANNE:
        return 3;
    default:
        g_assert_not_reached();
    }
}
