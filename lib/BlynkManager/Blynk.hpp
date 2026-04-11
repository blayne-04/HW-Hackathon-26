#ifndef BLYNK_MANAGER_HPP
#define BLYNK_MANAGER_HPP

/* Connect to WiFi and start the Blynk session — call once in setup() */
void blynk_manager_init(const char *auth, const char *ssid, const char *pass);

/* Maintain the Blynk connection — call every loop iteration */
void blynk_manager_run(void);

/* Push alert notifications to the Blynk app */
void blynk_send_smoke_alert(void);
void blynk_send_doorbell_alert(void);

/* Push a plain-text status string (e.g. "Monitoring" / "ALERT ACTIVE") */
void blynk_send_status(const char *status_text);

/* Called internally by the BLYNK_WRITE(V2) mute handler;
   also available for direct use (e.g. unit tests). */
void blynk_handle_mute(int value);

#endif /* BLYNK_MANAGER_HPP */
