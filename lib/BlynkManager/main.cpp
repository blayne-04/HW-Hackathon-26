#include <Arduino.h>
#include "Config.h"
#include "SoundClassifier.h"
#include "AudioAnalyzer.h"
#include "LEDStrip.h"
#include "DisplayManager.h"
#include "BlynkManager.h"

// Global instances
SoundClassifier g_classifier;
AudioAnalyzer g_audioAnalyzer;
LEDStrip g_ledStrip;
DisplayManager g_display;
BlynkManager g_blynkManager;

// Timing variables
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 50;  // 20Hz update rate

// Forward declarations
void setupHardware();
void updateDisplay();

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n==========================================");
    Serial.println("Sound Classifier System Starting...");
    Serial.println("==========================================");
    
    setupHardware();
    
    // Initialize all modules
    g_audioAnalyzer.begin();
    g_ledStrip.begin();
    g_display.begin();
    g_classifier.begin();
    
    // Connect to Blynk (WiFi connection happens here)
    if (g_blynkManager.begin()) {
        Serial.println("Blynk connected successfully!");
    } else {
        Serial.println("Warning: Blynk connection failed - running offline");
    }
    
    Serial.println("==========================================");
    Serial.println("System Ready!");
    Serial.println("Monitoring for smoke alarms and doorbells...");
    Serial.println("==========================================\n");
}

void setupHardware() {
    // Initialize relay pins
    pinMode(RELAY_SMOKE, OUTPUT);
    pinMode(RELAY_DOORBELL, OUTPUT);
    digitalWrite(RELAY_SMOKE, LOW);
    digitalWrite(RELAY_DOORBELL, LOW);
    
    Serial.println("Hardware initialized:");
    Serial.printf("  - LED Strip: GPIO %d (%d LEDs)\n", LED_DATA_PIN, NUM_LEDS);
    Serial.printf("  - Relays: Smoke=%d, Doorbell=%d\n", RELAY_SMOKE, RELAY_DOORBELL);
    Serial.printf("  - I2S Mic: WS=%d, SCK=%d, SD=%d\n", I2S_WS, I2S_SCK, I2S_SD);
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    // Run Blynk (handles WiFi and virtual pins)
    g_blynkManager.run();
    
    // Update LED strip animations
    g_ledStrip.update();
    
    // Update sound classification
    g_classifier.update();
    
    // Get current alert state
    AlertType currentAlert = g_classifier.getCurrentAlert();
    
    // Trigger LED strip if new alert
    static AlertType lastAlert = ALERT_NONE;
    if (currentAlert != lastAlert && currentAlert != ALERT_NONE) {
        g_ledStrip.triggerAlert(currentAlert);
        lastAlert = currentAlert;
    } else if (currentAlert == ALERT_NONE) {
        lastAlert = ALERT_NONE;
    }
    
    // Update display at regular intervals
    if (millis() - lastUpdate >= UPDATE_INTERVAL) {
        updateDisplay();
        lastUpdate = millis();
    }
    
    // Small delay for stability
    delay(10);
}

void updateDisplay() {
    // Get current alert
    AlertType alert = g_classifier.getCurrentAlert();
    
    // Get spectrum data (only if no alert showing)
    float* spectrum = nullptr;
    if (alert == ALERT_NONE) {
        spectrum = g_audioAnalyzer.getSpectrum();
    }
    
    // Update the display
    g_display.update(alert, spectrum);
}