/*
 * Copyright (C) 2026 Bluenixx
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

// Dependency-free, anti-aliased shape rasterizers used by the Material 3 Expressive recovery UI.
// Every Fill* helper walks the bounding box of the shape and reports a coverage value (0-255) for
// each pixel through the |blend(x, y, coverage)| callback, so the same code can be driven by the
// minui backend (graphics.cpp) and by the host preview tool.

#include <stdint.h>

#include <algorithm>
#include <cmath>

// One term of a radial "harmonic" shape: r(t) = R * (1 + sum(amp_i * cos(lobes_i * t + rot_i))).
// Animating the amplitudes morphs between circle, cookie, flower, squircle and so on.
struct GRShapeHarmonic {
  int lobes;
  float amp;
  float rot;  // radians
};

namespace md3 {

inline float Clamp01(float v) {
  return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
}

// Converts a signed distance in pixels (negative inside) to an anti-aliased coverage in [0, 255].
inline uint8_t CoverageFromDistance(float d) {
  return static_cast<uint8_t>(Clamp01(0.5f - d) * 255.f + 0.5f);
}

inline float EaseOutCubic(float t) {
  t = Clamp01(t);
  float u = 1.f - t;
  return 1.f - u * u * u;
}

inline float EaseInOutCubic(float t) {
  t = Clamp01(t);
  return t < 0.5f ? 4.f * t * t * t : 1.f - std::pow(-2.f * t + 2.f, 3.f) / 2.f;
}

// Emphasized-decelerate-like curve with a little overshoot, used for expressive "springy" motion.
inline float EaseOutBack(float t) {
  t = Clamp01(t);
  const float c1 = 1.2f, c3 = c1 + 1.f;
  float u = t - 1.f;
  return 1.f + c3 * u * u * u + c1 * u * u;
}

inline float Lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

// Signed distance to a rounded rectangle centered at (cx, cy) with half extents (hw, hh).
inline float RoundRectDistance(float px, float py, float cx, float cy, float hw, float hh,
                               float r) {
  float qx = std::fabs(px - cx) - (hw - r);
  float qy = std::fabs(py - cy) - (hh - r);
  float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
  return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - r;
}

inline float HarmonicRadius(float theta, float radius, const GRShapeHarmonic* h, int n,
                            float* d_dtheta) {
  float s = 1.f, ds = 0.f;
  for (int i = 0; i < n; ++i) {
    float a = h[i].lobes * theta + h[i].rot;
    s += h[i].amp * std::cos(a);
    ds -= h[i].amp * h[i].lobes * std::sin(a);
  }
  if (d_dtheta) *d_dtheta = radius * ds;
  return radius * s;
}

// First-order signed distance to a harmonic shape centered at the origin.
inline float HarmonicDistance(float dx, float dy, float radius, const GRShapeHarmonic* h, int n) {
  float rho = std::sqrt(dx * dx + dy * dy);
  float dr = 0.f;
  float r = HarmonicRadius(std::atan2(dy, dx), radius, h, n, &dr);
  float g = std::sqrt(1.f + (dr * dr) / std::max(rho * rho, 1.f));
  return (rho - r) / g;
}

template <class Blend>
void FillRoundRect(int x1, int y1, int x2, int y2, int radius, Blend&& blend) {
  if (x2 <= x1 || y2 <= y1) return;
  float hw = (x2 - x1) / 2.f, hh = (y2 - y1) / 2.f;
  float r = std::min<float>(radius, std::min(hw, hh));
  float cx = x1 + hw, cy = y1 + hh;
  int ir = static_cast<int>(std::ceil(r));
  for (int y = y1; y < y2; ++y) {
    bool y_in_corner = (y < y1 + ir) || (y >= y2 - ir);
    for (int x = x1; x < x2; ++x) {
      if (!y_in_corner || (x >= x1 + ir && x < x2 - ir)) {
        blend(x, y, 255);
        continue;
      }
      uint8_t c = CoverageFromDistance(RoundRectDistance(x + 0.5f, y + 0.5f, cx, cy, hw, hh, r));
      if (c) blend(x, y, c);
    }
  }
}

template <class Blend>
void FillShape(int cx, int cy, int radius, const GRShapeHarmonic* h, int n, Blend&& blend) {
  float max_amp = 0.f;
  for (int i = 0; i < n; ++i) max_amp += std::fabs(h[i].amp);
  const float r_in = std::max(0.f, radius * (1.f - max_amp) - 2.f);  // surely inside
  const float r_out = radius * (1.f + max_amp) + 2.f;                // surely outside
  const int ext = static_cast<int>(std::ceil(r_out));
  for (int y = cy - ext; y <= cy + ext; ++y) {
    for (int x = cx - ext; x <= cx + ext; ++x) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      float rho2 = dx * dx + dy * dy;
      if (rho2 >= r_out * r_out) continue;
      if (rho2 <= r_in * r_in) {
        blend(x, y, 255);
        continue;
      }
      uint8_t c = CoverageFromDistance(HarmonicDistance(dx, dy, static_cast<float>(radius), h, n));
      if (c) blend(x, y, c);
    }
  }
}

// A horizontal sine wave stroke from x1 to x2 with round caps. |phase| is in radians and moves the
// crest horizontally, which gives the "flowing" wavy progress indicator of Material 3 Expressive.
template <class Blend>
void FillWave(int x1, int x2, int cy, float amp, float wavelength, float phase, int thickness,
              Blend&& blend) {
  if (x2 <= x1 || thickness <= 0) return;
  const float k = 6.2831853f / std::max(wavelength, 1.f);
  const float half = thickness / 2.f;
  int ext = static_cast<int>(std::ceil(amp + half)) + 2;
  float sx = x1 + half, ex = x2 - half;  // centre-line end points (caps stay inside [x1, x2])
  for (int y = cy - ext; y <= cy + ext; ++y) {
    for (int x = x1; x < x2; ++x) {
      float px = x + 0.5f, py = y + 0.5f;
      float cxp = std::min(std::max(px, sx), ex);
      float wy = cy + amp * std::sin(k * (cxp - sx) + phase);
      float slope = amp * k * std::cos(k * (cxp - sx) + phase);
      float d;
      if (px >= sx && px <= ex) {
        d = std::fabs(py - wy) / std::sqrt(1.f + slope * slope) - half;
      } else {
        float dx = px - cxp, dy = py - wy;
        d = std::sqrt(dx * dx + dy * dy) - half;
      }
      uint8_t c = CoverageFromDistance(d);
      if (c) blend(x, y, c);
    }
  }
}

inline float SampleAlphaBilinear(const uint8_t* tex, int row_bytes, int box_w, int box_h, float u,
                                 float v) {
  int x0 = static_cast<int>(std::floor(u)), y0 = static_cast<int>(std::floor(v));
  float fx = u - x0, fy = v - y0;
  auto at = [&](int x, int y) -> float {
    x = std::min(std::max(x, 0), box_w - 1);
    y = std::min(std::max(y, 0), box_h - 1);
    return tex[y * row_bytes + x];
  };
  float a = Lerp(at(x0, y0), at(x0 + 1, y0), fx);
  float b = Lerp(at(x0, y0 + 1), at(x0 + 1, y0 + 1), fx);
  return Lerp(a, b, fy);
}

}  // namespace md3
