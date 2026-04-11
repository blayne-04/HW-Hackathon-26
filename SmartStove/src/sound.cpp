#include <Arduino.h>
#include <driver/i2s.h>
#include <FastLED.h>
#include "arduinoFFT.h"
#include <math.h> // for sqrt()
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ========== BLYNK CONFIGURATION ==========
// Get these from the Blynk IoT web console (Template -> Device Info)
#define BLYNK_TEMPLATE_ID "YourTemplateID"  // e.g. "TMPLxxxxxx"
#define BLYNK_DEVICE_NAME "SoundClassifier" // your device name
#define BLYNK_AUTH_TOKEN "YourAuthToken"    // from the device info

// Wi‑Fi credentials
char ssid[] = "YourWiFiSSID";
char pass[] = "YourWiFiPassword";

// Blynk Virtual Pins (create these as Datastreams in the template)
#define VIRTUAL_PIN_NOTIFICATION V1 // for sending notifications
#define VIRTUAL_PIN_MUTE V2         // for remote mute (button widget)

// ===== I2S Microphone Settings =====
#define I2S_WS 25
#define I2S_SCK 26
#define I2S_SD 33

const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256};

const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD};

// ===== Audio Buffer =====
const int bufferSize = 512;
int16_t sampleBuffer[bufferSize];

// ===== LED Settings =====
#define LED_DATA_PIN 15
#define NUM_LEDS 30
CRGB leds[NUM_LEDS];

// ===== Relay pins =====
#define RELAY_SMOKE 32
#define RELAY_DOORBELL 33

// ===== FFT config =====
const int samplesFFT = 256;
double vReal[samplesFFT];
double vImag[samplesFFT];
arduinoFFT FFT = arduinoFFT();

// ===== Sound classification thresholds =====
const float MIN_TOTAL_ENERGY = 5000.0;
const float DOORBELL_RATIO = 0.35;
const float SMOKE_RATIO = 0.45;

// ===== Debouncing =====
unsigned long lastDoorbell = 0;
unsigned long lastSmoke = 0;
const unsigned long COOLDOWN_MS = 8000;

// ===== Remote mute flag =====
bool alertsMuted = false;

// ===== Function prototypes =====
void triggerSmokeAlert();
void triggerDoorbellAlert();
void showLoudnessBar();

// ===== Blynk handler for mute button =====
BLYNK_WRITE(VIRTUAL_PIN_MUTE)
{
    alertsMuted = param.asInt(); // 1 = muted, 0 = unmuted
    Serial.print("Remote mute: ");
    Serial.println(alertsMuted ? "ON" : "OFF");
    if (alertsMuted)
    {
        fill_solid(leds, NUM_LEDS, CRGB::DarkBlue);
        FastLED.show();
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting Sound Classifier with Blynk...");

    // Initialize I2S microphone
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);

    // Initialize LED strip
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(100);
    FastLED.clear();
    FastLED.show();

    // Relay pins
    pinMode(RELAY_SMOKE, OUTPUT);
    pinMode(RELAY_DOORBELL, OUTPUT);
    digitalWrite(RELAY_SMOKE, LOW);
    digitalWrite(RELAY_DOORBELL, LOW);

    // Connect to Wi‑Fi and Blynk
    Serial.print("Connecting to Wi‑Fi...");
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    Serial.println(" Connected!");
    Serial.println("Setup complete. Listening...");
}

void loop()
{
    // Keep Blynk connection alive
    Blynk.run();

    // Read audio samples from I2S
    size_t bytesRead;
    i2s_read(I2S_NUM_0, &sampleBuffer, sizeof(sampleBuffer), &bytesRead, portMAX_DELAY);
    int samplesRead = bytesRead / sizeof(int16_t);
    if (samplesRead < samplesFFT)
        return;

    // Copy to FFT arrays
    for (int i = 0; i < samplesFFT; i++)
    {
        vReal[i] = (double)sampleBuffer[i];
        vImag[i] = 0.0;
    }

    // Perform FFT
    FFT.Windowing(vReal, samplesFFT, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(vReal, vImag, samplesFFT, FFT_FORWARD);
    FFT.ComplexToMagnitude(vReal, vImag, samplesFFT);

    // Compute band energies
    float totalEnergy = 0;
    float energyDoorbell = 0, energySmoke = 0;
    int binWidth = 16000 / samplesFFT; // ~62.5 Hz/bin

    for (int i = 1; i < samplesFFT / 2; i++)
    {
        float freq = i * binWidth;
        totalEnergy += vReal[i];
        if (freq >= 800 && freq <= 1500)
            energyDoorbell += vReal[i];
        if (freq >= 2800 && freq <= 3500)
            energySmoke += vReal[i];
    }

    // Classification (only if alerts not muted)
    if (!alertsMuted && totalEnergy > MIN_TOTAL_ENERGY)
    {
        float ratioDoorbell = energyDoorbell / totalEnergy;
        float ratioSmoke = energySmoke / totalEnergy;

        if (ratioSmoke > SMOKE_RATIO && millis() - lastSmoke > COOLDOWN_MS)
        {
            lastSmoke = millis();
            triggerSmokeAlert();
        }
        else if (ratioDoorbell > DOORBELL_RATIO && millis() - lastDoorbell > COOLDOWN_MS)
        {
            lastDoorbell = millis();
            triggerDoorbellAlert();
        }
        else
        {
            showLoudnessBar();
        }
    }
    else if (totalEnergy > MIN_TOTAL_ENERGY && alertsMuted)
    {
        // Sound is present but alerts are muted – show a muted pattern
        fill_solid(leds, NUM_LEDS, CRGB::DarkGray);
        FastLED.show();
    }
    else
    {
        // Silence
        fill_solid(leds, NUM_LEDS, CRGB::DarkBlue);
        FastLED.show();
    }
}

void triggerSmokeAlert()
{
    // Visual: red flashing
    for (int flash = 0; flash < 10; flash++)
    {
        fill_solid(leds, NUM_LEDS, CRGB::Red);
        FastLED.show();
        delay(100);
        fill_solid(leds, NUM_LEDS, CRGB::Black);
        FastLED.show();
        delay(100);
    }
    // Relay: external strobe
    digitalWrite(RELAY_SMOKE, HIGH);
    delay(2000);
    digitalWrite(RELAY_SMOKE, LOW);

    // Blynk notification
    Blynk.virtualWrite(VIRTUAL_PIN_NOTIFICATION, "🔥 SMOKE ALARM DETECTED! 🔥");
    Serial.println("Blynk: Smoke alert sent");
}

void triggerDoorbellAlert()
{
    // Visual: yellow sweep
    for (int i = 0; i < NUM_LEDS; i++)
    {
        leds[i] = CRGB::Yellow;
        FastLED.show();
        delay(20);
    }
    for (int i = NUM_LEDS - 1; i >= 0; i--)
    {
        leds[i] = CRGB::Black;
        FastLED.show();
        delay(20);
    }
    // Relay: short pulse
    digitalWrite(RELAY_DOORBELL, HIGH);
    delay(500);
    digitalWrite(RELAY_DOORBELL, LOW);

    // Blynk notification
    Blynk.virtualWrite(VIRTUAL_PIN_NOTIFICATION, "🔔 Doorbell pressed! 🔔");
    Serial.println("Blynk: Doorbell alert sent");
}

void showLoudnessBar()
{
    // Compute RMS from first 256 samples
    long sumSq = 0;
    for (int i = 0; i < samplesFFT; i++)
    {
        long s = sampleBuffer[i];
        sumSq += s * s;
    }
    int rms = sqrt(sumSq / samplesFFT);

    // Noise gate
    const int NOISE_FLOOR = 20;
    if (rms < NOISE_FLOOR)
        rms = 0;

    // Auto‑ranging peak follower
    static int peak = 100;
    if (rms > peak)
        peak = rms;
    else
        peak = (peak * 99 + rms) / 100;
    if (peak < 100)
        peak = 100;

    // Map to number of LEDs
    int ledCount = map(rms, 0, peak, 0, NUM_LEDS);
    ledCount = constrain(ledCount, 0, NUM_LEDS);

    // Colour gradient
    for (int i = 0; i < NUM_LEDS; i++)
    {
        if (i < ledCount)
        {
            uint8_t hue = map(i, 0, NUM_LEDS, 160, 0); // blue → red
            leds[i] = CHSV(hue, 255, 255);
        }
        else
        {
            leds[i] = CRGB::Black;
        }
    }
    FastLED.show();
}