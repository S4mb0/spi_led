// #define F_CPU 20000000

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8

#define MISO PB4
#define MOSI PB3
#define SCK PB5
#define SS PB2


#define CS_LOW()   (PORTB &= ~(1 << PB2))
#define CS_HIGH()  (PORTB |=  (1 << PB2))

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





void matrix_print(uint8_t (*matrix)[MATRIX_WIDTH]) {

    for (int i =  0; i < MATRIX_HEIGHT; i++) {
        unsigned int bin = 0;
        for (int j = 0; j < MATRIX_WIDTH; j++) {
            bin = (bin << 1) | matrix[i][j];
        }
        max7219_write(i+1, bin);
    }
}

void matrix_clear(uint8_t (*matrix)[MATRIX_WIDTH]) {
    for (int i =  0; i < MATRIX_HEIGHT; i++) {
        for (int j = 0; j < MATRIX_WIDTH; j++) {
            matrix[i][j] = 0;
        }
    }
}

int main(void) {
    spi_init();
    max7219_init();
    // for (uint8_t r = 1; r <= 255; r++) {
    //     max7219_write(1, r);
    //     _delay_ms(50);
    // }
    uint8_t matrix[MATRIX_WIDTH][MATRIX_HEIGHT] = {
        {1,1,0,1,1,1,1,1},
        {1,1,0,1,1,1,1,1},
        {1,1,0,1,1,0,0,0},
        {1,1,0,1,1,0,0,0},
        {1,1,0,1,1,0,0,0},
        {1,1,0,1,1,0,0,0},
        {1,1,1,1,1,0,0,0},
        {1,1,1,1,1,0,0,0}
    };

    matrix_clear(matrix);
    for (int i =  0; i < MATRIX_HEIGHT; i++) {
        for (int j = 0; j < MATRIX_WIDTH; j++) {
            matrix[i][j] = 1;
            matrix_print(matrix);
            _delay_ms(50);
        }
    }





}

