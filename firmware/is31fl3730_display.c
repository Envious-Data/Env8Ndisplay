// is31fl3730_display.c
#include "is31fl3730_display.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <math.h>
#include <stdbool.h>

#define I2C_DISPLAY_LINE i2c1
#define I2C_DISPLAY_SDA 14
#define I2C_DISPLAY_SCL 15

#define MODE_ADDR 0b00000000
#define BRIGHTNESS_ADDR  0b00011001
#define BRIGHTNESS_VALUE     0b000000100
#define UPDATE_ADDR 0b00001100
#define OPTION_ADDR 0b00001101
#define MATRIX_A_ADDR 0b00001110 // Corresponds to Python's CMD_MATRIX_2 (0x0E)
#define MATRIX_B_ADDR 0b00000001 // Corresponds to Python's CMD_MATRIX_1 (0x01)

#define DEFAULT_MODE 0b00011000
#define DEFAULT_OPTIONS 0b00001110

// Added defines from Python code
#define CMD_BRIGHTNESS 0x19
#define CMD_MODE 0x00
#define CMD_UPDATE 0x0C
#define CMD_OPTIONS 0x0D
#define CMD_MATRIX_1 0x01 // Corresponds to MATRIX_B_ADDR
#define CMD_MATRIX_2 0x0E // Corresponds to MATRIX_A_ADDR

#define MATRIX_TYPE_A 0 // Used to differentiate between the two matrices on a single chip
#define MATRIX_TYPE_B 1

uint8_t addresses[4] = {0x60, 0x62, 0x61, 0x63}; // Adjusted for 4 chips

uint8_t displays[8][8] = {{0}}; // displays[physical_index][row_data]

const int display_map[8] = {
    6, // Logical 0 -> Physical 6
    7, // Logical 1 -> Physical 7
    0, // Logical 2 -> Physical 0
    1, // Logical 3 -> Physical 1
    2, // Logical 4 -> Physical 2
    3, // Logical 5 -> Physical 3
    4, // Logical 6 -> Physical 4
    5  // Logical 7 -> Physical 5
};

// In your is31fl3730_display.c file, perhaps globally:
uint8_t char_index_map[128] = {0}; // Initialize all to 0 (for space or error)

void populate_char_index_map() {
    char_index_map[' '] = 0;
    char_index_map['!'] = 1;
    char_index_map['"'] = 2;
    char_index_map['#'] = 3;
    char_index_map['$'] = 4;
    char_index_map['%'] = 5;
    char_index_map['&'] = 6;
    char_index_map['\''] = 7;
    char_index_map['('] = 8;
    char_index_map[')'] = 9;
    char_index_map['*'] = 10;
    char_index_map['+'] = 11;
    char_index_map[','] = 12;
    char_index_map['-'] = 13;
    char_index_map['.'] = 14;
    char_index_map['/'] = 15;
    char_index_map['0'] = 16;
    char_index_map['1'] = 17;
    char_index_map['2'] = 18;
    char_index_map['3'] = 19;
    char_index_map['4'] = 20;
    char_index_map['5'] = 21;
    char_index_map['6'] = 22;
    char_index_map['7'] = 23;
    char_index_map['8'] = 24;
    char_index_map['9'] = 25;
    char_index_map[':'] = 26;
    char_index_map[';'] = 27;
    char_index_map['<'] = 28;
    char_index_map['='] = 29;
    char_index_map['>'] = 30;
    char_index_map['?'] = 31;
    char_index_map['@'] = 32;
    char_index_map['A'] = 33;
    char_index_map['B'] = 34;
    char_index_map['C'] = 35;
    char_index_map['D'] = 36;
    char_index_map['E'] = 37;
    char_index_map['F'] = 38;
    char_index_map['G'] = 39;
    char_index_map['H'] = 40;
    char_index_map['I'] = 41;
    char_index_map['J'] = 42;
    char_index_map['K'] = 43;
    char_index_map['L'] = 44;
    char_index_map['M'] = 45;
    char_index_map['N'] = 46;
    char_index_map['O'] = 47;
    char_index_map['P'] = 48;
    char_index_map['Q'] = 49;
    char_index_map['R'] = 50;
    char_index_map['S'] = 51;
    char_index_map['T'] = 52;
    char_index_map['U'] = 53;
    char_index_map['V'] = 54;
    char_index_map['W'] = 55;
    char_index_map['X'] = 56;
    char_index_map['Y'] = 57;
    char_index_map['Z'] = 58;
    char_index_map['a'] = 59;
    char_index_map['b'] = 60;
    char_index_map['c'] = 61;
    char_index_map['d'] = 62;
    char_index_map['e'] = 63;
    char_index_map['f'] = 64;
    char_index_map['g'] = 65;
    char_index_map['h'] = 66;
    char_index_map['i'] = 67;
    char_index_map['j'] = 68;
    char_index_map['k'] = 69;
    char_index_map['l'] = 70;
    char_index_map['m'] = 71;
    char_index_map['n'] = 72;
    char_index_map['o'] = 73;
    char_index_map['p'] = 74;
    char_index_map['q'] = 75;
    char_index_map['r'] = 76;
    char_index_map['s'] = 77;
    char_index_map['t'] = 78;
    char_index_map['u'] = 79;
    char_index_map['v'] = 80;
    char_index_map['w'] = 81;
    char_index_map['x'] = 82;
    char_index_map['y'] = 83;
    char_index_map['z'] = 84;
}

// Mystery font included in early Adafruit SSD1306 library
// modified it to fit my usecase since it was nicer than what ever junk I came up with.
uint8_t characters[][8] = {
    // Index 0: ' ' (ASCII 0x20)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Index 1: '!' (ASCII 0x21)
    {0x00, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Index 2: '"' (ASCII 0x22)
    {0x00, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00},
    // Index 3: '#' (ASCII 0x23)
    {0x14, 0x3E, 0x14, 0x3E, 0x14, 0x00, 0x00, 0x00},
    // Index 4: '$' (ASCII 0x24)
    {0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00, 0x00, 0x00},
    // Index 5: '%' (ASCII 0x25)
    {0x43, 0x33, 0x08, 0x66, 0x61, 0x00, 0x00, 0x00},
    // Index 6: '&' (ASCII 0x26)
    {0x36, 0x49, 0x55, 0x22, 0x50, 0x00, 0x00, 0x00},
    // Index 7: ''' (ASCII 0x27)
    {0x00, 0x05, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Index 8: '(' (ASCII 0x28)
    {0x00, 0x1C, 0x22, 0x41, 0x00, 0x00, 0x00, 0x00},
    // Index 9: ')' (ASCII 0x29)
    {0x00, 0x41, 0x22, 0x1C, 0x00, 0x00, 0x00, 0x00},
    // Index 10: '*' (ASCII 0x2A)
    {0x14, 0x08, 0x3E, 0x08, 0x14, 0x00, 0x00, 0x00},
    // Index 11: '+' (ASCII 0x2B)
    {0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x00, 0x00},
    // Index 12: ',' (ASCII 0x2C)
    {0x00, 0x50, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Index 13: '-' (ASCII 0x2D)
    {0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00},
    // Index 14: '.' (ASCII 0x2E)
    {0x00, 0x60, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Index 15: '/' (ASCII 0x2F)
    {0x20, 0x10, 0x08, 0x04, 0x02, 0x00, 0x00, 0x00},
    // Index 16: '0' (ASCII 0x30)
    {0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00, 0x00, 0x00},
    // Index 17: '1' (ASCII 0x31)
    {0x00, 0x04, 0x02, 0x7F, 0x00, 0x00, 0x00, 0x00},
    // Index 18: '2' (ASCII 0x32)
    {0x42, 0x61, 0x51, 0x49, 0x46, 0x00, 0x00, 0x00},
    // Index 19: '3' (ASCII 0x33)
    {0x22, 0x41, 0x49, 0x49, 0x36, 0x00, 0x00, 0x00},
    // Index 20: '4' (ASCII 0x34)
    {0x18, 0x14, 0x12, 0x7F, 0x10, 0x00, 0x00, 0x00},
    // Index 21: '5' (ASCII 0x35)
    {0x27, 0x45, 0x45, 0x45, 0x39, 0x00, 0x00, 0x00},
    // Index 22: '6' (ASCII 0x36)
    {0x3E, 0x49, 0x49, 0x49, 0x32, 0x00, 0x00, 0x00},
    // Index 23: '7' (ASCII 0x37)
    {0x01, 0x01, 0x71, 0x09, 0x07, 0x00, 0x00, 0x00},
    // Index 24: '8' (ASCII 0x38)
    {0x36, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00, 0x00},
    // Index 25: '9' (ASCII 0x39)
    {0x26, 0x49, 0x49, 0x49, 0x3E, 0x00, 0x00, 0x00},
    // Index 26: ':' (ASCII 0x3A) - MODIFIED: 3-pixel wide dots
    {0x00, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00},
    // Index 27: ';' (ASCII 0x3B) - Kept original 5x7 data for semicolon
    {0x00, 0x56, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00},
    // Index 28: '<' (ASCII 0x3C)
    {0x08, 0x14, 0x22, 0x41, 0x00, 0x00, 0x00, 0x00},
    // Index 29: '=' (ASCII 0x3D)
    {0x14, 0x14, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00},
    // Index 30: '>' (ASCII 0x3E)
    {0x00, 0x41, 0x22, 0x14, 0x08, 0x00, 0x00, 0x00},
    // Index 31: '?' (ASCII 0x3F)
    {0x02, 0x01, 0x51, 0x09, 0x06, 0x00, 0x00, 0x00},
    // Index 32: '@' (ASCII 0x40)
    {0x3E, 0x41, 0x59, 0x55, 0x5E, 0x00, 0x00, 0x00},
    // Index 33: 'A' (ASCII 0x41)
    {0x7E, 0x09, 0x09, 0x09, 0x7E, 0x00, 0x00, 0x00},
    // Index 34: 'B' (ASCII 0x42)
    {0x7F, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00, 0x00},
    // Index 35: 'C' (ASCII 0x43)
    {0x3E, 0x41, 0x41, 0x41, 0x22, 0x00, 0x00, 0x00},
    // Index 36: 'D' (ASCII 0x44)
    {0x7F, 0x41, 0x41, 0x41, 0x3E, 0x00, 0x00, 0x00},
    // Index 37: 'E' (ASCII 0x45)
    {0x7F, 0x49, 0x49, 0x49, 0x41, 0x00, 0x00, 0x00},
    // Index 38: 'F' (ASCII 0x46)
    {0x7F, 0x09, 0x09, 0x09, 0x01, 0x00, 0x00, 0x00},
    // Index 39: 'G' (ASCII 0x47)
    {0x3E, 0x41, 0x41, 0x49, 0x3A, 0x00, 0x00, 0x00},
    // Index 40: 'H' (ASCII 0x48)
    {0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00, 0x00, 0x00},
    // Index 41: 'I' (ASCII 0x49)
    {0x00, 0x41, 0x7F, 0x41, 0x00, 0x00, 0x00, 0x00},
    // Index 42: 'J' (ASCII 0x4A)
    {0x30, 0x40, 0x40, 0x40, 0x3F, 0x00, 0x00, 0x00},
    // Index 43: 'K' (ASCII 0x4B)
    {0x7F, 0x08, 0x14, 0x22, 0x41, 0x00, 0x00, 0x00},
    // Index 44: 'L' (ASCII 0x4C)
    {0x7F, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00, 0x00},
    // Index 45: 'M' (ASCII 0x4D)
    {0x7F, 0x02, 0x0C, 0x02, 0x7F, 0x00, 0x00, 0x00},
    // Index 46: 'N' (ASCII 0x4E)
    {0x7F, 0x02, 0x04, 0x08, 0x7F, 0x00, 0x00, 0x00},
    // Index 47: 'O' (ASCII 0x4F)
    {0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00, 0x00, 0x00},
    // Index 48: 'P' (ASCII 0x50)
    {0x7F, 0x09, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00},
    // Index 49: 'Q' (ASCII 0x51)
    {0x1E, 0x21, 0x21, 0x21, 0x5E, 0x00, 0x00, 0x00},
    // Index 50: 'R' (ASCII 0x52)
    {0x7F, 0x09, 0x09, 0x09, 0x76, 0x00, 0x00, 0x00},
    // Index 51: 'S' (ASCII 0x53)
    {0x26, 0x49, 0x49, 0x49, 0x32, 0x00, 0x00, 0x00},
    // Index 52: 'T' (ASCII 0x54)
    {0x01, 0x01, 0x7F, 0x01, 0x01, 0x00, 0x00, 0x00},
    // Index 53: 'U' (ASCII 0x55)
    {0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00, 0x00, 0x00},
    // Index 54: 'V' (ASCII 0x56)
    {0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00, 0x00, 0x00},
    // Index 55: 'W' (ASCII 0x57)
    {0x7F, 0x20, 0x10, 0x20, 0x7F, 0x00, 0x00, 0x00},
    // Index 56: 'X' (ASCII 0x58)
    {0x41, 0x22, 0x1C, 0x22, 0x41, 0x00, 0x00, 0x00},
    // Index 57: 'Y' (ASCII 0x59)
    {0x07, 0x08, 0x70, 0x08, 0x07, 0x00, 0x00, 0x00},
    // Index 58: 'Z' (ASCII 0x5A)
    {0x61, 0x51, 0x49, 0x45, 0x43, 0x00, 0x00, 0x00},
    // Index 59: 'a' (ASCII 0x61)
    {0x20, 0x54, 0x54, 0x54, 0x78, 0x00, 0x00, 0x00},
    // Index 60: 'b' (ASCII 0x62)
    {0x7F, 0x44, 0x44, 0x44, 0x38, 0x00, 0x00, 0x00},
    // Index 61: 'c' (ASCII 0x63)
    {0x38, 0x44, 0x44, 0x44, 0x44, 0x00, 0x00, 0x00},
    // Index 62: 'd' (ASCII 0x64)
    {0x38, 0x44, 0x44, 0x44, 0x7F, 0x00, 0x00, 0x00},
    // Index 63: 'e' (ASCII 0x65)
    {0x38, 0x54, 0x54, 0x54, 0x18, 0x00, 0x00, 0x00},
    // Index 64: 'f' (ASCII 0x66)
    {0x04, 0x04, 0x7E, 0x05, 0x05, 0x00, 0x00, 0x00},
    // Index 65: 'g' (ASCII 0x67)
    {0x08, 0x54, 0x54, 0x54, 0x3C, 0x00, 0x00, 0x00},
    // Index 66: 'h' (ASCII 0x68)
    {0x7F, 0x08, 0x04, 0x04, 0x78, 0x00, 0x00, 0x00},
    // Index 67: 'i' (ASCII 0x69)
    {0x00, 0x44, 0x7D, 0x40, 0x00, 0x00, 0x00, 0x00},
    // Index 68: 'j' (ASCII 0x6A)
    {0x20, 0x40, 0x44, 0x3D, 0x00, 0x00, 0x00, 0x00},
    // Index 69: 'k' (ASCII 0x6B)
    {0x7F, 0x10, 0x28, 0x44, 0x00, 0x00, 0x00, 0x00},
    // Index 70: 'l' (ASCII 0x6C)
    {0x00, 0x41, 0x7F, 0x40, 0x00, 0x00, 0x00, 0x00},
    // Index 71: 'm' (ASCII 0x6D)
    {0x7C, 0x04, 0x78, 0x04, 0x78, 0x00, 0x00, 0x00},
    // Index 72: 'n' (ASCII 0x6E)
    {0x7C, 0x08, 0x04, 0x04, 0x78, 0x00, 0x00, 0x00},
    // Index 73: 'o' (ASCII 0x6F)
    {0x38, 0x44, 0x44, 0x44, 0x38, 0x00, 0x00, 0x00},
    // Index 74: 'p' (ASCII 0x70)
    {0x7C, 0x14, 0x14, 0x14, 0x08, 0x00, 0x00, 0x00},
    // Index 75: 'q' (ASCII 0x71)
    {0x08, 0x14, 0x14, 0x14, 0x7C, 0x00, 0x00, 0x00},
    // Index 76: 'r' (ASCII 0x72)
    {0x00, 0x7C, 0x08, 0x04, 0x04, 0x00, 0x00, 0x00},
    // Index 77: 's' (ASCII 0x73)
    {0x48, 0x54, 0x54, 0x54, 0x20, 0x00, 0x00, 0x00},
    // Index 78: 't' (ASCII 0x74)
    {0x04, 0x04, 0x3F, 0x44, 0x44, 0x00, 0x00, 0x00},
    // Index 79: 'u' (ASCII 0x75)
    {0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00, 0x00, 0x00},
    // Index 80: 'v' (ASCII 0x76)
    {0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00, 0x00, 0x00},
    // Index 81: 'w' (ASCII 0x77)
    {0x3C, 0x40, 0x30, 0x40, 0x3C, 0x00, 0x00, 0x00},
    // Index 82: 'x' (ASCII 0x78)
    {0x44, 0x28, 0x10, 0x28, 0x44, 0x00, 0x00, 0x00},
    // Index 83: 'y' (ASCII 0x79)
    {0x0C, 0x50, 0x50, 0x50, 0x3C, 0x00, 0x00, 0x00},
    // Index 84: 'z' (ASCII 0x7A)
    {0x44, 0x64, 0x54, 0x4C, 0x44, 0x00, 0x00, 0x00}
};

uint8_t alternate_characters[count_of(characters)][8];

void convertLeftToRight(const uint8_t *left_matrix, uint8_t *right_matrix) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            uint8_t bit_value = (left_matrix[row] >> (col)) & 1;
            right_matrix[col] |= (bit_value << row);
        }
    }
}

void pre_generate_alternate_characters() {
    for (int i = 0; i < count_of(characters); i++) {
        convertLeftToRight(characters[i], alternate_characters[i]);
    }
}

static void set_option(int chip_index, uint8_t address, uint8_t value) {
    if (chip_index < 0 || chip_index > 3) return;

    uint8_t buf[2];
    buf[0] = address;
    buf[1] = value;

    i2c_write_timeout_us(I2C_DISPLAY_LINE, addresses[chip_index], buf, count_of(buf), false, 1000);
}

// New functions ported from NanoMatrix (Python)
void set_chip_brightness(int chip_index, float brightness) {
    if (chip_index < 0 || chip_index > 3) return;
    uint8_t bright_value = (uint8_t)(brightness * 127.0f);
    if (bright_value > 127) bright_value = 127;
    set_option(chip_index, CMD_BRIGHTNESS, bright_value);
}

void set_pixel(int chip_index, int matrix_type, int x, int y, bool c) {
    if (chip_index < 0 || chip_index > 3) return;
    if (x < 0 || x > 7 || y < 0 || y > 7) return; // Assuming 8x8 matrix now

    int physical_display_index = chip_index * 2 + matrix_type; // 0 for A, 1 for B

    if (c) { // Set pixel
        if (matrix_type == MATRIX_TYPE_A) { // MATRIX_A corresponds to _BUF_MATRIX_2 (Red) in Python, but your setup uses it differently
            // Original NanoMatrix Python set_pixel logic for MATRIX_2 (Red): self._BUF_MATRIX_2[x] |= (0b1 << y)
            // This means column 'x' has a bit set at row 'y'. This implies column-major representation.
            // Your `displays` are row-major. Let's assume MATRIX_A (physical_display_index % 2 == 0) for the normal character set.
            // For MATRIX_A, characters are defined as `characters[char_index][i]` where `i` is the row and bits are columns.
            // So setting pixel (x,y) means setting the x-th bit of the y-th row.
            displays[physical_display_index][y] |= (1 << x);
        } else { // MATRIX_TYPE_B corresponds to _BUF_MATRIX_1 (Green) in Python
            // Original NanoMatrix Python set_pixel logic for MATRIX_1 (Green): self._BUF_MATRIX_1[y] |= (0b1 << x)
            // This means row 'y' has a bit set at column 'x'. This implies row-major representation.
            // Your `alternate_characters` are column-major after conversion.
            // If physical_display_index is odd, it uses alternate_characters (column-major).
            // So setting pixel (x,y) means setting the y-th bit of the x-th column.
            displays[physical_display_index][x] |= (1 << y);
        }
    } else { // Clear pixel
        if (matrix_type == MATRIX_TYPE_A) {
            displays[physical_display_index][y] &= ~(1 << x);
        } else {
            displays[physical_display_index][x] &= ~(1 << y);
        }
    }
}

void set_decimal(int chip_index, int matrix_type, bool state) {
    if (chip_index < 0 || chip_index > 3) return;

    int physical_display_index = chip_index * 2 + matrix_type;

    if (matrix_type == MATRIX_TYPE_A) { // Corresponds to Python's MATRIX_2 (red)
        // Python: self._BUF_MATRIX_2[7] |= 0b01000000
        // This means the 7th element (0-indexed) which is the 8th column, and the 6th bit (0-indexed) which is 7th row.
        // In your `displays` array, the structure is `displays[display_num][row_data]`.
        // The original NanoMatrix uses column-major for MATRIX_2 (Red), where `_BUF_MATRIX_2[7]` would be the 8th column.
        // The decimal point is the 7th bit of the 8th column.
        // For your `displays` array which is row-major for `characters`, this maps to a specific bit.
        // Given your character definitions are row-major, and alternate_characters are column-major,
        // we need to be careful.
        // If MATRIX_TYPE_A corresponds to your `characters` (row-major), then the decimal point
        // might be handled differently depending on how the hardware maps it.
        // The python code indicates _BUF_MATRIX_2[7] for the red DP.
        // Assuming your 'characters' (MATRIX_TYPE_A) are normally oriented as columns in bytes,
        // and 'alternate_characters' (MATRIX_TYPE_B) are rows in bytes.
        // Based on the python code's set_decimal:
        // MATRIX_1 decimal: self._BUF_MATRIX_1[6] |= 0b10000000 -> 7th row, 8th bit (MSB)
        // MATRIX_2 decimal: self._BUF_MATRIX_2[7] |= 0b01000000 -> 8th col, 7th bit (MSB-1)

        // For MATRIX_TYPE_A (your normal `characters` array), this likely corresponds to the second matrix on the chip.
        // Python's CMD_MATRIX_2 (0x0E) is MATRIX_A_ADDR in your code.
        // In Python, CMD_MATRIX_2 (red) decimal is on _BUF_MATRIX_2[7] (8th column) at bit 6 (0x40).
        // If your displays[physical_display_index] for MATRIX_A is column-major:
        if (state) {
            displays[physical_display_index][7] |= (1 << 6); // 8th column, 7th bit (0-indexed)
        } else {
            displays[physical_display_index][7] &= ~(1 << 6);
        }
    } else { // MATRIX_TYPE_B (your `alternate_characters` array)
        // Python: self._BUF_MATRIX_1[6] |= 0b10000000
        // This means the 7th row (0-indexed) which is `_BUF_MATRIX_1[6]`, and the 7th bit (0-indexed) which is MSB (0x80).
        // If your displays[physical_display_index] for MATRIX_B is row-major:
        if (state) {
            displays[physical_display_index][6] |= (1 << 7); // 7th row, 8th bit (0-indexed)
        } else {
            displays[physical_display_index][6] &= ~(1 << 7);
        }
    }
}

void clear_chip_matrix(int chip_index, int matrix_type) {
    if (chip_index < 0 || chip_index > 3) return;
    int physical_display_index = chip_index * 2 + matrix_type;
    for (int i = 0; i < 8; i++) {
        displays[physical_display_index][i] = 0;
    }
}


void init_display() {
    i2c_init(I2C_DISPLAY_LINE, 100 * 1000);
    gpio_set_function(I2C_DISPLAY_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_DISPLAY_SCL, GPIO_FUNC_I2C);

    // Add this line - critical for alternate displays
    pre_generate_alternate_characters();
	populate_char_index_map();

    for (int i = 0; i < count_of(addresses); i++) {
        set_option(i, CMD_MODE, DEFAULT_MODE); // Use CMD_MODE
        set_option(i, CMD_OPTIONS, DEFAULT_OPTIONS); // Use CMD_OPTIONS
        set_option(i, CMD_BRIGHTNESS, BRIGHTNESS_VALUE); // Use CMD_BRIGHTNESS
    }
}


void update_display() {
    for (int i = 0; i < count_of(addresses); i++) {
        // Send MATRIX_B (CMD_MATRIX_1) data first
        uint8_t buf_b[9] = {CMD_MATRIX_1};
        memcpy(buf_b + 1, displays[i * 2 + 1], 8); // displays[i*2 + 1] corresponds to MATRIX_B (odd physical indices)
        i2c_write_timeout_us(I2C_DISPLAY_LINE, addresses[i], buf_b, count_of(buf_b), false, 1000);

        // Send MATRIX_A (CMD_MATRIX_2) data
        uint8_t buf_a[9] = {CMD_MATRIX_2};
        memcpy(buf_a + 1, displays[i * 2], 8); // displays[i*2] corresponds to MATRIX_A (even physical indices)
        i2c_write_timeout_us(I2C_DISPLAY_LINE, addresses[i], buf_a, count_of(buf_a), false, 1000);

        set_option(i, CMD_UPDATE, 0x01); // Use CMD_UPDATE
    }
}

void transform_buffer_for_display(char letter, int display, uint8_t *buffer) {
    uint8_t char_index = 0; // Default to 0 (space)

    if (letter >= 0 && letter < 128) {
        char_index = char_index_map[(int)letter];
    }

    for (int i = 0; i < 8; i++) {
        // Here, the display % 2 == 0 (physical index is even) means it's one type of matrix
        // (let's say it aligns with how 'characters' are defined: row-major for display functions,
        // where each byte is a row and bits are columns).
        // If display % 2 == 1 (physical index is odd), it's the other type of matrix,
        // which uses 'alternate_characters' (column-major in your definition).
        if (display % 2 == 0) { // This corresponds to MATRIX_A_ADDR (CMD_MATRIX_2) and uses 'characters'
            buffer[i] = characters[char_index][i];
        } else { // This corresponds to MATRIX_B_ADDR (CMD_MATRIX_1) and uses 'alternate_characters'
            buffer[i] = alternate_characters[char_index][i];
        }
    }
}


void set_char(int display_index, char letter, bool update) {
    // Map the logical display index to the physical one
    int physical_index = display_map[display_index];

    if (physical_index < 0 || physical_index > 7) return;
    uint8_t char_buffer[8];
    transform_buffer_for_display(letter, physical_index, char_buffer);
    for (int i = 0; i < 8; i++) {
        displays[physical_index][i] = char_buffer[i];
    }
    if (update) update_display();
}

void scroll_display_string(const char *string) {
    size_t len = strlen(string);
    char *scroll_str = append_chars(string, ' ', 8);
    size_t scroll_len = strlen(scroll_str);

    clear_all(false);

    if (scroll_len > 8) {
        for (size_t i = 0; i <= scroll_len - 8; i++) {
            for (int display = 0; display < 8; display++) {
                char char_to_display = scroll_str[i + display];
                set_char(display, char_to_display, false);
            }
            update_display();
            sleep_ms(250);
        }
        sleep_ms(1000);
    } else {
        display_string(string);
        sleep_ms(1000);
    }
    free(scroll_str);
    clear_all(true);
}

void display_string(const char *string) {
    clear_all(false);
    size_t len = strlen(string);
    for (size_t i = 0; i < 8; i++) {
        if (i < len) {
            set_char(i, string[i], false); // Set characters in the buffer
        } else {
            set_char(i, ' ', false);
        }
    }
    update_display(); // Send the newly populated buffer to the hardware
}

void clear_all(bool update) {
    for (int i = 0; i < count_of(displays); i++) {
        for (int j = 0; j < 8; j++) {
            displays[i][j] = 0;
        }
    }
    if (update) update_display();
}

char *append_chars(const char *original, char character_to_append, size_t num_chars) {
    size_t original_length = strlen(original);
    char *new_string = (char *)malloc(original_length + num_chars + 1);
    if (new_string == NULL) return NULL;
    strcpy(new_string, original);
    for (size_t i = 0; i < num_chars; i++) {
        new_string[original_length + i] = character_to_append;
    }
    new_string[original_length + num_chars] = '\0';
    return new_string;
}