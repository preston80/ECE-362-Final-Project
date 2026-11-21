#ifndef CONFIG_H
#define CONFIG_H

// =====================================================
// MICROPHONE CONFIGURATION
// =====================================================

// GPIO numbers, not physical header positions.
#define MIC_BCLK_PIN       6u
#define MIC_WS_PIN         7u
#define MIC_DATA_PIN       8u

#define MIC_SAMPLE_RATE    48000u
#define MIC_WARMUP_FRAMES  256u
#define MIC_TEST_FRAMES    4096u

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

// =====================================================
// TRACKING STABILITY
// =====================================================

#define ULTRASONIC_TRIG_PIN       10u
#define ULTRASONIC_ECHO_PIN       11u
#define AUDIO_TO_SERVO_GAIN       3.0f
#define AUDIO_TRACK_WINDOW        3u
#define AUDIO_TRACK_MIN_HITS      2u
#define AUDIO_TRACK_SWITCH_HITS   4u
#define AUDIO_TRACK_CENTER_HITS   16u
#define AUDIO_TRACK_ALPHA         0.35f
#define AUDIO_TRACK_HOLD_BAND_DEG 10.0f
#define AUDIO_TRACK_CENTER_DEADBAND_DEG 5.0f
#define AUDIO_TRACK_CANDIDATE_DEG 15.0f
#define AUDIO_TRACK_CANDIDATE_GAP_MS 300u
#define AUDIO_TRACK_RESET_MS      2000u
#define SERVO_TRACK_MIN_ANGLE     10.0f
#define SERVO_TRACK_MAX_ANGLE     170.0f
#define SERVO_TRACK_MAX_STEP      30.0f

#endif
