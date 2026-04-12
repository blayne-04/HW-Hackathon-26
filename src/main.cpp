/* I2S microphone — FFT frequency-band display
 * Pins: WS=GPIO25  SCK=GPIO26  SD=GPIO33
 * Captures 1024 samples @ 16 kHz, runs Hamming-windowed FFT,
 * and prints named frequency bands as ASCII energy bars.
 */
#include <Arduino.h>
#include <math.h>
#include "AudioProcessor.h"

/* bin resolution = AUDIO_SAMPLE_RATE / AUDIO_FFT_SAMPLES = 15.625 Hz */
#define BIN_HZ  ((float)AUDIO_SAMPLE_RATE / (float)AUDIO_FFT_SAMPLES)

/* ------------------------------------------------------------------ */
/*  Named frequency bands                                              */
/* ------------------------------------------------------------------ */
struct Band { const char *name; float lo; float hi; };

static const Band BANDS[] = {
    { "Sub-bass  ",   20.f,   60.f },
    { "Bass      ",   60.f,  250.f },
    { "Low-mid   ",  250.f,  500.f },
    { "Mid       ",  500.f, 2000.f },
    { "High-mid  ", 2000.f, 4000.f },
    { "Presence  ", 4000.f, 6000.f },
    { "Brilliance", 6000.f, 8000.f },
};
#define NUM_BANDS  (int)(sizeof(BANDS) / sizeof(BANDS[0]))

/* Static — keeps ~8 KB of float buffers off the stack */
static int16_t s_buf[AUDIO_FFT_SAMPLES];
static float   s_re[AUDIO_FFT_SAMPLES];
static float   s_im[AUDIO_FFT_SAMPLES];

/* ------------------------------------------------------------------ */
static void print_bar(float value, float max_val)
{
    const int BAR_WIDTH = 20;
    int filled = (max_val > 0.f) ? (int)(value / max_val * BAR_WIDTH) : 0;
    if (filled > BAR_WIDTH) filled = BAR_WIDTH;
    Serial.print('[');
    for (int i = 0; i < BAR_WIDTH; i++)
        Serial.print(i < filled ? '#' : ' ');
    Serial.print(']');
}

/* ------------------------------------------------------------------ */
void setup()
{
    Serial.begin(115200);
    while (!Serial) {}
    Serial.println("\n=== Frequency Band Monitor ===");
    Serial.printf("Sample rate : %d Hz\n", AUDIO_SAMPLE_RATE);
    Serial.printf("FFT size    : %d pts  (%.2f Hz/bin)\n",
                  AUDIO_FFT_SAMPLES, BIN_HZ);
    Serial.println("------------------------------");
    audio_init();
    Serial.println("I2S ready.\n");
}

void loop()
{
    /* 1. Capture */
    int n = audio_read_samples(s_buf, AUDIO_FFT_SAMPLES);
    if (n <= 0) { Serial.println("[WARN] no samples"); return; }

    /* 2. Convert int16 → float, accumulate RMS, zero imaginary part */
    int64_t sum_sq = 0;
    for (int i = 0; i < n; i++) {
        int32_t s = s_buf[i];
        sum_sq  += s * s;
        s_re[i]  = (float)s;
        s_im[i]  = 0.f;
    }
    int rms = (int)sqrtf((float)(sum_sq / n));

    /* 3. Hamming window → FFT → magnitude spectrum */
    fft_hamming_window(s_re, n);
    fft_compute(s_re, s_im, n);
    fft_complex_to_magnitude(s_re, s_im, n);

    /* 4. Sum energy per band; track peak for bar scaling */
    float band_e[NUM_BANDS];
    float max_e = 1.f;
    for (int b = 0; b < NUM_BANDS; b++) {
        band_e[b] = fft_band_energy(s_re, n, BANDS[b].lo, BANDS[b].hi);
        if (band_e[b] > max_e) max_e = band_e[b];
    }

    /* 5. Display */
    Serial.printf("RMS=%d\n", rms);
    for (int b = 0; b < NUM_BANDS; b++) {
        Serial.printf("%-10s [%4.0f-%4.0f Hz] ",
                      BANDS[b].name, BANDS[b].lo, BANDS[b].hi);
        print_bar(band_e[b], max_e);
        Serial.printf(" %.1f\n", band_e[b]);
    }
    Serial.println();

    /* Human-readable refresh rate */
    delay(1000);
}
