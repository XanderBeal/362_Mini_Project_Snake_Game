//STM32 Pin setups / enablers
#include "stm32f0xx.h" 

#include <stdint.h>
#include <stdlib.h>

#include <lcd.h>
#include <spi_setup.h>

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



uint16_t display[34] = {
    0x002, // Command to set the cursor at the first position line 1
    0x200+'E', 0x200+'A', 0x200+'T', 0x200+' ', 0x200+'A', + 0x200+'S', 0x200+' ', 0x200+'M',
    0x200+'U', 0x200+'C', 0x200+'H', 0x200+' ', + 0x200+'A', 0x200+'S', 0x200+' ', 0x200+' ',
    0x0c0, // Command to set the cursor at the first position line 2
    0x200+'Y', 0x200+'O', 0x200+'U', 0x200+' ', 0x200+'C', + 0x200+'A', 0x200+'N', 0x200+' ',
    0x200+'G', 0x200+'U', 0x200+'D', 0x200+' ', + 0x200+'L', 0x200+'U', 0x200+'C', 0x200+'K',
};

int msg_index = 0;
uint16_t msg[8] = { 0x0000,0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700 };
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
void UI_Setup(u16 Color);
void game_logic_loop(void);
void game_setup(void);

int volume = 2400; // Analog-to-digital conversion for a volume level


// TRRS Audio Jack Components
// TODO: Add TRRS audio jack setup code
void setup_audio_pwm() {
    // Enable GPIOA and TIM14 clocks
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;

    // Set PA4 to alternate function mode
    GPIOA->MODER &= ~(3 << (4 * 2));
    GPIOA->MODER |=  (2 << (4 * 2));  // Alternate function

    // Select AF4 for PA4 = TIM14_CH1
    GPIOA->AFR[0] &= ~(0xF << (4 * 4));
    GPIOA->AFR[0] |=  (4 << (4 * 4));  // AF4 = TIM14

    // Configure TIM14 for PWM mode
    TIM14->PSC = 0;        // No prescaler (48 MHz clock)
    TIM14->ARR = 999;      // Set for ~48kHz base rate

    TIM14->CCR1 = 0;       // Start with sound off

    TIM14->CCMR1 &= ~(7 << 4);  // Clear OC1M bits
    TIM14->CCMR1 |=  (6 << 4);  // Set PWM Mode 1
    TIM14->CCMR1 |= TIM_CCMR1_OC1PE;  // Enable preload

    TIM14->CCER |= TIM_CCER_CC1E;     // Enable output on CH1
    TIM14->CR1 |= TIM_CR1_ARPE;       // Auto-reload preload enable
    TIM14->EGR |= TIM_EGR_UG;         // Force update
    TIM14->CR1 |= TIM_CR1_CEN;        // Enable the timer
}


void play_tone(int freq, int duration_ms) {
    int arr = 48000000 / freq;
    if (arr < 1) arr = 1;
    TIM14->ARR = arr - 1;
    TIM14->CCR1 = arr / 2;  // 50% duty for square wave

    for (int i = 0; i < duration_ms * 1000; i += 100) {
        nano_wait(100000);
    }

    TIM14->CCR1 = 0; // Turn off sound
}


void sound_apple_eaten() {
    play_tone(800, 100);  // short chirp
}

void sound_death() {
    play_tone(300, 200);  // low tone
    nano_wait(200000);
    play_tone(200, 300);  // even lower tone
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

void lcd_test(void){
    //lcd setup
    LCD_Setup();
    LCD_Clear(BLACK);

    //initial UI setup
    UI_Setup(YELLOW);

    //loading picture / screen
    //LCD_DrawPicture();

    game_setup();

    game_logic_loop();


}

void UI_Setup(u16 Color){
    //setup game boundry
    LCD_DrawRectangle(10, 10, 230, 310, Color); //x1, y1, x2, y2, color
    LCD_DrawRectangle(20, 20, 220, 300, Color); //x1, y1, x2, y2, color

}

void game_setup(){
    //snake starting position and food starting position

}

// For RGB Game Correlations
typedef enum {
    STATUS_PLAYING,
    STATUS_EATING,
    STATUS_DEAD
} GameStatus;

volatile GameStatus current_status = STATUS_PLAYING;
void set_status_color(GameStatus status) {
    static int on = 0;  // toggle state

    on = !on;

    switch (status) {
        case STATUS_PLAYING:  // Green
            if (on)
                setrgb(0x009900);  // 99% green
            else
                setrgb(0x000000);  // Off
            break;

        case STATUS_EATING:   // Blue
            if (on)
                setrgb(0x000099);  // 99% blue
            else
                setrgb(0x000000);  // Off
            break;

        case STATUS_DEAD:     // Red
            if (on)
                setrgb(0x990000);  // 99% red
            else
                setrgb(0x000000);  // Off
            break;
    }
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
#define SNAKE_COLOR GREEN
#define APPLE_COLOR RED
#define BG_COLOR BLACK

void place_apple(void);
void LCD_DrawString(u16 x, u16 y, u16 fc, u16 bg, const char *p, u8 size, u8 mode);
void LCD_FillRect(u16 x, u16 y, u16 w, u16 h, u16 color);


typedef struct {
    int x;
    int y;
} Point;

Point snake[MAX_SNAKE_LENGTH];
int snake_length = 3;
int direction = 0; // 0=up, 1=right, 2=down, 3=left

Point apple = {10, 10};

int just_ate_apple = 0;
int snake_dead = 0;

void draw_cell(int x, int y, u16 color) {
    LCD_FillRect(
        ORIGIN_X + x * CELL_SIZE,
        ORIGIN_Y + y * CELL_SIZE,
        CELL_SIZE - 1,
        CELL_SIZE - 1,
        color
    );
}

void clear_board() {
    for (int x = 0; x < BOARD_WIDTH; x++) {
        for (int y = 0; y < BOARD_HEIGHT; y++) {
            draw_cell(x, y, BG_COLOR);
        }
    }
}

void update_display() {
    // Clear board first (or just update old segments if optimized later)
    clear_board();

    // Draw snake
    for (int i = 0; i < snake_length; i++) {
        draw_cell(snake[i].x, snake[i].y, SNAKE_COLOR);
    }

    // Draw apple
    draw_cell(apple.x, apple.y, APPLE_COLOR);

    if (snake_dead) {
        LCD_Clear(RED);
        LCD_DrawString(60, 140, WHITE, RED, "YOU DIED", 1, 1);
        LCD_DrawString(40, 160, WHITE, RED, "Press * to restart", 1, 1);
    }
    
    // Optional win message (e.g., 100 apples)
    if (score >= 100) {
        LCD_Clear(BLUE);
        LCD_DrawString(60, 140, YELLOW, BLUE, "YOU WIN!", 1, 1);
    }
    
}

void update_score() {
    if (just_ate_apple) {
        score++;

        int tens = (score / 10) % 10;
        int ones = score % 10;

        msg[5] = font['0' + tens];
        msg[6] = font['0' + ones];
    }

    if (snake_dead) {
        score = 0;
        msg[5] = font['0'];
        msg[6] = font['0'];
    }
}

void update_snake() {
    just_ate_apple = 0;

    // Move body
    for (int i = snake_length - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }

    // Move head
    switch (direction) {
        case 0: snake[0].y--; break; // Up
        case 1: snake[0].x++; break; // Right
        case 2: snake[0].y++; break; // Down
        case 3: snake[0].x--; break; // Left
    }

    // Check wall collision
    if (snake[0].x < 0 || snake[0].x >= BOARD_WIDTH || snake[0].y < 0 || snake[0].y >= BOARD_HEIGHT) {
        snake_dead = 1;
        return;
    }

    // Check self collision
    for (int i = 1; i < snake_length; i++) {
        if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
            snake_dead = 1;
            return;
        }
    }

    // Check apple collision
    if (snake[0].x == apple.x && snake[0].y == apple.y) {
        if (snake_length < MAX_SNAKE_LENGTH) {
            snake_length++;
        }
        just_ate_apple = 1;
        sound_apple_eaten();  //  Play sound
        apple.x = rand() % BOARD_WIDTH;
        apple.y = rand() % BOARD_HEIGHT;
    }
}

void place_apple() {
    apple.x = rand() % BOARD_WIDTH;
    apple.y = rand() % BOARD_HEIGHT;
}


void reset_game() {
    snake_length = 3;
    snake[0].x = 5; snake[0].y = 5;
    snake[1].x = 4; snake[1].y = 5;
    snake[2].x = 3; snake[2].y = 5;
    direction = DIR_RIGHT;

    score = 0;
    snake_dead = 0;
    just_ate_apple = 0;
    current_status = STATUS_PLAYING;

    place_apple();          // Random apple location
    update_display();       // Refresh visual state
}


void game_logic_loop() {
    while (1) {
        update_snake();
        update_score();
        drive_bb();  //updating the display

        if (snake_dead) {
            sound_death();  //Death sound
        }

        // Update game status color
        if (snake_dead) {
            current_status = STATUS_DEAD;
        } else if (just_ate_apple) {
            current_status = STATUS_EATING;
        } else {
            current_status = STATUS_PLAYING;
        }

        set_status_color(current_status);
        update_display();

        char key = get_key_event();
        if (key == '*') {
            reset_game();
        }


        nano_wait(10000000);  // Adjust delay for game speed
    }
}

    //snake position update
        //keep in mind boundry

    //score update

    //display update
        //current position / score / win / lose condition





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
    setup_tim1();
    init_tim7();
    lcd_test();
    setup_audio_pwm();

    game_logic_loop();
}
