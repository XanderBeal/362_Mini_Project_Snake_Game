#include "stdlib.h"



//TFT Components
void init_spi1();
void spi_cmd(unsigned int data);
void spi_data(unsigned int data);
void spi1_init_oled();
void spi1_display1(const char *string);
void spi1_display2(const char *string);
void spi1_setup_dma(void);
void spi1_enable_dma(void);

void init_spi2(void);
void spi2_setup_dma(void);
void spi2_enable_dma(void);
void init_tim15(void);