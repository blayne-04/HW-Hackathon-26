#include <Arduino.h>
#include "MLInterface.hpp"

void setup()
{
    ml_interface_init();
}

void loop()
{
    ml_interface_tick();
}