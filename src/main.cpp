#include <Arduino.h>
#include "AudioProcessor.h"
#include "Disp.hpp"
#include "FSM.h"

static float s_audio_frame[FFT_SIZE];
static float s_fft_magnitudes[FFT_BIN_COUNT];
static float s_spectrum_bars[DISP_NUM_BARS];
static uint32_t s_last_display_ms = 0;
static AlertState s_last_rendered_state = ALERT_NONE;

/* How often to dump the spectrum peaks to Serial (ms) */
#define SERIAL_SPECTRUM_INTERVAL_MS 2000UL
static uint32_t s_last_serial_ms = 0;

void setup()
{
    Serial.begin(115200);
    audio_init();
    fsm_init();
    disp_init();
    disp_clear();
}

void loop()
{
    AudioFeatures features;
    AlertState    detected, state;
    uint32_t      now_ms = millis();

    /* --- Capture + clean one audio frame --- */
    if (audio_read_and_clean(s_audio_frame, FFT_SIZE) < FFT_SIZE)
        return;

    /* --- FFT + feature extraction --- */
    audio_process_fft(s_audio_frame, s_fft_magnitudes);
    audio_extract_features(s_fft_magnitudes, &features);
    audio_compute_spectrum_bars(s_fft_magnitudes, s_spectrum_bars, DISP_NUM_BARS);

    /* --- Serial Plotter: waveform (every frame) ---
     * Open Tools > Serial Plotter at 115200 to see the normalised
     * waveform with ±1.0 reference lines. Comment out the plotter
     * call if you want the text output below to be readable.      */
    // audio_print_waveform();

    /* --- Serial Monitor: top-5 peaks every 2 s --- */
    if (now_ms - s_last_serial_ms >= SERIAL_SPECTRUM_INTERVAL_MS) {
        s_last_serial_ms = now_ms;
        printf("dom: %.0f Hz  energy: %.2f  door: %.2f  smoke: %.2f\n",
               features.dominant_frequency_hz,
               features.total_energy,
               features.energy_doorbell,
               features.energy_smoke);
        audio_print_spectrum(5);
    }

    /* --- FSM alert logic --- */
    detected = fsm_classify(features.total_energy,
                            features.energy_doorbell,
                            features.energy_smoke);
    if (detected != ALERT_NONE)
        fsm_trigger(detected, now_ms);

    state = fsm_update(now_ms);

    /* --- Display refresh --- */
    if (state != s_last_rendered_state ||
        (now_ms - s_last_display_ms) >= DISPLAY_REFRESH_MS)
    {
        if (state == ALERT_NONE)
            disp_render_monitoring(s_spectrum_bars,
                                   features.dominant_frequency_hz,
                                   features.total_energy);
        else
            disp_render_alert(state, now_ms);

        s_last_display_ms     = now_ms;
        s_last_rendered_state = state;
    }
}

