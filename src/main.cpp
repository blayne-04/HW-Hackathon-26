// #include <Arduino.h>

// // put function declarations here:
// int myFunction(int, int);

// void setup() {
//   // put your setup code here, to run once:
//   int result = myFunction(2, 3);
// }

// void loop() {
//   // put your main code here, to run repeatedly:
// }

// // put function definitions here:
// int myFunction(int x, int y) {
//   return x + y;
// }

#include <Arduino.h>
#define LED_PIN 15

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Setup done");
}

void loop()
{
  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
  Serial.println("Looping...");
}

// GPIO 25, 26, 33
// Word Select, Serial Clock, Serial Data