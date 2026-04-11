/* Constants.h must be included BEFORE BlynkSimpleEsp32.h so that
   BLYNK_TEMPLATE_ID / BLYNK_TEMPLATE_NAME are defined first. */
#include "Constants.h"
#include <BlynkSimpleEsp32.h>

#include "Blynk.hpp"
#include "FSM.h"
#include "Disp.hpp"

/* Virtual pin assignments (must match the Blynk dashboard) */
#define VPIN_NOTIFICATION  V1
#define VPIN_MUTE          V2
#define VPIN_STATUS        V3

/* ---- Blynk remote-mute handler ---- */
BLYNK_WRITE(VPIN_MUTE)
{
    blynk_handle_mute(param.asInt());
}

/* ================================================================ */

void blynk_manager_init(const char *auth, const char *ssid, const char *pass)
{
    Blynk.begin(auth, ssid, pass);
}

void blynk_manager_run(void)
{
    Blynk.run();
}

void blynk_send_smoke_alert(void)
{
    Blynk.virtualWrite(VPIN_NOTIFICATION, "SMOKE ALARM DETECTED");
}

void blynk_send_doorbell_alert(void)
{
    Blynk.virtualWrite(VPIN_NOTIFICATION, "Doorbell pressed");
}

void blynk_send_status(const char *status_text)
{
    Blynk.virtualWrite(VPIN_STATUS, status_text);
}

void blynk_handle_mute(int value)
{
    fsm_set_muted(value);
    if (value)
        disp_show_muted();
    else
        disp_clear();
}
