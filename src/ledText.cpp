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

// Alert states
enum AlertState {
    STATE_SAFE,
    STATE_SMOKE_ALARM,
    STATE_GLASS_BREAKING,
    STATE_HIGH_FREQ_TONE,
    STATE_VOICE_DETECTED
};

AlertState currentAlertState = STATE_SAFE;
AlertState lastAlertState = STATE_SAFE;
String currentAlertText = "NONE";
bool alertTriggered = false;
unsigned long lastBlynkSend = 0;
unsigned long alertStartTime = 0;
unsigned long alertCooldownTime = 0;

// ===== BLYNK VIRTUAL PINS =====
#define VPIN_FREQUENCY      V0
#define VPIN_ALERT_TEXT     V1  // For Labeled Value widget
#define VPIN_STATUS         V2  // System status
#define VPIN_LED_TEXT       V3  // NEW: For LED Text display widget
#define VPIN_ALERT_STATE    V4  // NEW: Numeric alert state (0-4)

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
        
        // Word wrap for long alert texts
        String wrappedText = alertText;
        if (wrappedText.length() > 18) {
            // Split into two lines if too long
            String line1 = wrappedText.substring(0, 18);
            String line2 = wrappedText.substring(18);
            tft.println(line1);
            tft.setCursor(20, 165);
            tft.println(line2);
        } else {
            tft.println(wrappedText);
        }
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

// ===== IMPROVED ALERT LOGIC =====
AlertState checkForAlertState(float frequency, float amplitude)
{
    if (amplitude < 50.0)
    { // Noise floor threshold
        return STATE_SAFE;
    }

    // Check for specific frequency ranges
    if (frequency > 2800 && frequency < 3200)
    {
        return STATE_SMOKE_ALARM;
    }
    else if (frequency > 3500 && frequency < 4500)
    {
        return STATE_GLASS_BREAKING;
    }
    else if (frequency > 1800 && frequency < 2200)
    {
        return STATE_HIGH_FREQ_TONE;
    }
    else if (frequency >= VOICE_FREQ_MIN && frequency <= VOICE_FREQ_MAX)
    {
        return STATE_VOICE_DETECTED;
    }

    return STATE_SAFE;
}

String getAlertTextFromState(AlertState state)
{
    switch(state) {
        case STATE_SMOKE_ALARM:
            return "SMOKE ALARM DETECTED";
        case STATE_GLASS_BREAKING:
            return "GLASS BREAKING";
        case STATE_HIGH_FREQ_TONE:
            return "HIGH FREQ TONE";
        case STATE_VOICE_DETECTED:
            return "VOICE DETECTED";
        case STATE_SAFE:
        default:
            return "NONE";
    }
}

// Get formatted LED text for display (shorter version for LED widgets)
String getLEDTextFromState(AlertState state)
{
    switch(state) {
        case STATE_SMOKE_ALARM:
            return "SMOKE!";
        case STATE_GLASS_BREAKING:
            return "GLASS!";
        case STATE_HIGH_FREQ_TONE:
            return "HIGH TONE!";
        case STATE_VOICE_DETECTED:
            return "VOICE!";
        case STATE_SAFE:
        default:
            return "SAFE";
    }
}

// Send all alert data to Blynk
void sendAlertToBlynk(AlertState state, float frequency, String fullText)
{
    // Send to standard text widget (V1)
    Blynk.virtualWrite(VPIN_ALERT_TEXT, fullText);
    
    // Send to LED Text widget (V3) - shorter version for LED displays
    String ledText = getLEDTextFromState(state);
    Blynk.virtualWrite(VPIN_LED_TEXT, ledText);
    
    // Send numeric state (V4) - useful for color mapping in Blynk
    Blynk.virtualWrite(VPIN_ALERT_STATE, (int)state);
    
    // Send frequency data
    Blynk.virtualWrite(VPIN_FREQUENCY, frequency);
    
    // Send status
    String statusText = (state == STATE_SAFE) ? "Monitoring" : "ALERT ACTIVE";
    Blynk.virtualWrite(VPIN_STATUS, statusText);
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
    Serial.print("Connecting to Blynk...");
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    Serial.println(" Connected!");

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 100);
    tft.println("Ready!");
    delay(1000);
    tft.fillScreen(TFT_BLACK);

    Serial.println("Frequency Alert System Started");
    Serial.println("Virtual Pins: V0=Freq, V1=AlertText, V2=Status, V3=LEDText, V4=State");
}

// ===== MAIN LOOP =====
void loop()
{
    Blynk.run(); // Maintain Blynk connection

    int32_t audioSamples[SAMPLES];
    int samplesRead = readSamples(audioSamples, SAMPLES);

    if (samplesRead == SAMPLES) {
        float amplitude;
        float frequency = getDominantFrequency(audioSamples, &amplitude);

        // Get current alert state
        AlertState newAlertState = checkForAlertState(frequency, amplitude);
        
        // Update current state
        currentAlertState = newAlertState;
        currentAlertText = getAlertTextFromState(currentAlertState);
        
        // Check if alert state changed
        bool stateChanged = (currentAlertState != lastAlertState);
        
        // Update display
        updateDisplay(currentAlertText, frequency, amplitude);

        // Send to serial monitor with formatted output
        Serial.print("Freq: ");
        Serial.print(frequency, 0);
        Serial.print(" Hz | Amp: ");
        Serial.print(amplitude, 1);
        Serial.print(" | State: ");
        Serial.print(currentAlertState);
        Serial.print(" | Alert: ");
        Serial.println(currentAlertText);

        // Send ALL alert data to Blynk
        sendAlertToBlynk(currentAlertState, frequency, currentAlertText);

        // Trigger notification ONLY on state change and if not safe
        if (stateChanged && currentAlertState != STATE_SAFE)
        {
            Serial.println("*** TRIGGERING ALERT NOTIFICATION ***");
            Blynk.logEvent("frequency_alert",
                           String("Alert: ") + currentAlertText + 
                           " at " + String(frequency, 0) + " Hz");
            alertTriggered = true;
            alertStartTime = millis();
        }
        
        // Reset alert flag when returning to safe state
        if (currentAlertState == STATE_SAFE && alertTriggered) {
            alertTriggered = false;
            Serial.println("System returned to SAFE state");
        }
        
        // Send heartbeat status every 10 seconds (but always send state changes immediately)
        if (millis() - lastBlynkSend > 10000 || stateChanged)
        {
            // Force send current status
            sendAlertToBlynk(currentAlertState, frequency, currentAlertText);
            lastBlynkSend = millis();
        }
        
        // Update last state
        lastAlertState = currentAlertState;
    }

    delay(100);  // Small delay for stability
}