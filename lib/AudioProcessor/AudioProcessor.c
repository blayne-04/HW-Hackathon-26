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

static float fft_buffer[FFT_SIZE * 2] __attribute__((aligned(16)));
static float window[FFT_SIZE];
static int32_t raw_i2s_samples[FFT_SIZE]; 

/* =================================================================
 * I2S Initialization
 * ================================================================= */
void audio_init(void) {
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // Alignment for INMP441
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S),
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

    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);

    // Initialize esp-dsp library tables
    dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    dsps_wind_blackman_harris_f32(window, FFT_SIZE);
}

/* =================================================================
 * ML Substrate: Cleaning & Normalization
 * ================================================================= */
int audio_read_and_clean(float *output_buf, int n) {
    size_t bytes_read = 0;
    
    // Read from I2S into our static data-segment buffer
    i2s_read(I2S_NUM_0, raw_i2s_samples, sizeof(raw_i2s_samples), &bytes_read, portMAX_DELAY);
    int count = bytes_read / sizeof(int32_t);

    float max_peak = 0.01f;

    for (int i = 0; i < count; i++) {
        // 1. Shift 24-bit data from the 32-bit I2S frame
        float sample = (float)(raw_i2s_samples[i] >> 8);

        // 2. DC Offset Removal (Leaky Integrator)
        float filtered = sample - last_input + (DC_ALPHA * last_output);
        last_input = sample;
        last_output = filtered;

        output_buf[i] = filtered;

        // Track peak for normalization
        float abs_val = fabsf(filtered);
        if (abs_val > max_peak) max_peak = abs_val;
    }

    // 3. Normalization to [-1.0, 1.0] 
    // Essential for ML model stability and consistency
    for (int i = 0; i < count; i++) {
        output_buf[i] /= max_peak;
    }

    return count;
}

/* =================================================================
 * Optimized FFT (esp-dsp)
 * ================================================================= */
void audio_process_fft(float *input_samples, float *magnitudes) {
    for (int i = 0; i < FFT_SIZE; i++) {
        fft_buffer[i * 2] = input_samples[i] * window[i]; // Real
        fft_buffer[i * 2 + 1] = 0;                       // Imaginary
    }

    // Assembly-optimized Radix-2 FFT
    dsps_fft2r_fc32(fft_buffer, FFT_SIZE);
    dsps_bit_rev_fc32(fft_buffer, FFT_SIZE);
    
    // Convert Complex results back to real magnitudes
    dsps_cplx2re_fc32(fft_buffer, FFT_SIZE);

    for (int i = 0; i < FFT_SIZE / 2; i++) {
        magnitudes[i] = fft_buffer[i];
    }
}

// Temporary buffer for the test plot
static float test_buffer[FFT_SIZE];

void audio_test_plotter(void) {
    // 1. Grab and clean the data
    int count = audio_read_and_clean(test_buffer, FFT_SIZE);

    // 2. Print a slice of the waveform for the Serial Plotter
    // We only print a small window (128 samples) to keep Serial stable
    for (int i = 0; i < 128; i++) {
        // Format: "Value1,Value2" allows for multi-trace plotting
        // We plot the sample and two reference lines at 1.0 and -1.0
        printf("%.2f,1.0,-1.0\n", test_buffer[i]);
    }