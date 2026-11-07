#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

// Initialize PWM hardware for servo control
void servo_init(void);

// Set the PWM pulse width directly in microseconds
void servo_set_pulse_us(uint16_t pulse_us);

// Command a logical angle between 0 and 180 degrees
void servo_set_angle(float angle);

// Move gradually to a target angle.
// This function blocks until the movement completes.
void servo_move_smooth(float target_angle,
                       float step_degrees);

// Return the last commanded logical angle
float servo_get_angle(void);

#endif
