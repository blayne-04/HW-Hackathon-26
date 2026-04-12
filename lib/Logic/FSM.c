#include "FSM.h"
#include "Constants.h"

/* Thresholds and timing now come from Constants.h. */

/* ---- Alert auto-clear durations ---- */
/* Smoke: 10 flashes × 200 ms = 2 000 ms */
/* Doorbell: NUM_LEDS * 2 steps * 20 ms = 30 * 2 * 20 = 1 200 ms */

/* ---- Private state ---- */
static AlertState s_state      = ALERT_NONE;
static uint32_t   s_start_ms   = 0;
static uint32_t   s_last_smoke = 0;
static uint32_t   s_last_door  = 0;
static int        s_muted      = 0;

/* ================================================================ */

void fsm_init(void)
{
    s_state      = ALERT_NONE;
    s_start_ms   = 0;
    s_last_smoke = 0;
    s_last_door  = 0;
    s_muted      = 0;
}

int fsm_is_muted(void)       { return s_muted; }
void fsm_set_muted(int m)    { s_muted = m; }

AlertState fsm_get_state(void)              { return s_state; }
uint32_t   fsm_get_alert_start(void)        { return s_start_ms; }
uint32_t   fsm_get_elapsed(uint32_t now_ms) { return now_ms - s_start_ms; }

AlertState fsm_trigger(AlertState type, uint32_t now_ms)
{
    if (s_muted || s_state != ALERT_NONE)
        return s_state;

    if (type == ALERT_SMOKE) {
        if (now_ms - s_last_smoke < ALERT_COOLDOWN_MS)
            return s_state;
        s_last_smoke = now_ms;
    } else if (type == ALERT_DOORBELL) {
        if (now_ms - s_last_door < ALERT_COOLDOWN_MS)
            return s_state;
        s_last_door = now_ms;
    } else {
        return s_state;
    }

    s_state    = type;
    s_start_ms = now_ms;
    return s_state;
}

AlertState fsm_update(uint32_t now_ms)
{
    uint32_t elapsed;

    if (s_state == ALERT_NONE)
        return ALERT_NONE;

    elapsed = now_ms - s_start_ms;

    if (s_state == ALERT_SMOKE && elapsed >= ALERT_SMOKE_DURATION_MS)
        s_state = ALERT_NONE;
    else if (s_state == ALERT_DOORBELL && elapsed >= ALERT_DOORBELL_DURATION_MS)
        s_state = ALERT_NONE;

    return s_state;
}

AlertState fsm_classify(float total_energy,
                        float energy_doorbell,
                        float energy_smoke)
{
    float ratio_smoke, ratio_door;

    if (total_energy < MIN_TOTAL_ENERGY)
        return ALERT_NONE;

    ratio_smoke = energy_smoke    / total_energy;
    ratio_door  = energy_doorbell / total_energy;

    if (ratio_smoke > SMOKE_RATIO)
        return ALERT_SMOKE;
    if (ratio_door  > DOORBELL_RATIO)
        return ALERT_DOORBELL;

    return ALERT_NONE;
}
