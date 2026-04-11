/* I2S microphone read demo
 * Pins: WS=GPIO25  SCK=GPIO26  SD=GPIO33
 * Reads 256 samples at 16 kHz and prints them to Serial.
 */
#include <Arduino.h>
#include "AudioProcessor.h"

#define PRINT_N_SAMPLES 32   /* how many raw values to print per read */

static int16_t buf[AUDIO_FFT_SAMPLES];

void setup()
{
    Serial.begin(115200);
    while (!Serial) { /* wait for USB-Serial on some boards */ }
    Serial.println("=== I2S Mic Demo ===");
    Serial.printf("Sample rate : %d Hz\n", AUDIO_SAMPLE_RATE);
    Serial.printf("Buffer size : %d samples\n", AUDIO_FFT_SAMPLES);
    Serial.println("WS=GPIO25  SCK=GPIO26  SD=GPIO33");
    Serial.println("--------------------");

    audio_init();
    Serial.println("I2S driver installed. Listening...\n");
}

void loop()
{
    int n = audio_read_samples(buf, AUDIO_FFT_SAMPLES);

    if (n <= 0) {
        Serial.println("[WARN] No samples read");
        return;
    }

    /* Compute RMS */
    int64_t sum_sq = 0;
    for (int i = 0; i < n; i++) {
        int32_t s = buf[i];
        sum_sq += s * s;
    }
    int rms = (int)sqrt((double)(sum_sq / n));

    /* Print RMS */
    Serial.printf("--- n=%d  RMS=%d ---\n", n, rms);

    /* Print first PRINT_N_SAMPLES raw values */
    int print_n = (n < PRINT_N_SAMPLES) ? n : PRINT_N_SAMPLES;
    for (int i = 0; i < print_n; i++) {
        Serial.printf("  [%3d] %6d\n", i, (int)buf[i]);
    }
    Serial.println();
}
