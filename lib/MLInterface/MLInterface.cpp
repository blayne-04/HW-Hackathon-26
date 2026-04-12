#include "MLInterface.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <string.h>

#include "AudioProcessor.h"

// === WiFi & API Credentials ===
static const char* ssid = "iPhone";
static const char* password = "12345678";
static const char* serverURL = "https://npcbsk6bghefpck2safyecsc5u0rmdwa.lambda-url.us-east-2.on.aws/";

// === Audio Configuration ===
#define SAMPLE_BUFFER_SIZE 4096
#define WAV_HEADER_SIZE 44
#define WAV_PACKET_BYTES (SAMPLE_BUFFER_SIZE * 2 + WAV_HEADER_SIZE)
#define BASE64_ENCODED_BYTES (((WAV_PACKET_BYTES + 2) / 3) * 4)
#define JSON_PREFIX "{\"audio_data\":\""
#define JSON_SUFFIX "\"}"
#define JSON_PAYLOAD_MAX ((sizeof(JSON_PREFIX) - 1) + BASE64_ENCODED_BYTES + (sizeof(JSON_SUFFIX) - 1) + 1)
#define DIAG_TARGET_FREQ_HZ 5000.0f

static int16_t s_audioBuffer[SAMPLE_BUFFER_SIZE];
static uint8_t s_wavBuffer[SAMPLE_BUFFER_SIZE * 2 + WAV_HEADER_SIZE];
static char s_audioBase64[BASE64_ENCODED_BYTES + 1];
static char s_jsonPayload[JSON_PAYLOAD_MAX];
static float s_fftRe[AUDIO_FFT_SAMPLES];
static float s_fftIm[AUDIO_FFT_SAMPLES];

static void connect_to_wifi();
static bool apply_noise_gate(int sampleCount);
static void create_wav_header(uint32_t dataSize);
static void classify_audio(int sampleCount);
static void send_audio_for_classification(const char* audioBase64, size_t audioBase64Len);
static void print_frequency_diagnostics(int sampleCount, int avgAmplitude, int peak);

static bool is_base64_char(char c)
{
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '+' || c == '/' || c == '=';
}

void ml_interface_init()
{
    Serial.begin(115200);
    while (!Serial) {}

    Serial.println("=================================");
    Serial.println("  H.E.L.P. System Booting (AWS)  ");
    Serial.println("=================================");

    if (strcmp(ssid, "YOUR_WIFI_NAME") == 0 || strcmp(password, "YOUR_WIFI_PASSWORD") == 0) {
        Serial.println("WiFi config missing: set ssid/password in MLInterface.cpp first.");
        return;
    }

    connect_to_wifi();
    audio_init();

    Serial.println("Real-time audio classification ready!");
}

void ml_interface_tick()
{
    int samples = audio_read_samples(s_audioBuffer, SAMPLE_BUFFER_SIZE);

    if (samples > 0) {
        if (apply_noise_gate(samples)) {
            classify_audio(samples);
        }
    } else {
        Serial.println("Error reading audio from DMA.");
    }

    delay(10);
}

static void connect_to_wifi()
{
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

static bool apply_noise_gate(int sampleCount)
{
    if (sampleCount <= 0) {
        return false;
    }

    long sum = 0;
    int peak = 0;
    int clipped = 0;
    for (int i = 0; i < sampleCount; i++) {
        sum += abs(s_audioBuffer[i]);
        int a = abs(s_audioBuffer[i]);
        if (a > peak) peak = a;
        if (a >= 32760) clipped++;
    }

    int avgAmplitude = sum / sampleCount;
    float clippedPct = (100.0f * (float)clipped) / (float)sampleCount;
    Serial.printf("Snippet stats: n=%d avg=%d peak=%d clip=%.2f%%\n", sampleCount, avgAmplitude, peak, clippedPct);

    static uint32_t s_last_diag_ms = 0;
    uint32_t now = millis();
    if (now - s_last_diag_ms >= 350U) {
        s_last_diag_ms = now;
        print_frequency_diagnostics(sampleCount, avgAmplitude, peak);
    }

    if (avgAmplitude < 100) {
        return false;
    }

    return true;
}

static void print_frequency_diagnostics(int sampleCount, int avgAmplitude, int peak)
{
    int n = sampleCount;
    if (n > AUDIO_FFT_SAMPLES) n = AUDIO_FFT_SAMPLES;
    if (n < 128) return;

    float mean = 0.0f;
    for (int i = 0; i < n; i++) mean += (float)s_audioBuffer[i];
    mean /= (float)n;

    for (int i = 0; i < n; i++) {
        s_fftRe[i] = (float)s_audioBuffer[i] - mean;
        s_fftIm[i] = 0.0f;
    }

    fft_hamming_window(s_fftRe, n);
    fft_compute(s_fftRe, s_fftIm, n);
    fft_complex_to_magnitude(s_fftRe, s_fftIm, n);

    float dominantMag = 0.0f;
    float dominantHz = fft_dominant_frequency(s_fftRe, n, &dominantMag);
    float totalE = fft_total_energy(s_fftRe, n);
    if (totalE < 1e-6f) totalE = 1e-6f;

    float e5k = fft_band_energy(s_fftRe, n, DIAG_TARGET_FREQ_HZ - 200.0f, DIAG_TARGET_FREQ_HZ + 200.0f);
    float eLow = fft_band_energy(s_fftRe, n, 60.0f, 300.0f);
    float eMid = fft_band_energy(s_fftRe, n, 300.0f, 2000.0f);
    float eHi = fft_band_energy(s_fftRe, n, 2000.0f, 6800.0f);

    Serial.printf("Snippet stats (freq): n=%d dom_hz=%.0f dom_mag=%.1f band_5k_pct=%.1f low_pct=%.1f mid_pct=%.1f high_pct=%.1f avg=%d peak=%d\n",
                  n,
                  dominantHz,
                  dominantMag,
                  100.0f * e5k / totalE,
                  100.0f * eLow / totalE,
                  100.0f * eMid / totalE,
                  100.0f * eHi / totalE,
                  avgAmplitude,
                  peak);
}

static void classify_audio(int sampleCount)
{
    if (sampleCount <= 0 || sampleCount > SAMPLE_BUFFER_SIZE) {
        Serial.println("Invalid sample count; dropping frame.");
        return;
    }

    uint32_t pcmBytes = (uint32_t)sampleCount * 2U;
    uint32_t wavBytes = WAV_HEADER_SIZE + pcmBytes;

    create_wav_header(pcmBytes);
    memcpy(s_wavBuffer + WAV_HEADER_SIZE, s_audioBuffer, pcmBytes);

    size_t outLen = 0;
    int rc = mbedtls_base64_encode(
        (unsigned char*)s_audioBase64,
        sizeof(s_audioBase64),
        &outLen,
        (const unsigned char*)s_wavBuffer,
        wavBytes);

    if (rc != 0) {
        Serial.printf("Base64 encode failed: %d\n", rc);
        return;
    }

    size_t expectedOutLen = ((wavBytes + 2U) / 3U) * 4U;
    if (outLen != expectedOutLen) {
        Serial.printf("Base64 length mismatch: got=%u expected=%u\n", (unsigned)outLen, (unsigned)expectedOutLen);
        return;
    }
    s_audioBase64[outLen] = '\0';

    for (size_t i = 0; i < outLen; i++) {
        if (!is_base64_char(s_audioBase64[i])) {
            Serial.println("Base64 integrity check failed: invalid character");
            return;
        }
    }

    float ms = 1000.0f * ((float)sampleCount / (float)AUDIO_SAMPLE_RATE);
    Serial.printf("Payload integrity: pcm=%uB wav=%uB b64=%uB duration=%.1fms\n",
                  (unsigned)pcmBytes, (unsigned)wavBytes, (unsigned)outLen, ms);

    send_audio_for_classification(s_audioBase64, outLen);
}

static void create_wav_header(uint32_t dataSize)
{
    uint32_t fileSize = dataSize + WAV_HEADER_SIZE - 8;

    memcpy(s_wavBuffer, "RIFF", 4);
    memcpy(s_wavBuffer + 4, &fileSize, 4);
    memcpy(s_wavBuffer + 8, "WAVE", 4);
    memcpy(s_wavBuffer + 12, "fmt ", 4);

    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = AUDIO_SAMPLE_RATE;
    uint32_t byteRate = AUDIO_SAMPLE_RATE * 2;
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;

    memcpy(s_wavBuffer + 16, &fmtSize, 4);
    memcpy(s_wavBuffer + 20, &audioFormat, 2);
    memcpy(s_wavBuffer + 22, &numChannels, 2);
    memcpy(s_wavBuffer + 24, &sampleRate, 4);
    memcpy(s_wavBuffer + 28, &byteRate, 4);
    memcpy(s_wavBuffer + 32, &blockAlign, 2);
    memcpy(s_wavBuffer + 34, &bitsPerSample, 2);
    memcpy(s_wavBuffer + 36, "data", 4);
    memcpy(s_wavBuffer + 40, &dataSize, 4);
}

static void send_audio_for_classification(const char* audioBase64, size_t audioBase64Len)
{
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
            Serial.println("DETECTED: " + predictedClass + " (" + String(confidence * 100, 1) + "%)");

            if (predictedClass == "fire_alarm" || predictedClass == "glass_break") {
                Serial.println("ALERT: HAZARD DETECTED!");
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
