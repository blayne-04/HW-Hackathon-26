#include "AudioProcessor.h"

#include <math.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include "../include/Constants.h"

/* =================================================================
 * I2S / Audio
 * ================================================================= */

void audio_init(void)
{
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = AUDIO_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

    i2s_pin_config_t pins = {
        .bck_io_num = PIN_I2S_SCK,
        .ws_io_num = PIN_I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_SD};

    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
}

int audio_read_samples(int16_t *buf, int n)
{
    size_t bytes_read = 0;
    i2s_read(I2S_NUM_0, buf,
             (size_t)(n * (int)sizeof(int16_t)),
             &bytes_read, portMAX_DELAY);
    return (int)(bytes_read / sizeof(int16_t));
}

/* =================================================================
 * FFT internals
 * ================================================================= */

/* Bit-reversal permutation — operates on float arrays */
static void bit_reverse(float *re, float *im, int n)
{
    int i, j, bit;
    float tmp;

    for (i = 1, j = 0; i < n; i++)
    {
        bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
        {
            tmp = re[i]; re[i] = re[j]; re[j] = tmp;
            tmp = im[i]; im[i] = im[j]; im[j] = tmp;
        }
    }
}

/* Precomputed Hamming-window LUT — rebuilt only when n changes.
 * Avoids AUDIO_FFT_SAMPLES cosf() calls every frame at run-time. */
static float s_hamming_lut[AUDIO_FFT_SAMPLES];
static int   s_hamming_lut_n = 0;

/* Apply a Hamming window to data[] in place */
void fft_hamming_window(float *data, int n)
{
    int i;
    if (n != s_hamming_lut_n)
    {
        float inv = 1.0f / (float)(n - 1);
        s_hamming_lut_n = n;
        for (i = 0; i < n; i++)
            s_hamming_lut[i] = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * (float)i * inv);
    }
    for (i = 0; i < n; i++)
        data[i] *= s_hamming_lut[i];
}

/* Cooley-Tukey radix-2 DIT FFT (in-place).
 * Uses float throughout — ESP32 Xtensa LX6 has a hardware single-
 * precision FPU; double falls back to ~8x slower software emulation.
 * Twiddle factors are computed once per stage (not in the inner loop),
 * then propagated via incremental complex multiplication. */
void fft_compute(float *vReal, float *vImag, int n)
{
    int len, i, j;

    bit_reverse(vReal, vImag, n);

    for (len = 2; len <= n; len <<= 1)
    {
        float ang = -2.0f * (float)M_PI / (float)len;
        float wRe = cosf(ang);
        float wIm = sinf(ang);
        int half = len >> 1;

        for (i = 0; i < n; i += len)
        {
            float curRe = 1.0f, curIm = 0.0f;
            for (j = 0; j < half; j++)
            {
                int u = i + j;
                int v = u + half;
                float uRe = vReal[u], uIm = vImag[u];
                float tRe = vReal[v] * curRe - vImag[v] * curIm;
                float tIm = vReal[v] * curIm + vImag[v] * curRe;
                vReal[u] = uRe + tRe;
                vImag[u] = uIm + tIm;
                vReal[v] = uRe - tRe;
                vImag[v] = uIm - tIm;

                float nRe = curRe * wRe - curIm * wIm;
                float nIm = curRe * wIm + curIm * wRe;
                curRe = nRe;
                curIm = nIm;
            }
        }
    }
}

/* Replace vReal[i] with |vReal[i] + j*vImag[i]|; zero vImag[].
 * sqrtf() maps to a single-precision FPU instruction on the ESP32. */
void fft_complex_to_magnitude(float *vReal, float *vImag, int n)
{
    int i;
    for (i = 0; i < n; i++)
    {
        vReal[i] = sqrtf(vReal[i] * vReal[i] + vImag[i] * vImag[i]);
        vImag[i] = 0.0f;
    }
}

/* =================================================================
 * Spectral analysis
 * ================================================================= */

float fft_dominant_frequency(const float *magnitudes, int n, float *amplitude_out)
{
    float peak = 0.0f;
    int peak_idx = 1;
    int half = n / 2;
    int i;

    for (i = 1; i < half; i++)
    {
        if (magnitudes[i] > peak)
        {
            peak = magnitudes[i];
            peak_idx = i;
        }
    }

    if (amplitude_out)
        *amplitude_out = peak;

    return (float)peak_idx * ((float)AUDIO_SAMPLE_RATE / (float)n);
}

float fft_band_energy(const float *magnitudes, int n, float freq_lo, float freq_hi)
{
    float bin_width = (float)AUDIO_SAMPLE_RATE / (float)n;
    int lo_bin = (int)(freq_lo / bin_width);
    int hi_bin = (int)(freq_hi / bin_width);
    int half = n / 2;
    int i;
    float energy = 0.0f;

    if (lo_bin < 1)
        lo_bin = 1;
    if (hi_bin >= half)
        hi_bin = half - 1;

    for (i = lo_bin; i <= hi_bin; i++)
        energy += (float)magnitudes[i];

    return energy;
}

float fft_total_energy(const float *magnitudes, int n)
{
    float energy = 0.0f;
    int half = n / 2;
    int i;

    for (i = 1; i < half; i++)
        energy += (float)magnitudes[i];

    return energy;
}

void fft_fill_bar_magnitudes(const float *magnitudes, int n,
                             float *bars, int num_bars)
{
    int bins_per_bar = (n / 2) / num_bars;
    int b, j;

    if (bins_per_bar < 1)
        bins_per_bar = 1;

    for (b = 0; b < num_bars; b++)
    {
        float sum = 0.0f;
        int start = b * bins_per_bar;
        int end = start + bins_per_bar;
        if (end > n / 2)
            end = n / 2;

        for (j = start; j < end; j++)
            sum += (float)magnitudes[j];

        bars[b] = sum / (float)bins_per_bar;
    }
}