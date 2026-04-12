#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "Constants.h"

/* ----------------------------------------------------------------
 * AudioFeatures — output of one processed frame
 * ---------------------------------------------------------------- */
typedef struct {
    float total_energy;           /* sum of power across all bins          */
    float energy_doorbell;        /* power in 800–1500 Hz band             */
    float energy_smoke;           /* power in 2800–3500 Hz band            */
    float dominant_frequency_hz;  /* frequency of highest-power bin        */
    float dominant_magnitude;     /* magnitude of highest-power bin        */
} AudioFeatures;

/* ----------------------------------------------------------------
 * Lifecycle
 * ---------------------------------------------------------------- */

/* Installs the I2S driver and pre-computes the window.
   Must be called before any other audio_* function.            */
void audio_init(void);

/* ----------------------------------------------------------------
 * Capture
 * ---------------------------------------------------------------- */

/* Read one FFT_SIZE block from the I2S mic, remove DC bias, and
   normalize to [-1.0, 1.0].  Returns the number of samples placed
   in output_buf (== FFT_SIZE on success, 0 on error).           */
int audio_read_and_clean(float *output_buf, int n);

/* ----------------------------------------------------------------
 * DSP
 * ---------------------------------------------------------------- */

/* Apply Blackman-Harris window + Cooley-Tukey FFT to input_samples
   (length FFT_SIZE).  Writes FFT_BIN_COUNT magnitude values into
   magnitudes[].                                                  */
void audio_process_fft(const float *input_samples, float *magnitudes);

/* Derive dominant frequency, total energy, and band energies from
   the magnitude spectrum.                                        */
void audio_extract_features(const float *magnitudes, AudioFeatures *features);

/* Condense FFT_BIN_COUNT bins into bar_count averaged bars for the
   spectrum display.                                              */
void audio_compute_spectrum_bars(const float *magnitudes,
                                 float       *bar_magnitudes,
                                 int          bar_count);

/* ----------------------------------------------------------------
 * Debug / Serial Plotter
 * ---------------------------------------------------------------- */

/* Capture one frame, print the waveform (128 samples) to Serial in
   Arduino Serial Plotter format: "sample,+1.0,-1.0"             */
void audio_print_waveform(void);

/* Capture one frame, run FFT, print the top-N peaks to Serial as:
   "freq_hz magnitude" — one line per peak, sorted by magnitude.  */
void audio_print_spectrum(int top_n);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PROCESSOR_H */
