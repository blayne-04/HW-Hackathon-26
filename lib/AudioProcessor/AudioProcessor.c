#include "AudioProcessor.h"
#include <math.h>
#include <stdio.h>
#include <driver/i2s.h>
#include "dsps_fft2r.h"
#include "dsps_wind_blackman_harris.h"

/* * STATIC MEMORY ALLOCATION
 * Moved out of function scope to protect the FreeRTOS task stack.
 * 16-byte alignment is required for ESP32 SIMD (Assembly) optimizations.
 */
static float last_input = 0.0f;
static float last_output = 0.0f;
static int s_audio_initialized = 0;

static float fft_buffer[FFT_SIZE * 2] __attribute__((aligned(16)));
static float window[FFT_SIZE];
static int32_t raw_i2s_samples[FFT_SIZE];

/* =================================================================
 * I2S Initialization
 * ================================================================= */
void audio_init(void)
{
    if (s_audio_initialized)
        return;

    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // Alignment for INMP441
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false};

    i2s_pin_config_t pins = {
        .bck_io_num = PIN_I2S_SCK,
        .ws_io_num = PIN_I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_SD};

    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK)
        return;
    if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK)
        return;

    // Initialize esp-dsp library tables
    dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    dsps_wind_blackman_harris_f32(window, FFT_SIZE);
    s_audio_initialized = 1;
}

/* =================================================================
 * ML Substrate: Cleaning & Normalization
 * ================================================================= */
int audio_read_and_clean(float *output_buf, int n)
{
    int capacity;
    size_t bytes_read = 0;
    int count;
    float max_peak = 0.01f;

    if (output_buf == NULL || n <= 0 || !s_audio_initialized)
        return 0;

    capacity = n;
    if (capacity > FFT_SIZE)
        capacity = FFT_SIZE;

    // Read from I2S into our static data-segment buffer
    if (i2s_read(I2S_NUM_0, raw_i2s_samples, sizeof(raw_i2s_samples), &bytes_read, portMAX_DELAY) != ESP_OK)
        return 0;
    count = (int)(bytes_read / sizeof(int32_t));
    if (count > capacity)
        count = capacity;
    if (count <= 0)
        return 0;

    for (int i = 0; i < count; i++)
    {
        // 1. Shift 24-bit data from the 32-bit I2S frame
        float sample = (float)(raw_i2s_samples[i] >> 8);

        // 2. DC Offset Removal (Leaky Integrator)
        float filtered = sample - last_input + (DC_ALPHA * last_output);
        last_input = sample;
        last_output = filtered;

        output_buf[i] = filtered;

        // Track peak for normalization
        float abs_val = fabsf(filtered);
        if (abs_val > max_peak)
            max_peak = abs_val;
    }

    // 3. Normalization to [-1.0, 1.0]
    // Essential for ML model stability and consistency
    for (int i = 0; i < count; i++)
    {
        output_buf[i] /= max_peak;
    }

    return count;
}

/* =================================================================
 * Optimized FFT (esp-dsp)
 * ================================================================= */
void audio_process_fft(const float *input_samples, float *magnitudes)
{
    if (input_samples == NULL || magnitudes == NULL)
        return;

    for (int i = 0; i < FFT_SIZE; i++)
    {
        fft_buffer[i * 2] = input_samples[i] * window[i]; // Real
        fft_buffer[i * 2 + 1] = 0;                        // Imaginary
    }

    // Assembly-optimized Radix-2 FFT
    dsps_fft2r_fc32(fft_buffer, FFT_SIZE);
    dsps_bit_rev_fc32(fft_buffer, FFT_SIZE);

    for (int i = 0; i < FFT_BIN_COUNT; i++)
    {
        float real = fft_buffer[i * 2];
        float imag = fft_buffer[i * 2 + 1];
        magnitudes[i] = sqrtf((real * real) + (imag * imag));
    }

    magnitudes[0] = 0.0f;
}

void audio_extract_features(const float *magnitudes, AudioFeatures *features)
{
    float dominant_power = 0.0f;
    float bin_hz = (float)SAMPLE_RATE / (float)FFT_SIZE;

    if (magnitudes == NULL || features == NULL)
        return;

    features->total_energy = 0.0f;
    features->energy_doorbell = 0.0f;
    features->energy_smoke = 0.0f;
    features->dominant_frequency_hz = 0.0f;
    features->dominant_magnitude = 0.0f;

    for (int i = 1; i < FFT_BIN_COUNT; i++)
    {
        float freq = (float)i * bin_hz;
        float magnitude = magnitudes[i];
        float power = magnitude * magnitude;

        features->total_energy += power;

        if (power > dominant_power)
        {
            dominant_power = power;
            features->dominant_frequency_hz = freq;
            features->dominant_magnitude = magnitude;
        }

        if (freq >= DOORBELL_FREQ_MIN && freq <= DOORBELL_FREQ_MAX)
            features->energy_doorbell += power;

        if (freq >= SMOKE_FREQ_MIN && freq <= SMOKE_FREQ_MAX)
            features->energy_smoke += power;
    }
}

void audio_compute_spectrum_bars(const float *magnitudes,
                                 float *bar_magnitudes,
                                 int bar_count)
{
    int usable_bins;
    int bins_per_bar;

    if (magnitudes == NULL || bar_magnitudes == NULL || bar_count <= 0)
        return;

    usable_bins = FFT_BIN_COUNT - 1;
    bins_per_bar = usable_bins / bar_count;
    if (bins_per_bar <= 0)
        bins_per_bar = 1;

    for (int bar = 0; bar < bar_count; bar++)
    {
        float sum = 0.0f;
        int start = 1 + (bar * bins_per_bar);
        int end = start + bins_per_bar;

        if (bar == bar_count - 1 || end > FFT_BIN_COUNT)
            end = FFT_BIN_COUNT;

        for (int bin = start; bin < end; bin++)
            sum += magnitudes[bin];

        bar_magnitudes[bar] = sum / (float)(end - start);
    }
}

// Temporary buffer for the test plot
static float test_buffer[FFT_SIZE];

void audio_test_plotter(void)
{
    // 1. Grab and clean the data
    int count = audio_read_and_clean(test_buffer, FFT_SIZE);

    // 2. Print a slice of the waveform for the Serial Plotter
    // We only print a small window (128 samples) to keep Serial stable
    for (int i = 0; i < 128 && i < count; i++)
    {
        // Format: "Value1,Value2" allows for multi-trace plotting
        // We plot the sample and two reference lines at 1.0 and -1.0
        printf("%.2f,1.0,-1.0\n", test_buffer[i]);
    }
}
