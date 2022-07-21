#ifndef FCH_SPI_H
#define FCH_SPI_H

#define TYPE_FCH_SPI "fch-spi"

DeviceState *fch_spi_add_flash(DeviceState *fch_spi, BlockBackend *blk, const char *flash_type);

#endif
