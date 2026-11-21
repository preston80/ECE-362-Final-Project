#include <stdio.h>
#include "pico/stdlib.h"
#include "config.h"
#include "microphone.h"
#include "audio_processing.h"

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    if (!microphone_init()) {
        printf("ERROR: microphone_init() failed!\n");
        while (true) tight_loop_contents();
    }

    static int32_t left_buf[MIC_TEST_FRAMES];
    static int32_t right_buf[MIC_TEST_FRAMES];
    float angle = 0.0f;

    printf("Direction Test Ready (6.1 in spacing). Clap Left, Center, or Right...\n");

    while (true) {
        if (microphone_read_samples(left_buf, right_buf, MIC_TEST_FRAMES)) {
            if (audio_estimate_angle(left_buf, right_buf, MIC_TEST_FRAMES, &angle)) {
                printf("Detected Angle: %+6.1f deg\n", angle);
            }
        }
        sleep_ms(50);
    }
}