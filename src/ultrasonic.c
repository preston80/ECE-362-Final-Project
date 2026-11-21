
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "config.h"
#include "ultrasonic.h"

#define ECHO_TIMEOUT_US 30000

void ultrasonic_init(void) {
    // TRIG is an output.
    gpio_init(ULTRASONIC_TRIG_PIN);
    gpio_set_dir(ULTRASONIC_TRIG_PIN, GPIO_OUT);
    gpio_put(ULTRASONIC_TRIG_PIN, 0);

    // ECHO is an input.
    gpio_init(ULTRASONIC_ECHO_PIN);
    gpio_set_dir(ULTRASONIC_ECHO_PIN, GPIO_IN);
    gpio_disable_pulls(ULTRASONIC_ECHO_PIN);
}

void ultrasonic_trigger(void) {
    // Ensure TRIG is LOW first.
    gpio_put(ULTRASONIC_TRIG_PIN, 0);
    sleep_us(2);

    // Send the 10 microsecond pulse.
    gpio_put(ULTRASONIC_TRIG_PIN, 1);
    sleep_us(10);

    // Finish the pulse.
    gpio_put(ULTRASONIC_TRIG_PIN, 0);
}

float ultrasonic_read_cm(void) {
    // A previously HIGH ECHO signal is invalid.
    if (gpio_get(ULTRASONIC_ECHO_PIN)) {
        return -1.0f;
    }

    // Start ultrasonic measurement.
    ultrasonic_trigger();

    // Wait for ECHO to become HIGH.
    uint64_t deadline =
        time_us_64() + ECHO_TIMEOUT_US;

    while (gpio_get(ULTRASONIC_ECHO_PIN) == 0) {
        if (time_us_64() >= deadline) {
            return -1.0f;
        }
    }

    // Record the beginning of ECHO HIGH.
    uint64_t start_time = time_us_64();

    // Wait for ECHO to become LOW.
    deadline = start_time + ECHO_TIMEOUT_US;

    while (gpio_get(ULTRASONIC_ECHO_PIN) == 1) {
        if (time_us_64() >= deadline) {
            return -1.0f;
        }
    }

    // Measure how long ECHO remained HIGH.
    uint64_t end_time = time_us_64();

    uint64_t duration_us =
        end_time - start_time;

    // Convert microseconds into centimeters.
    float distance_cm =
        (float)duration_us / 58.0f;

    // Reject values outside the usual
    // HC-SR04 nominal measurement range.
    if (distance_cm < 2.0f ||
        distance_cm > 400.0f) {
        return -1.0f;
    }

    return distance_cm;
}


/*
int main(void) {
    stdio_init_all();

    ultrasonic_init();

    // Allow USB serial to initialize.
    sleep_ms(2000);

    printf("HC-SR04 Test Starting...\n");

    while (true) {
        float distance = ultrasonic_read_cm();

        if (distance < 0.0f) {
            printf("No valid echo detected\n");
        } else {
            printf("Distance: %.2f cm\n", distance);
        }

        // Avoid triggering the sensor too frequently.
        sleep_ms(100);
    }

    return 0;
}
*/