#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <string.h>

#include "../include/Constants.h"
#include "AudioProcessor.h"

// === WiFi & API Credentials ===
const char* ssid = "iPhone";
const char* password = "12345678";
const char* serverURL = "https://npcbsk6bghefpck2safyecsc5u0rmdwa.lambda-url.us-east-2.on.aws/";

// === Audio Configuration ===
#define SAMPLE_BUFFER_SIZE 4096  // 0.256 seconds at 16kHz (cuts payload + heap pressure)
#define WAV_HEADER_SIZE 44
#define WAV_PACKET_BYTES (SAMPLE_BUFFER_SIZE * 2 + WAV_HEADER_SIZE)
#define BASE64_ENCODED_BYTES (((WAV_PACKET_BYTES + 2) / 3) * 4)
#define JSON_PREFIX "{\"audio_data\":\""
#define JSON_SUFFIX "\"}"
#define JSON_PAYLOAD_MAX ((sizeof(JSON_PREFIX) - 1) + BASE64_ENCODED_BYTES + (sizeof(JSON_SUFFIX) - 1) + 1)

// Global Buffers (Placed in global memory to save FreeRTOS heap space)
int16_t audioBuffer[SAMPLE_BUFFER_SIZE];
uint8_t wavBuffer[SAMPLE_BUFFER_SIZE * 2 + WAV_HEADER_SIZE];
static char s_audioBase64[BASE64_ENCODED_BYTES + 1];
static char s_jsonPayload[JSON_PAYLOAD_MAX];

// Forward declarations
void connectToWiFi();
bool applyNoiseGate();
void createWAVHeader();
void classifyAudio();
void sendAudioForClassification(const char* audioBase64, size_t audioBase64Len);

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("=================================");
    Serial.println("  H.E.L.P. System Booting (AWS)  ");
    Serial.println("=================================");

    if (strcmp(ssid, "YOUR_WIFI_NAME") == 0 || strcmp(password, "YOUR_WIFI_PASSWORD") == 0) {
        Serial.println("WiFi config missing: set ssid/password in main.cpp first.");
        return;
    }
    
    // Connect to WiFi
    connectToWiFi();
    
    // Initialize our safe, custom I2S hardware pipeline
    audio_init();
    
    Serial.println("Real-time audio classification ready!");
}

void loop() {
    // 1. Capture audio chunk using our safe 32-bit shifted function
    int samples = audio_read_samples(audioBuffer, SAMPLE_BUFFER_SIZE);
    
    if (samples > 0) {
        // 2. Apply noise gate. If it's too quiet, skip the WiFi request to save time!
        if (applyNoiseGate()) {
            // 3. Convert to WAV and send to Lambda
            classifyAudio();
        }
    } else {
        Serial.println("Error reading audio from DMA.");
    }
    
    // Prevent watchdog crashes
    delay(10);
}

void connectToWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000UL) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    } else {
        wl_status_t st = WiFi.status();
        Serial.println("\nWiFi connect failed.");
        Serial.print("Status code: ");
        Serial.println((int)st);
        Serial.println("Check: SSID/password, 2.4GHz network, and router signal strength.");
    }
}

bool applyNoiseGate() {
    // Calculate RMS to detect if there's actual sound
    long sum = 0;
    for (int i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
        sum += abs(audioBuffer[i]);
    }
    
    int avgAmplitude = sum / SAMPLE_BUFFER_SIZE;
    
    // If it's too quiet, don't waste 3 seconds waiting for AWS to reply
    if (avgAmplitude < 100) {
        return false; // Skip classification
    }
    
    return true; // Sound detected, proceed to classify
}

void classifyAudio() {
    // 1. Create WAV header
    createWAVHeader();
    
    // 2. Copy audio data directly after the header
    memcpy(wavBuffer + WAV_HEADER_SIZE, audioBuffer, SAMPLE_BUFFER_SIZE * 2);
    
    // 3. Base64 encode the entire payload into a static buffer (no String heap alloc)
    size_t outLen = 0;
    int rc = mbedtls_base64_encode(
        (unsigned char*)s_audioBase64,
        sizeof(s_audioBase64),
        &outLen,
        (const unsigned char*)wavBuffer,
        sizeof(wavBuffer));

    if (rc != 0) {
        Serial.printf("Base64 encode failed: %d\n", rc);
        return;
    }
    s_audioBase64[outLen] = '\0';

    // 4. Send to Lambda
    sendAudioForClassification(s_audioBase64, outLen);
}

void createWAVHeader() {
    uint32_t dataSize = SAMPLE_BUFFER_SIZE * 2;
    uint32_t fileSize = dataSize + WAV_HEADER_SIZE - 8;
    
    memcpy(wavBuffer, "RIFF", 4);
    memcpy(wavBuffer + 4, &fileSize, 4);
    memcpy(wavBuffer + 8, "WAVE", 4);
    memcpy(wavBuffer + 12, "fmt ", 4);
    
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = AUDIO_SAMPLE_RATE; // From AudioProcessor.h
    uint32_t byteRate = AUDIO_SAMPLE_RATE * 2;
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;
    
    memcpy(wavBuffer + 16, &fmtSize, 4);
    memcpy(wavBuffer + 20, &audioFormat, 2);
    memcpy(wavBuffer + 22, &numChannels, 2);
    memcpy(wavBuffer + 24, &sampleRate, 4);
    memcpy(wavBuffer + 28, &byteRate, 4);
    memcpy(wavBuffer + 32, &blockAlign, 2);
    memcpy(wavBuffer + 34, &bitsPerSample, 2);
    memcpy(wavBuffer + 36, "data", 4);
    memcpy(wavBuffer + 40, &dataSize, 4);
}

void sendAudioForClassification(const char* audioBase64, size_t audioBase64Len) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected!");
        return;
    }

    Serial.printf("Heap before HTTPS: free=%u, maxAlloc=%u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

    static WiFiClientSecure s_client;
    static bool s_tls_initialized = false;
    if (!s_tls_initialized) {
        s_client.setInsecure();
        s_tls_initialized = true;
    }

    HTTPClient http;
    http.begin(s_client, serverURL);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000); 

    // Build JSON in a static char buffer to avoid heap growth and fragmentation.
    size_t pos = 0;
    const size_t prefixLen = sizeof(JSON_PREFIX) - 1;
    const size_t suffixLen = sizeof(JSON_SUFFIX) - 1;
    const size_t totalLen = prefixLen + audioBase64Len + suffixLen;
    if (totalLen >= sizeof(s_jsonPayload)) {
        Serial.println("Payload too large for static JSON buffer");
        http.end();
        return;
    }
    memcpy(s_jsonPayload + pos, JSON_PREFIX, prefixLen);
    pos += prefixLen;
    memcpy(s_jsonPayload + pos, audioBase64, audioBase64Len);
    pos += audioBase64Len;
    memcpy(s_jsonPayload + pos, JSON_SUFFIX, suffixLen);
    pos += suffixLen;
    s_jsonPayload[pos] = '\0';

    Serial.printf("Heap before POST: free=%u, maxAlloc=%u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
    
    Serial.println("Sending audio to AWS Lambda...");
    int httpResponseCode = http.POST((uint8_t*)s_jsonPayload, pos);
    
    if (httpResponseCode == 200) {
        String response = http.getString();
        
        JsonDocument responseDoc;
        deserializeJson(responseDoc, response);
        
        String predictedClass = responseDoc["predicted_class"];
        float confidence = responseDoc["confidence"];
        
        if (confidence > 0.8) {
            Serial.println("🔊 DETECTED: " + predictedClass + " (" + String(confidence * 100, 1) + "%)");
            
            if (predictedClass == "fire_alarm" || predictedClass == "glass_break") {
                Serial.println("🚨 ALERT: HAZARD DETECTED!");
                // Trigger GPIO relays, Blynk notifications, etc. here
            }
        } else {
             Serial.println("Uncertain detection (" + predictedClass + ")");
        }
    } else {
        Serial.print("HTTP Error: ");
        Serial.println(httpResponseCode);
        if (httpResponseCode > 0) {
            Serial.println("Response: " + http.getString());
        }
    }
    
    http.end();
}