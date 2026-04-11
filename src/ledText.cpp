/* ledText.cpp — contents distributed to the project libraries:
 *
 *   I2S init / audio read  → lib/AudioProcessor/AudioProcessor.c
 *   FFT (rewritten in C)   → lib/AudioProcessor/AudioProcessor.c
 *   TFT display functions  → lib/DisplayManager/Disp.cpp
 *   Alert classification   → lib/Logic/FSM.c
 *   Blynk integration      → lib/BlynkManager/Blynk.cpp
 *
 * The active application entry-point is src/sound.cpp.
 * This file is excluded from the build via platformio.ini.
 */
#if 0  /* entire file disabled */
#include <Arduino.h>
#include <BlynkSimpleEsp32.h>
#include <TFT_eSPI.h>
#include <arduinoFFT.h>
#include <driver/i2s.h>

// ===== BLYNK CONFIGURATION =====
#define BLYNK_TEMPLATE_ID "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "Frequency Alert System"
#define BLYNK_AUTH_TOKEN "YOUR_AUTH_TOKEN"

char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

// ===== DISPLAY CONFIGURATION =====
TFT_eSPI tft = TFT_eSPI();
#define TFT_BACKLIGHT 21

// ===== AUDIO CONFIGURATION =====
#define I2S_BCK 14 // Bit Clock (SCK)
#define I2S_WS 15  // Word Select (L/R)
#define I2S_DIN 32 // Data In (SD)

#define SAMPLE_RATE 16000 // Hz
#define SAMPLES 512       // Must be power of 2 for FFT
#define FREQ_RESOLUTION (float)SAMPLE_RATE / SAMPLES

// FFT variables
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT FFT = arduinoFFT();

// Frequency thresholds for alerts (in Hz)
#define SMOKE_ALARM_FREQ 3000 // Typical smoke alarm ~3kHz
#define GLASS_BREAK_FREQ 4000 // Glass break ~4kHz
#define VOICE_FREQ_MIN 300
#define VOICE_FREQ_MAX 3400

// Alert state
String currentAlert = "NONE";
bool alertTriggered = false;
unsigned long lastBlynkSend = 0;

// ===== BLYNK VIRTUAL PINS =====
#define VPIN_FREQUENCY V0
#define VPIN_ALERT_TEXT V1
#define VPIN_STATUS V2

// ===== DISPLAY FUNCTIONS =====
void initDisplay()
{
    pinMode(TFT_BACKLIGHT, OUTPUT);
    digitalWrite(TFT_BACKLIGHT, HIGH); // Turn on backlight

    tft.init();
    tft.setRotation(2); // Adjust orientation as needed
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);

    // Draw title
    tft.setCursor(30, 30);
    tft.println("Audio Monitor");
    delay(2000);
    tft.fillScreen(TFT_BLACK);
}

void updateDisplay(String alertText, float frequency, float amplitude)
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);

    // Show current frequency
    tft.setCursor(20, 30);
    tft.setTextColor(TFT_CYAN);
    tft.print("Freq: ");
    tft.print(frequency, 0);
    tft.println(" Hz");

    // Show amplitude
    tft.setCursor(20, 60);
    tft.setTextColor(TFT_YELLOW);
    tft.print("Level: ");
    tft.print(amplitude, 1);

    // Show alert status
    tft.setTextSize(2);
    tft.setCursor(20, 110);

    if (alertText != "NONE")
    {
        tft.setTextColor(TFT_RED);
        tft.println("⚠️ ALERT!");
        tft.setCursor(20, 140);
        tft.println(alertText);
    }
    else
    {
        tft.setTextColor(TFT_GREEN);
        tft.println("SAFE");
    }

    // Show system status
    tft.setTextSize(1);
    tft.setCursor(20, 200);
    tft.setTextColor(TFT_WHITE);
    tft.println("System Monitoring");
}

// ===== I2S AUDIO SETUP =====
void initI2SMicrophone()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = SAMPLES,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DIN};

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
}

// Read audio samples from I2S microphone
int readSamples(int32_t *samples, int numSamples)
{
    size_t bytesRead;
    i2s_read(I2S_NUM_0, samples, numSamples * sizeof(int32_t), &bytesRead, portMAX_DELAY);
    return bytesRead / sizeof(int32_t);
}

// Perform FFT and get dominant frequency
float getDominantFrequency(int32_t *audioData, float *amplitudeOut)
{
    // Convert 32-bit samples to double for FFT
    for (int i = 0; i < SAMPLES; i++)
    {
        vReal[i] = (double)audioData[i] / 2147483648.0; // Scale to -1.0 to 1.0
        vImag[i] = 0.0;
    }

    // Perform FFT
    FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
    FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);

    // Find peak frequency
    double peakMagnitude = 0;
    int peakIndex = 0;

    // Skip DC component (index 0)
    for (int i = 1; i < (SAMPLES / 2); i++)
    {
        if (vReal[i] > peakMagnitude)
        {
            peakMagnitude = vReal[i];
            peakIndex = i;
        }
    }

    float frequency = peakIndex * FREQ_RESOLUTION;
    *amplitudeOut = (float)peakMagnitude;

    return frequency;
}

// ===== ALERT LOGIC =====
String checkForAlert(float frequency, float amplitude)
{
    if (amplitude < 50.0)
    { // Noise floor threshold
        return "NONE";
    }

    // Check for specific frequency ranges
    if (frequency > 2800 && frequency < 3200)
    {
        return "SMOKE ALARM DETECTED";
    }
    else if (frequency > 3500 && frequency < 4500)
    {
        return "GLASS BREAKING";
    }
    else if (frequency > 1800 && frequency < 2200)
    {
        return "HIGH FREQ TONE";
    }
    else if (frequency >= VOICE_FREQ_MIN && frequency <= VOICE_FREQ_MAX)
    {
        return "VOICE DETECTED";
    }

    return "NONE";
}

// ===== SETUP =====
void setup()
{
    Serial.begin(115200);

    // Initialize display
    initDisplay();
    tft.setCursor(20, 100);
    tft.println("Starting...");

    // Initialize microphone
    initI2SMicrophone();
    delay(100);

    // Connect to Blynk
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 100);
    tft.println("Ready!");
    delay(1000);
    tft.fillScreen(TFT_BLACK);

    Serial.println("Frequency Alert System Started");
}

// ===== MAIN LOOP =====
void loop()
{
    Blynk.run(); // Maintain Blynk connection

    int32_t audioSamples[SAMPLES];
    int samplesRead = readSamples(audioSamples, SAMPLES);

    if (samplesRead == SAMPLES)
    {
        float amplitude;
        float frequency = getDominantFrequency(audioSamples, &amplitude);

        String alert = checkForAlert(frequency, amplitude);

        // Update display
        updateDisplay(alert, frequency, amplitude);

        // Send to serial monitor
        Serial.print("Freq: ");
        Serial.print(frequency, 0);
        Serial.print(" Hz | Amp: ");
        Serial.print(amplitude, 1);
        Serial.print(" | Alert: ");
        Serial.println(alert);

        // Send to Blynk
        Blynk.virtualWrite(VPIN_FREQUENCY, frequency);
        Blynk.virtualWrite(VPIN_ALERT_TEXT, alert);

        // Trigger notification on new alert
        if (alert != "NONE" && alert != currentAlert)
        {
            Blynk.logEvent("frequency_alert",
                           String("Alert: ") + alert + " at " + String(frequency, 0) + " Hz");
            currentAlert = alert;
            alertTriggered = true;
        }
        else if (alert == "NONE")
        {
            currentAlert = "NONE";
        }

        // Send heartbeat status every 10 seconds
        if (millis() - lastBlynkSend > 10000)
        {
            Blynk.virtualWrite(VPIN_STATUS,
                               alert == "NONE" ? "Monitoring" : "ALERT ACTIVE");
            lastBlynkSend = millis();
        }
    }

    delay(100);
}
#endif /* entire file disabled */