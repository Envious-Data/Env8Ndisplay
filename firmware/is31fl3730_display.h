// is31fl3730_display.h
#ifndef IS31FL3730_DISPLAY_H
#define IS31FL3730_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

#define I2C_DISPLAY_LINE i2c1   // Use I2C1
#define I2C_DISPLAY_SDA 14      // GP26 (Match .c file)
#define I2C_DISPLAY_SCL 15      // GP27 (Match .c file)

#define MATRIX_A_ADDR 0b00001110
#define MATRIX_B_ADDR 0b00000001
#define UPDATE_ADDR   0b00001100

extern uint8_t displays[8][8]; // Match .c file

void init_display();
void update_display(void);
void set_char(int display, char letter, bool update);
void scroll_display_string(const char *string);
void display_string(const char *string);
void clear_all(bool update);
char *append_chars(const char *original, char character_to_append, size_t num_chars);
void pre_generate_alternate_characters();
// Add this declaration
extern const int display_map[8];

#endif // IS31FL3730_DISPLAY_H