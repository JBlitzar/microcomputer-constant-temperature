#include <Arduino.h>
#include <Preferences.h>
#include <SerialCommands.h>

#include "recipe.h"

namespace {

Recipe recipe, staging;

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

} // namespace

bool recipe_setup() {
  for (SerialCommand &c : commands)
    cmds.AddCommand(&c);
  cmds.SetDefaultHandler(
      [](SerialCommands *, const char *) { Serial.println("ERR unknown"); });
  return recipe_load();
}

void recipe_read_serial() { cmds.ReadSerial(); }

const Recipe &current_recipe() { return recipe; }
