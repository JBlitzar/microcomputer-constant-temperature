#pragma once
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

struct Seg {
  uint16_t sp;
  uint32_t ms;
};

namespace solver {

constexpr float T0 = 22.0f;
constexpr float MAX_ERR = 1.0f;
constexpr int LAG_S = 5;
constexpr int GRID_MIN = 30, GRID_MAX = 400;
constexpr int MAX_S = 900;

constexpr int ERR_PARAM = -1;
constexpr int ERR_NOSOLUTION = -2;

// https://www.desmos.com/calculator/reyvmryl8c
constexpr float R = 2.2f;

inline float k_of(float S) {
  return fmaxf(0.0348f - 9.8e-5f * (S - 50.0f), 0.005f);
}

inline float press_s(float dS) { return 0.3f + 0.05f * fabsf(dS); }

inline uint32_t press_ms(int dS) {
  return dS ? 300 + 50 * (uint32_t)abs(dS) : 0;
}

inline float advance(float T, float S, float dt) {
  float k = k_of(S);
  float Tk = S - R / k;
  if (T >= Tk) {
    return S + (T - S) * expf(-k * dt);
  }
  float tk = (Tk - T) / R;
  if (dt <= tk) {
    return T + R * dt;
  }
  return S - (R / k) * expf(-k * (dt - tk));
}

inline int solve(const int (*stages)[3], int nstages, Seg *out, int max_segs) {
  static float target[MAX_S + 1];
  int N = 0;
  float from = T0;
  target[0] = T0;
  for (int s = 0; s < nstages; s++) {
    int temp = stages[s][0], ramp = stages[s][1], hold = stages[s][2];
    if (temp <= from || temp > GRID_MAX || ramp < 1 || hold < 0 ||
        N + ramp + hold > MAX_S) {
      return ERR_PARAM;
    }
    for (int i = 1; i <= ramp + hold; i++) {
      target[N + i] =
          i <= ramp ? from + ((temp - from) * i) / ramp : (float)temp;
    }
    N += ramp + hold;
    from = (float)temp;
  }

  int t = 0, tl = -1, n_out = 0;
  float T = T0;
  while (t < N) {
    int best_n = 0, best_S = -1;
    for (int S = GRID_MIN; S <= GRID_MAX; S++) {
      if (S <= T || (tl >= 0 && t - tl < press_s(S - out[n_out - 1].sp))) {
        continue;
      }
      int n = 0;
      while (t + n < N &&
             fabsf(advance(T, S, n + 1) - target[t + n + 1]) <= MAX_ERR) {
        n++;
      }
      if (n > best_n) {
        best_n = n;
        best_S = S;
      }
    }
    if (best_S < 0 || n_out >= max_segs) {
      return ERR_NOSOLUTION;
    }
    if (n_out > 0) {
      out[n_out - 1].ms =
          (t - tl) * 1000u - press_ms(best_S - out[n_out - 1].sp);
    }
    out[n_out++] = {(uint16_t)best_S, 0};
    T = advance(T, best_S, best_n);
    tl = t;
    t += best_n;
  }
  out[n_out - 1].ms = (N - tl + LAG_S) * 1000u;
  return n_out;
}

} // namespace solver
