#include "Disp.hpp"
#include <Arduino.h>
#include <string.h>

#define SCREEN_W       240
#define SCREEN_H       240
#define BAR_WIDTH      (SCREEN_W / DISP_NUM_BARS)
#define ALERT_DISP_MS  3000UL

static TFT_eSPI      s_tft;
static bool          s_showing_alert = false;
static uint16_t      s_alert_color   = TFT_WHITE;
static char          s_alert_text[32] = "";
static unsigned long s_alert_start   = 0;

/* ------------------------------------------------------------------ */

void disp_init(void)
{
    s_tft.init();
    s_tft.setRotation(0);
    s_tft.fillScreen(TFT_BLACK);
    s_tft.setTextSize(2);
    s_tft.setTextColor(TFT_CYAN);
    s_tft.setCursor(20, 80);
    s_tft.println("Sound Classifier");
    s_tft.setCursor(40, 120);
    s_tft.println("Ready...");
    delay(2000);
    s_tft.fillScreen(TFT_BLACK);
}

void disp_draw_spectrum(const float *bar_magnitudes)
{
    float max_mag = 0.0f;
    int   i;

    for (i = 0; i < DISP_NUM_BARS; i++)
        if (bar_magnitudes[i] > max_mag)
            max_mag = bar_magnitudes[i];
    if (max_mag < 1.0f)
        max_mag = 1.0f;

    for (i = 0; i < DISP_NUM_BARS; i++) {
        int      x     = i * BAR_WIDTH;
        int      bar_h = (int)((bar_magnitudes[i] / max_mag) * 200);
        int      y;
        uint16_t color;

        if (bar_h < 0) bar_h = 0;
        y = SCREEN_H - bar_h;

        s_tft.fillRect(x, 0, BAR_WIDTH, SCREEN_H, TFT_BLACK);

        if      (bar_h < 70)  color = TFT_GREEN;
        else if (bar_h < 140) color = TFT_YELLOW;
        else                  color = TFT_RED;

        s_tft.fillRect(x, y, BAR_WIDTH - 1, bar_h, color);
    }
}

void disp_show_alert_text(const char *text, uint16_t color)
{
    s_showing_alert = true;
    s_alert_color   = color;
    s_alert_start   = millis();
    strncpy(s_alert_text, text, sizeof(s_alert_text) - 1);
    s_alert_text[sizeof(s_alert_text) - 1] = '\0';
}

void disp_update_alert_text(unsigned long now_ms)
{
    if (!s_showing_alert)
        return;

    if (now_ms - s_alert_start >= ALERT_DISP_MS) {
        s_tft.fillScreen(TFT_BLACK);
        s_showing_alert = false;
        return;
    }

    s_tft.fillScreen(TFT_BLACK);
    s_tft.setTextSize(4);
    s_tft.setTextColor(s_alert_color, TFT_BLACK);
    s_tft.setCursor(20, 100);
    s_tft.println(s_alert_text);
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
    if (strcmp(alert_text, "NONE") != 0) {
        s_tft.setTextColor(TFT_RED);
        s_tft.println("! ALERT !");
        s_tft.setCursor(20, 140);
        s_tft.println(alert_text);
    } else {
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

void disp_clear(void)
{
    s_tft.fillScreen(TFT_BLACK);
}
