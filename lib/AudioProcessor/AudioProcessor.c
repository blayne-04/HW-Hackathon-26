#include "AudioProcessor.h"

#include <math.h>
#include <stdio.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>

/*
 * Simple microphone pipeline (pure C style):
 * 1) Init I2S
 * 2) Read raw samples
 * 3) Remove DC offset and normalize
 *
 * The "FFT"-named APIs are kept for compatibility with existing callers,
 * but implemented as lightweight coarse indicators instead of full DSP.
 */

static int32_t s_raw[FFT_SIZE];
static float s_last_frame[FFT_SIZE];
static float s_dc_estimate = 0.0f;
static int s_initialized = 0;

void audio_init(void)
{
    if (s_initialized)
        return;

    i2s_comm_format_t comm_fmt;
#if defined(I2S_COMM_FORMAT_STAND_I2S)
    comm_fmt = I2S_COMM_FORMAT_STAND_I2S;
#else
    comm_fmt = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S);
#endif

    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = comm_fmt,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false
    };

    i2s_pin_config_t pins = {
        .bck_io_num = PIN_I2S_SCK,
        .ws_io_num = PIN_I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_SD
    };

    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK)
        return;
    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK)
        return;

    s_initialized = 1;
}

int audio_read_and_clean(float *output_buf, int n)
{
    size_t bytes_read = 0;
    int i;
    int count;
    int limit;
    float peak = 0.01f;

    if (!s_initialized || !output_buf || n <= 0)
        return 0;

    limit = (n < FFT_SIZE) ? n : FFT_SIZE;

    if (i2s_read(I2S_NUM_0, s_raw, sizeof(s_raw), &bytes_read, portMAX_DELAY) != ESP_OK)
        return 0;

    count = (int)(bytes_read / sizeof(int32_t));
    if (count > limit)
        count = limit;
    if (count <= 0)
        return 0;

    for (i = 0; i < count; i++) {
        /* INMP441 style 24-bit audio in 32-bit frame */
        float sample = (float)(s_raw[i] >> 8);

        /* Very simple DC tracker */
        s_dc_estimate = (DC_ALPHA * s_dc_estimate) + ((1.0f - DC_ALPHA) * sample);
        sample = sample - s_dc_estimate;

        output_buf[i] = sample;
        s_last_frame[i] = sample;

        if (fabsf(sample) > peak)
            peak = fabsf(sample);
    }

    /* Normalize to roughly [-1, 1] */
    for (i = 0; i < count; i++) {
        output_buf[i] /= peak;
        s_last_frame[i] = output_buf[i];
    }

    return count;
}

void audio_process_fft(const float *input_samples, float *magnitudes)
{
    int i;

    if (!input_samples || !magnitudes)
        return;

    /*
     * Lightweight placeholder for "spectrum" indication:
     * each bin is abs-average of two neighboring samples.
     */
    for (i = 0; i < FFT_BIN_COUNT; i++) {
        int idx = i * 2;
        float a = fabsf(input_samples[idx]);
        float b = fabsf(input_samples[idx + 1]);
        magnitudes[i] = 0.5f * (a + b);
    }

    if (FFT_BIN_COUNT > 0)
        magnitudes[0] = 0.0f;
}

void audio_extract_features(const float *magnitudes, AudioFeatures *features)
{
    int i;
    float bin_hz;
    float best = 0.0f;
    int best_idx = 0;

    if (!magnitudes || !features)
        return;

    features->total_energy = 0.0f;
    features->energy_doorbell = 0.0f;
    features->energy_smoke = 0.0f;
    features->dominant_frequency_hz = 0.0f;
    features->dominant_magnitude = 0.0f;

    bin_hz = (float)SAMPLE_RATE / (float)FFT_SIZE;

    for (i = 1; i < FFT_BIN_COUNT; i++) {
        float freq = (float)i * bin_hz;
        float mag = magnitudes[i];
        float power = mag * mag;

        features->total_energy += power;

        if (power > best) {
            best = power;
            best_idx = i;
            features->dominant_magnitude = mag;
        }

        if (freq >= DOORBELL_FREQ_MIN && freq <= DOORBELL_FREQ_MAX)
            features->energy_doorbell += power;
        if (freq >= SMOKE_FREQ_MIN && freq <= SMOKE_FREQ_MAX)
            features->energy_smoke += power;
    }

    features->dominant_frequency_hz = (float)best_idx * bin_hz;
}

void audio_compute_spectrum_bars(const float *magnitudes,
                                 float *bar_magnitudes,
                                 int bar_count)
{
    int bar;
    int bins_per_bar;

    if (!magnitudes || !bar_magnitudes || bar_count <= 0)
        return;

    bins_per_bar = FFT_BIN_COUNT / bar_count;
    if (bins_per_bar < 1)
        bins_per_bar = 1;

    for (bar = 0; bar < bar_count; bar++) {
        int start = bar * bins_per_bar;
        int end = start + bins_per_bar;
        int j;
        float sum = 0.0f;

        if (end > FFT_BIN_COUNT)
            end = FFT_BIN_COUNT;

        for (j = start; j < end; j++)
            sum += magnitudes[j];

        if (end > start)
            bar_magnitudes[bar] = sum / (float)(end - start);
        else
            bar_magnitudes[bar] = 0.0f;
    }
}

void audio_print_waveform(void)
{
    float frame[FFT_SIZE];
    int n = audio_read_and_clean(frame, FFT_SIZE);
    int i;
    int print_n = (n < 128) ? n : 128;

    for (i = 0; i < print_n; i++)
        printf("%.3f,1.0,-1.0\n", frame[i]);
}

void audio_print_spectrum(int top_n)
{
    float mags[FFT_BIN_COUNT];
    int i;
    float frame[FFT_SIZE];
    int n = audio_read_and_clean(frame, FFT_SIZE);

    if (top_n <= 0 || n < FFT_SIZE)
        return;

    if (top_n > 10)
        top_n = 10;

    audio_process_fft(frame, mags);

    printf("\n--- Coarse spectrum sample ---\n");
    for (i = 1; i <= top_n; i++) {
        int idx = (i * (FFT_BIN_COUNT - 1)) / top_n;
        float freq = ((float)idx * (float)SAMPLE_RATE) / (float)FFT_SIZE;
        printf("bin %3d  %5.0f Hz  mag=%.3f\n", idx, freq, mags[idx]);
    }
    printf("------------------------------\n");
}
