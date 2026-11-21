#include "audio_processing.h"
#include "config.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

bool audio_estimate_angle(const int32_t *left_buf,
                          const int32_t *right_buf,
                          size_t count,
                          float *angle_deg)
{
    if (!left_buf || !right_buf || !angle_deg || count < 64) return false;

    // 1. Check Peak-to-Peak on BOTH channels (require both mics to hear the event)
    int32_t min_l = left_buf[0], max_l = left_buf[0];
    int32_t min_r = right_buf[0], max_r = right_buf[0];

    for (size_t i = 1; i < count; ++i) {
        if (left_buf[i] < min_l) min_l = left_buf[i];
        if (left_buf[i] > max_l) max_l = left_buf[i];
        if (right_buf[i] < min_r) min_r = right_buf[i];
        if (right_buf[i] > max_r) max_r = right_buf[i];
    }

    uint32_t p2p_l = (uint32_t)(max_l - min_l);
    uint32_t p2p_r = (uint32_t)(max_r - min_r);
    if (p2p_l < NOISE_GATE_P2P || p2p_r < NOISE_GATE_P2P) {
        return false;
    }

    // 2. First-difference high-pass filter: removes DC offset AND low-frequency room rumble
    static int32_t diff_l[MIC_TEST_FRAMES];
    static int32_t diff_r[MIC_TEST_FRAMES];
    diff_l[0] = 0;
    diff_r[0] = 0;

    int64_t energy_l = 0;
    int64_t energy_r = 0;
    for (size_t i = 1; i < count; ++i) {
        diff_l[i] = (left_buf[i] - left_buf[i - 1]) >> 4;
        diff_r[i] = (right_buf[i] - right_buf[i - 1]) >> 4;
        energy_l += (int64_t)diff_l[i] * diff_l[i];
        energy_r += (int64_t)diff_r[i] * diff_r[i];
    }

    if (energy_l == 0 || energy_r == 0) return false;

    // 3. Cross-correlate filtered transient edges across [-max_lag, +max_lag]
    int max_lag = (int)ceilf((MIC_SPACING_M * (float)MIC_SAMPLE_RATE) / SPEED_OF_SOUND_MPS);
    if (max_lag >= (int)(count / 2)) {
        max_lag = (int)(count / 2) - 1;
    }

    int best_lag = 0;
    int64_t best_corr = INT64_MIN;

    for (int lag = -max_lag; lag <= max_lag; ++lag) {
        int64_t corr = 0;
        size_t start = (lag < 0) ? (size_t)(-lag) + 1 : 1;
        size_t end   = (lag > 0) ? (count - (size_t)lag) : count;

        for (size_t i = start; i < end; ++i) {
            corr += (int64_t)diff_l[i] * (int64_t)diff_r[i + lag];
        }

        if (corr > best_corr) {
            best_corr = corr;
            best_lag = lag;
        }
    }

    // 4. Reject weakly correlated noise and reflections.
    double norm = sqrt((double)energy_l * (double)energy_r);
    double confidence = (double)best_corr / norm;
    if (confidence < MIN_CORRELATION_CONFIDENCE) {
        return false;
    }

    // 5. Convert sample lag to angle
    float sin_theta = ((float)best_lag * SPEED_OF_SOUND_MPS) /
                      ((float)MIC_SAMPLE_RATE * MIC_SPACING_M);

    if (sin_theta > 1.0f) sin_theta = 1.0f;
    if (sin_theta < -1.0f) sin_theta = -1.0f;

    *angle_deg = -asinf(sin_theta) * (180.0f / (float)M_PI);
    return true;
}