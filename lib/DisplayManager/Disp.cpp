#include "Disp.hpp"
#include <Arduino.h>
#include <stdio.h>
#include "Constants.h"

#define SCREEN_W 240
#define SCREEN_H 240
#define BAR_WIDTH (SCREEN_W / DISP_NUM_BARS)

typedef enum
{
    DISP_MODE_NONE = 0,
    DISP_MODE_MONITORING = 1,
    DISP_MODE_ALERT = 2
} DispMode;

static TFT_eSPI s_tft;
static AlertState s_last_fancy_state = (AlertState)(-1);
static uint32_t s_last_footer_sec = UINT32_MAX;
static DispMode s_mode = DISP_MODE_NONE;

static void disp_draw_fancy_background(void)
{
    for (int y = 0; y < SCREEN_H; y++)
    {
        uint8_t g = (uint8_t)(8 + (y * 24) / SCREEN_H);
        uint8_t b = (uint8_t)(18 + (y * 36) / SCREEN_H);
        uint16_t c = s_tft.color565(4, g, b);
        s_tft.drawFastHLine(0, y, SCREEN_W, c);
    }

    s_tft.drawCircle(SCREEN_W / 2, SCREEN_H / 2, 118, TFT_DARKGREY);
    s_tft.drawCircle(SCREEN_W / 2, SCREEN_H / 2, 117, TFT_DARKGREY);
}

static void disp_draw_state_icon(AlertState state, uint16_t color)
{
    const int cx = SCREEN_W / 2;
    const int cy = 95;

    if (state == ALERT_SMOKE)
    {
        s_tft.fillTriangle(cx, cy - 26, cx - 26, cy + 20, cx + 26, cy + 20, color);
        s_tft.setTextColor(TFT_BLACK, color);
        s_tft.setTextDatum(MC_DATUM);
        s_tft.drawCentreString("!", cx, cy + 2, 4);
    }
    else if (state == ALERT_DOORBELL)
    {
        /* Hanger + neck */
        s_tft.fillRoundRect(cx - 5, cy - 30, 10, 6, 3, color);
        s_tft.fillRect(cx - 3, cy - 24, 6, 7, color);

        /* Dome */
        s_tft.fillCircle(cx, cy - 10, 16, color);

        /* Bell body with a flared lower shape */
        s_tft.fillTriangle(cx - 24, cy + 2, cx + 24, cy + 2, cx, cy + 22, color);
        s_tft.fillRoundRect(cx - 22, cy + 14, 44, 6, 3, color);

        /* Shoulder cuts so it looks like a bell and not a blob */
        s_tft.fillTriangle(cx - 24, cy + 2, cx - 12, cy + 2, cx - 21, cy + 16, TFT_BLACK);
        s_tft.fillTriangle(cx + 24, cy + 2, cx + 12, cy + 2, cx + 21, cy + 16, TFT_BLACK);

        /* Clapper */
        s_tft.fillRect(cx - 2, cy + 7, 4, 8, TFT_BLACK);
        s_tft.fillCircle(cx, cy + 18, 4, TFT_BLACK);
    }
    else
    {
        s_tft.fillCircle(cx, cy, 18, color);
        s_tft.drawLine(cx - 9, cy, cx - 2, cy + 8, TFT_BLACK);
        s_tft.drawLine(cx - 2, cy + 8, cx + 10, cy - 8, TFT_BLACK);
    }
}

/* ------------------------------------------------------------------ */

void disp_init(void)
{
    pinMode(PIN_TFT_BACKLIGHT, OUTPUT);
    digitalWrite(PIN_TFT_BACKLIGHT, HIGH);

    pinMode(TFT_RST, OUTPUT);
    digitalWrite(TFT_RST, LOW);
    delay(10);
    digitalWrite(TFT_RST, HIGH);
    delay(120);

    s_tft.init();
    s_tft.setRotation(2);
    s_tft.fillScreen(TFT_BLACK);
}

#if 0
void disp_hello_world(void)
{
    const char *msg = "HELLO WORLD";

    s_tft.fillScreen(TFT_BLACK);
    s_tft.drawRect(0, 0, 240, 240, TFT_RED);
    s_tft.setTextSize(2);
    s_tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    int x = (SCREEN_W - s_tft.textWidth(msg, 2)) / 2;
    int y = (SCREEN_H / 2) - 8;
    s_tft.drawString(msg, x, y, 2);
}

void disp_show_state_text(AlertState state)
{
    switch (state)
    {
    case ALERT_SMOKE:
        disp_show_alert_text("FIRE ALARM!", TFT_RED);
        break;
    case ALERT_DOORBELL:
        disp_show_alert_text("DOORBELL!", TFT_CYAN);
        break;
    case ALERT_NONE:
    default:
        disp_show_alert_text("MONITORING", TFT_GREEN);
        break;
    }
}
#endif

static void disp_render_fancy_state(AlertState state, uint32_t now_ms)
{
    const char *label;
    uint16_t accent;
    char footer[24];

    if (state == ALERT_SMOKE)
    {
        label = "FIRE ALARM";
        accent = TFT_RED;
    }
    else if (state == ALERT_DOORBELL)
    {
        label = "DOORBELL";
        accent = TFT_CYAN;
    }
    else
    {
        label = "MONITORING";
        accent = TFT_GREEN;
    }

    uint32_t now_sec = now_ms / 1000UL;

    if (state != s_last_fancy_state)
    {
        disp_draw_fancy_background();

        s_tft.setTextColor(TFT_LIGHTGREY);
        s_tft.setTextDatum(MC_DATUM);
        s_tft.drawCentreString("SOUND CLASSIFIER", SCREEN_W / 2, 20, 2);

        disp_draw_state_icon(state, accent);

        s_tft.setTextColor(accent);
        s_tft.drawCentreString(label, SCREEN_W / 2, 160, 4);

        s_last_fancy_state = state;
        s_last_footer_sec = UINT32_MAX;
    }

    if (now_sec != s_last_footer_sec)
    {
        s_tft.fillRect(0, 198, SCREEN_W, 30, TFT_BLACK);
        snprintf(footer, sizeof(footer), "t=%lus", (unsigned long)now_sec);
        s_tft.setTextColor(TFT_WHITE);
        s_tft.setTextDatum(MC_DATUM);
        s_tft.drawCentreString(footer, SCREEN_W / 2, 210, 2);
        s_last_footer_sec = now_sec;
    }
}

static void disp_draw_spectrum(const float *bar_magnitudes)
{
    float max_mag = 0.0f;
    int i;

    for (i = 0; i < DISP_NUM_BARS; i++)
        if (bar_magnitudes[i] > max_mag)
            max_mag = bar_magnitudes[i];
    if (max_mag < 1.0f)
        max_mag = 1.0f;

    for (i = 0; i < DISP_NUM_BARS; i++)
    {
        int x = i * BAR_WIDTH;
        int bar_h = (int)((bar_magnitudes[i] / max_mag) * 200);
        int y;
        uint16_t color;

        if (bar_h < 0)
            bar_h = 0;
        y = SCREEN_H - bar_h;

        s_tft.fillRect(x, 0, BAR_WIDTH, SCREEN_H, TFT_BLACK);

        if (bar_h < 70)
            color = TFT_GREEN;
        else if (bar_h < 140)
            color = TFT_YELLOW;
        else
            color = TFT_RED;

        s_tft.fillRect(x, y, BAR_WIDTH - 1, bar_h, color);
    }
}

#if 0
void disp_show_alert_text(const char *text, uint16_t color)
{
    s_showing_alert = true;
    s_alert_color = color;
    s_alert_start = millis();
    strncpy(s_alert_text, text, sizeof(s_alert_text) - 1);
    s_alert_text[sizeof(s_alert_text) - 1] = '\0';
}

void disp_update_alert_text(unsigned long now_ms)
{
    if (!s_showing_alert)
        return;

    if (now_ms - s_alert_start >= DISPLAY_ALERT_DURATION_MS)
    {
        s_tft.fillScreen(TFT_BLACK);
        s_showing_alert = false;
        return;
    }

    s_tft.fillScreen(TFT_BLACK);
    s_tft.setTextSize(3);
    s_tft.setTextColor(s_alert_color, TFT_BLACK);
    s_tft.setTextDatum(MC_DATUM);
    s_tft.drawCentreString(s_alert_text, 120, 120, 2);
}

void disp_update_status(const char *alert_text,
                        float frequency, float amplitude)
{
    s_tft.fillScreen(TFT_BLACK);
    s_tft.setTextSize(2);

    s_tft.setCursor(20, 30);
    s_tft.setTextColor(TFT_CYAN);
    s_tft.print("Freq: ");
    s_tft.print(frequency, 0);
    s_tft.println(" Hz");

    s_tft.setCursor(20, 60);
    s_tft.setTextColor(TFT_YELLOW);
    s_tft.print("Level: ");
    s_tft.print(amplitude, 1);

    s_tft.setCursor(20, 110);
    if (strcmp(alert_text, "NONE") != 0)
    {
        s_tft.setTextColor(TFT_RED);
        s_tft.println("! ALERT !");
        s_tft.setCursor(20, 140);
        s_tft.println(alert_text);
    }
    else
    {
        s_tft.setTextColor(TFT_GREEN);
        s_tft.println("SAFE");
    }

    s_tft.setTextSize(1);
    s_tft.setCursor(20, 200);
    s_tft.setTextColor(TFT_WHITE);
    s_tft.println("System Monitoring");
}

void disp_show_muted(void)
{
    s_tft.fillScreen(TFT_BLACK);
    s_tft.setTextSize(3);
    s_tft.setTextColor(TFT_WHITE);
    s_tft.setCursor(50, 100);
    s_tft.println("MUTED");
}
#endif

void disp_clear(void)
{
    s_tft.fillScreen(TFT_BLACK);
    s_mode = DISP_MODE_NONE;
    s_last_fancy_state = (AlertState)(-1);
}

void disp_render_monitoring(const float *bar_magnitudes,
                            float dominant_frequency_hz,
                            float total_energy)
{
    static const float s_empty_bars[DISP_NUM_BARS] = {0};
    const float *bars = (bar_magnitudes != NULL) ? bar_magnitudes : s_empty_bars;
    char line[32];

    if (s_mode != DISP_MODE_MONITORING)
    {
        s_tft.fillScreen(TFT_BLACK);
        s_mode = DISP_MODE_MONITORING;
        s_last_fancy_state = (AlertState)(-1);
        s_last_footer_sec = UINT32_MAX;
    }

    disp_draw_spectrum(bars);

    s_tft.fillRect(0, 0, SCREEN_W, 74, TFT_BLACK);
    s_tft.fillRect(0, 204, SCREEN_W, 36, TFT_BLACK);

    s_tft.setTextDatum(MC_DATUM);
    s_tft.setTextColor(TFT_GREEN, TFT_BLACK);
    s_tft.drawCentreString("MONITORING", SCREEN_W / 2, 14, 4);

    s_tft.setTextColor(TFT_CYAN, TFT_BLACK);
    snprintf(line, sizeof(line), "Peak %.0f Hz", dominant_frequency_hz);
    s_tft.drawCentreString(line, SCREEN_W / 2, 48, 2);

    s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    snprintf(line, sizeof(line), "Energy %.0f", total_energy);
    s_tft.drawCentreString(line, SCREEN_W / 2, 214, 2);
}

void disp_render_alert(AlertState state, uint32_t now_ms)
{
    if (s_mode != DISP_MODE_ALERT)
    {
        s_mode = DISP_MODE_ALERT;
        s_last_fancy_state = (AlertState)(-1);
        s_last_footer_sec = UINT32_MAX;
    }

    disp_render_fancy_state(state, now_ms);
}

#if 0
void disp_demo_setup(void)
{
    fsm_init();
    disp_init();

#if DISP_TEST_CYCLE_STATES
    disp_clear();
#else
    disp_hello_world();
#endif
}

void disp_demo_loop(uint32_t now_ms)
{
    AlertState state;

#if DISP_TEST_CYCLE_STATES
    state = disp_test_cycle_state(now_ms);
#else
    fsm_update(now_ms);
    state = fsm_get_state();
#endif

#if DISP_USE_FANCY_UI
    disp_render_fancy_state(state, now_ms);
#else
    disp_show_state_text(state);
    disp_update_alert_text(now_ms);
#endif
}
#endif
