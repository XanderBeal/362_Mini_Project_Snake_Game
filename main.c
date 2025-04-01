//STM32 Pin setups / enablers
#include "stm32f0xx.h" 

void set_char_msg(int, char);
void nano_wait(unsigned int);
void game(void);
void internal_clock();

void enable_ports(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER &= ~0xffff;
    GPIOC->MODER |= 0x55 << (4*2);
    GPIOC->OTYPER &= ~0xff;
    GPIOC->OTYPER |= 0xf0;
    GPIOC->PUPDR &= ~0xff;
    GPIOC->PUPDR |= 0x55;
}

//TFT Components
void init_spi1();
void spi_cmd(unsigned int data);
void spi_data(unsigned int data);
void spi1_init_oled();
void spi1_display1(const char *string);
void spi1_display2(const char *string);
void spi1_setup_dma(void);
void spi1_enable_dma(void);

uint16_t display[34] = {
    0x002,
    0x200+'R', 0x200+'E', 0x200+'A', 0x200+'D', 0x200+'Y', 0x200+' ', 0x200+'T', 0x200+'0',
    0x200+'', 0x200+'P', 0x200+'L', 0x200+'A', 0x200+'Y', 0x200+' ', 0x200+' ', 0x200+' ',
    0x0c0,
    0x200+'E', 0x200+'A', 0x200+'T', 0x200+' ', 0x200+'A', 0x200+'S', 0x200+' ', 0x200+'M',
    0x200+'U', 0x200+'C', 0x200+'H', 0x200+' ', 0x200+'A', 0x200+'S', 0x200+' ', 0x200+'Y',
    0x200+'O', 0x200+'U', 0x200+' ', 0x200+'C', 0x200+'A', 0x200+'N', 0x200+' ', 0x200+' '
};

// 7-BIT Score Display Components
void setup_bb(void);
void small_delay(void);
void bb_write_bit(int val);
void bb_write_halfword(int halfword);
void drive_bb(void);
void init_spi2(void);
void spi2_setup_dma(void);
void spi2_enable_dma(void);
void init_tim15(void);

int msg_index = 0;
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
extern const char font[];

// Keypad Components
void init_tim7(void);
void TIM7_IRQHandler(void);
void drive_column(int);
int  read_rows();
void update_history(int col, int rows);
char get_key_event(void);
char get_keypress(void);
float getfloat(void);
void show_keys(void);

uint8_t col;

// TRRS Audio Jack Components
// TODO: Add TRRS audio jack setup code

// RGB LED Components
// TODO: Add RGB LED setup code

// Main function
int main(void) {
    internal_clock();

    msg[0] |= font['S'];
    msg[1] |= font['C'];
    msg[2] |= font['0'];
    msg[3] |= font['R'];
    msg[4] |= font['E'];
    msg[5] |= font[' '];
    msg[6] |= font['0'];
    msg[7] |= font[' '];

    enable_ports();
    init_tim7();
}
/*
//#define BIT_BANG
#if defined(BIT_BANG)
    setup_bb();
    drive_bb();
#endif

//#define SPI_LEDS
#if defined(SPI_LEDS)
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    init_tim15();
    show_keys();
#endif

//#define SPI_LEDS_DMA
#if defined(SPI_LEDS_DMA)
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    show_keys();
#endif

//#define SPI_OLED
#if defined(SPI_OLED)
    init_spi1();
    spi1_init_oled();
    spi1_display1("Hello again,");
    spi1_display2(username);
#endif

//#define SPI_OLED_DMA
#if defined(SPI_OLED_DMA)
    init_spi1();
    spi1_init_oled();
    spi1_setup_dma();
    spi1_enable_dma();
#endif

    game();
}
*/