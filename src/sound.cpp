#include "header.hpp"
#include <FastLED.h>
#include <Arduino.h>

/* ---- WiFi credentials (replace with your values) ---- */
static const char WIFI_SSID[] = "YourWiFiSSID";
static const char WIFI_PASS[] = "YourWiFiPassword";

/* ---- LED strip ---- */
CRGB leds[NUM_LEDS];

/* ---- Relay pins ---- */
#define RELAY_SMOKE    32
#define RELAY_DOORBELL 33  /* NOTE: pin 33 is also I2S_SD — check wiring */

/* ---- Audio + FFT working buffers ---- */
static int16_t s_audio_buf[AUDIO_FFT_SAMPLES];
static double  s_vReal[AUDIO_FFT_SAMPLES];
static double  s_vImag[AUDIO_FFT_SAMPLES];
static float   s_bar_mags[DISP_NUM_BARS];

/* =================================================================
 * LED loudness bar (raw RMS → colour gradient across the strip)
 * ================================================================= */
void show_loudness_bar(void)
{
    long sum_sq = 0;
    int  i;

    for (i = 0; i < AUDIO_FFT_SAMPLES; i++) {
        long s = s_audio_buf[i];
        sum_sq += s * s;
    }
    int rms = (int)sqrt((double)(sum_sq / AUDIO_FFT_SAMPLES));

    const int NOISE_FLOOR = 20;
    if (rms < NOISE_FLOOR) rms = 0;

    static int peak = 100;
    if (rms > peak) peak = rms;
    else            peak = (peak * 99 + rms) / 100;
    if (peak < 100) peak = 100;

    int led_count = map(rms, 0, peak, 0, NUM_LEDS);
    led_count = constrain(led_count, 0, NUM_LEDS);

    for (i = 0; i < NUM_LEDS; i++) {
        if (i < led_count) {
            uint8_t hue = (uint8_t)map(i, 0, NUM_LEDS, 160, 0);
            leds[i] = CHSV(hue, 255, 255);
        } else {
            leds[i] = CRGB::Black;
        }
    }
    FastLED.show();
}

/* =================================================================
 * LED alert animation — called every loop while an alert is active
 * ================================================================= */
void update_alert_leds(AlertState state, uint32_t elapsed_ms)
{
    int i;

    if (state == ALERT_SMOKE) {
        int  flash_idx = (int)(elapsed_ms / 200U);
        bool on        = (flash_idx % 2 == 0);
        fill_solid(leds, NUM_LEDS, on ? CRGB::Red : CRGB::Black);
        FastLED.show();
        if (elapsed_ms >= 2000U && digitalRead(RELAY_SMOKE) == HIGH)
            digitalWrite(RELAY_SMOKE, LOW);

    } else if (state == ALERT_DOORBELL) {
        int total_steps = NUM_LEDS * 2;
        int step        = (int)(elapsed_ms / 20U);
        if (step >= total_steps) step = total_steps - 1;

        fill_solid(leds, NUM_LEDS, CRGB::Black);
        if (step < NUM_LEDS) {
            leds[step] = CRGB::Yellow;
        } else {
            int off_idx = step - NUM_LEDS;
            for (i = off_idx; i < NUM_LEDS; i++)
                leds[i] = CRGB::Yellow;
        }
        FastLED.show();
        if (elapsed_ms >= 500U && digitalRead(RELAY_DOORBELL) == HIGH)
            digitalWrite(RELAY_DOORBELL, LOW);
    }
}

/* =================================================================
 * Hardware side-effects at the moment an alert fires
 * ================================================================= */
void on_alert_triggered(AlertState state)
{
    if (state == ALERT_SMOKE) {
        digitalWrite(RELAY_SMOKE, HIGH);
        disp_show_alert_text("SMOKE ALARM!", TFT_RED);
        blynk_send_smoke_alert();
    } else if (state == ALERT_DOORBELL) {
        digitalWrite(RELAY_DOORBELL, HIGH);
        disp_show_alert_text("DOORBELL", TFT_YELLOW);
        blynk_send_doorbell_alert();
    }
}

/* Relay + LED cleanup when alert duration expires */
void on_alert_cleared(AlertState previous)
{
    if (previous == ALERT_SMOKE)
        digitalWrite(RELAY_SMOKE, LOW);
    else if (previous == ALERT_DOORBELL)
        digitalWrite(RELAY_DOORBELL, LOW);

    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
}

/* =================================================================
 * setup
 * ================================================================= */
void setup(void)
{
    Serial.begin(115200);
    Serial.println("Sound Classifier starting...");

    /* Relay outputs */
    pinMode(RELAY_SMOKE,    OUTPUT);
    pinMode(RELAY_DOORBELL, OUTPUT);
    digitalWrite(RELAY_SMOKE,    LOW);
    digitalWrite(RELAY_DOORBELL, LOW);

    /* LED strip */
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(100);
    FastLED.clear();
    FastLED.show();

    disp_init();
    audio_init();
    fsm_init();
    blynk_manager_init(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);

    Serial.println("Setup complete. Listening...");
}

/* =================================================================
 * loop
 * ================================================================= */
void loop(void)
{
    blynk_manager_run();

    uint32_t   now        = (uint32_t)millis();
    AlertState prev_state = fsm_get_state();
    AlertState cur_state  = fsm_update(now);
    uint32_t   elapsed    = fsm_get_elapsed(now);

    /* Detect end-of-alert and clean up hardware */
    if (prev_state != ALERT_NONE && cur_state == ALERT_NONE)
        on_alert_cleared(prev_state);

    disp_update_alert_text((unsigned long)now);

    if (cur_state != ALERT_NONE) {
        update_alert_leds(cur_state, elapsed);
        delay(10);
        return;
    }

    /* ---- Read audio ---- */
    int n = audio_read_samples(s_audio_buf, AUDIO_FFT_SAMPLES);
    if (n < AUDIO_FFT_SAMPLES)
        return;

    /* ---- Load FFT input buffers ---- */
    for (int i = 0; i < AUDIO_FFT_SAMPLES; i++) {
        s_vReal[i] = (double)s_audio_buf[i];
        s_vImag[i] = 0.0;
    }

    /* ---- FFT (custom C implementation) ---- */
    fft_hamming_window(s_vReal, AUDIO_FFT_SAMPLES);
    fft_compute(s_vReal, s_vImag, AUDIO_FFT_SAMPLES);
    fft_complex_to_magnitude(s_vReal, s_vImag, AUDIO_FFT_SAMPLES);

    /* ---- Spectral analysis ---- */
    float total_e    = fft_total_energy(s_vReal, AUDIO_FFT_SAMPLES);
    float doorbell_e = fft_band_energy(s_vReal, AUDIO_FFT_SAMPLES,  800.0f, 1500.0f);
    float smoke_e    = fft_band_energy(s_vReal, AUDIO_FFT_SAMPLES, 2800.0f, 3500.0f);
    fft_fill_bar_magnitudes(s_vReal, AUDIO_FFT_SAMPLES, s_bar_mags, DISP_NUM_BARS);

    /* ---- Classify and (possibly) trigger alert ---- */
    AlertState classified = fsm_classify(total_e, doorbell_e, smoke_e);

    if (classified != ALERT_NONE) {
        AlertState triggered = fsm_trigger(classified, now);
        if (triggered == classified)
            on_alert_triggered(triggered);
    } else if (fsm_is_muted()) {
        fill_solid(leds, NUM_LEDS, CRGB::DarkGray);
        FastLED.show();
    } else if (total_e > 5000.0f) {
        disp_draw_spectrum(s_bar_mags);
        show_loudness_bar();
    } else {
        fill_solid(leds, NUM_LEDS, CRGB::DarkBlue);
        FastLED.show();
        disp_draw_spectrum(s_bar_mags);
    }
}