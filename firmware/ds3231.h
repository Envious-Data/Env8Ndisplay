#ifndef DS3231_H
#define DS3231_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define CLOCK_ADDR 0x68

#define CLOCK_I2C_LINE      i2c1
#define CLOCK_I2C_SDA       14
#define CLOCK_I2C_SCL       15

// Declare clock_buffer as extern so other files can access it
extern char clock_buffer[]; // Add this line

unsigned char bcd_to_int(unsigned char bcd);
unsigned char int_to_bcd(int n);
void clock_set_time(unsigned char second, unsigned char minute, unsigned char hour, unsigned char day, unsigned char week, unsigned char year);
void clock_read_time();

#endif // DS3231_H