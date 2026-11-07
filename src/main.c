#include "pico/stdlib.h"

#include "servo.h"
#include "ultrasonic.h"
#include "microphone.h"
#include "audio_processing.h"
#include "display.h"

int main(void)
{
    stdio_init_all();

    servo_init();
    ultrasonic_init();
    microphone_init();
    display_init();

    while (true)
    {
        // Acquire synchronized audio samples.
        // Estimate the direction of the sound.
        // Rotate the servo toward the target.
        // Read ultrasonic distance.
        // Update the TFT display.
    }
}