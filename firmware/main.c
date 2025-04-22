#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "is31fl3730_display.h"
#include <stdio.h>
#include <string.h>

void format_uptime_ms(uint64_t uptime_ms, char *uptime_str, size_t buffer_size, bool blink_sec, bool blink_min) {
    uint32_t milliseconds = uptime_ms % 1000;
    uint32_t seconds = (uptime_ms / 1000) % 60;
    uint32_t minutes = (uptime_ms / (1000 * 60)) % 60;
    uint32_t hours = (uptime_ms / (1000 * 60 * 60)) % 24;
    uint32_t days = uptime_ms / (1000 * 60 * 60 * 24);

    if (days > 0) {
        snprintf(uptime_str, buffer_size, "%luD%c%02luH%c%02luM",
                 days, (blink_min ? ':' : ' '), hours, (blink_sec ? ':' : ' '), minutes);
    } else if (hours > 0) {
        snprintf(uptime_str, buffer_size, "%02lu%c%02lu%c%02lu",
                 hours, (blink_min ? ':' : ' '), minutes, (blink_sec ? ':' : ' '), seconds);
    } else {
        snprintf(uptime_str, buffer_size, "%02lu%c%02lu%c%03lu",
                 minutes, ':', seconds, (blink_sec ? ':' : ' '), milliseconds);
    }
}

int main() {
    stdio_init_all();
    init_display();
    printf("Display initialized.\n");

    char uptime_text[12]; // Adjust size as needed
    uint32_t last_second = 0;
    bool blink_seconds_state = false;
    uint32_t last_minute = 0;
    bool blink_minutes_state = false;

    while (1) {
        uint64_t current_uptime_ms = us_to_ms(time_us_64());
        uint32_t current_second = (current_uptime_ms / 1000) % 60;
        uint32_t current_minute = (current_uptime_ms / (1000 * 60)) % 60;

        // Toggle seconds blink state every second
        if (current_second != last_second) {
            blink_seconds_state = !blink_seconds_state;
            last_second = current_second;
        }

        // Toggle minutes blink state every minute
        if (current_minute != last_minute) {
            blink_minutes_state = !blink_minutes_state;
            last_minute = current_minute;
        }

        format_uptime_ms(current_uptime_ms, uptime_text, sizeof(uptime_text), blink_seconds_state, blink_minutes_state);
        display_string(uptime_text);
        sleep_ms(10); // Update frequently for smooth blinking
    }

    return 0;
}