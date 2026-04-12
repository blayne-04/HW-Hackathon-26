#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "Constants.h"

typedef struct
{
    float total_energy;
    float energy_doorbell;
    float energy_smoke;
    float dominant_frequency_hz;
    float dominant_magnitude;
} AudioFeatures;

/* ---------- Verification & Debug ---------- */
/**
 * @brief Reads a block and prints it to Serial in a format 
 * compatible with the Arduino Serial Plotter.
 */
void audio_test_plotter(void);

/* ---------- Lifecycle & I2S ---------- */
void audio_init(void);

/**
 * @brief Reads samples, removes DC bias, and normalizes to [-1.0, 1.0].
 */
int audio_read_and_clean(float *output_buf, int n);

/* ---------- DSP Functions ---------- */
void audio_process_fft(const float *input_samples, float *magnitudes);
void audio_extract_features(const float *magnitudes, AudioFeatures *features);
void audio_compute_spectrum_bars(const float *magnitudes,
                                 float *bar_magnitudes,
                                 int bar_count);

#ifdef __cplusplus
}
#endif

#endif
