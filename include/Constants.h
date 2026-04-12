#ifndef CONSTANTS_H
#define CONSTANTS_H

// === Blynk Credentials ===
#define BLYNK_TEMPLATE_ID "TMPL26PleId9c"
#define BLYNK_TEMPLATE_NAME "LED Text"
#define BLYNK_AUTH_TOKEN "sVOI8SxxY5HdtJzS-j2bUYnA8ycil2Ms"

// === Blynk Virtual Pins ===
#define VPIN_NOTIFICATION V1
#define VPIN_MUTE V2
#define VPIN_LED_TEXT V3
#define VPIN_ALERT_STATE V4
#define VPIN_STATUS V5
#define VPIN_LED_STRIP V6

// === Hardware Pinout (NodeMCU-32S) ===
#define PIN_I2S_WS 25
#define PIN_I2S_SCK 26
#define PIN_I2S_SD 33

#define PIN_TFT_SCLK 18
#define PIN_TFT_MOSI 23
#define PIN_TFT_CS 5
#define PIN_TFT_DC 16
#define PIN_TFT_RST 4

#define LED_DATA_PIN 15

// === Audio DSP Config ===
#define SAMPLE_RATE 16000
#define FFT_SIZE 1024   // 64ms window
#define DC_ALPHA 0.995f // Leaky integrator constant

#endif