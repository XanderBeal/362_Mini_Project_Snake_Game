//STM32 Pin setups / enablers
#include "stm32f0xx.h" 

#include <stdint.h>
#include <stdlib.h>

#include <lcd.h>
#include <spi_setup.h>


int msg_index = 0;
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
extern const char font[];


uint8_t col; // the column being scanned


uint16_t display[34] = {
    0x002, // Command to set the cursor at the first position line 1
    0x200+'E', 0x200+'C', 0x200+'E', 0x200+'3', 0x200+'6', + 0x200+'2', 0x200+' ', 0x200+'i',
    0x200+'s', 0x200+' ', 0x200+'t', 0x200+'h', + 0x200+'e', 0x200+' ', 0x200+' ', 0x200+' ',
    0x0c0, // Command to set the cursor at the first position line 2
    0x200+'c', 0x200+'l', 0x200+'a', 0x200+'s', 0x200+'s', + 0x200+' ', 0x200+'f', 0x200+'o',
    0x200+'r', 0x200+' ', 0x200+'y', 0x200+'o', + 0x200+'u', 0x200+'!', 0x200+' ', 0x200+' ',
};


void init_spi1() {
    //disable spi1
    SPI1->CR1 &= ~SPI_CR1_SPE;
    //clocks
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    //spi1 gpio config
    GPIOA->MODER &= ~(GPIO_MODER_MODER5 | GPIO_MODER_MODER7 | GPIO_MODER_MODER15);
    GPIOA->MODER |= (GPIO_MODER_MODER5_1 | GPIO_MODER_MODER7_1 | GPIO_MODER_MODER15_1);
    //spi1 config
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_1;
    //10bit
    SPI1->CR2 = SPI_CR2_DS_3 | SPI_CR2_DS_0 | SPI_CR2_NSSP;
    //enable
    SPI1->CR1 |= SPI_CR1_SPE;
}

void spi_cmd(unsigned int data) {
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = data & 0x3FF;
}

void spi_data(unsigned int data) {
    spi_cmd(data | 0x200);
}

void spi1_init_oled() {
    nano_wait(1000000); 

    spi_cmd(0x38);  
    spi_cmd(0x08);
    spi_cmd(0x01); 
    nano_wait(2000000);

    spi_cmd(0x06);
    spi_cmd(0x02);
    spi_cmd(0x0C);
}

void spi1_display1(const char *string) {
    spi_cmd(0x02);
    while (*string) {
        spi_data(*string++);
    }
    
}

void spi1_display2(const char *string) {
    spi_cmd(0xC0);
    while (*string) {
        spi_data(*string++);
    }
}

void spi1_setup_dma(void) {
    // Disable DMA1 Channel 3 before configuring
    DMA1_Channel3->CCR &= ~DMA_CCR_EN;

    // Enable clock for DMA1
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    // Configure DMA1 Channel 3 for SPI1 TX
    DMA1_Channel3->CPAR = (uint32_t) &SPI1->DR;  // Peripheral address: SPI1 data register
    DMA1_Channel3->CMAR = (uint32_t) display;    // Memory address: display buffer
    DMA1_Channel3->CNDTR = sizeof(display);      // Number of data items to transfer

    // Configure DMA settings
    DMA1_Channel3->CCR = 0;  // Reset configuration
    DMA1_Channel3->CCR |= DMA_CCR_DIR;   // Memory-to-peripheral
    DMA1_Channel3->CCR |= DMA_CCR_MINC;  // Enable memory increment mode
    DMA1_Channel3->CCR |= DMA_CCR_PSIZE_0;  // Peripheral size = 8 bits
    DMA1_Channel3->CCR |= DMA_CCR_MSIZE_0;  // Memory size = 8 bits
    DMA1_Channel3->CCR |= DMA_CCR_PL_1;  // High priority

    // Enable Circular Mode if continuous transfer is needed
    DMA1_Channel3->CCR |= DMA_CCR_CIRC;

    // Enable SPI1 TX DMA request
    SPI1->CR2 |= SPI_CR2_TXDMAEN;
}

void spi1_enable_dma(void) {
        DMA1_Channel3->CCR |= DMA_CCR_EN;
}



//============================================================================
// Configure Timer 15 for an update rate of 1 kHz.
// Trigger the DMA channel on each update.
// Copy this from lab 4 or lab 5.
//============================================================================
void init_tim15(void) {
    //TIM15 is the DMA interupt enable reg

    //Set clock for timer 15
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN;

    //Set prescalar and ARR
    TIM15->PSC = 47;   
    TIM15->ARR = 999; 


    //Set (UDE) Update DMA request Enable in the TIM15_DIER (TIM15 DMA/interupt enable register)
    TIM15->DIER |= TIM_DIER_UDE;

    //enable timer 15
    TIM15->CR1 |= TIM_CR1_CEN;

}


//===========================================================================
// Configure timer 7 to invoke the update interrupt at 1kHz
// Copy from lab 4 or 5.
//===========================================================================

void init_tim7() {
    //Set clock for timer 7
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;

    //Set prescalar and ARR
    TIM7->PSC = 47;   
    TIM7->ARR = 999; 

    TIM7->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM7_IRQn); 

    TIM7->CR1 |= TIM_CR1_CEN; 
}


//===========================================================================
// Copy the Timer 7 ISR from lab 5
//===========================================================================
// TODO To be copied
void TIM7_IRQHandler(void){

    TIM7->SR &= ~TIM_SR_UIF;

    int rows = read_rows();
    update_history(col, rows);
    col = (col + 1) & 3;
    drive_column(col);
}


//===========================================================================
// Initialize the SPI2 peripheral.
//===========================================================================
void init_spi2(void) {
    //diable spi2
    SPI2->CR1 &= ~SPI_CR1_SPE;

    //spi2 clock
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    //GPIO setup
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

    GPIOB->MODER &= ~(GPIO_MODER_MODER12 | GPIO_MODER_MODER13 | GPIO_MODER_MODER15);
    GPIOB->MODER |= (GPIO_MODER_MODER12_1 | GPIO_MODER_MODER13_1 | GPIO_MODER_MODER15_1); // AF mode
    //push-pull output
    //GPIOB->OTYPER &= ~(GPIO_OTYPER_OT_12 | GPIO_OTYPER_OT_13 | GPIO_OTYPER_OT_15);
    //No pull-up/pull-down
    //GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR12 | GPIO_PUPDR_PUPDR13 | GPIO_PUPDR_PUPDR15);


    //spi data size 16bit
    SPI2->CR2 |= SPI_CR2_DS_3 | SPI_CR2_DS_2 | SPI_CR2_DS_1 | SPI_CR2_DS_0;

    //spi2 as master
    SPI2->CR1 |= SPI_CR1_MSTR;

    //set lowest baud rate (111)
    SPI2->CR1 |= SPI_CR1_BR_2 | SPI_CR1_BR_1 | SPI_CR1_BR_0; 

    //ss and nssp enable
    SPI2->CR2 |= SPI_CR2_SSOE | SPI_CR2_NSSP;

    //enable dma over txdmaen
    SPI2->CR2 |= SPI_CR2_TXDMAEN;

    // spi enable
    SPI2->CR1 |= SPI_CR1_SPE;
}

//===========================================================================
// Configure the SPI2 peripheral to trigger the DMA channel when the
// transmitter is empty.  Use the code from setup_dma from lab 5.
//===========================================================================
void spi2_setup_dma(void) {
    //enable DMA 1 clock
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;

    //Turn off the DMA
    DMA1_Channel5->CCR &= ~DMA_CCR_EN;

    //Set CMAR address
    DMA1_Channel5->CMAR = (uint32_t) msg;

    //Set CPAR address
    DMA1_Channel5->CPAR = (uint32_t) (&SPI2->DR);

    // Set CNDTR
    DMA1_Channel5->CNDTR = 8;

    //Configure the CCR
    //set DIR as 1 to set direction as from memory
    DMA1_Channel5->CCR |= DMA_CCR_DIR;

    //Set MINC to increment CMAR every transfer
    DMA1_Channel5->CCR |= DMA_CCR_MINC;

    //Set memory size M to 16 bit (MSIZE = 8, MSIZE_0 = 16, MSIZE_1 = 32)?????
    DMA1_Channel5->CCR |= DMA_CCR_MSIZE_0; 

    //Set memory size P to 16 bit (PSIZE = 8, PSIZE_0 = 16, PSIZE_1 = 32)?????
    DMA1_Channel5->CCR |= DMA_CCR_PSIZE_0; 

    //Set Circular operation
    DMA1_Channel5->CCR |= DMA_CCR_CIRC;

    //Enable DMA request when TXE flag is set
    SPI2->CR2 |= SPI_CR2_TXDMAEN;  
}

//===========================================================================
// Enable the DMA channel.
//===========================================================================
void spi2_enable_dma(void) {
    //Enable DMA
    DMA1_Channel5->CCR |= DMA_CCR_EN;
}