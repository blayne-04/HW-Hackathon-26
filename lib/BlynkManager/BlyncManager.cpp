#include "BlynkManager.h"

// Forward declarations for global instances
extern SoundClassifier g_classifier;
extern LEDStrip g_ledStrip;
extern DisplayManager g_display;

// Blynk widget callback for mute button
BLYNK_WRITE(VPIN_MUTE) {
    int muted = param.asInt();
    g_classifier.setMuted(muted);
    
    if (muted) {
        g_ledStrip.turnOff();
        g_display.showMuted();
    } else {
        g_display.clear();
    }
    
    Serial.print("Mute: ");
    Serial.println(muted ? "ON" : "OFF");
}

BlynkManager::BlynkManager() {
    m_lastHeartbeat = 0;
    m_connected = false;
}

bool BlynkManager::begin() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    
    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect();
    
    m_connected = Blynk.connected();
    
    if (m_connected) {
        Serial.println("Blynk connected!");
        sendStatus("STARTING");
        sendLEDStripState("READY");
    } else {
        Serial.println("Blynk connection failed");
    }
    
    return m_connected;
}

void BlynkManager::run() {
    Blynk.run();
    
    // Periodic heartbeat
    if (millis() - m_lastHeartbeat > HEARTBEAT_INTERVAL) {
        sendHeartbeat();
        m_lastHeartbeat = millis();
    }
}

void BlynkManager::sendAlert(AlertType alert, String message) {
    Blynk.virtualWrite(VPIN_NOTIFICATION, message);
    Blynk.virtualWrite(VPIN_LED_TEXT, g_classifier.getShortAlertText(alert));
    Blynk.virtualWrite(VPIN_ALERT_STATE, (int)alert);
    Blynk.virtualWrite(VPIN_LED_STRIP, getLEDStripState(alert));
    sendStatus(alert == ALERT_NONE ? "MONITORING" : "ALERT ACTIVE");
}

void BlynkManager::sendStatus(String status) {
    Blynk.virtualWrite(VPIN_STATUS, status);
}

void BlynkManager::sendLEDText(String text) {
    Blynk.virtualWrite(VPIN_LED_TEXT, text);
}

void BlynkManager::sendLEDStripState(String state) {
    Blynk.virtualWrite(VPIN_LED_STRIP, state);
}

void BlynkManager::sendAlertState(int state) {
    Blynk.virtualWrite(VPIN_ALERT_STATE, state);
}

void BlynkManager::logEvent(AlertType alert, String message) {
    String eventName = getEventName(alert);
    if (eventName.length() > 0) {
        Blynk.logEvent(eventName, message);
    }
}

void BlynkManager::sendHeartbeat() {
    if (g_classifier.isAlertActive()) return;
    
    Blynk.virtualWrite(VPIN_STATUS, "MONITORING");
    Blynk.virtualWrite(VPIN_LED_TEXT, "OK");
    Blynk.virtualWrite(VPIN_LED_STRIP, "IDLE");
}

bool BlynkManager::isConnected() {
    return Blynk.connected();
}

String BlynkManager::getEventName(AlertType alert) {
    switch(alert) {
        case ALERT_SMOKE:    return "smoke_alert";
        case ALERT_DOORBELL: return "doorbell_alert";
        default:             return "";
    }
}

String BlynkManager::getLEDStripState(AlertType alert) {
    switch(alert) {
        case ALERT_SMOKE:    return "SMOKE_FLASH";
        case ALERT_DOORBELL: return "DOORBELL_FADE";
        default:             return "IDLE";
    }
}