#include <stdio.h>
#include "pico/stdlib.h"
#include "config.h"
#include "microphone.h"
#include "audio_processing.h"
#include "servo.h"
#include "ultrasonic.h"
#include "display.h"

typedef struct {
    float angles[AUDIO_TRACK_WINDOW];
    size_t count;
    size_t next;
    float filtered_angle;
    float candidate_angle;
    size_t candidate_hits;
    uint64_t last_detection_ms;
    uint64_t candidate_time_ms;
    bool initialized;
} angle_tracker_t;

static bool angles_within(float left, float right, float tolerance)
{
    float difference = left - right;
    return difference >= -tolerance && difference <= tolerance;
}

static void add_candidate(angle_tracker_t *tracker,
                          float angle,
                          uint64_t now_ms)
{
    if (tracker->candidate_hits == 0 ||
        now_ms - tracker->candidate_time_ms > AUDIO_TRACK_CANDIDATE_GAP_MS ||
        !angles_within(angle, tracker->candidate_angle,
                       AUDIO_TRACK_CANDIDATE_DEG)) {
        tracker->candidate_angle = angle;
        tracker->candidate_hits = 1;
    } else {
        tracker->candidate_angle +=
            (angle - tracker->candidate_angle) /
            (float)(tracker->candidate_hits + 1);
        tracker->candidate_hits++;
    }
    tracker->candidate_time_ms = now_ms;
}

static bool update_angle_tracker(angle_tracker_t *tracker,
                                 float angle,
                                 float *filtered_angle)
{
    uint64_t now_ms = to_ms_since_boot(get_absolute_time());
    if (tracker->last_detection_ms == 0 ||
        now_ms - tracker->last_detection_ms > AUDIO_TRACK_RESET_MS) {
        tracker->count = 0;
        tracker->next = 0;
        tracker->initialized = false;
        tracker->candidate_hits = 0;
    }
    tracker->last_detection_ms = now_ms;

    tracker->angles[tracker->next] = angle;
    tracker->next = (tracker->next + 1) % AUDIO_TRACK_WINDOW;
    if (tracker->count < AUDIO_TRACK_WINDOW) {
        tracker->count++;
    }
    if (tracker->count < AUDIO_TRACK_MIN_HITS) {
        return false;
    }

    float sorted[AUDIO_TRACK_WINDOW];
    for (size_t i = 0; i < tracker->count; ++i) {
        sorted[i] = tracker->angles[i];
    }
    for (size_t i = 1; i < tracker->count; ++i) {
        float value = sorted[i];
        size_t j = i;
        while (j > 0 && sorted[j - 1] > value) {
            sorted[j] = sorted[j - 1];
            --j;
        }
        sorted[j] = value;
    }

    float median_angle = sorted[tracker->count / 2];
    if (!tracker->initialized) {
        add_candidate(tracker, median_angle, now_ms);
        if (tracker->candidate_hits < AUDIO_TRACK_MIN_HITS) {
            return false;
        }
        tracker->filtered_angle = tracker->candidate_angle;
        tracker->initialized = true;
        tracker->candidate_hits = 0;
    } else {
        if (angles_within(median_angle, tracker->filtered_angle,
                          AUDIO_TRACK_HOLD_BAND_DEG)) {
            tracker->filtered_angle +=
                AUDIO_TRACK_ALPHA * (median_angle - tracker->filtered_angle);
            tracker->candidate_hits = 0;
        } else {
            add_candidate(tracker, median_angle, now_ms);
            size_t required_hits = AUDIO_TRACK_SWITCH_HITS;
            bool returning_to_center =
                (tracker->filtered_angle > AUDIO_TRACK_HOLD_BAND_DEG ||
                 tracker->filtered_angle < -AUDIO_TRACK_HOLD_BAND_DEG) &&
                angles_within(median_angle, 0.0f,
                              AUDIO_TRACK_CENTER_DEADBAND_DEG);
            if (returning_to_center) {
                required_hits = AUDIO_TRACK_CENTER_HITS;
            }
            if (tracker->candidate_hits < required_hits) {
                return false;
            }
            tracker->filtered_angle = tracker->candidate_angle;
            tracker->candidate_hits = 0;
        }
    }

    *filtered_angle = tracker->filtered_angle;
    return true;
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    servo_init();
    ultrasonic_init();
    display_init(DISPLAY_ROTATION_0);
    display_fill_screen(COLOR_RED);
    sleep_ms(500);
    display_fill_screen(COLOR_GREEN);
    sleep_ms(500);
    display_fill_screen(COLOR_BLUE);
    sleep_ms(500);
    display_fill_screen(COLOR_BLACK);

    if (!microphone_init()) {
        printf("ERROR: microphone_init() failed!\n");
        while (true) tight_loop_contents();
    }

    static int32_t left_buf[MIC_TEST_FRAMES];
    static int32_t right_buf[MIC_TEST_FRAMES];
    float angle = 0.0f;
    float filtered_angle = 0.0f;
    float distance_cm = -1.0f;
    float display_direction_deg = 0.0f;
    uint64_t last_sound_ms = 0;
    absolute_time_t next_distance_read = get_absolute_time();
    static angle_tracker_t tracker;

    printf("Sound tracking ready.\n");

    while (true) {
        if (time_reached(next_distance_read)) {
            distance_cm = ultrasonic_read_cm();
            next_distance_read = make_timeout_time_ms(250);
        }

        uint64_t now_ms = to_ms_since_boot(get_absolute_time());
        bool person_present =
            distance_cm >= DISPLAY_PERSON_MIN_CM &&
            distance_cm <= DISPLAY_PERSON_MAX_CM;
        bool sound_detected =
            person_present &&
            last_sound_ms != 0 &&
            now_ms - last_sound_ms <= DISPLAY_SOUND_HOLD_MS;
        display_show_tracking(person_present, sound_detected,
                              distance_cm, display_direction_deg);

        if (!microphone_read_samples(left_buf, right_buf, MIC_TEST_FRAMES)) {
            printf("ERROR: microphone sample capture failed.\n");
        } else if (audio_estimate_angle(left_buf, right_buf, MIC_TEST_FRAMES, &angle) &&
                   update_angle_tracker(&tracker, angle, &filtered_angle)) {
            last_sound_ms = to_ms_since_boot(get_absolute_time());
            display_direction_deg = -filtered_angle;
            float target_angle =
                ((SERVO_MIN_ANGLE + SERVO_MAX_ANGLE) * 0.5f) +
                (-filtered_angle * AUDIO_TO_SERVO_GAIN);
            if (target_angle < SERVO_TRACK_MIN_ANGLE) {
                target_angle = SERVO_TRACK_MIN_ANGLE;
            } else if (target_angle > SERVO_TRACK_MAX_ANGLE) {
                target_angle = SERVO_TRACK_MAX_ANGLE;
            }

            float current_angle = servo_get_angle();
            float step = target_angle - current_angle;
            if (step > SERVO_TRACK_MAX_STEP) {
                target_angle = current_angle + SERVO_TRACK_MAX_STEP;
            } else if (step < -SERVO_TRACK_MAX_STEP) {
                target_angle = current_angle - SERVO_TRACK_MAX_STEP;
            }

            if (target_angle != current_angle) {
                servo_set_angle(target_angle);
                printf("Sound: %+6.1f deg filtered, servo: %.1f deg, distance: %.1f cm\n",
                       (double)filtered_angle,
                       (double)servo_get_angle(),
                       (double)distance_cm);
            }
        }
        now_ms = to_ms_since_boot(get_absolute_time());
        sound_detected =
            person_present &&
            last_sound_ms != 0 &&
            now_ms - last_sound_ms <= DISPLAY_SOUND_HOLD_MS;
        display_show_tracking(person_present, sound_detected,
                              distance_cm, display_direction_deg);
        sleep_ms(10);
    }
}