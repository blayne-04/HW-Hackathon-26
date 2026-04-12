#ifndef CONSTANTS_H
#define CONSTANTS_H

<<<<<<< HEAD
// === Blynk & WiFi ===
#define BLYNK_TEMPLATE_ID "YourTemplateID"
#define BLYNK_TEMPLATE_NAME "SoundClassifier"
#define BLYNK_AUTH_TOKEN "YourAuthToken"
#define VIRTUAL_PIN_NOTIF V1
#define VIRTUAL_PIN_MUTE V2

// === I2S MIC (Pins 8, 9, 10) ===
#define I2S_WS 26
#define I2S_SCK 25
#define I2S_SD 33

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
=======
// === Blynk credentials ===
#define BLYNK_TEMPLATE_ID   "TMPL26PleId9c"
#define BLYNK_TEMPLATE_NAME "LED Text"
#define BLYNK_AUTH_TOKEN    "sVOI8SxxY5HdtJzS-j2bUYnA8ycil2Ms"

// === Blynk virtual pins ===
#define VPIN_NOTIFICATION   V1
#define VPIN_MUTE           V2
#define VPIN_LED_TEXT       V3
#define VPIN_ALERT_STATE    V4
#define VPIN_STATUS         V5
#define VPIN_LED_STRIP      V6

// === I2S microphone (Physical: WS=Pin9, SCK=Pin10, SD=Pin8) ===
#define PIN_I2S_WS   25
#define PIN_I2S_SCK  26
#define PIN_I2S_SD   33

// === SPI display (Physical: SCL=Pin30, SDA=Pin37, CS=Pin29, DC=Pin27, RES=Pin26) ===
#define PIN_TFT_SCLK 18
#define PIN_TFT_MOSI 23
#define PIN_TFT_CS   5
#define PIN_TFT_DC   16
#define PIN_TFT_RST  4

// === LED strip ===
#define LED_DATA_PIN        15
#define NUM_LEDS            30
#define LED_BRIGHTNESS      255
#define LED_ALERT_DURATION  2000
>>>>>>> Refactor

// === Relay outputs ===
#define RELAY_SMOKE         32
#define RELAY_DOORBELL      27

// === Audio / FFT ===
#define SAMPLE_RATE     16000
#define FFT_SAMPLES     256
#define BUFFER_SIZE     512

// === Frequency band edges (Hz) ===
#define DOORBELL_FREQ_MIN   800
#define DOORBELL_FREQ_MAX   1500
#define SMOKE_FREQ_MIN      2800
#define SMOKE_FREQ_MAX      3500

// === Classification thresholds ===
#define MIN_TOTAL_ENERGY    5000.0f
#define DOORBELL_RATIO      0.35f
#define SMOKE_RATIO         0.45f

// === Timing ===
#define COOLDOWN_MS             8000
#define HEARTBEAT_INTERVAL      10000
#define DISPLAY_ALERT_DURATION  3000

#endif /* CONSTANTS_H */
