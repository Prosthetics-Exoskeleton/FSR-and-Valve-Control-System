#include <Arduino.h>

const int SENSE = 33;


void setup() {
  // Set pin mode
  Serial.begin(9600);

}

void loop() {
  int volt = analogRead(SENSE);
  Serial.println(volt);
  delay(500);
}