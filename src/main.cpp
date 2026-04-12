#include <Arduino.h>
#include "../lib/AudioProcessor/AudioProcessor.h"

void setup()
{
    Serial.begin(115200);
    audio_init();
}

void loop()
{
    audio_test_plotter();
    delay(10);
}