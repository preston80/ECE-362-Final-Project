#include "microphone.h"
#include "config.h"
#include <limits.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"

#if MIC_WS_PIN != MIC_BCLK_PIN + 1
#error "MIC_WS_PIN must immediately follow MIC_BCLK_PIN."
#endif

#if MIC_BCLK_PIN > 28 || MIC_DATA_PIN > 29
#error "This receiver supports GPIO 0 through 29."
#endif

#if MIC_DATA_PIN == MIC_BCLK_PIN || MIC_DATA_PIN == MIC_WS_PIN
#error "MIC_DATA_PIN must be separate from the clock pins."
#endif

#if MIC_SAMPLE_RATE != 48000 || MIC_TEST_FRAMES < 1
#error "Use 48000 Hz and at least one test frame for this test."
#endif

static PIO mic_pio = pio0;
static uint mic_sm, offset;
static pio_sm_config config;
static bool initialized;

// Side-set bit 0 = BCLK; bit 1 = WS.
#define SIDE(value) pio_encode_sideset(2, (value))
#define LOW(value) (SIDE(value) | pio_encode_delay(1))

static inline int32_t decode_i2s_word(uint32_t raw_word)
{
    // Raw word: 1 delay bit, 24 audio bits, 7 unused bits.
    uint32_t value = (raw_word >> 7) & 0x00ffffffu;
    return (value & 0x00800000u)
        ? (int32_t)value - 16777216
        : (int32_t)value;
}

bool microphone_init(void)
{
    if (initialized) return true;

    // Never take pins already assigned to UART terminal output.
    if (gpio_get_function(MIC_BCLK_PIN) == GPIO_FUNC_UART ||
        gpio_get_function(MIC_WS_PIN) == GPIO_FUNC_UART ||
        gpio_get_function(MIC_DATA_PIN) == GPIO_FUNC_UART) {
        return false;
    }

    // 32 bits per slot, left then right; 4 PIO cycles per bit.
    // Sample one PIO cycle after BCLK rises.
    uint16_t instructions[] = {
        pio_encode_set(pio_x, 30) | LOW(0),
        pio_encode_nop() | SIDE(1),
        pio_encode_in(pio_pins, 1) | SIDE(1),
        pio_encode_jmp_x_dec(1) | LOW(0),
        pio_encode_nop() | SIDE(1),
        pio_encode_in(pio_pins, 1) | SIDE(1),

        pio_encode_set(pio_x, 30) | LOW(2),
        pio_encode_nop() | SIDE(3),
        pio_encode_in(pio_pins, 1) | SIDE(3),
        pio_encode_jmp_x_dec(7) | LOW(2),
        pio_encode_nop() | SIDE(3),
        pio_encode_in(pio_pins, 1) | SIDE(3)
    };

    const struct pio_program program = {
        .instructions = instructions,
        .length = 12,
        .origin = -1
    };

    if (!pio_can_add_program(mic_pio, &program)) return false;

    int available = pio_claim_unused_sm(mic_pio, false);
    if (available < 0) return false;

    mic_sm = (uint)available;
    offset = pio_add_program(mic_pio, &program);

    config = pio_get_default_sm_config();

    sm_config_set_wrap(&config, offset, offset + 11);
    sm_config_set_sideset(&config, 2, false, false);
    sm_config_set_sideset_pins(&config, MIC_BCLK_PIN);
    sm_config_set_in_pins(&config, MIC_DATA_PIN);

    // Shift left and automatically push completed 32-bit words.
    sm_config_set_in_shift(&config, false, true, 32);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);

    sm_config_set_clkdiv(
        &config,
        (float)clock_get_hz(clk_sys) / (MIC_SAMPLE_RATE * 256.0f)
    );

    pio_gpio_init(mic_pio, MIC_BCLK_PIN);
    pio_gpio_init(mic_pio, MIC_WS_PIN);
    pio_gpio_init(mic_pio, MIC_DATA_PIN);

    gpio_pull_down(MIC_DATA_PIN);

    pio_sm_init(mic_pio, mic_sm, offset, &config);

    pio_sm_set_pins_with_mask(
        mic_pio, mic_sm, 0,
        (1u << MIC_BCLK_PIN) | (1u << MIC_WS_PIN)
    );

    pio_sm_set_consecutive_pindirs(
        mic_pio, mic_sm, MIC_BCLK_PIN, 2, true
    );

    pio_sm_set_consecutive_pindirs(
        mic_pio, mic_sm, MIC_DATA_PIN, 1, false
    );

    initialized = true;
    return true;
}

static void stop_capture(void)
{
    pio_sm_set_enabled(mic_pio, mic_sm, false);

    pio_sm_set_pins_with_mask(
        mic_pio, mic_sm, 0,
        (1u << MIC_BCLK_PIN) | (1u << MIC_WS_PIN)
    );
}

static bool read_word(uint32_t *word, absolute_time_t deadline)
{
    while (pio_sm_is_rx_fifo_empty(mic_pio, mic_sm)) {
        if (time_reached(deadline)) return false;
        tight_loop_contents();
    }

    *word = pio_sm_get(mic_pio, mic_sm);
    return true;
}

bool microphone_read_samples(int32_t *left_buf, int32_t *right_buf, size_t count)
{
    if (!initialized || !left_buf || !right_buf || count == 0) return false;

    pio_sm_init(mic_pio, mic_sm, offset, &config);

    absolute_time_t deadline = make_timeout_time_ms(1000);
    pio_sm_set_enabled(mic_pio, mic_sm, true);

    for (size_t i = 0; i < MIC_WARMUP_FRAMES + count; ++i) {
        uint32_t left_raw, right_raw;

        if (!read_word(&left_raw, deadline) ||
            !read_word(&right_raw, deadline)) {
            stop_capture();
            return false;
        }

        if (i < MIC_WARMUP_FRAMES) continue;

        size_t idx = i - MIC_WARMUP_FRAMES;
        left_buf[idx] = decode_i2s_word(left_raw);
        right_buf[idx] = decode_i2s_word(right_raw);
    }

    stop_capture();
    return true;
}

bool microphone_read_stereo_level(stereo_level_result_t *result)
{
    if (!initialized || !result) return false;

    static int32_t left_buf[MIC_TEST_FRAMES];
    static int32_t right_buf[MIC_TEST_FRAMES];

    if (!microphone_read_samples(left_buf, right_buf, MIC_TEST_FRAMES)) {
        return false;
    }

    result->left.minimum = INT32_MAX;
    result->left.maximum = INT32_MIN;
    result->right.minimum = INT32_MAX;
    result->right.maximum = INT32_MIN;

    for (size_t i = 0; i < MIC_TEST_FRAMES; ++i) {
        if (left_buf[i] < result->left.minimum) result->left.minimum = left_buf[i];
        if (left_buf[i] > result->left.maximum) result->left.maximum = left_buf[i];
        if (right_buf[i] < result->right.minimum) result->right.minimum = right_buf[i];
        if (right_buf[i] > result->right.maximum) result->right.maximum = right_buf[i];
    }

    result->left.level = (uint32_t)(result->left.maximum - result->left.minimum);
    result->right.level = (uint32_t)(result->right.maximum - result->right.minimum);

    return true;
}