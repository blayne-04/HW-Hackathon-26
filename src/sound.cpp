#include <Arduino.h>
#include <driver/i2s.h>
#include <FastLED.h>
#include "arduinoFFT.h"
#include <math.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>

// ========== BLYNK IoT ==========
#define BLYNK_TEMPLATE_ID "YourTemplateID"    // Replace with your actual Template ID
#define BLYNK_TEMPLATE_NAME "SoundClassifier" // Your device name
#define BLYNK_AUTH_TOKEN "YourAuthToken"      // Your Auth Token

char ssid[] = "YourWiFiSSID";
char pass[] = "YourWiFiPassword";

// ===== BLYNK VIRTUAL PINS =====
#define VIRTUAL_PIN_NOTIFICATION V1  // Full alert text for notifications
#define VIRTUAL_PIN_MUTE V2          // Mute control
#define VIRTUAL_PIN_LED_TEXT V3      // NEW: Short LED text for display widget
#define VIRTUAL_PIN_ALERT_STATE V4   // NEW: Numeric alert state (0=none,1=smoke,2=doorbell)
#define VIRTUAL_PIN_STATUS V5        // NEW: System status

// ===== DISPLAY =====
TFT_eSPI tft = TFT_eSPI();
const int NUM_BARS = 12;
const int BAR_WIDTH = 240 / NUM_BARS;
float barMagnitudes[NUM_BARS] = {0};

// ===== I2S MIC =====
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

// ===== AUDIO BUFFER =====
const int bufferSize = 512;
int16_t sampleBuffer[bufferSize];

// ===== LED STRIP =====
#define LED_DATA_PIN 15
#define NUM_LEDS 30
CRGB leds[NUM_LEDS];

// ===== RELAYS =====
#define RELAY_SMOKE 32
#define RELAY_DOORBELL 33

// ===== FFT =====
const int samplesFFT = 256;
double vReal[samplesFFT];
double vImag[samplesFFT];
arduinoFFT FFT = arduinoFFT();

// ===== CLASSIFICATION THRESHOLDS =====
const float MIN_TOTAL_ENERGY = 5000.0;
const float DOORBELL_RATIO = 0.35;
const float SMOKE_RATIO = 0.45;

// ===== DEBOUNCING =====
unsigned long lastDoorbell = 0;
unsigned long lastSmoke = 0;
const unsigned long COOLDOWN_MS = 8000;

// ===== REMOTE MUTE =====
bool alertsMuted = false;

// ===== ENHANCED ALERT STATE MACHINE =====
enum AlertState
{
    ALERT_NONE = 0,
    ALERT_SMOKE = 1,
    ALERT_DOORBELL = 2
};

AlertState currentAlert = ALERT_NONE;
AlertState lastAlert = ALERT_NONE;
unsigned long alertStartTime = 0;
int alertStep = 0;
unsigned long alertLastUpdate = 0;

// For display alert text
unsigned long displayAlertStart = 0;
bool showingAlertText = false;
String currentAlertText = "";
uint16_t currentAlertColor = TFT_WHITE;

// Blynk heartbeat
unsigned long lastBlynkSend = 0;

// ===== FUNCTION PROTOTYPES =====
void triggerSmokeAlert();
void triggerDoorbellAlert();
void updateAlert();
void showLoudnessBar();
void drawSpectrum();
void updateDisplayAlert();
void sendAlertToBlynk(AlertState state, String fullText);
String getLEDTextFromState(AlertState state);
String getFullAlertTextFromState(AlertState state);

// ===== BLYNK HANDLER FOR MUTE =====
BLYNK_WRITE(VIRTUAL_PIN_MUTE)
{
    alertsMuted = param.asInt();
    Serial.print("Remote mute: ");
    Serial.println(alertsMuted ? "ON" : "OFF");
    
    // Update status on Blynk
    Blynk.virtualWrite(VIRTUAL_PIN_STATUS, alertsMuted ? "MUTED" : "ACTIVE");
    
    if (alertsMuted)
    {
        fill_solid(leds, NUM_LEDS, CRGB::DarkBlue);
        FastLED.show();
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(3);
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(50, 100);
        tft.println("MUTED");
        
        // Send muted state to LED text widget
        Blynk.virtualWrite(VIRTUAL_PIN_LED_TEXT, "MUTED");
        Blynk.virtualWrite(VIRTUAL_PIN_ALERT_STATE, -1); // Special code for muted
    }
    else
    {
        tft.fillScreen(TFT_BLACK);
        Blynk.virtualWrite(VIRTUAL_PIN_LED_TEXT, "ACTIVE");
        Blynk.virtualWrite(VIRTUAL_PIN_ALERT_STATE, 0);
    }
}

// ===== HELPER FUNCTIONS FOR ALERT TEXT =====
String getFullAlertTextFromState(AlertState state)
{
    switch(state)
    {
        case ALERT_SMOKE:
            return "SMOKE ALARM DETECTED";
        case ALERT_DOORBELL:
            return "DOORBELL PRESSED";
        case ALERT_NONE:
        default:
            return "NONE";
    }
}

String getLEDTextFromState(AlertState state)
{
    switch(state)
    {
        case ALERT_SMOKE:
            return "SMOKE!";
        case ALERT_DOORBELL:
            return "DOOR!";
        case ALERT_NONE:
        default:
            return "OK";
    }
}

void sendAlertToBlynk(AlertState state, String fullText)
{
    // Send full text to notification pin
    Blynk.virtualWrite(VIRTUAL_PIN_NOTIFICATION, fullText);
    
    // Send short LED text (V3) - perfect for Labeled Value widget
    String ledText = getLEDTextFromState(state);
    Blynk.virtualWrite(VIRTUAL_PIN_LED_TEXT, ledText);
    
    // Send numeric state (V4) - for color mapping and gauges
    Blynk.virtualWrite(VIRTUAL_PIN_ALERT_STATE, (int)state);
    
    // Send status (V5)
    String statusText;
    if (alertsMuted)
        statusText = "MUTED";
    else if (state != ALERT_NONE)
        statusText = "ALERT ACTIVE";
    else
        statusText = "MONITORING";
    Blynk.virtualWrite(VIRTUAL_PIN_STATUS, statusText);
    
    Serial.print("Blynk Update - LED Text: ");
    Serial.print(ledText);
    Serial.print(" | State: ");
    Serial.println((int)state);
}

// ===== SETUP =====
void setup()
{
    Serial.begin(115200);
    Serial.println("Starting Sound Classifier with Blynk & Display...");

    // I2S mic
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);

    // LED strip
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(100);
    FastLED.clear();
    FastLED.show();

    // Relays
    pinMode(RELAY_SMOKE, OUTPUT);
    pinMode(RELAY_DOORBELL, OUTPUT);
    digitalWrite(RELAY_SMOKE, LOW);
    digitalWrite(RELAY_DOORBELL, LOW);

    // Display
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN);
    tft.setCursor(20, 80);
    tft.println("Sound Classifier");
    tft.setCursor(40, 120);
    tft.println("Ready...");
    delay(2000);
    tft.fillScreen(TFT_BLACK);

    // Wi-Fi and Blynk
    Serial.print("Connecting to Wi-Fi...");
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    Serial.println(" Connected!");
    
    // Send initial status to Blynk
    sendAlertToBlynk(ALERT_NONE, "NONE");
    
    Serial.println("Setup complete. Listening...");
}

// ===== MAIN LOOP =====
void loop()
{
    Blynk.run();
    updateAlert();
    updateDisplayAlert();

    // Send heartbeat to Blynk every 10 seconds
    if (millis() - lastBlynkSend > 10000)
    {
        if (currentAlert == ALERT_NONE && !alertsMuted)
        {
            // Send heartbeat only when safe and not muted
            Blynk.virtualWrite(VIRTUAL_PIN_STATUS, "MONITORING");
            Blynk.virtualWrite(VIRTUAL_PIN_LED_TEXT, "OK");
        }
        lastBlynkSend = millis();
    }

    if (currentAlert != ALERT_NONE)
    {
        delay(10);
        return;
    }

    // Read audio
    size_t bytesRead;
    i2s_read(I2S_NUM_0, &sampleBuffer, sizeof(sampleBuffer), &bytesRead, portMAX_DELAY);
    int samplesRead = bytesRead / sizeof(int16_t);
    if (samplesRead < samplesFFT)
        return;

    // Copy to FFT
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
    int binWidth = 16000 / samplesFFT;

    for (int i = 1; i < samplesFFT / 2; i++)
    {
        float freq = i * binWidth;
        totalEnergy += vReal[i];
        if (freq >= 800 && freq <= 1500)
            energyDoorbell += vReal[i];
        if (freq >= 2800 && freq <= 3500)
            energySmoke += vReal[i];
    }

    // Prepare spectrum (12 bars)
    int binsPerBar = (samplesFFT / 2) / NUM_BARS;
    for (int i = 0; i < NUM_BARS; i++)
    {
        float sum = 0;
        int startBin = i * binsPerBar;
        int endBin = (i + 1) * binsPerBar;
        for (int j = startBin; j < endBin && j < samplesFFT / 2; j++)
        {
            sum += vReal[j];
        }
        barMagnitudes[i] = sum / binsPerBar;
    }

    // Classification
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
            drawSpectrum();
            showLoudnessBar();
        }
    }
    else if (totalEnergy > MIN_TOTAL_ENERGY && alertsMuted)
    {
        fill_solid(leds, NUM_LEDS, CRGB::DarkGray);
        FastLED.show();
    }
    else
    {
        fill_solid(leds, NUM_LEDS, CRGB::DarkBlue);
        FastLED.show();
        drawSpectrum();
    }
}

// ===== ALERT TRIGGERS =====
void triggerSmokeAlert()
{
    if (currentAlert != ALERT_NONE)
        return;
    
    currentAlert = ALERT_SMOKE;
    alertStartTime = millis();
    alertStep = 0;
    alertLastUpdate = millis();

    showingAlertText = true;
    currentAlertText = "SMOKE ALARM!";
    currentAlertColor = TFT_RED;
    displayAlertStart = millis();

    digitalWrite(RELAY_SMOKE, HIGH);
    
    // Send enhanced Blynk alerts
    String fullText = "🔥 SMOKE ALARM DETECTED! 🔥";
    sendAlertToBlynk(ALERT_SMOKE, fullText);
    Blynk.logEvent("smoke_alert", fullText);
    
    Serial.println("Blynk: Smoke alert sent - LED Text: SMOKE!");
}

void triggerDoorbellAlert()
{
    if (currentAlert != ALERT_NONE)
        return;
    
    currentAlert = ALERT_DOORBELL;
    alertStartTime = millis();
    alertStep = 0;
    alertLastUpdate = millis();

    showingAlertText = true;
    currentAlertText = "DOORBELL";
    currentAlertColor = TFT_YELLOW;
    displayAlertStart = millis();

    digitalWrite(RELAY_DOORBELL, HIGH);
    
    // Send enhanced Blynk alerts
    String fullText = "🔔 Doorbell pressed! 🔔";
    sendAlertToBlynk(ALERT_DOORBELL, fullText);
    Blynk.logEvent("doorbell_alert", fullText);
    
    Serial.println("Blynk: Doorbell alert sent - LED Text: DOOR!");
}

// ===== NON-BLOCKING ALERT ANIMATION =====
void updateAlert()
{
    if (currentAlert == ALERT_NONE)
        return;

    unsigned long now = millis();
    unsigned long elapsed = now - alertStartTime;

    if (currentAlert == ALERT_SMOKE)
    {
        int flashIndex = elapsed / 200;
        if (flashIndex >= 10)
        {
            digitalWrite(RELAY_SMOKE, LOW);
            currentAlert = ALERT_NONE;
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            FastLED.show();
            // Send clear state to Blynk
            sendAlertToBlynk(ALERT_NONE, "NONE");
            return;
        }
        bool on = (flashIndex % 2 == 0);
        fill_solid(leds, NUM_LEDS, on ? CRGB::Red : CRGB::Black);
        FastLED.show();

        if (elapsed >= 2000 && digitalRead(RELAY_SMOKE) == HIGH)
            digitalWrite(RELAY_SMOKE, LOW);
    }
    else if (currentAlert == ALERT_DOORBELL)
    {
        int totalSteps = NUM_LEDS * 2;
        int step = elapsed / 20;
        if (step >= totalSteps)
        {
            digitalWrite(RELAY_DOORBELL, LOW);
            currentAlert = ALERT_NONE;
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            FastLED.show();
            // Send clear state to Blynk
            sendAlertToBlynk(ALERT_NONE, "NONE");
            return;
        }
        if (step < NUM_LEDS)
        {
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            leds[step] = CRGB::Yellow;
        }
        else
        {
            int offIndex = step - NUM_LEDS;
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            for (int i = offIndex; i < NUM_LEDS; i++)
                leds[i] = CRGB::Yellow;
        }
        FastLED.show();

        if (elapsed >= 500 && digitalRead(RELAY_DOORBELL) == HIGH)
            digitalWrite(RELAY_DOORBELL, LOW);
    }
}

// ===== DISPLAY ALERT TEXT =====
void updateDisplayAlert()
{
    if (!showingAlertText)
        return;

    unsigned long now = millis();
    if (now - displayAlertStart >= 3000)
    {
        tft.fillScreen(TFT_BLACK);
        showingAlertText = false;
    }
    else
    {
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(4);
        tft.setTextColor(currentAlertColor, TFT_BLACK);
        tft.setCursor(20, 100);
        tft.println(currentAlertText);
    }
}

// ===== SPECTRUM ANALYZER =====
void drawSpectrum()
{
    float maxMag = 0;
    for (int i = 0; i < NUM_BARS; i++)
        if (barMagnitudes[i] > maxMag)
            maxMag = barMagnitudes[i];
    if (maxMag < 1.0)
        maxMag = 1.0;

    for (int i = 0; i < NUM_BARS; i++)
    {
        int x = i * BAR_WIDTH;
        tft.fillRect(x, 0, BAR_WIDTH, 240, TFT_BLACK);

        int barHeight = (int)((barMagnitudes[i] / maxMag) * 200);
        if (barHeight < 0)
            barHeight = 0;
        int y = 240 - barHeight;

        uint16_t color;
        if (barHeight < 70)
            color = TFT_GREEN;
        else if (barHeight < 140)
            color = TFT_YELLOW;
        else
            color = TFT_RED;

        tft.fillRect(x, y, BAR_WIDTH - 1, barHeight, color);
    }
}

// ===== LED LOUDNESS BAR =====
void showLoudnessBar()
{
    long sumSq = 0;
    for (int i = 0; i < samplesFFT; i++)
    {
        long s = sampleBuffer[i];
        sumSq += s * s;
    }
    int rms = sqrt(sumSq / samplesFFT);

    const int NOISE_FLOOR = 20;
    if (rms < NOISE_FLOOR)
        rms = 0;

    static int peak = 100;
    if (rms > peak)
        peak = rms;
    else
        peak = (peak * 99 + rms) / 100;
    if (peak < 100)
        peak = 100;

    int ledCount = map(rms, 0, peak, 0, NUM_LEDS);
    ledCount = constrain(ledCount, 0, NUM_LEDS);

    for (int i = 0; i < NUM_LEDS; i++)
    {
        if (i < ledCount)
        {
            uint8_t hue = map(i, 0, NUM_LEDS, 160, 0);
            leds[i] = CHSV(hue, 255, 255);
        }
        else
        {
            leds[i] = CRGB::Black;
        }
    }
    FastLED.show();
}