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
#define BUTTON_DDR DDRD
#define BUTTON_PORT PORTD
#define BUTTON_PINR PIND
#define BUTTON_PIN PD6


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



static void button_init(){
    BUTTON_DDR &= ~(1 << BUTTON_PIN);

    // NO pull-up (external resistor used)
    BUTTON_PORT &= ~(1 << BUTTON_PIN);
}

uint8_t button_pressed(void)
{
    if (BUTTON_PINR & (1 << BUTTON_PIN)) {
        _delay_ms(20);  // debounce
        if (BUTTON_PINR & (1 << BUTTON_PIN)) {
            return 1;
        }
    }
    return 0;
}


void matrix_print(uint8_t (*matrix)[MATRIX_WIDTH]) {

    for (int i =  0; i < MATRIX_HEIGHT; i++) {
        unsigned int bin = 0;
        for (int j = 0; j < MATRIX_WIDTH; j++) {
            bin = (bin << 1) | matrix[j][i];
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

void spawn(uint8_t *a, uint8_t size) {

    uint8_t cnt = 0;
    for (int i = 0; i < MATRIX_WIDTH; i++) {
        if (cnt < size) {
            a[i] = 1;
            cnt++;
        }
    }

}

void clean_row(uint8_t *a, uint8_t *last_row) {
    uint8_t cnt = 0;
    uint8_t high = 0;
    uint8_t high_bit = 0;

    //cut of smaller side
    for (int i = 0; i < MATRIX_WIDTH; i++) {
        if (a[i] == 1) {
            cnt++;
        } else {
            cnt = 0;
        }
        if (cnt > high) {
            high = cnt;
            high_bit = i;
        }
    }

    for (int i = 0; i < MATRIX_WIDTH; i++) {
        if (i < high_bit - high || i > high_bit || last_row[i] == 0) {
            a[i] = 0;
        }
    }

}

uint8_t count_ones(uint8_t *a) {
    uint8_t count = 0;
    for (int i = 0; i < MATRIX_WIDTH; i++) {
        if (a[i] == 1) {
            count++;
        }
    }
    return count;
}

void rotate90_cw(int n, uint8_t a[n][n])
{
    // transpose
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            uint8_t tmp = a[i][j];
            a[i][j] = a[j][i];
            a[j][i] = tmp;
        }
    }

    // reverse each row
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n / 2; j++) {
            uint8_t tmp = a[i][j];
            a[i][j] = a[i][n - 1 - j];
            a[i][n - 1 - j] = tmp;
        }
    }
}

int main(void) {

    button_init();
    spi_init();
    max7219_init();
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

    uint8_t rowx = 7;
    
    uint8_t *last_row = matrix[rowx];
    while(1){
        if (button_pressed()) {
            clean_row(matrix[rowx], last_row);
            uint8_t size = count_ones(matrix[rowx]);
            last_row = matrix[rowx];
            rowx--;
            spawn(matrix[rowx], size);

        }
        _delay_ms(100);
        matrix_print(matrix);
        shift_right(matrix[rowx]);

    }
}
