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

// Material 3 Expressive look & feel for the recovery UI: palette, shape/progress components and
// the OTA install screen. Only depends on the minui drawing API, so it can also be built on the
// host to render preview images (see tools/md3_preview).

#include <stdint.h>

#include <string>

#include "minui/minui.h"

namespace md3 {

struct Rgb {
  uint8_t r, g, b;
};

// Dark color scheme generated from the Bluenixx blue seed.
namespace palette {
constexpr Rgb kSurface{15, 20, 32};
constexpr Rgb kSurfaceContainer{24, 31, 48};
constexpr Rgb kSurfaceContainerHigh{32, 40, 64};
constexpr Rgb kSurfaceContainerHighest{41, 50, 77};
constexpr Rgb kOnSurface{226, 229, 242};
constexpr Rgb kOnSurfaceVariant{180, 185, 204};
constexpr Rgb kOutline{126, 132, 154};
constexpr Rgb kOutlineVariant{58, 65, 88};
constexpr Rgb kPrimary{173, 198, 255};
constexpr Rgb kOnPrimary{10, 47, 110};
constexpr Rgb kPrimaryContainer{43, 82, 168};
constexpr Rgb kOnPrimaryContainer{220, 230, 255};
constexpr Rgb kSecondary{196, 184, 255};
constexpr Rgb kSecondaryContainer{59, 52, 112};
constexpr Rgb kOnSecondaryContainer{230, 222, 255};
constexpr Rgb kTertiary{127, 219, 208};
constexpr Rgb kOnTertiary{0, 55, 51};
constexpr Rgb kTertiaryContainer{15, 79, 74};
constexpr Rgb kOnTertiaryContainer{168, 242, 234};
constexpr Rgb kError{255, 180, 171};
constexpr Rgb kErrorContainer{147, 0, 10};
}  // namespace palette

inline void SetColor(Rgb c, uint8_t alpha = 255) {
  gr_color(c.r, c.g, c.b, alpha);
}

struct Metrics {
  int width = 0;
  int height = 0;
  float dp = 1.f;
  int margin_w = 0;
  int margin_h = 0;
  int char_w = 0;
  int char_h = 0;

  int Dp(float v) const {
    return static_cast<int>(v * dp + 0.5f);
  }
  bool landscape() const {
    return width > height;
  }
};

// What the package that is being installed tells us about itself (all optional).
struct OtaInfo {
  std::string package_name;      // File name of the package, or "ADB sideload".
  int64_t package_size = 0;      // Bytes, 0 if unknown.
  std::string ota_type;          // "AB", "BLOCK", "BRICK" or empty.
  std::string device;            // pre-device.
  std::string pre_build;         // pre-build-incremental.
  std::string post_build;        // post-build-incremental.
  std::string security_patch;    // post-security-patch-level.
  bool valid() const {
    return !package_name.empty() || !post_build.empty();
  }
};

enum class OtaStage { NONE = 0, VERIFYING = 1, INSTALLING = 2, FINALIZING = 3 };

struct InstallScreenState {
  OtaInfo info;
  OtaStage stage = OtaStage::NONE;
  bool erasing = false;
  bool determinate = false;
  float progress = 0.f;       // 0..1
  double elapsed_s = 0.0;     // Seconds since the install started.
  double anim_s = 0.0;        // Monotonic animation clock, in seconds.
  std::string status_line;    // Last line printed by the updater.
  int battery = 0;
  bool battery_present = true;
  bool charging = false;
  // Optional 8-bit alpha image of the "bluenixx" wordmark (res/images/brand_wordmark.png). When
  // null the wordmark is drawn with the system font.
  const GRSurface* wordmark = nullptr;
};

// ---- Building blocks -------------------------------------------------------------------------

// Fills a pill (fully rounded) in the current color.
void FillPill(int x, int y, int w, int h);

// Fills one menu row. |selected| rows use the primary pill, others a tonal container tile.
void DrawMenuRow(const Metrics& m, int x, int y, int w, int h, bool selected, bool pressed);

// Rounded scroll indicator on the right edge.
void DrawScrollIndicator(const Metrics& m, int right, int y, int h);

// Battery chip whose right edge is at |right| and top at |y|. Returns the chip width.
int DrawBatteryChip(const Metrics& m, int right, int y, int percent, bool charging);

// Brand bar: cookie badge, wordmark and RECOVERY chip. Returns the bar height.
int DrawBrandBar(const Metrics& m, int x, int y, const char* mode_label,
                 const GRSurface* wordmark = nullptr);

// Wavy linear progress indicator with a rotating cookie thumb. |x|..|x + w| is the whole track.
void DrawWavyProgress(const Metrics& m, int x, int y, int w, float progress, bool determinate,
                      double anim_s);

// Rotating, morphing hero shape (circle -> cookie -> flower -> squircle) around (cx, cy).
void DrawMorphingShape(int cx, int cy, int radius, double anim_s, Rgb color, float phase_offset,
                       float rotation_speed);

// Draws the complete OTA / erase screen.
void DrawInstallScreen(const Metrics& m, const InstallScreenState& s);

// Formatting helpers (exposed for tests).
std::string FormatBytes(int64_t bytes);
std::string FormatDuration(double seconds);

}  // namespace md3
