#include <Arduino.h>
#include "../lib/AudioProcessor/AudioProcessor.h"
#include "../lib/BlynkManager/Blynk.hpp"
#include "../lib/DisplayManager/Disp.hpp"
#include "../lib/Logic/FSM.h"

void setup()
{
  Serial.begin(115200);
  audio_init(); // Initialize the "Ears"
}

void loop()
{
 audio_test_plotter();
  // Small delay so we don't spam the Serial port too hard
  delay(10);
}