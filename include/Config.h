#ifndef CONFIG_H
#define CONFIG_H

#define BLYNK_TEMPLATE_ID "TMPL26PleId9c"
#define BLYNK_TEMPLATE_NAME "LED Text"
#define BLYNK_AUTH_TOKEN "sVOI8SxxY5HdtJzS-j2bUYnA8ycil2Ms"

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Blync Pins
#define VPIN_NOTIFICATION   V1
#define VPIN_MUTE           V2
#define VPIN_LED_TEXT       V3
#define VPIN_ALERT_STATE    V4
#define VPIN_STATUS         V5
#define VPIN_LED_STRIP      V6

// Hardware Pins *FIX TO ACTUAL BREADBOARD*
#define LED_DATA_PIN        15
#define RELAY_SMOKE         32
#define RELAY_DOORBELL      33
#define TFT_BACKLIGHT       21

// I2S Microphone Pins *FIX TO ACTUAL BREADBOARD*
#define I2S_WS              25
#define I2S_SCK             26
#define I2S_SD              33

// Led Config
#define NUM_LEDS            30
#define LED_ALERT_DURATION  2000    // 2 seconds
#define LED_BRIGHTNESS      255

// Audio Config
#define SAMPLE_RATE         16000
#define SAMPLES_FFT         256
#define BUFFER_SIZE         512

// Frequency Ranges
#define DOORBELL_FREQ_MIN   800
#define DOORBELL_FREQ_MAX   1500
#define SMOKE_FREQ_MIN      2800
#define SMOKE_FREQ_MAX      3500

// Classification Thresholds
#define MIN_TOTAL_ENERGY    5000.0
#define DOORBELL_RATIO      0.35
#define SMOKE_RATIO         0.45

// Sys config
#define COOLDOWN_MS         8000    // 8 seconds between alerts
#define HEARTBEAT_INTERVAL  10000   // 10 seconds
#define DISPLAY_ALERT_DURATION 3000 // 3 seconds

#endif // CONFIG_H