#include <Arduino.h>
#include <Preferences.h>
#include <SerialCommands.h>
#include <driver/gpio.h>

#include "solver.h"

constexpr uint8_t DOWN = 21;
constexpr uint8_t UP = 22;
constexpr uint8_t SET = 25;
constexpr uint8_t ENT = 26;
constexpr uint8_t pins[] = {DOWN, UP, SET, ENT};

constexpr int SETPOINT_MIN = 0;
constexpr int SETPOINT_MAX = 400;
constexpr int MAX_SEGS = 64;

constexpr int MV_LOW = 500;
constexpr int MV_HIGH = 1000;
constexpr int DOUBLEPRESS_MAX = 1000;
constexpr int PRESS_MIN_MS = 50;
constexpr int PRESS_MAX_MS = 800;





int setpoint = 0;

struct Recipe {
  uint16_t count;
  Seg segs[MAX_SEGS];
};
Recipe recipe, staging;

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

bool recipe_ok(const Recipe &r) {
  if (r.count < 1 || r.count > MAX_SEGS)
    return false;
  for (int i = 0; i < r.count; i++) {
    if (r.segs[i].sp > SETPOINT_MAX)
      return false;
  }
  return true;
}

bool recipe_load() {
  Preferences p;
  bool ok = p.begin("recipe", true) &&
            p.getBytes("r", &recipe, sizeof recipe) == sizeof recipe;
  p.end();
  if (!ok || !recipe_ok(recipe))
    recipe.count = 0;
  return recipe.count > 0;
}

void recipe_save(const Recipe &r) {
  Preferences p;
  p.begin("recipe", false);
  bool ok = p.putBytes("r", &r, sizeof r) == sizeof r;
  p.end();
  if (ok && recipe_load()) {
    uint32_t total = 0;
    for (int i = 0; i < recipe.count; i++)
      total += recipe.segs[i].ms;
    Serial.printf("OK %u %lu\n", recipe.count, (unsigned long)total);
  } else {
    Serial.println("ERR nvs");
  }
}

bool read_n_integer_args(SerialCommands *s, int *a, int n) {
  for (int i = 0; i < n; i++) {
    char *tok = s->Next(), *end;
    if (!tok)
      return false;
    a[i] = strtol(tok, &end, 10);
    if (*end)
      return false;
  }
  return s->Next() == nullptr;
}

void solve_stages(SerialCommands *s, int nstages) {
  int a[6];
  int n = read_n_integer_args(s, a, 3 * nstages)
              ? solver::solve(reinterpret_cast<const int (*)[3]>(a), nstages,
                              staging.segs, MAX_SEGS)
              : solver::ERR_PARAM;
  if (n == solver::ERR_PARAM)
    Serial.println("ERR param");
  else if (n < 0)
    Serial.println("ERR nosolution");
  else {
    staging.count = n;
    recipe_save(staging);
  }
}


void cmd_begin(SerialCommands *s) {
  staging.count = 0;
  Serial.println("OK");
}

void cmd_seg(SerialCommands *s) {
  int a[2];
  if (!read_n_integer_args(s, a, 2) || a[0] < 0 || a[0] > SETPOINT_MAX || a[1] < 0)
    Serial.println("ERR range");
  else if (staging.count >= MAX_SEGS)
    Serial.println("ERR full");
  else {
    staging.segs[staging.count++] = {(uint16_t)a[0], (uint32_t)a[1]};
    Serial.println("OK");
  }
}

void cmd_commit(SerialCommands *s) {
  if (staging.count == 0)
    Serial.println("ERR empty");
  else
    recipe_save(staging);
}

void cmd_prh(SerialCommands *s) { solve_stages(s, 1); }
void cmd_sr(SerialCommands *s) { solve_stages(s, 2); }

void cmd_get(SerialCommands *s) {
  for (int i = 0; i < recipe.count; i++) {
    Serial.printf("SEG %u %lu\n", recipe.segs[i].sp,
                  (unsigned long)recipe.segs[i].ms);
  }
  Serial.println("END");
}

void cmd_clear(SerialCommands *s) {
  Preferences p;
  p.begin("recipe", false);
  p.remove("r");
  p.end();
  recipe.count = 0;
  Serial.println("OK");
}

char cmd_buf[96];
SerialCommands cmds(&Serial, cmd_buf, sizeof cmd_buf, "\n", " \r");
SerialCommand commands[] = {
    {"BEGIN", cmd_begin}, {"SEG", cmd_seg}, {"COMMIT", cmd_commit},
    {"PRH", cmd_prh},     {"SR", cmd_sr},   {"GET", cmd_get},
    {"CLEAR", cmd_clear},
};

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
  for (SerialCommand &c : commands)
    cmds.AddCommand(&c);
  cmds.SetDefaultHandler(
      [](SerialCommands *, const char *) { Serial.println("ERR unknown"); });
  disarm_and_ignore_for_ms(2000);
  Serial.println(recipe_load() ? "READY recipe" : "READY none");
}

void loop() {
  cmds.ReadSerial();
  if (double_pressed(millis(), analogReadMilliVolts(ENT))) {
    if (recipe.count > 0)
      run_recipe();
    else
      Serial.println("ARM norecipe");
  }
  delay(2);
}
