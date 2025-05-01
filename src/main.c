//STM32 Pin setups / enablers
#include "stm32f0xx.h" 

#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include <lcd.h>
#include <spi_setup.h>
#include "lcd.h"

#define WHITE       0xFFFF
#define BLACK       0x0000
#define BLUE        0x001F
#define YELLOW      0XFFE0
#define GBLUE       0X07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define GREEN       0x07E0
#define CYAN        0x7FFF
#define BROWN       0XBC40
#define BRRED       0XFC07
#define GRAY        0X8430
#define DARKBLUE    0X01CF
#define LIGHTBLUE   0X7D7C
#define GRAYBLUE    0X5458
#define LIGHTGREEN  0X841F
#define LIGHTGRAY   0XEF5B
#define LGRAY       0XC618
#define LGRAYBLUE   0XA651
#define LBBLUE      0X2B12


void set_char_msg(int, char);
void nano_wait(unsigned int);
void game(void);
void internal_clock();

void enable_ports(void) {
    // Only enable port C for the keypad
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    GPIOC->MODER &= ~0xffff;
    GPIOC->MODER |= 0x55 << (4*2);
    GPIOC->OTYPER &= ~0xff;
    GPIOC->OTYPER |= 0xf0;
    GPIOC->PUPDR &= ~0xff;
    GPIOC->PUPDR |= 0x55;
}


uint8_t col; // the column being scanned

void drive_column(int);   // energize one of the column outputs
int  read_rows();         // read the four row inputs
void update_history(int col, int rows); // record the buttons of the driven column
char get_key_event(void); // wait for a button event (press or release)
char get_keypress(void);  // wait for only a button press event.
float getfloat(void);     // read a floating-point number from keypad
void show_keys(void);     // demonstrate get_key_event()

// Bit Bang SPI LED Array
int msg_index = 0;
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
extern const char font[];

// Configure PB12 (CS), PB13 (SCK), and PB15 (SDI) for outputs
void setup_bb(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;

    GPIOB->MODER &= ~((3 << (12 * 2)) | (3 << (13 * 2)) | (3 << (15 * 2)));
    GPIOB->MODER |= (1 << (12 * 2)) | (1 << (13 * 2)) | (1 << (15 * 2));
    GPIOB->ODR |= (1 << 12); // Set CS high
    GPIOB->ODR &= ~(1 << 13); // Set SCK low
}

void small_delay(void) {
    nano_wait(5000);
}

// Set the MOSI bit, then set the clock high and low.
// Pause between doing these steps with small_delay().
void bb_write_bit(int val) {
    if (val) {
        GPIOB->ODR |= (1 << 15); // Set MOSI high
    } else {
        GPIOB->ODR &= ~(1 << 15); // Set MOSI low
    }

    small_delay();

    GPIOB->ODR |= (1 << 13); // Set SCK high
    small_delay();

    GPIOB->ODR &= ~(1 << 13); // Set SCK low
}

// Set CS (PB12) low,
// write 16 bits using bb_write_bit,
// then set CS high.
void bb_write_halfword(int halfword) {
    GPIOB->ODR &= ~(1 << 12); // Set CS low

    for (int i = 15; i >= 0; i--) {
        bb_write_bit((halfword >> i) & 1); // Send each bit
    }

    GPIOB->ODR |= (1 << 12); // Set CS high
}

// Continually bitbang the msg[] array.
void drive_bb(void) {
    for(;;)
        for(int d=0; d<8; d++) {
            bb_write_halfword(msg[d]);
            nano_wait(1000000); // wait 1 ms between digits
        }
}

// Configure Timer 15 for an update rate of 1 kHz.
// Trigger the DMA channel on each update.

void init_tim15(void) {
    RCC->APB2ENR |= RCC_APB2ENR_TIM15EN; // Enable the RCC 

    (void)RCC->APB2ENR;
    TIM15->PSC = 47;
    TIM15->ARR = 999;
    TIM15->DIER |= TIM_DIER_UDE;
    TIM15->CR1 |= TIM_CR1_CEN;
}

// Configure timer 7 to invoke the update interrupt at 1kHz
void init_tim7(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;

    (void)RCC->APB1ENR;
    TIM7->PSC = 47;
    TIM7->ARR = 999; 
    TIM7->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM7_IRQn);
    NVIC_SetPriority(TIM7_IRQn, 2);

    // Enable the timer
    TIM7->CR1 |= TIM_CR1_CEN;
}

// Copy the Timer 7 ISR from lab 5

void TIM7_IRQHandler(void) {
    TIM7->SR &= ~TIM_SR_UIF;
    int rows = read_rows();
    update_history(col, rows);

    col = (col + 1) & 3;
    drive_column(col);
}

//Timer Setups
void setup_tim1() { // Might want to switch to TIM14 or TIM 3 to trigger faster
    // Enable clocks
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;

    // Set PA8, PA9, PA10 to alternate function mode
    GPIOA->MODER &= ~((3 << (8 * 2)) | (3 << (9 * 2)) | (3 << (10 * 2)));
    GPIOA->MODER |=  ((2 << (8 * 2)) | (2 << (9 * 2)) | (2 << (10 * 2)));

    // Set alternate function to AF2 (TIM1) for PA8, PA9, PA10
    GPIOA->AFR[1] &= ~((0xF << ((8 - 8) * 4)) | (0xF << ((9 - 8) * 4)) | (0xF << ((10 - 8) * 4)));
    GPIOA->AFR[1] |=  ((2 << ((8 - 8) * 4)) | (2 << ((9 - 8) * 4)) | (2 << ((10 - 8) * 4)));

    // Timer setup
    TIM1->PSC = 0;                // No prescaler
    TIM1->ARR = 9999;            // For 20 kHz: 48 MHz / (9999+1) = 4800 Hz (adjust for 20kHz if needed)

    // Set PWM mode 1 on channels 1-3
    TIM1->CCMR1 &= ~((7 << 4) | (7 << 12)); // Clear OC1M, OC2M
    TIM1->CCMR1 |=  ((6 << 4) | (6 << 12)); // PWM mode 1
    TIM1->CCMR1 |= TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE;

    TIM1->CCMR2 &= ~(7 << 4);     // Clear OC3M
    TIM1->CCMR2 |=  (6 << 4);     // PWM mode 1
    TIM1->CCMR2 |= TIM_CCMR2_OC3PE;

    // Enable outputs for channels 1–3
    TIM1->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E;

    // Enable main output
    TIM1->BDTR |= TIM_BDTR_MOE;

    // Start timer
    TIM1->CR1 |= TIM_CR1_CEN;

    // Default off (full off = 100% duty cycle = CCR = ARR)
    TIM1->CCR1 = 9999;
    TIM1->CCR2 = 9999;
    TIM1->CCR3 = 9999;
}


extern const char font[];
void print(const char str[]);

int score = 0;

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


//TFT lcd 2.2 inch spi display
void lcd_test(void);
//void UI_Setup(u16 Color);
void game_logic_loop(void);
void game_setup(void);

int volume = 2400; // Analog-to-digital conversion for a volume level


// TRRS Audio Jack Components
// TODO: Add TRRS audio jack setup code
#define N 1000
#define RATE 20000
short int wavetable[N];
int step0 = 0;
int offset0 = 0;
int step1 = 0;
int offset1 = 0;

void init_wavetable(void) {
    for(int i=0; i < N; i++)
        wavetable[i] = 32767 * sin(2 * M_PI * i / N);
}

void set_freq(int chan, float f) {
    if (chan == 0) {
        if (f == 0.0) {
            step0 = 0;
            offset0 = 0;
        } else
            step0 = (f * N / RATE) * (1<<16);
    }
    if (chan == 1) {
        if (f == 0.0) {
            step1 = 0;
            offset1 = 0;
        } else
            step1 = (f * N / RATE) * (1<<16);
    }
}

void setup_dac(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA->MODER |= (0x3 << (4 * 2));
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;

    // DAC->CR &= ~DAC_CR_EN1;
    DAC->CR &= ~DAC_CR_TSEL1;
    DAC->CR |= DAC_CR_TEN1;
    DAC->CR|= DAC_CR_EN1;
}

void TIM6_DAC_IRQHandler(void){
    TIM6->SR &= ~TIM_SR_UIF;
    offset0 += step0;
    offset1 += step1;

    if (offset0 >= (N<<16)){
        offset0 -= (N<<16);
    } else if (offset1 >= (N<<16)){
        offset1 -= (N<<16);
    }
    int samp = wavetable[offset0 >> 16] + wavetable[offset1 >> 16];

    samp = (samp * volume) >> 17;
    samp += 2048;
    DAC->DHR12R1 = samp;

}

void init_tim6(void) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
    TIM6->PSC = (48000000 / (RATE * 100)) - 1;
    TIM6->ARR = 100 - 1;
    TIM6->CR2 &= ~TIM_CR2_MMS;
    TIM6->CR2 |= TIM_CR2_MMS_1;
    TIM6->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM6_DAC_IRQn);

    TIM6->CR1 |= TIM_CR1_CEN;
}

void play_note(float freq, int duration_ms) {
    set_freq(0, freq);
    for (int i = 0; i < duration_ms * 100; i++)
        nano_wait(10000);  // rough ms delay
    set_freq(0, 0.0f);
    nano_wait(100000);  // small pause between notes
}

void sound_apple_eaten() {
    play_note(300.0f, 100);
    nano_wait(10000);
    play_note(400.0f, 100);
    nano_wait(10000);
    play_note(500.0f, 100);
    nano_wait(10000);
    play_note(650.0f, 100);
    nano_wait(10000);
    play_note(800.0f, 100);
    nano_wait(10000);
    play_note(1000.0f, 150);

    play_note(0.0f,0);
}

void sound_death() {
    play_note(1000.0f, 150);
    play_note(800.0f, 150);
    play_note(600.0f, 150);
    play_note(400.0f, 200);
    play_note(200.0f, 300);
}

// RGB LED Components
// TODO: Add RGB LED setup code
void setrgb(uint32_t rgbval) {
    // Extract BCD digits
    int r = ((rgbval >> 20) & 0xF) * 10 + ((rgbval >> 16) & 0xF);
    int g = ((rgbval >> 12) & 0xF) * 10 + ((rgbval >> 8) & 0xF);
    int b = ((rgbval >> 4) & 0xF) * 10 + (rgbval & 0xF);

    // Invert brightness: higher value = less duty = brighter
    // Scale 0–99 to ARR (9999)
    int red_pwm   = 9999 - (r * 9999 / 99);
    int green_pwm = 9999 - (g * 9999 / 99);
    int blue_pwm  = 9999 - (b * 9999 / 99);

    // Update CCR registers
    TIM1->CCR1 = red_pwm;
    TIM1->CCR2 = green_pwm;
    TIM1->CCR3 = blue_pwm;
}



//240 (horiz) x 320 (vert) pixel resolution
void UI_Setup(u16 Color) {
    // Fill the full game playfield with the specified color
    // e.g., 200x240 grid at 10px per cell = 20x30 playfield

    LCD_DrawFillRectangle(0, 0, 239, 319, Color);  // Full 240x320 screen
}




void lcd_test(void){
    //lcd setup
    LCD_Setup();
    LCD_Clear(BLACK);

    //initial UI setup
    UI_Setup(WHITE);

    //loading picture / screen
   // LCD_DrawPicture();

    //game_setup();

    //game_logic_loop();


}


//Example logic: 
//  https://www.youtube.com/watch?v=JcvyrU2A8r4&ab_channel=TFTSTM32
//  Website:    https://vivonomicon.com/2018/06/17/drawing-to-a-small-tft-display-the-ili9341-and-stm32/
//  Phils lab:  https://www.youtube.com/watch?v=RWujOLXBFrc&ab_channel=Phil%E2%80%99sLab

#define MAX_SNAKE_LENGTH 100
#define BOARD_WIDTH 20
#define BOARD_HEIGHT 20
#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3
#define CELL_SIZE 10
#define ORIGIN_X 20
#define ORIGIN_Y 20
#define SNAKE_COLOR BLUE
#define APPLE_COLOR RED
#define BG_COLOR GREEN
#define CELL_SIZE   10
#define GRID_COLS   24
#define GRID_ROWS   32
#define OFFSET_X    0
#define OFFSET_Y    0




void draw_apple(int grid_x, int grid_y) {
    int x0 = OFFSET_X + grid_x * CELL_SIZE;
    int y0 = OFFSET_Y + grid_y * CELL_SIZE;
    int x1 = x0 + CELL_SIZE - 1;
    int y1 = y0 + CELL_SIZE - 1;
    LCD_DrawFillRectangle(x0, y0, x1, y1, APPLE_COLOR);
}

#include <stdlib.h>

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point body[MAX_SNAKE_LENGTH];
    int length;
    int direction;
} Snake;


Point spawn_random_apple(void) {
    Point apple;
    apple.x = (rand() % (GRID_COLS / 2)) * 2;  // even columns only
    apple.y = (rand() % (GRID_ROWS / 2)) * 2;  // even rows only
    draw_apple(apple.x, apple.y);
    return apple;
}

void draw_cell(int grid_x, int grid_y, uint16_t color) {
    int x0 = OFFSET_X + grid_x * CELL_SIZE;
    int y0 = OFFSET_Y + grid_y * CELL_SIZE;
    int x1 = x0 + CELL_SIZE - 1;
    int y1 = y0 + CELL_SIZE - 1;
    LCD_DrawFillRectangle(x0, y0, x1, y1, color);
}

void draw_snake(const Snake *snake) {
    for (int i = 0; i < snake->length; i++) {
        uint16_t color = (i == 0) ? BLACK : SNAKE_COLOR;
        draw_cell(snake->body[i].x, snake->body[i].y, color);
    }
}


Snake init_snake(void) {
    Snake s;
    s.length = 2;
    s.direction = DIR_RIGHT;  // Start moving right

    int start_x = GRID_COLS / 2;
    int start_y = GRID_ROWS / 2;

    s.body[0].x = start_x;
    s.body[0].y = start_y;
    s.body[1].x = start_x - 1;
    s.body[1].y = start_y;

    draw_snake(&s);

    return s;
}

void handle_input(Snake *snake) {
    char key = get_key_event();  // assumes this function already works

    switch (key) {
        case '2':
            if (snake->direction != DIR_DOWN)
                snake->direction = DIR_UP;
            break;
        case '4':
            if (snake->direction != DIR_RIGHT)
                snake->direction = DIR_LEFT;
            break;
        case '5':
            if (snake->direction != DIR_UP)
                snake->direction = DIR_DOWN;
            break;
        case '6':
            if (snake->direction != DIR_LEFT)
                snake->direction = DIR_RIGHT;
            break;
        default:
            break;  // ignore unrecognized input
    }
}

int move_snake(Snake *snake, Point apple) {
    Point new_head = snake->body[0];  // start from current head

    // Compute next head position
    switch (snake->direction) {
        case DIR_UP:    new_head.y--; break;
        case DIR_DOWN:  new_head.y++; break;
        case DIR_LEFT:  new_head.x--; break;
        case DIR_RIGHT: new_head.x++; break;
    }

    // Check if apple is eaten
    int ate_apple = (new_head.x == apple.x && new_head.y == apple.y);

    // Move the body
    if (ate_apple && snake->length < MAX_SNAKE_LENGTH) {
        snake->length++;  // Grow snake
    }

    // Shift body segments (either keep or overwrite the tail)
    for (int i = snake->length - 1; i > 0; i--) {
        snake->body[i] = snake->body[i - 1];
    }

    // Set new head
    snake->body[0] = new_head;

    return ate_apple;
}

int check_collision(const Snake *snake) {
    Point head = snake->body[0];

    // Check boundary collision
    if (head.x < 0 || head.x >= GRID_COLS || head.y < 0 || head.y >= GRID_ROWS) {
        return 1;
    }

    // Check self collision
    for (int i = 1; i < snake->length; i++) {
        if (head.x == snake->body[i].x && head.y == snake->body[i].y) {
            return 1;
        }
    }

    return 0;
}

void reset_game(Snake *snake, Point *apple, int *score) {
    *score = 0;
    //LCD_Setup();  // Reinitialize display controller


    // Clear screen and show message
    LCD_Clear(RED);

    msg[0] = (0 << 8) | font['Y'];
    msg[1] = (1 << 8) | font['O'];
    msg[2] = (2 << 8) | font['U'];
    msg[3] = (3 << 8) | font[' '];
    msg[4] = (4 << 8) | font['D'];
    msg[5] = (5 << 8) | font['I'];
    msg[6] = (6 << 8) | font['E'];
    msg[7] = (7 << 8) | font['D'];
 
    
    nano_wait(1000000000); // 1 second delay
    

    msg[0] = (0 << 8) | font['A'];
    msg[1] = (1 << 8) | font['P'];
    msg[2] = (2 << 8) | font['P'];
    msg[3] = (3 << 8) | font['L'];
    msg[4] = (4 << 8) | font['E'];
    msg[5] = (5 << 8) | font['S'];
    msg[6] = (6 << 8) | font[' '];  // space before the score
    msg[7] = (7 << 8) | font['0'];

    *snake = init_snake();
    *apple = spawn_random_apple();
    draw_snake(snake);
    draw_apple(apple->x, apple->y);
}

#include "stm32f0xx.h"

void internal_clock(void);
void enable_ports(void);
void init_tim7(void);
void init_tim15(void);
//void setup_tim1(void);
//void setup_audio_pwm(void);
void setup_bb(void);
//void init_spi1(void);
void LCD_Setup(void);
void lcd_test(void);
void spi2_setup_dma(void);
void spi2_enable_dma(void);
void game_logic_loop(void);
void init_lcd_spi(void);

void lcd_color_test(void) {
    LCD_Setup();

    LCD_Clear(RED);
    nano_wait(1000000);

    LCD_Clear(GREEN);
    nano_wait(1000000);

    LCD_Clear(BLUE);
    nano_wait(1000000);

    LCD_Clear(WHITE);
}

void init_lcd_spi(void) {
    //clock 
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    GPIOB->MODER &= ~(GPIO_MODER_MODER8_Msk | GPIO_MODER_MODER11_Msk | GPIO_MODER_MODER14_Msk);
    GPIOB->MODER |=  (GPIO_MODER_MODER8_0  | GPIO_MODER_MODER11_0  | GPIO_MODER_MODER14_0);

    //PB3 (SCK), PB4 (MISO), PB5 (MOSI) to alternate function mode (AF0)
    GPIOB->MODER &= ~(GPIO_MODER_MODER3_Msk | GPIO_MODER_MODER4_Msk | GPIO_MODER_MODER5_Msk);
    GPIOB->MODER |=  (GPIO_MODER_MODER3_1  | GPIO_MODER_MODER4_1  | GPIO_MODER_MODER5_1);
    GPIOB->AFR[0] &= ~(GPIO_AFRL_AFSEL3_Msk | GPIO_AFRL_AFSEL4_Msk | GPIO_AFRL_AFSEL5_Msk); // AF0

    //Set slow baud rate 12Mhz, master mode, software NSS management
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (SPI1->CR1 & ~SPI_CR1_BR_Msk) | (SPI_CR1_BR_0);

    //Set 8-bit data size and FIFO reception threshold for 8-bit
    SPI1->CR2 = SPI_CR2_FRXTH | (7 << SPI_CR2_DS_Pos); // DS = 7 for 8-bit

    //enable SPI
    SPI1->CR1 |= SPI_CR1_SPE;
}

void init_spi1_slow(void) {
    //Enable GPIOB and SPI1 clocks
    RCC->AHBENR  |= RCC_AHBENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    //Set PB3 (SCK), PB4 (MISO), PB5 (MOSI) to alternate function mode (AF0)
    GPIOB->MODER &= ~(GPIO_MODER_MODER3_Msk | GPIO_MODER_MODER4_Msk | GPIO_MODER_MODER5_Msk);
    GPIOB->MODER |=  (GPIO_MODER_MODER3_1  | GPIO_MODER_MODER4_1  | GPIO_MODER_MODER5_1);

    GPIOB->AFR[0] &= ~(GPIO_AFRL_AFSEL3_Msk | GPIO_AFRL_AFSEL4_Msk | GPIO_AFRL_AFSEL5_Msk); // AF0

    //Disable SPI before configuration
    SPI1->CR1 &= ~SPI_CR1_SPE;

    //Set slow baud rate (fPCLK/256), master mode, software NSS management
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_Msk;

    //Set 8-bit data size and FIFO reception threshold for 8-bit
    SPI1->CR2 = SPI_CR2_FRXTH | (7 << SPI_CR2_DS_Pos); // DS = 7 for 8-bit

    //Enable SPI
    SPI1->CR1 |= SPI_CR1_SPE;
}

int main(void) {

    internal_clock();

    //rgblight
    setup_tim1();

    //keypad
    enable_ports();
    init_tim7();
    init_tim15(); 

    //sound (trss)
    setup_dac();
    init_wavetable();
    init_tim6();

    //7-bit display
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    init_tim15();

    msg[0] |= font['A'];
    msg[1] |= font['P'];
    msg[2] |= font['P'];
    msg[3] |= font['L'];
    msg[4] |= font['E'];
    msg[5] |= font['S'];
    msg[6] |= font[' '];
    msg[7] |= font['0'];

    LCD_Setup();
    LCD_Clear(BG_COLOR);
    srand(12345);

    Point apple = spawn_random_apple();
    Snake snake = init_snake();

    int last_direction = snake.direction;
    score = 0;
    msg[6] |= font[' '];
    msg[7] |= font['0'];
    //game_logic_loop();
    while (1) {
        
        static char last_key = 0;
        char key = get_key_event();
        
        if (key != last_key && key != 0) {
            last_key = key;
        
            // handle direction (no reverse allowed)
            if (key == '2' && snake.direction != DIR_DOWN)
                snake.direction = DIR_UP;
            else if (key == '4' && snake.direction != DIR_RIGHT)
                snake.direction = DIR_LEFT;
            else if (key == '5' && snake.direction != DIR_UP)
                snake.direction = DIR_DOWN;
            else if (key == '6' && snake.direction != DIR_LEFT)
                snake.direction = DIR_RIGHT;
        
        } else if (key == 0) {
            last_key = 0;  // reset last_key when no key is pressed
        }
        

            if (key == '5' && last_direction != DIR_DOWN) {
                snake.direction = DIR_UP;
            } else if (key == '2' && last_direction != DIR_UP) {
                snake.direction = DIR_DOWN;
            } else if (key == '6' && last_direction != DIR_RIGHT) {
                snake.direction = DIR_LEFT;
            } else if (key == '4' && last_direction != DIR_LEFT) {
                snake.direction = DIR_RIGHT;
            }
    
            last_direction = snake.direction;
    

            int ate = move_snake(&snake, apple);
            if (check_collision(&snake)) {
                sound_death();
                setrgb(0x990000);
                reset_game(&snake, &apple, &score);
                continue;  // skip rest of loop this frame
            }

            if (ate) {
                setrgb(0x000099);
                apple = spawn_random_apple();
                sound_apple_eaten();
            
            

            score++;

            int tens = (score / 10) % 10;
            int ones = score % 10;

            if (score < 10)
                msg[6] = (6 << 8) | font[' '];           // blank space
            else
                msg[6] = (6 << 8) | font['0' + tens];    // actual digit

            msg[7] = (7 << 8) | font['0' + ones];        // always show ones place
            //drive_bb();
        } else {
            setrgb(0x009900);
        }
    
            LCD_Clear(BG_COLOR);
            draw_apple(apple.x, apple.y);
            draw_snake(&snake);
    
            nano_wait(150000000); 
        }
        //Point apple = spawn_random_apple();
        //update_display();
        //game_setup();
        //game_logic_loop();
        //lcd_color_test();
    

   /* 
   

    //keypad
    enable_ports();
    init_tim7();
    init_tim15();

    //idk
    setup_tim1();
    setup_audio_pwm();

    //7-bit display
    init_spi2();
    spi2_setup_dma();
    spi2_enable_dma();
    init_tim15();

    //show_keys(); //use to test keypad and 7-bit display interface

    //TFT Display
    //init_spi1();
    //setup_tim1();
    LCD_Setup();
    LCD_Clear(WHITE);



    msg[0] |= font['S'];
    msg[1] |= font['C'];
    msg[2] |= font['0'];
    msg[3] |= font['R'];
    msg[4] |= font['E'];
    msg[5] |= font[' '];
    msg[6] |= font['0'];
    msg[7] |= font[' '];

    lcd_test();

    while (1) {
        GPIOB->ODR &= ~(1 << 8); // CS LOW
       // LCD_WriteData16(0xAAAA);
        GPIOB->ODR |= (1 << 8);  // CS HIGH

        nano_wait(1000);
    
    }
    
*/
  //  game_logic_loop();
}
