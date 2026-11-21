#include "servo.h"
#include "config.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#include <stdint.h>

// =====================================================
// PRIVATE VARIABLES
// =====================================================

static uint servo_slice;
static uint servo_channel;

static float current_angle = 90.0f;

// =====================================================
// HELPER FUNCTIONS
// =====================================================

static float clamp_angle(float angle)
{
    if (angle < SERVO_MIN_ANGLE)
        return SERVO_MIN_ANGLE;

    if (angle > SERVO_MAX_ANGLE)
        return SERVO_MAX_ANGLE;

    return angle;
}

static uint16_t angle_to_pulse(float angle)
{
    angle = clamp_angle(angle);

    float ratio =
        (angle - SERVO_MIN_ANGLE) /
        (SERVO_MAX_ANGLE - SERVO_MIN_ANGLE);

    float pulse =
        SERVO_MIN_PULSE_US +
        ratio * (SERVO_MAX_PULSE_US -
                 SERVO_MIN_PULSE_US);

    return (uint16_t)(pulse + 0.5f);
}

// =====================================================
// SERVO INITIALIZATION
// =====================================================

void servo_init(void)
{
    // Assign servo pin to hardware PWM
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);

    // Identify the PWM slice and channel
    servo_slice =
        pwm_gpio_to_slice_num(SERVO_PIN);

    servo_channel =
        pwm_gpio_to_channel(SERVO_PIN);

    // Get default PWM configuration
    pwm_config cfg = pwm_get_default_config();

    // Set PWM counter frequency to 1 MHz.
    // One counter increment = 1 microsecond.
    float clkdiv =
        (float)clock_get_hz(clk_sys) / 1000000.0f;

    pwm_config_set_clkdiv(&cfg, clkdiv);

    // 50 Hz = 20 ms period = 20000 counts
    pwm_config_set_wrap(&cfg, 20000 - 1);

    // Initialize PWM without enabling it yet
    pwm_init(servo_slice, &cfg, false);

    // Start servo at center pulse
    pwm_set_chan_level(
        servo_slice,
        servo_channel,
        SERVO_CENTER_PULSE_US
    );

    current_angle = 90.0f;

    // Enable PWM output
    pwm_set_enabled(servo_slice, true);
}

// =====================================================
// DIRECT PULSE WIDTH CONTROL
// =====================================================

void servo_set_pulse_us(uint16_t pulse_us)
{
    // Prevent commands outside configured limits
    if (pulse_us < SERVO_MIN_PULSE_US)
        pulse_us = SERVO_MIN_PULSE_US;

    if (pulse_us > SERVO_MAX_PULSE_US)
        pulse_us = SERVO_MAX_PULSE_US;

    // Update PWM compare register
    pwm_set_chan_level(
        servo_slice,
        servo_channel,
        pulse_us
    );

    // Keep the logical position synchronized
    current_angle =
        SERVO_MIN_ANGLE +
        ((float)(pulse_us - SERVO_MIN_PULSE_US) /
         (SERVO_MAX_PULSE_US -
          SERVO_MIN_PULSE_US)) *
        (SERVO_MAX_ANGLE - SERVO_MIN_ANGLE);
}

// =====================================================
// ANGLE CONTROL
// =====================================================

void servo_set_angle(float angle)
{
    angle = clamp_angle(angle);

    uint16_t pulse_us = angle_to_pulse(angle);

    servo_set_pulse_us(pulse_us);

    current_angle = angle;
}

// =====================================================
// SMOOTH MOVEMENT
// =====================================================

void servo_move_smooth(float target_angle,
                       float step_degrees)
{
    target_angle = clamp_angle(target_angle);

    // Reject invalid step sizes
    if (!(step_degrees > 0.0f))
        return;

    float position = current_angle;

    while (position != target_angle)
    {
        if (position < target_angle)
        {
            position += step_degrees;

            if (position > target_angle)
                position = target_angle;
        }
        else
        {
            position -= step_degrees;

            if (position < target_angle)
                position = target_angle;
        }

        servo_set_angle(position);

        sleep_ms(SERVO_STEP_DELAY_MS);
    }
}

// =====================================================
// GET CURRENT COMMANDED ANGLE
// =====================================================

float servo_get_angle(void)
{
    return current_angle;
}
