#ifndef CONSTANTS_H
#define CONSTANTS_H

/*
 * Canonical core application configuration.
 *
 * Phase 2 focuses the firmware on:
 *   AudioProcessor -> FSM -> Display
 */

// === Hardware Pinout (NodeMCU-32S) ===
#define PIN_I2S_WS 25
#define PIN_I2S_SCK 26
#define PIN_I2S_SD 33

#define PIN_TFT_SCLK 18
#define PIN_TFT_MOSI 23
#define PIN_TFT_CS 5
#define PIN_TFT_DC 16
#define PIN_TFT_RST 4
#define PIN_TFT_BACKLIGHT 21

// === Audio DSP Config ===
#define SAMPLE_RATE 16000
#define FFT_SIZE 1024
#define FFT_BIN_COUNT (FFT_SIZE / 2)
#define DC_ALPHA 0.995f

// === Frequency Bands ===
#define DOORBELL_FREQ_MIN 800.0f
#define DOORBELL_FREQ_MAX 1500.0f
#define SMOKE_FREQ_MIN 2800.0f
#define SMOKE_FREQ_MAX 3500.0f

// === Classification Thresholds ===
#define MIN_TOTAL_ENERGY 5000.0f
#define DOORBELL_RATIO 0.35f
#define SMOKE_RATIO 0.45f

// === Alert Timing ===
#define ALERT_COOLDOWN_MS 8000UL
#define ALERT_SMOKE_DURATION_MS 2000UL
#define ALERT_DOORBELL_DURATION_MS 1200UL

// === Display Settings ===
#define DISP_NUM_BARS 12
#define DISPLAY_REFRESH_MS 80UL

#endif
