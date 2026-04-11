#ifndef HEADER_HPP
#define HEADER_HPP

/* ---- Project-wide constants (pins, thresholds, credentials) ---- */
#include "Constants.h"

/* ---- C modules: AudioProcessor and FSM ----
   Both headers carry their own extern "C" guards so they are safe
   to include from either C or C++ translation units.              */
#include "AudioProcessor.h"
#include "FSM.h"

/* ---- C++ modules: Display and Blynk ---- */
#ifdef __cplusplus
#include "Disp.hpp"
#include "Blynk.hpp"
#endif

/* ================================================================
 * Extern declarations for functions defined in sound.cpp that may
 * be called from other C++ translation units.
 * ================================================================ */
#ifdef __cplusplus
extern void show_loudness_bar(void);
extern void update_alert_leds(AlertState state, uint32_t elapsed_ms);
extern void on_alert_triggered(AlertState state);
extern void on_alert_cleared(AlertState previous);
#endif

#endif /* HEADER_HPP */
