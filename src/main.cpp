#include <Arduino.h>

constexpr uint8_t pins[] = {21, 22, 25, 26};
constexpr uint8_t mypin = 21;

void setup() {
  for (uint8_t pin : pins) {
    pinMode(pin, OUTPUT_OPEN_DRAIN);
    digitalWrite(pin, HIGH);
  }
}

void loop() {
  digitalWrite(mypin, LOW);
  delay(500);
  digitalWrite(mypin, HIGH);
  delay(2500);
}
