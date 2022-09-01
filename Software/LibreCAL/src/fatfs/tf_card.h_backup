#ifndef _TF_CARD_H_
#define _TF_CARD_H_

#define USE_SPI0 // to use PIN_SPI0_xxx
//#define USE_SPI1 // to use PIN_SPI1_xxx

// CLK_FAST: actually set to clk_peri (= 125.0 MHz) / N,
// which is determined by spi_set_baudrate() in pico-sdk/src/rp2_common/hardware_spi/spi.c
#define CLK_FAST	(50 * MHZ)

/* SPI pin assignment */
#define PIN_SPI0_MISO   4   // 0, 4, 16
#define PIN_SPI0_CS     5   // 1, 5, 17
#define PIN_SPI0_SCK    2   // 2, 6, 18
#define PIN_SPI0_MOSI   3   // 3, 7, 19

#define PIN_SPI1_MISO   8   //  8, 12
#define PIN_SPI1_CS     9   //  9, 13
#define PIN_SPI1_SCK    10  // 10, 14
#define PIN_SPI1_MOSI   11  // 11, 15

#endif // _TF_CARD_H_
