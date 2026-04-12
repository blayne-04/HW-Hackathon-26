#ifndef DISP_HPP
#define DISP_HPP

#include "Constants.h" 
#include <TFT_eSPI.h>
#include "FSM.h"

/* Number of spectrum bars drawn on screen */

/* Initialise the TFT — call once in setup() */
void disp_init(void);

/* Fill screen with black */
void disp_clear(void);

/* Live monitoring view with spectrum bars and core metrics. */
void disp_render_monitoring(const float *bar_magnitudes,
                            float dominant_frequency_hz,
                            float total_energy);

/* Alert view driven by the FSM state machine. */
void disp_render_alert(AlertState state, uint32_t now_ms);

#endif /* DISP_HPP */
