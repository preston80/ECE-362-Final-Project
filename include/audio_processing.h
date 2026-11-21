#ifndef AUDIO_PROCESSING_H
#define AUDIO_PROCESSING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MIC_SPACING_M        0.15494f  // 6.1 inches
#define SPEED_OF_SOUND_MPS   343.0f
#define NOISE_GATE_P2P       200000u
#define MIN_CORRELATION_CONFIDENCE 0.35

bool audio_estimate_angle(const int32_t *left_buf,
                          const int32_t *right_buf,
                          size_t count,
                          float *angle_deg);

#endif