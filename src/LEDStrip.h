#ifndef LED_STRIP_H
#define LED_STRIP_H

#include <Arduino.h>
#include <FastLED.h>
#include "Config.h"
#include "SoundClassifier.h"

class LEDStrip {
public:
    LEDStrip();
    void begin();
    void update();
    void triggerAlert(AlertType alert);
    void turnOff();
    bool isActive();
    
private:
    CRGB m_leds[NUM_LEDS];
    bool m_active;
    AlertType m_currentAlert;
    unsigned long m_startTime;
    
    void playSmokeEffect();
    void playDoorbellEffect();
    void setAllLEDs(CRGB color);
};

#endif // LED_STRIP_H