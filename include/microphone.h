#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int32_t minimum;
    int32_t maximum;
    uint32_t level;  // Peak-to-peak sample units, NOT decibels.
} microphone_result_t;

typedef struct {
    microphone_result_t left;
    microphone_result_t right;
} stereo_level_result_t;

bool microphone_init(void);
bool microphone_read_stereo_level(stereo_level_result_t *result);
bool microphone_read_samples(int32_t *left_buf, int32_t *right_buf, size_t count);

#endif