// #define F_CPU 20000000

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define NUM_MODULES 4

#define MISO PB4
#define MOSI PB3
#define SCK PB5
#define SS PB2
#define BUTTON_DDR DDRD
#define BUTTON_PORT PORTD
#define BUTTON_PINR PIND
#define BUTTON_PIN PD2

enum GAME_STATE {
    INGAME,
    END
};

enum GAME_STATE game_state = INGAME;

uint16_t speed = 0;

void delay_ms(uint16_t ms)
{
    while (ms--) {
        _delay_ms(1);   // constant -> OK
    }
}

uint8_t matrix[MATRIX_WIDTH][MATRIX_HEIGHT] = {
        {0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0},
        {0,0,0,0,0,0,0,0},
        {0,1,1,1,1,1,1,0}
    };
#define CS_LOW()   (PORTB &= ~(1 << PB2))
#define CS_HIGH()  (PORTB |=  (1 << PB2))

volatile uint8_t button_pressed = 0;

static inline void spi_write(uint8_t b) {
    SPDR = b;                          // start transfer
    while (!(SPSR & (1 << SPIF))) { }  // wait done
}

static inline void max7219_write(uint8_t reg, uint8_t data) {
    CS_LOW();
    spi_write(reg);
    spi_write(data);
    CS_HIGH();                         // latch into MAX7219
}

static inline void max7219_write_module(uint8_t module, uint8_t reg, uint8_t data)
{
    CS_LOW();

    for (int8_t m = NUM_MODULES - 1; m >= 0; m--) {
        if ((uint8_t)m == module) {
            spi_write(reg);
            spi_write(data);
        } else {
            spi_write(0x00); // NOP
            spi_write(0x00);
        }
    }

    CS_HIGH();
}

static inline void max7219_init(void) {
    // Leave CS high when idle
    CS_HIGH();

    // Decode mode: 0 = no decode for LED matrix
    max7219_write(0x09, 0x00);

    // Intensity: 0x00..0x0F
    max7219_write(0x0A, 0x08);

    // Scan limit: 0..7 (display digits/rows 1..8)
    max7219_write(0x0B, 0x07);

    // Shutdown: 0 = shutdown, 1 = normal operation
    max7219_write(0x0C, 0x01);

    // Display test: 1 = all on, 0 = normal
    max7219_write(0x0F, 0x00);

    // Clear all rows (registers 1..8)
    for (uint8_t r = 1; r <= 8; r++) {
        max7219_write(r, 0x00);
    }
}

static void spi_init() {
    // MOSI (PB3), SCK (PB5), SS (PB2) as outputs
    DDRB |= (1 << MOSI) | (1 << SCK) | (1 << SS);

    // MISO (PB4) as input
    DDRB &= ~(1 << PB4);

    // Enable SPI, Master mode, f_CPU/16
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
}



static void button_init(){
    BUTTON_DDR &= ~(1 << BUTTON_PIN);
    
//    BUTTON_PORT &= ~(1 << BUTTON_PIN); // NO pull-up (external resistor used)
    BUTTON_PORT |= (1 << BUTTON_PIN);   // pull-up
}


void matrix_print(uint8_t (*matrix)[MATRIX_WIDTH]) {

    for (int i =  0; i < MATRIX_HEIGHT; i++) {
        unsigned int bin = 0;
        for (int j = 0; j < MATRIX_WIDTH; j++) {
            // bin = (bin << 1) | matrix[j][i];
            bin = (bin << 1) | (matrix[j][i] & 1u); //saver
        }
        max7219_write(i+1, bin);
    }
}

void matrix_clear(uint8_t (*matrix)[MATRIX_WIDTH]) {
    for (int i =  0; i < MATRIX_WIDTH; i++) {
        for (int j = 0; j < MATRIX_HEIGHT; j++) {
            matrix[i][j] = 0;
        }
    }
}

    void rotate_right(int *a, int n)
{
    int last = a[n - 1];
    for (int i = n - 1; i > 0; i--) {
        a[i] = a[i - 1];
    }
    a[0] = last;
}

void shift_right(uint8_t *a) 
{
    uint8_t first = a[0];

    for (int i = 0; i < MATRIX_WIDTH - 1; i++) {
        a[i] = a[i + 1];
    }

    a[MATRIX_WIDTH - 1] = first;
}


uint8_t count_ones(uint8_t *row) {
    uint8_t count = 0;
    for (int i = 0; i < MATRIX_WIDTH; i++) {
        if (matrix[*row][i] == 1) {
            count++;
        }
    }
    return count;
}


static void spawn_row(uint8_t row, uint8_t size)
{
    for (int i = 0; i < MATRIX_WIDTH; i++) matrix[row][i] = 0;
    for (int i = MATRIX_WIDTH - 1; i > 0 && i >= MATRIX_WIDTH - size; i--) matrix[row][i] = 1;
}
// static void spawn_row(uint8_t row, uint8_t size)
// {
//     for (int i = 0; i < MATRIX_WIDTH; i++) matrix[row][i] = 0;
//     for (int i = 0; i < MATRIX_WIDTH && i < size; i++) matrix[row][i] = 1;
// }

static void init_game(uint8_t *row)
{
    speed = 0;

    *row = 7;
    matrix_clear(matrix);

    // bottom row: 0 1 1 1 1 1 1 0
    matrix[*row][0] = 0;
    for (int i = 1; i < MATRIX_WIDTH - 1; i++) matrix[*row][i] = 1;
    matrix[*row][MATRIX_WIDTH - 1] = 0;

    game_state = INGAME;
}

static void clean_row(uint8_t row)
{
    // Find longest run of 1s in matrix[row]
    uint8_t cnt = 0, high = 0, high_bit = 0;

    for (int i = 0; i < MATRIX_WIDTH; i++) {
        if (matrix[row][i] == 1) cnt++;
        else cnt = 0;

        if (cnt > high) {
            high = cnt;
            high_bit = i;
        }
    }

    // If no ones, clear row
    if (high == 0) {
        for (int i = 0; i < MATRIX_WIDTH; i++) matrix[row][i] = 0;
        return;
    }

    uint8_t start = high_bit - high + 1;

    for (int i = 0; i < MATRIX_WIDTH; i++) {
        // keep only longest run
        if (i < start || i > high_bit) {
            matrix[row][i] = 0;
            continue;
        }

        // support check: if there is a row below (row+1), require support
        if (row < MATRIX_HEIGHT - 1) {
            if (matrix[row + 1][i] == 0) matrix[row][i] = 0;
        }
    }
}

static void check_game_state(uint8_t row)
{
    if (row < 0) {
        game_state = END;
    }
}

static inline uint8_t button_is_down(void)
{
    return !(BUTTON_PINR & (1 << BUTTON_PIN)); // active LOW
}

static void handle_button_press(uint8_t *row)
{
    _delay_ms(30); // debounce press
    if (!button_is_down()) {
        button_pressed = 0;
        return;
    }

    if (game_state == INGAME) {
        clean_row(*row);
        speed += 10;
        if (*row == 0) {
            game_state = END;
        } else {
            uint8_t size = count_ones(row);
            (*row)--;
            spawn_row(*row, size);
            if (count_ones(row) == 0) {
                game_state = END;
            }

            check_game_state(*row);
        }
    } else { // END -> restart
        init_game(row);
    }


    button_pressed = 0;
    // EIFR  |= (1 << INTF0);  // clear pending flag
    // EIMSK |= (1 << INT0);   // re-enable INT0
    _delay_ms(100);
}

int main(void)
{
    button_init();

    // INT0 falling edge
    EIFR  |= (1 << INTF0);
    EICRA |= (1 << ISC01);
    EICRA &= ~(1 << ISC00);
    EIMSK |= (1 << INT0);

    spi_init();
    max7219_init();
    sei();

    uint8_t rowx;
    init_game(&rowx);

    while (1) {
        if (button_pressed) {
            handle_button_press(&rowx);
        }

        if (game_state == INGAME) {
            shift_right(matrix[rowx]);
        }

        matrix_print(matrix);
        delay_ms(200-speed);
    }
}

ISR(INT0_vect)
{
    button_pressed = 1;
    // EIMSK &= ~(1 << INT0);   // disable INT0
}
