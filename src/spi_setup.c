//STM32 Pin setups / enablers
#include "stm32f0xx.h" 

#include <stdint.h>
#include <stdlib.h>

#include <lcd.h>
#include <spi_setup.h>
void nano_wait(unsigned int);


//int msg_index = 0;
//uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
extern const char font[];


uint8_t col; // the column being scanned

extern int msg_index;
extern uint16_t msg[8];
extern uint16_t display[34];

#define CS_BIT (1 << 8)  // PB8 is the Chip Select pin
int  read_rows();
void update_history(int col, int rows);
void drive_column(int);




//uint16_t display[34] = {
  //  0x002, // Command to set the cursor at the first position line 1
    //0x200+'E', 0x200+'C', 0x200+'E', 0x200+'3', 0x200+'6', + 0x200+'2', 0x200+' ', 0x200+'i',
    //0x200+'s', 0x200+' ', 0x200+'t', 0x200+'h', + 0x200+'e', 0x200+' ', 0x200+' ', 0x200+' ',
    //0x0c0, // Command to set the cursor at the first position line 2
    //0x200+'c', 0x200+'l', 0x200+'a', 0x200+'s', 0x200+'s', + 0x200+' ', 0x200+'f', 0x200+'o',
    //0x200+'r', 0x200+' ', 0x200+'y', 0x200+'o', + 0x200+'u', 0x200+'!', 0x200+' ', 0x200+' ',//
//};


/**/
void init_spi1(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    GPIOB->MODER |= 0x10410000; //gpiob 8,11,14 outputs

     // Set PB3 (bits 7:6) and PB5 (bits 11:10) to AF mode (10)
     GPIOB->MODER = (GPIOB->MODER & ~0x00000CC0) | 0x00000880;

     // Set AF0 for PB3 (AFR[0] bits 15:12) and PB5 (bits 23:20)
     GPIOB->AFR[0] &= ~((0xF << 12) | (0xF << 20));

    // Set PB3 and PB5 to Alternate Function mode (10b)
    //GPIOB->MODER &= ~0x00000CC0; // Clear mode for PB3 (bits 7:6) and PB5 (bits 11:10)
    //GPIOB->MODER |=  0x00000880; // Set AF mode for PB3 and PB5

    // Set PB3 and PB5 to AF0 (AFR[0], bits 15:0)
    //GPIOB->AFR[1] &= 0xFFFFF0F0; // Clear AFRL for PB3 (bits 15:12) and PB5 (bits 23:20)
    //GPIOB->AFR[0] &= ~((0xF << 12) | (0xF << 20));

    //GPIOB->MODER |= 0x88000000; //13,15 afmode 
    //GPIOB->AFR[1] &= 0x0F0FFFFF; //13,15 af[0]
    SPI1->CR1 &= ~SPI_CR1_SPE; //spe clear
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI; //master mode and ssm/ssi bits
    SPI1->CR2 = SPI_CR2_DS_3 | SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0; // 16-bit mode
    SPI1->CR1 |= SPI_CR1_SPE; //spe enable

}

//===========================================================================
// Initialize the SPI2 peripheral.
//===========================================================================
void init_spi2(void) {
    // Enable SPI2 peripheral clock
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    SPI2->CR1 &= ~SPI_CR1_SPE;
    SPI2->CR1 |= SPI_CR1_BR;
    SPI2->CR2 &= ~SPI_CR2_DS;
    SPI2->CR2 |= (0xF << SPI_CR2_DS_Pos);
    SPI2->CR1 |= SPI_CR1_MSTR;
    SPI2->CR2 |= SPI_CR2_SSOE | SPI_CR2_NSSP;

    SPI2->CR2 |= SPI_CR2_TXDMAEN;
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB->MODER &= ~((3 << (12 * 2)) | (3 << (13 * 2)) | (3 << (15 * 2))); // Clear mode
    GPIOB->MODER |= ((2 << (12 * 2)) | (2 << (13 * 2)) | (2 << (15 * 2)));  // Alternate function mode
    GPIOB->AFR[1] |= (0 << ((12 - 8) * 4)) | (0 << ((13 - 8) * 4)) | (0 << ((15 - 8) * 4)); // AF0 for SPI2

    SPI2->CR1 |= SPI_CR1_SPE;
}



//===========================================================================
// Configure the SPI2 peripheral to trigger the DMA channel when the
// transmitter is empty.  Use the code from setup_dma from lab 5.
//===========================================================================
void spi2_setup_dma(void) {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN; // Enable DMA1 clock

    DMA1_Channel5->CCR &= ~DMA_CCR_EN;
    DMA1_Channel5->CMAR = (uint32_t)msg;
    DMA1_Channel5->CPAR = (uint32_t)&SPI2->DR;
    DMA1_Channel5->CNDTR = 8;

    DMA1_Channel5->CCR = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_DIR | DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0;
    SPI2->CR2 |= SPI_CR2_TXDMAEN;
}


//===========================================================================
// Enable the DMA channel.
//===========================================================================
void spi2_enable_dma(void) {
    DMA1_Channel5->CCR |= DMA_CCR_EN; // Enable DMA
}