#ifndef CONFIG_H
#define CONFIG_H

// =====================================================
// SERVO CONFIGURATION
// =====================================================

// RP2350 GPIO used for the servo PWM signal
#define SERVO_PIN 18

// Standard positional servo PWM frequency
#define SERVO_PWM_FREQ_HZ 50

// Initial pulse width limits (microseconds).
// Calibrate for your specific DS3218 before
// assuming the full mechanical travel range.
#define SERVO_MIN_PULSE_US    1000
#define SERVO_CENTER_PULSE_US 1500
#define SERVO_MAX_PULSE_US    2000

// Logical angle limits used by our software.
// Actual physical angles must be calibrated.
#define SERVO_MIN_ANGLE 0.0f
#define SERVO_MAX_ANGLE 180.0f

// Delay between small movements
#define SERVO_STEP_DELAY_MS 20

#endif
