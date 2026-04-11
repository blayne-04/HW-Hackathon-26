#ifndef FSM_H
#define FSM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    ALERT_NONE     = 0,
    ALERT_SMOKE    = 1,
    ALERT_DOORBELL = 2
} AlertState;

/* Initialise all FSM state to defaults */
void       fsm_init(void);

/* Mute flag — when muted, fsm_trigger() is a no-op */
int        fsm_is_muted(void);
void       fsm_set_muted(int muted);

/* Attempt to activate an alert.  Respects mute flag and per-type cooldown.
   Returns the resulting state (may be unchanged if blocked). */
AlertState fsm_trigger(AlertState type, uint32_t now_ms);

/* Per-loop call: auto-clears alerts that have finished their duration.
   Returns the current state after any auto-clear. */
AlertState fsm_update(uint32_t now_ms);

/* Inspection */
AlertState fsm_get_state(void);
uint32_t   fsm_get_alert_start(void);
uint32_t   fsm_get_elapsed(uint32_t now_ms);

/* Pure classification: returns ALERT_* or ALERT_NONE based on band ratios.
   No side-effects; does NOT respect mute or cooldown. */
AlertState fsm_classify(float total_energy,
                        float energy_doorbell,
                        float energy_smoke);

#ifdef __cplusplus
}
#endif

#endif /* FSM_H */
