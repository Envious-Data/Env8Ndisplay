#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "is31fl3730_display.h" // Display code handles I2C init
#include "hardware/uart.h"       // Still useful for general UART functions if needed, but stdio_usb handles USB serial
#include "hardware/gpio.h"
#include "hardware/sync.h"
// #include "hardware/i2c.h" // No longer explicitly needed here as display code handles it, and ds3231 also uses it
#include <stdio.h>
#include <string.h>
#include "ds3231.h" // Assuming this contains clock_read_time, clock_set_time, bcd_to_int, int_to_bcd, and clock_buffer

// For the BOOTSEL button functionality
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

// Define the buffer size for serial input
#define SERIAL_BUFFER_SIZE 32

// Global buffer for serial input (single writer, single reader on core 1)
char serial_data_buffer[SERIAL_BUFFER_SIZE];

// Enum for display modes
typedef enum {
    MODE_CLOCK,
    MODE_DATE,
    MODE_EXAMPLE_STRING,
    NUM_MODES // Keep this last to easily get the number of modes
} DisplayMode;

// Global variable to store the current display mode
volatile DisplayMode current_display_mode = MODE_CLOCK;

// Global variable to store the arbitrary string for example mode
char example_string[9] = " 8nDisp "; // Max 8 characters for display, plus null terminator

// Function to parse the time string and set the RTC
void set_rtc_from_string(const char* time_str) {
    int year, month, day, hour, minute, second;
    // Expected format:YYYY-MM-DD HH:MM:SS
    if (sscanf(time_str, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
        printf("Attempting to set RTC to: %04d-%02d-%02d %02d:%02d:%02d\n", year, month, day, hour, minute, second);

        // Convert integers to BCD before calling clock_set_time
        unsigned char bcd_second = int_to_bcd(second);
        unsigned char bcd_minute = int_to_bcd(minute);
        unsigned char bcd_hour = int_to_bcd(hour);
        unsigned char bcd_day = int_to_bcd(day);
        unsigned char bcd_month = int_to_bcd(month);
        unsigned char bcd_year = int_to_bcd(year % 100); // DS3231 stores 2-digit year (00-99)

        clock_set_time(bcd_second, bcd_minute, bcd_hour, bcd_day, bcd_month, bcd_year);
        printf("RTC set command issued.\n");
    } else {
        printf("Invalid time string format received: %s. Expected YYYY-MM-DD HH:MM:SS\n", time_str);
    }
}

// Function to set the example string from serial input
void set_example_string_from_string(const char* str) {
    // Ensure the string fits and is null-terminated
    strncpy(example_string, str, sizeof(example_string) - 1);
    example_string[sizeof(example_string) - 1] = '\0';
    printf("Example string set to: \"%s\"\n", example_string);
}


// BOOTSEL button functionality
bool __no_inline_not_in_flash_func(get_bootsel_button)() {
    const uint CS_PIN_INDEX = 1;

    // Must disable interrupts, as interrupt handlers may be in flash, and we
    // are about to temporarily disable flash access!
    uint32_t flags = save_and_disable_interrupts();

    // Set chip select to Hi-Z
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Note we can't call into any sleep functions in flash right now
    for (volatile int i = 0; i < 1000; ++i);

    // The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
    // Note the button pulls the pin *low* when pressed.
#if PICO_RP2040
    #define CS_BIT (1u << 1)
#else
    #define CS_BIT SIO_GPIO_HI_IN_QSPI_CSN_BITS
#endif
    bool button_state = !(sio_hw->gpio_hi_in & CS_BIT);

    // Need to restore the state of chip select, else we are going to have a
    // bad time when we return to code in flash!
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);

    return button_state;
}

void core1_entry() {
    // Clear the buffer once at startup for Core 1
    memset(serial_data_buffer, 0, SERIAL_BUFFER_SIZE);
    size_t current_buffer_len = 0; // Keep track of current length

    while (true) {
        if (stdio_usb_connected()) {
            int received_char = getchar_timeout_us(0); // Non-blocking read

            if (received_char != PICO_ERROR_TIMEOUT && received_char != EOF) {
                if (received_char == '\n' || received_char == '\r') {
                    // Null-terminate the string
                    serial_data_buffer[current_buffer_len] = '\0';

                    // Check for commands
                    if (strncmp(serial_data_buffer, "settime ", 8) == 0) {
                        // Extract the time string (skip "settime ")
                        set_rtc_from_string(serial_data_buffer + 8);
                    } else if (strncmp(serial_data_buffer, "setexample ", 11) == 0) {
                        // Extract the example string (skip "setexample ")
                        set_example_string_from_string(serial_data_buffer + 11);
                    }
                    else {
                        printf("Unknown command received: %s\n", serial_data_buffer);
                    }

                    // Clear the buffer for the next command
                    memset(serial_data_buffer, 0, SERIAL_BUFFER_SIZE);
                    current_buffer_len = 0; // Reset length
                } else if (current_buffer_len < SERIAL_BUFFER_SIZE - 1) {
                    // Append character to buffer if not full and not a newline/carriage return
                    serial_data_buffer[current_buffer_len++] = (char)received_char;
                } else {
                    // Buffer is full, discard new characters until newline/carriage return
                    printf("Serial buffer full, discarding character: %c\n", (char)received_char);
                }
            }
        }
        sleep_ms(1); // Small delay to prevent busy-waiting
    }
}

int main() {
    // Initialize stdio with USB
    stdio_init_all();
    printf("USB Serial Started!\n");
    sleep_ms(1);

    // Initialize the display. This function is assumed to handle I2C initialization internally.
    // Since is31fl3730_display.c and ds3231.c both use i2c1 on the same pins,
    // initializing it once here is sufficient.
    init_display();
    printf("Display Init\n");

    // --- Start Display Driver Demonstration ---
    printf("Starting display driver demonstration...\n");
    clear_all(true);
    sleep_ms(500);

    // 1. Welcome Message
    display_string("STARTING");
    sleep_ms(250);
    clear_all(true);
    sleep_ms(250);

    // 2. Pixel by Pixel - Fill all matrices in logical order
    printf("Demonstrating individual pixels (filling all matrices in logical order)...\n");
    for (int logical_idx = 0; logical_idx < 8; logical_idx++) { // Iterate through all 8 logical displays
        int physical_idx = display_map[logical_idx]; // Get the physical index from the map
        int chip_idx = physical_idx / 2; // Calculate chip index
        int matrix_type = physical_idx % 2; // Calculate matrix type (A or B)

        printf("Filling Logical Display %d (Chip %d, Matrix Type %d)...\n", logical_idx, chip_idx, matrix_type);
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                set_pixel(chip_idx, matrix_type, i, j, true); // Set pixel on the determined chip and matrix
                update_display();
                //sleep_ms(); // Faster fill for all matrices
            }
        }
        //sleep_ms(1);
        //clear_chip_matrix(chip_idx, matrix_type); // Clear current matrix
        update_display();
        sleep_ms(1);
    }
    sleep_ms(250);
    clear_all(true); // Ensure everything is clear after the demo
    sleep_ms(250);


    // 4. Scrolling Message
    printf("Demonstrating scrolling string...\n");
    scroll_display_string("PICO CLOCK DEMO! ");
    sleep_ms(250);
    scroll_display_string("Envious.Designs - EnvOpenClock - 8nDisp");
    sleep_ms(250);

    // 5. Static Message with characters
    printf("Displaying static characters...\n");
    display_string("PICO!");
    sleep_ms(250);
    clear_all(true);
    sleep_ms(250);

    printf("Display driver demonstration finished.\n");
    // --- End Display Driver Demonstration ---

    // Launch the second core
    multicore_launch_core1(core1_entry);
    printf("Multicore launched\n");

    bool prev_bootsel_state = false; // To detect rising edge (button release)
    uint64_t last_button_press_time = 0;
    const uint64_t DEBOUNCE_DELAY_MS = 200; // 200ms debounce

    // Core 0 loop for displaying time and handling mode changes
    char display_buffer[9]; // HH:MM:SS\0 or DD-MM-YY\0 or 8 chars + null

    while (true) {
        // --- BOOTSEL Button Handling ---
        bool current_bootsel_state = get_bootsel_button();
        uint64_t current_time_ms = to_ms_since_boot(get_absolute_time());

        // Check for a *release* of the button to trigger mode change
        // This makes it less sensitive to accidental presses and acts on button "click"
        if (!current_bootsel_state && prev_bootsel_state && (current_time_ms - last_button_press_time > DEBOUNCE_DELAY_MS)) {
            // Button released (falling edge) and debounced
            current_display_mode = (current_display_mode + 1) % NUM_MODES;
            printf("Mode changed to: ");
            switch (current_display_mode) {
                case MODE_CLOCK: printf("CLOCK\n"); break;
                case MODE_DATE: printf("DATE\n"); break;
                case MODE_EXAMPLE_STRING: printf("EXAMPLE STRING\n"); break;
                default: printf("UNKNOWN\n"); break;
            }
            last_button_press_time = current_time_ms; // Update last press time
        }
        prev_bootsel_state = current_bootsel_state; // Update previous state

        // --- Display Logic based on Mode ---
        clock_read_time(); // Always read time to keep it updated internally

        // Get RTC data using bcd_to_int and correct clock_buffer indices
        unsigned char hour = bcd_to_int(clock_buffer[2]);
        unsigned char minute = bcd_to_int(clock_buffer[1]);
        unsigned char second = bcd_to_int(clock_buffer[0]);
        unsigned char day = bcd_to_int(clock_buffer[4]);   // Day of month
        unsigned char month = bcd_to_int(clock_buffer[5]); // Month
        unsigned char year = bcd_to_int(clock_buffer[6]);  // Year

        switch (current_display_mode) {
            case MODE_CLOCK:
                // Format the time as HH:MM:SS
                snprintf(display_buffer, sizeof(display_buffer), "%02d:%02d:%02d", hour, minute, second);
                break;
            case MODE_DATE:
                // Format the date as DD-MM-YY
                snprintf(display_buffer, sizeof(display_buffer), "%02d-%02d-%02d", day, month, year);
                break;
            case MODE_EXAMPLE_STRING:
                // Display the arbitrary string
                strncpy(display_buffer, example_string, sizeof(display_buffer) - 1);
                display_buffer[sizeof(display_buffer) - 1] = '\0'; // Ensure null termination
                break;
            default:
                strncpy(display_buffer, "ERROR   ", sizeof(display_buffer)); // Fallback
                break;
        }

        // Display the string on the LED matrices
        display_string(display_buffer);

        // Print to serial for debugging (optional)
        printf("Current Display: %s (Mode: %d)\n", display_buffer, current_display_mode);

        sleep_ms(100); // Update display and check button every 100ms
    }

    return 0;
}