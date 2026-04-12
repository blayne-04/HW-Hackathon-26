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

/* Bit-reversal permutation */
static void bit_reverse(double *re, double *im, int n)
{
    int i, j, bit;
    double tmp;

    for (i = 1, j = 0; i < n; i++)
    {
        bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
        {
            tmp = re[i];
            re[i] = re[j];
            re[j] = tmp;
            tmp = im[i];
            im[i] = im[j];
            im[j] = tmp;
        }
    }
}

/* Apply a Hamming window to data[] in place */
void fft_hamming_window(double *data, int n)
{
    int i;
    for (i = 0; i < n; i++)
    {
        double w = 0.54 - 0.46 * cos(2.0 * M_PI * i / (n - 1));
        data[i] *= w;
    }
}

/* Cooley-Tukey radix-2 DIT FFT (in-place).
   vReal[] and vImag[] must both have length n (power of 2). */
void fft_compute(double *vReal, double *vImag, int n)
{
    int len, i, j;

    bit_reverse(vReal, vImag, n);

    for (len = 2; len <= n; len <<= 1)
    {
        double ang = -2.0 * M_PI / len;
        double wRe = cos(ang);
        double wIm = sin(ang);
        int half = len >> 1;

        for (i = 0; i < n; i += len)
        {
            double curRe = 1.0, curIm = 0.0;
            for (j = 0; j < half; j++)
            {
                int u = i + j;
                int v = u + half;
                double uRe = vReal[u], uIm = vImag[u];
                double tRe = vReal[v] * curRe - vImag[v] * curIm;
                double tIm = vReal[v] * curIm + vImag[v] * curRe;
                vReal[u] = uRe + tRe;
                vImag[u] = uIm + tIm;
                vReal[v] = uRe - tRe;
                vImag[v] = uIm - tIm;

                double nRe = curRe * wRe - curIm * wIm;
                double nIm = curRe * wIm + curIm * wRe;
                curRe = nRe;
                curIm = nIm;
            }
        }
    }
}

/* Replace vReal[i] with |vReal[i] + j*vImag[i]|; zero vImag[]. */
void fft_complex_to_magnitude(double *vReal, double *vImag, int n)
{
    int i;
    for (i = 0; i < n; i++)
    {
        vReal[i] = sqrt(vReal[i] * vReal[i] + vImag[i] * vImag[i]);
        vImag[i] = 0.0;
    }
}

/* =================================================================
 * Spectral analysis
 * ================================================================= */

float fft_dominant_frequency(const double *magnitudes, int n, float *amplitude_out)
{
    double peak = 0.0;
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
        *amplitude_out = (float)peak;

    return (float)peak_idx * ((float)AUDIO_SAMPLE_RATE / (float)n);
}

float fft_band_energy(const double *magnitudes, int n, float freq_lo, float freq_hi)
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

float fft_total_energy(const double *magnitudes, int n)
{
    float energy = 0.0f;
    int half = n / 2;
    int i;

    for (i = 1; i < half; i++)
        energy += (float)magnitudes[i];

    return energy;
}

void fft_fill_bar_magnitudes(const double *magnitudes, int n,
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