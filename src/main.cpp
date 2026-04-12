#include <Arduino.h>
#include "AudioProcessor.h"
#include "Disp.hpp"
#include "FSM.h"

static float s_audio_frame[FFT_SIZE];
static float s_fft_magnitudes[FFT_BIN_COUNT];
static float s_spectrum_bars[DISP_NUM_BARS];
static uint32_t s_last_display_ms = 0;
static AlertState s_last_rendered_state = ALERT_NONE;

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
    AlertState detected;
    AlertState state;
    uint32_t now_ms;

    if (audio_read_and_clean(s_audio_frame, FFT_SIZE) < FFT_SIZE)
        return;

    audio_process_fft(s_audio_frame, s_fft_magnitudes);
    audio_extract_features(s_fft_magnitudes, &features);
    audio_compute_spectrum_bars(s_fft_magnitudes, s_spectrum_bars, DISP_NUM_BARS);

    now_ms = millis();
    detected = fsm_classify(features.total_energy,
                            features.energy_doorbell,
                            features.energy_smoke);
    if (detected != ALERT_NONE)
        fsm_trigger(detected, now_ms);

    state = fsm_update(now_ms);

    if (state != s_last_rendered_state || (now_ms - s_last_display_ms) >= DISPLAY_REFRESH_MS)
    {
        if (state == ALERT_NONE)
        {
            disp_render_monitoring(s_spectrum_bars,
                                   features.dominant_frequency_hz,
                                   features.total_energy);
        }
        else
        {
            disp_render_alert(state, now_ms);
        }

        s_last_display_ms = now_ms;
        s_last_rendered_state = state;
    }
}
