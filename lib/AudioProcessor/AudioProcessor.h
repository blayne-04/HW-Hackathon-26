#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "Constants.h"

typedef enum {
    SOUND_CLASS_SILENCE,
    SOUND_CLASS_AMBIENT,
    SOUND_CLASS_SMOKE_ALARM,
    SOUND_CLASS_BABY_CRY,
    SOUND_CLASS_DOG_BARK,
    SOUND_CLASS_DOORBELL
} SoundClass;

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
 * Call this to get the data for your ML model.
 */
int audio_read_and_clean(float *output_buf, int n);

/* ---------- DSP Functions ---------- */
void audio_process_fft(float *input_samples, float *magnitudes);

#ifdef __cplusplus
}
#endif

#endif