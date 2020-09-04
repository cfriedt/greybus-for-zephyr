#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_GREYBUS_SPI_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_GREYBUS_SPI_H_

/* SPI Protocol Mode Bit Masks */
#define GB_SPI_MODE_CPHA        0x01    /* clock phase */
#define GB_SPI_MODE_CPOL        0x02    /* clock polarity */
#define GB_SPI_MODE_CS_HIGH     0x04    /* chipselect active high */
#define GB_SPI_MODE_LSB_FIRST   0x08    /* per-word bits-on-wire */
#define GB_SPI_MODE_3WIRE       0x10    /* SI/SO signals shared */
#define GB_SPI_MODE_LOOP        0x20    /* loopback mode */
#define GB_SPI_MODE_NO_CS       0x40    /* one dev/bus, no chipselect */
#define GB_SPI_MODE_READY       0x80    /* slave pulls low to pause */

/* SPI Protocol Flags */
#define GB_SPI_FLAG_HALF_DUPLEX 0x01    /* can't do full duplex */
#define GB_SPI_FLAG_NO_RX       0x02    /* can't do buffer read */
#define GB_SPI_FLAG_NO_TX       0x04    /* can't do buffer write */

/* SPI Device Type */
#define GB_SPI_SPI_DEV          0x00    /* generic bit bang SPI device */
#define GB_SPI_SPI_NOR          0x01    /* supports JEDEC id */
#define GB_SPI_SPI_MODALIAS     0x02    /* driver is in name field */

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_GREYBUS_SPI_H_ */
