#ifndef CONSTANTS_H
#define CONSTANTS_H

// === Blynk & WiFi ===
#define BLYNK_TEMPLATE_ID "YourTemplateID"
#define BLYNK_TEMPLATE_NAME "SoundClassifier"
#define BLYNK_AUTH_TOKEN "YourAuthToken"
#define VIRTUAL_PIN_NOTIF V1
#define VIRTUAL_PIN_MUTE V2

// === I2S MIC (Pins 8, 9, 10) ===
#define PIN_I2S_WS 25
#define PIN_I2S_SCK 26
#define PIN_I2S_SD 33

// === SPI Display (Pins 26, 27, 29, 30, 37) ===
#define TFT_SCLK 18
#define TFT_MOSI 23
#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 4

// === Peripherals ===
#define LED_DATA_PIN 15
#define NUM_LEDS 30

// === Audio/FFT Config ===
#define SAMPLE_RATE 16000
#define FFT_SAMPLES 256
#define SMOKE_RATIO 0.45
#define DOOR_RATIO 0.35

#endif