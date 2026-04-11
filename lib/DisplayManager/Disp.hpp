#ifndef DISP_HPP
#define DISP_HPP

#include <TFT_eSPI.h>

/* Number of spectrum bars drawn on screen */
#define DISP_NUM_BARS 12

/* Initialise the TFT — call once in setup() */
void disp_init(void);

/* Draw a DISP_NUM_BARS-wide spectrum analyser from bar_magnitudes[] */
void disp_draw_spectrum(const float *bar_magnitudes);

/* Start a 3 s timed alert overlay (text + colour) */
void disp_show_alert_text(const char *text, uint16_t color);

/* Must be called every loop iteration; clears the overlay after 3 s */
void disp_update_alert_text(unsigned long now_ms);

/* General status screen: dominant frequency, amplitude, alert label */
void disp_update_status(const char *alert_text,
                        float frequency, float amplitude);

/* Show full-screen MUTED message */
void disp_show_muted(void);

/* Fill screen with black */
void disp_clear(void);

#endif /* DISP_HPP */
