#include "SoundClassifier.h"
#include "AudioAnalyzer.h"
#include "BlynkManager.h"

extern AudioAnalyzer g_audioAnalyzer;
extern BlynkManager g_blynkManager;

SoundClassifier::SoundClassifier() {
    m_currentAlert = ALERT_NONE;
    m_lastAlert = ALERT_NONE;
    m_alertStartTime = 0;
    m_lastSmokeTime = 0;
    m_lastDoorbellTime = 0;
    m_muted = false;
}

void SoundClassifier::begin() {
    Serial.println("Sound Classifier initialized");
}

void SoundClassifier::update() {
    // Skip if alert is active
    if (m_currentAlert != ALERT_NONE) {
        if (millis() - m_alertStartTime >= COOLDOWN_MS) {
            resetAlert();
        }
        return;
    }
    
    // Skip if muted
    if (m_muted) return;
    
    // Get audio analysis results
    AudioResult result = g_audioAnalyzer.analyze();
    
    if (!result.isValid) return;
    
    // Classify sound
    if (result.ratioSmoke > SMOKE_RATIO && 
        millis() - m_lastSmokeTime > COOLDOWN_MS) {
        m_lastSmokeTime = millis();
        triggerAlert(ALERT_SMOKE);
    }
    else if (result.ratioDoorbell > DOORBELL_RATIO && 
             millis() - m_lastDoorbellTime > COOLDOWN_MS) {
        m_lastDoorbellTime = millis();
        triggerAlert(ALERT_DOORBELL);
    }
}

void SoundClassifier::triggerAlert(AlertType alert) {
    m_currentAlert = alert;
    m_alertStartTime = millis();
    
    String alertText = getAlertText(alert);
    String shortText = getShortAlertText(alert);
    
    Serial.print("ALERT TRIGGERED: ");
    Serial.println(alertText);
    
    // Activate relays
    if (alert == ALERT_SMOKE) {
        digitalWrite(RELAY_SMOKE, HIGH);
    } else if (alert == ALERT_DOORBELL) {
        digitalWrite(RELAY_DOORBELL, HIGH);
    }
    
    // Send to Blynk
    g_blynkManager.sendAlert(alert, alertText);
    g_blynkManager.logEvent(alert, alertText);
}

void SoundClassifier::resetAlert() {
    if (m_currentAlert == ALERT_SMOKE) {
        digitalWrite(RELAY_SMOKE, LOW);
    } else if (m_currentAlert == ALERT_DOORBELL) {
        digitalWrite(RELAY_DOORBELL, LOW);
    }
    
    m_currentAlert = ALERT_NONE;
    g_blynkManager.sendAlert(ALERT_NONE, "NONE");
}

AlertType SoundClassifier::getCurrentAlert() {
    return m_currentAlert;
}

String SoundClassifier::getAlertText(AlertType alert) {
    switch(alert) {
        case ALERT_SMOKE:    return "SMOKE ALARM DETECTED";
        case ALERT_DOORBELL: return "DOORBELL PRESSED";
        default:             return "NONE";
    }
}

String SoundClassifier::getShortAlertText(AlertType alert) {
    switch(alert) {
        case ALERT_SMOKE:    return "SMOKE!";
        case ALERT_DOORBELL: return "DOOR!";
        default:             return "OK";
    }
}

bool SoundClassifier::isAlertActive() {
    return m_currentAlert != ALERT_NONE;
}

void SoundClassifier::setMuted(bool muted) {
    m_muted = muted;
}

bool SoundClassifier::isMuted() {
    return m_muted;
}