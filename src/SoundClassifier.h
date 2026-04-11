#ifndef SOUND_CLASSIFIER_H
#define SOUND_CLASSIFIER_H

#include <Arduino.h>
#include "Config.h"

enum AlertType {
    ALERT_NONE = 0,
    ALERT_SMOKE = 1,
    ALERT_DOORBELL = 2
};

class SoundClassifier {
public:
    SoundClassifier();
    void begin();
    void update();
    AlertType getCurrentAlert();
    String getAlertText(AlertType alert);
    String getShortAlertText(AlertType alert);
    bool isAlertActive();
    void clearAlert();
    void setMuted(bool muted);
    bool isMuted();
    
private:
    AlertType m_currentAlert;
    AlertType m_lastAlert;
    unsigned long m_alertStartTime;
    unsigned long m_lastSmokeTime;
    unsigned long m_lastDoorbellTime;
    bool m_muted;
    
    void triggerAlert(AlertType alert);
    void resetAlert();
};

#endif // SOUND_CLASSIFIER_H