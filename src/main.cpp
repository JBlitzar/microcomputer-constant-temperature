#include <Arduino.h>
#include <driver/gpio.h>

#include "recipe.h"

constexpr uint8_t DOWN = 21;
constexpr uint8_t UP = 22;
constexpr uint8_t SET = 25;
constexpr uint8_t ENT = 26;
constexpr uint8_t pins[] = {DOWN, UP, SET, ENT};

constexpr int SETPOINT_MIN = 0;

constexpr int MV_LOW = 500;
constexpr int MV_HIGH = 1000;
constexpr int DOUBLEPRESS_MAX = 1000;
constexpr int PRESS_MIN_MS = 50;
constexpr int PRESS_MAX_MS = 800;

int setpoint = 0;

void press_button_n_times(uint8_t btn, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(btn, LOW);
    delay(25);
    digitalWrite(btn, HIGH);
    delay(25);
  }
}

void press_button(uint8_t btn) {
  digitalWrite(btn, LOW);
  delay(50);
  digitalWrite(btn, HIGH);
  delay(100);
}

void resync_setpoint() {
  press_button(SET);
  press_button_n_times(DOWN, SETPOINT_MAX);
  press_button(ENT);
  setpoint = SETPOINT_MIN;
}

void set_setpoint(int target) {
  target = constrain(target, SETPOINT_MIN, SETPOINT_MAX);
  int diff = target - setpoint;
  if (diff == 0) {
    return;
  }
  press_button(SET);
  press_button_n_times(diff > 0 ? UP : DOWN, abs(diff));
  press_button(ENT);
  setpoint = target;
}

bool pressed = false;
int presses = 0;
uint32_t press_start = 0, last_release = 0, ignore_until = 0;

void disarm_and_ignore_for_ms(uint32_t ms) {
  pressed = false;
  presses = 0;
  ignore_until = millis() + ms;
}

bool double_pressed(uint32_t now, int mv) {
  if ((int32_t)(now - ignore_until) < 0)
    return false;
  if (!pressed) {
    if (mv < MV_LOW) {
      if (now - last_release > DOUBLEPRESS_MAX)
        presses = 0;
      pressed = true;
      press_start = now;
    } else if (presses == 2 && now - last_release > DOUBLEPRESS_MAX) {
      presses = 0;
      return true;
    }
    return false;
  }
  if (mv > MV_HIGH) {
    uint32_t held = now - press_start;
    pressed = false;
    if (held >= PRESS_MIN_MS && held <= PRESS_MAX_MS && presses < 2) {
      presses++;
      last_release = now;
    } else {
      presses = 0;
    }
  }
  return false;
}

void run_recipe() {
  const Recipe &recipe = current_recipe();
  Serial.println("RUN start");

  gpio_set_level((gpio_num_t)ENT, 1);
  pinMode(ENT, OUTPUT_OPEN_DRAIN);
  digitalWrite(ENT, HIGH);
  delay(1500);
  set_setpoint(22);
  delay(1000);
  resync_setpoint();
  for (int i = 0; i < recipe.count; i++) {
    set_setpoint(recipe.segs[i].sp);
    Serial.printf("RUN seg %d sp %u ms %lu\n", i, recipe.segs[i].sp,
                  (unsigned long)recipe.segs[i].ms);
    delay(recipe.segs[i].ms);
  }
  set_setpoint(0);
  pinMode(ENT, INPUT);
  disarm_and_ignore_for_ms(2000);
  Serial.println("RUN done");
}

void setup() {
  Serial.begin(115200);
  for (uint8_t pin : pins) {
    pinMode(pin, OUTPUT_OPEN_DRAIN);
    digitalWrite(pin, HIGH);
  }
  pinMode(ENT, INPUT);
  bool loaded = recipe_setup();
  disarm_and_ignore_for_ms(2000);
  Serial.println(loaded ? "READY recipe" : "READY none");
}

void loop() {
  recipe_read_serial();
  if (double_pressed(millis(), analogReadMilliVolts(ENT))) {
    if (current_recipe().count > 0)
      run_recipe();
    else
      Serial.println("ARM norecipe");
  }
  delay(2);
}
