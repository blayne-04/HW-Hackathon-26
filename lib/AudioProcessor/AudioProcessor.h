#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define AUDIO_SAMPLE_RATE  16000
#define AUDIO_FFT_SAMPLES  256

/* ---------- I2S audio ---------- */
void audio_init(void);
int  audio_read_samples(int16_t *buf, int n);

/* ---------- FFT (in-place, length n must be power of 2) ---------- */
void fft_hamming_window(double *data, int n);
void fft_compute(double *vReal, double *vImag, int n);
void fft_complex_to_magnitude(double *vReal, double *vImag, int n);

/* ---------- Spectral analysis ---------- */
float fft_dominant_frequency(const double *magnitudes, int n, float *amplitude_out);
float fft_band_energy(const double *magnitudes, int n, float freq_lo, float freq_hi);
float fft_total_energy(const double *magnitudes, int n);
void  fft_fill_bar_magnitudes(const double *magnitudes, int n,
                              float *bars, int num_bars);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PROCESSOR_H */