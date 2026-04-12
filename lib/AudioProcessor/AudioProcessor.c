#include "AudioProcessor.h"
#include <math.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include "../include/Constants.h"

#define AUDIO_RAW_SCRATCH_MAX 4096

static int32_t s_raw_scratch[AUDIO_RAW_SCRATCH_MAX];

/* =================================================================
 * I2S / Audio
 * ================================================================= */

void audio_init(void)
{
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = AUDIO_SAMPLE_RATE,             // 16000
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // MANDATORY for INMP441
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, // Bypasses ESP32 mono-bug
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,                          // Optimized buffer size
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pins = {
        .bck_io_num = PIN_I2S_SCK,   // 26
        .ws_io_num = PIN_I2S_WS,     // 25
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_SD    // 33
    };

    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

int audio_read_samples(int16_t *buf, int n)
{
    if (n <= 0 || n > AUDIO_RAW_SCRATCH_MAX) {
        return 0;
    }

    size_t bytes_read = 0;
    
    // Read the raw 32-bit data from the hardware DMA
    i2s_read(I2S_NUM_0, s_raw_scratch,
             (size_t)(n * sizeof(int32_t)), 
             &bytes_read, portMAX_DELAY);
             
    int samples_read = bytes_read / sizeof(int32_t);

    // Shift the 32-bit data down to perfectly clean 16-bit PCM for the ML model
    for (int i = 0; i < samples_read; i++) {
        buf[i] = (int16_t)(s_raw_scratch[i] >> 16);
    }

    return samples_read;
}

/* =================================================================
 * FFT internals (Untouched - Retained for your custom DSP needs)
 * ================================================================= */

static void bit_reverse(float *re, float *im, int n) {
    int i, j, bit; float tmp;
    for (i = 1, j = 0; i < n; i++) {
        bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            tmp = re[i]; re[i] = re[j]; re[j] = tmp;
            tmp = im[i]; im[i] = im[j]; im[j] = tmp;
        }
    }
}

static float s_hamming_lut[AUDIO_FFT_SAMPLES];
static int   s_hamming_lut_n = 0;

void fft_hamming_window(float *data, int n) {
    int i;
    if (n != s_hamming_lut_n) {
        float inv = 1.0f / (float)(n - 1);
        s_hamming_lut_n = n;
        for (i = 0; i < n; i++)
            s_hamming_lut[i] = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * (float)i * inv);
    }
    for (i = 0; i < n; i++) data[i] *= s_hamming_lut[i];
}

void fft_compute(float *vReal, float *vImag, int n) {
    int len, i, j;
    bit_reverse(vReal, vImag, n);
    for (len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * (float)M_PI / (float)len;
        float wRe = cosf(ang); float wIm = sinf(ang);
        int half = len >> 1;
        for (i = 0; i < n; i += len) {
            float curRe = 1.0f, curIm = 0.0f;
            for (j = 0; j < half; j++) {
                int u = i + j; int v = u + half;
                float uRe = vReal[u], uIm = vImag[u];
                float tRe = vReal[v] * curRe - vImag[v] * curIm;
                float tIm = vReal[v] * curIm + vImag[v] * curRe;
                vReal[u] = uRe + tRe; vImag[u] = uIm + tIm;
                vReal[v] = uRe - tRe; vImag[v] = uIm - tIm;
                float nRe = curRe * wRe - curIm * wIm;
                float nIm = curRe * wIm + curIm * wRe;
                curRe = nRe; curIm = nIm;
            }
        }
    }
}

void fft_complex_to_magnitude(float *vReal, float *vImag, int n) {
    int i;
    for (i = 0; i < n; i++) {
        vReal[i] = sqrtf(vReal[i] * vReal[i] + vImag[i] * vImag[i]);
        vImag[i] = 0.0f;
    }
}

float fft_dominant_frequency(const float *magnitudes, int n, float *amplitude_out) {
    float peak = 0.0f; int peak_idx = 1; int half = n / 2; int i;
    for (i = 1; i < half; i++) {
        if (magnitudes[i] > peak) { peak = magnitudes[i]; peak_idx = i; }
    }
    if (amplitude_out) *amplitude_out = peak;
    return (float)peak_idx * ((float)AUDIO_SAMPLE_RATE / (float)n);
}

float fft_band_energy(const float *magnitudes, int n, float freq_lo, float freq_hi) {
    float bin_width = (float)AUDIO_SAMPLE_RATE / (float)n;
    int lo_bin = (int)(freq_lo / bin_width); int hi_bin = (int)(freq_hi / bin_width);
    int half = n / 2; int i; float energy = 0.0f;
    if (lo_bin < 1) lo_bin = 1;
    if (hi_bin >= half) hi_bin = half - 1;
    for (i = lo_bin; i <= hi_bin; i++) energy += (float)magnitudes[i];
    return energy;
}

float fft_total_energy(const float *magnitudes, int n) {
    float energy = 0.0f; int half = n / 2; int i;
    for (i = 1; i < half; i++) energy += (float)magnitudes[i];
    return energy;
}

void fft_fill_bar_magnitudes(const float *magnitudes, int n, float *bars, int num_bars) {
    int bins_per_bar = (n / 2) / num_bars; int b, j;
    if (bins_per_bar < 1) bins_per_bar = 1;
    for (b = 0; b < num_bars; b++) {
        float sum = 0.0f; int start = b * bins_per_bar; int end = start + bins_per_bar;
        if (end > n / 2) end = n / 2;
        for (j = start; j < end; j++) sum += (float)magnitudes[j];
        bars[b] = sum / (float)bins_per_bar;
    }
}