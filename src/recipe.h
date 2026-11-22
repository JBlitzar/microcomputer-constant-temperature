#pragma once

#include "solver.h"

constexpr int SETPOINT_MAX = 400;
constexpr int MAX_SEGS = 64;

struct Recipe {
  uint16_t count;
  Seg segs[MAX_SEGS];
};

bool recipe_setup();
void recipe_read_serial();
const Recipe &current_recipe();
