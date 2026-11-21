
#ifndef ULTRASONIC_H
#define ULTRASONIC_H

// Initialize ultrasonic GPIO pins.
void ultrasonic_init(void);

// Send a 10 microsecond trigger pulse.
void ultrasonic_trigger(void);

// Measure distance in centimeters.
// Returns -1.0f if the measurement times out
// or the sensor reports an invalid signal.
float ultrasonic_read_cm(void);

#endif