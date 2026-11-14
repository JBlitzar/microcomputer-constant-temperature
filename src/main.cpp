#include <Arduino.h>
#include <cstdint>

constexpr uint8_t pins[] = {21, 22, 25, 26};
constexpr uint8_t DOWN = 21;
constexpr uint8_t UP = 22;
constexpr uint8_t SET = 25;
constexpr uint8_t ENT = 26;

int setpoint = 0;

void press_button_n_times(uint8_t btn, int times){
    for (int i=0;i<times;i++){
        digitalWrite(btn, LOW);
        delay(25);
        digitalWrite(btn, HIGH);
        delay(25);
    }
}

void press_button(uint8_t btn){
    digitalWrite(btn, LOW);
    delay(50);
    digitalWrite(btn, HIGH);
    delay(100);
}

void set_setpoint(int target){
    int diff = target - setpoint;
    if(diff == 0){
        return;
    }
    if(diff > 0){
        press_button(SET);
        press_button_n_times(UP, diff);
        press_button(ENT);
    }else{
        press_button(SET);
        press_button_n_times(DOWN, -1 * diff);
        press_button(ENT);

    }
    setpoint = target;
}

void setup() {
  for (uint8_t pin : pins) {
    pinMode(pin, OUTPUT_OPEN_DRAIN);
    digitalWrite(pin, HIGH);
  }
  delay(5*1000);

  press_button(SET);
  press_button_n_times(DOWN, 400);
  press_button(ENT);

  delay(1000 * 10);
  set_setpoint(100);
  delay(1000 * 10);
  set_setpoint(50);
  delay(1000 * 5);
  set_setpoint(0);
  delay(1000 * 5);
}


void loop() {

    delay(1000);

}
