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

#include "recovery_ui/md3_ui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace md3 {

namespace {

constexpr float kPi = 3.14159265f;

float ScaleForDp(const Metrics& m, float line_dp) {
  return std::max(0.5f, line_dp * m.dp / std::max(1, m.char_h));
}

int TextWidth(const std::string& s, float scale) {
  return std::max(0, gr_measure_scaled(gr_sys_font(), s.c_str(), scale));
}

int LineHeight(const Metrics& m, float scale) {
  return static_cast<int>(std::lround(m.char_h * scale));
}

void Text(int x, int y, const std::string& s, float scale, bool bold = false) {
  gr_text_scaled(gr_sys_font(), x, y, s.c_str(), bold, scale);
}

void TextCentered(int cx, int y, const std::string& s, float scale, bool bold = false) {
  Text(cx - TextWidth(s, scale) / 2, y, s, scale, bold);
}

std::string Ellipsize(const std::string& s, int max_px, float scale, const Metrics& m) {
  int cw = std::max(1, static_cast<int>(std::lround(m.char_w * scale)));
  int max_chars = max_px / cw;
  if (static_cast<int>(s.size()) <= max_chars) return s;
  if (max_chars <= 3) return s.substr(0, std::max(0, max_chars));
  // Keep the tail (version numbers and extensions live there) of long package names.
  int keep_head = (max_chars - 3) / 2, keep_tail = max_chars - 3 - keep_head;
  return s.substr(0, keep_head) + "..." + s.substr(s.size() - keep_tail);
}

void FillRound(int x, int y, int w, int h, int r) {
  gr_fill_round(x, y, x + w, y + h, r);
}

// Animated harmonics: circle -> cookie -> flower -> squircle -> circle, 2 s per step.
void MorphHarmonics(double t, float phase_offset, float rotation, GRShapeHarmonic out[3]) {
  static const float kStates[4][3] = {
    { 0.f, 0.f, 0.f },       // circle
    { 0.f, 0.f, 0.060f },    // 9-lobe cookie
    { 0.f, 0.150f, 0.f },    // 6-petal flower
    { 0.085f, 0.f, 0.f },    // squircle
  };
  const double seg = 2.0, morph = 1.2;
  double u = std::fmod(t + phase_offset, seg * 4.0);
  if (u < 0) u += seg * 4.0;
  int i = static_cast<int>(u / seg) % 4;
  float k = md3::EaseInOutCubic(static_cast<float>((u - i * seg) / morph));
  const int lobes[3] = { 4, 6, 9 };
  for (int j = 0; j < 3; ++j) {
    float a = md3::Lerp(kStates[i][j], kStates[(i + 1) % 4][j], k);
    float base = (j == 0) ? kPi : 0.f;  // squircle corners point at the diagonals
    out[j] = { lobes[j], a, base - lobes[j] * rotation };
  }
}

std::string StageTitle(const InstallScreenState& s) {
  if (s.erasing) return "Erasing data";
  switch (s.stage) {
    case OtaStage::VERIFYING:
      return "Verifying package";
    case OtaStage::FINALIZING:
      return "Finishing up";
    default:
      return "Installing update";
  }
}

std::string OtaTypeLabel(const std::string& t) {
  if (t == "AB") return "A/B (seamless)";
  if (t == "BLOCK") return "Block (non-A/B)";
  if (t == "BRICK") return "Full flash (brick)";
  return t;
}

struct Row {
  const char* label;
  std::string value;
};

}  // namespace

std::string FormatBytes(int64_t bytes) {
  char buf[32];
  const double kib = 1024.0;
  double b = static_cast<double>(bytes);
  if (b >= kib * kib * kib) {
    snprintf(buf, sizeof(buf), "%.2f GiB", b / (kib * kib * kib));
  } else if (b >= kib * kib) {
    snprintf(buf, sizeof(buf), "%.1f MiB", b / (kib * kib));
  } else {
    snprintf(buf, sizeof(buf), "%.0f KiB", b / kib);
  }
  return buf;
}

std::string FormatDuration(double seconds) {
  int total = static_cast<int>(std::max(0.0, seconds) + 0.5);
  char buf[32];
  if (total >= 3600) {
    snprintf(buf, sizeof(buf), "%d:%02d:%02d", total / 3600, (total / 60) % 60, total % 60);
  } else {
    snprintf(buf, sizeof(buf), "%02d:%02d", total / 60, total % 60);
  }
  return buf;
}

void FillPill(int x, int y, int w, int h) {
  gr_fill_round(x, y, x + w, y + h, std::min(w, h) / 2);
}

void DrawMenuRow(const Metrics& m, int x, int y, int w, int h, bool selected, bool pressed) {
  if (selected) {
    SetColor(pressed ? palette::kTertiary : palette::kPrimary);
    FillPill(x, y, w, h);
  } else {
    SetColor(palette::kSurfaceContainer);
    FillRound(x, y, w, h, m.Dp(12));
  }
}

void DrawScrollIndicator(const Metrics& m, int right, int y, int h) {
  int w = std::max(3, m.Dp(4));
  SetColor(palette::kOutline);
  FillRound(right - w, y, w, h, w / 2);
}

int DrawBatteryChip(const Metrics& m, int right, int y, int percent, bool charging) {
  percent = std::min(100, std::max(0, percent));
  const float scale = ScaleForDp(m, 15.f);
  std::string label = std::to_string(percent) + (charging ? "+" : "%");
  const int pad = m.Dp(12), icon_w = m.Dp(24), icon_h = m.Dp(13), gap = m.Dp(8);
  const int chip_h = m.Dp(32);
  const int chip_w = pad + icon_w + m.Dp(2) + gap + TextWidth(label, scale) + pad;
  const int x = right - chip_w;

  SetColor(palette::kSurfaceContainerHigh);
  FillPill(x, y, chip_w, chip_h);

  const int ix = x + pad, iy = y + (chip_h - icon_h) / 2;
  const bool low = percent <= 15 && !charging;
  SetColor(low ? palette::kError : palette::kOnSurfaceVariant);
  FillRound(ix, iy, icon_w, icon_h, m.Dp(4));                       // body
  FillRound(ix + icon_w, iy + icon_h / 4, m.Dp(2), icon_h / 2, 1);  // nub
  SetColor(palette::kSurfaceContainerHigh);
  const int bw = std::max(1, m.Dp(1.6f));
  FillRound(ix + bw, iy + bw, icon_w - 2 * bw, icon_h - 2 * bw, std::max(1, m.Dp(3)));
  SetColor(low ? palette::kError : (charging ? palette::kTertiary : palette::kPrimary));
  const int inner_w = icon_w - 2 * bw - 2 * std::max(1, m.Dp(1.5f));
  FillRound(ix + bw + std::max(1, m.Dp(1.5f)), iy + bw + std::max(1, m.Dp(1.5f)),
            std::max(m.Dp(3), inner_w * percent / 100), icon_h - 2 * bw - 2 * std::max(1, m.Dp(1.5f)),
            std::max(1, m.Dp(2)));

  SetColor(palette::kOnSurface);
  Text(ix + icon_w + m.Dp(2) + gap, y + (chip_h - LineHeight(m, scale)) / 2, label, scale, true);
  return chip_w;
}

int DrawBrandBar(const Metrics& m, int x, int y, const char* mode_label,
                 const GRSurface* wordmark) {
  const int bar_h = m.Dp(44);
  const int r = m.Dp(19);
  const int cx = x + r, cy = y + bar_h / 2;

  const GRShapeHarmonic cookie[1] = { { 9, 0.075f, 0.f } };
  SetColor(palette::kPrimary);
  gr_fill_shape(cx, cy, r, cookie, 1);
  const float glyph = ScaleForDp(m, 30.f);
  SetColor(palette::kOnPrimary);
  TextCentered(cx, cy - LineHeight(m, glyph) / 2 - m.Dp(1), "b", glyph, true);

  const int wx = x + 2 * r + m.Dp(10);
  int word_w;
  SetColor(palette::kOnSurface);
  if (wordmark != nullptr) {
    word_w = static_cast<int>(gr_get_width(wordmark));
    gr_texticon(wx, cy - static_cast<int>(gr_get_height(wordmark)) / 2, wordmark);
  } else {
    const float word = ScaleForDp(m, 24.f);
    word_w = TextWidth("bluenixx", word);
    Text(wx, cy - LineHeight(m, word) / 2, "bluenixx", word, true);
  }

  const float small = ScaleForDp(m, 13.f);
  const int chip_h = m.Dp(26), pad = m.Dp(10);
  const int chip_x = wx + word_w + m.Dp(10);
  const int chip_w = TextWidth(mode_label, small) + 2 * pad;
  SetColor(palette::kSecondaryContainer);
  FillPill(chip_x, cy - chip_h / 2, chip_w, chip_h);
  SetColor(palette::kOnSecondaryContainer);
  Text(chip_x + pad, cy - LineHeight(m, small) / 2, mode_label, small, true);
  return bar_h;
}

void DrawMorphingShape(int cx, int cy, int radius, double anim_s, Rgb color, float phase_offset,
                       float rotation_speed) {
  GRShapeHarmonic h[3];
  MorphHarmonics(anim_s, phase_offset, static_cast<float>(anim_s) * rotation_speed, h);
  SetColor(color);
  gr_fill_shape(cx, cy, radius, h, 3);
}

void DrawWavyProgress(const Metrics& m, int x, int y, int w, float progress, bool determinate,
                      double anim_s) {
  const int th = std::max(4, m.Dp(8));
  const int gap = m.Dp(6);
  const float amp = m.Dp(3.5f);
  const float wavelength = static_cast<float>(m.Dp(30));
  const int thumb_r = m.Dp(12);
  const int cy = y + m.Dp(14);
  const float phase = -static_cast<float>(anim_s) * 6.0f;
  progress = md3::Clamp01(progress);

  const GRShapeHarmonic thumb[1] = { { 8, 0.14f, static_cast<float>(anim_s) * 2.2f } };

  if (determinate) {
    const int head = std::min(std::max(x + static_cast<int>(progress * w), x + thumb_r),
                              x + w - thumb_r);
    // Remaining track (flat, rounded) with a stop indicator at its very end.
    const int track_x = head + thumb_r + gap;
    if (track_x < x + w - th) {
      SetColor(palette::kOutlineVariant);
      FillRound(track_x, cy - th / 2, x + w - track_x, th, th / 2);
      SetColor(palette::kPrimary);
      gr_fill_circle(x + w - th / 2, cy, std::max(2, th / 5));
    }
    // Active wavy part.
    const int wave_end = head - thumb_r - gap / 2;
    if (wave_end - x > th) {
      SetColor(palette::kPrimary);
      float k = md3::Clamp01((wave_end - x) / wavelength);
      gr_fill_wave(x, wave_end, cy, amp * k, wavelength, phase, th);
    } else {
      SetColor(palette::kPrimary);
      FillRound(x, cy - th / 2, std::max(th, head - x), th, th / 2);
    }
    // Cookie thumb with a small dot, so the head of the bar is a shape, not a line.
    SetColor(palette::kPrimary);
    gr_fill_shape(head, cy, thumb_r, thumb, 1);
    SetColor(palette::kOnPrimary);
    gr_fill_circle(head, cy, std::max(2, thumb_r / 3));
  } else {
    SetColor(palette::kOutlineVariant);
    FillRound(x, cy - th / 2, w, th, th / 2);
    const int seg = static_cast<int>(w * 0.38f);
    const double cyc = std::fmod(anim_s * 0.55, 1.0);
    const int pos = static_cast<int>(cyc * (w + seg)) - seg;
    const int a = std::max(x, x + pos), b = std::min(x + w, x + pos + seg);
    if (b - a > th) {
      SetColor(palette::kPrimary);
      gr_fill_wave(a, b, cy, amp, wavelength, phase, th);
    }
  }
}

void DrawInstallScreen(const Metrics& m, const InstallScreenState& s) {
  using namespace palette;
  SetColor(kSurface);
  gr_clear();

  const int mw = std::max(m.margin_w, m.Dp(20));
  const int mh = std::max(m.margin_h, m.Dp(24));
  const bool land = m.landscape();
  const int top = mh + m.Dp(8);
  const int right = m.width - mw;

  const int bar_h = DrawBrandBar(m, mw, top, s.erasing ? "ERASING" : "RECOVERY", s.wordmark);
  if (s.battery_present) {
    DrawBatteryChip(m, right, top + (bar_h - m.Dp(32)) / 2, s.battery, s.charging);
  }

  // Column geometry.
  int lx = mw, lw = m.width - 2 * mw;       // main (left) column
  int rx = mw, rw = lw;                     // details column (portrait: below the main column)
  if (land) {
    lw = (m.width - 3 * mw) * 45 / 100;
    rx = lx + lw + mw;
    rw = m.width - mw - rx;
  }

  const bool show_details = !s.erasing && (s.info.valid() || s.stage != OtaStage::NONE);
  const OtaStage stage = (s.stage == OtaStage::NONE) ? OtaStage::INSTALLING : s.stage;

  // ---- Hero -------------------------------------------------------------------------------
  int hero_r = land ? std::min(lw * 28 / 100, (m.height - top - bar_h) * 22 / 100)
                    : std::min(lw * 27 / 100, m.height * 16 / 100);
  int y = top + bar_h + m.Dp(land ? 24 : 20);
  const int cx = lx + lw / 2;
  const int cy = y + static_cast<int>(hero_r * 1.28f);

  // Orbiting satellites (tertiary cookie + secondary dot).
  const float orbit = hero_r * 1.2f;
  const float ang = static_cast<float>(s.anim_s) * 0.9f;
  const GRShapeHarmonic sat[1] = { { 7, 0.12f, static_cast<float>(s.anim_s) * -1.6f } };
  SetColor(kTertiary);
  gr_fill_shape(cx + static_cast<int>(orbit * std::cos(ang - 0.9f)),
                cy + static_cast<int>(orbit * 0.82f * std::sin(ang - 0.9f)), hero_r * 17 / 100, sat,
                1);
  SetColor(kSecondary);
  gr_fill_circle(cx + static_cast<int>(orbit * std::cos(ang + 2.5f)),
                 cy + static_cast<int>(orbit * 0.82f * std::sin(ang + 2.5f)), hero_r * 7 / 100);

  DrawMorphingShape(cx, cy, hero_r, s.anim_s, kPrimaryContainer, 0.f, 0.35f);
  DrawMorphingShape(cx, cy, hero_r * 78 / 100, s.anim_s, kPrimary, 1.0f, -0.55f);

  SetColor(kOnPrimary);
  if (s.determinate || s.erasing) {
    std::string pct = std::to_string(static_cast<int>(md3::Clamp01(s.progress) * 100.f + 0.5f)) + "%";
    const float ps = (hero_r * 0.62f) / std::max(1, m.char_h);
    const int ty = cy - static_cast<int>(m.char_h * ps / 2) - m.Dp(2);
    TextCentered(cx, ty, pct, ps, true);
  } else {
    const float ps = (hero_r * 0.62f) / std::max(1, m.char_h);
    TextCentered(cx, cy - static_cast<int>(m.char_h * ps / 2), "...", ps, true);
  }
  y = cy + static_cast<int>(hero_r * 1.28f) + m.Dp(6);

  // ---- Title and status line --------------------------------------------------------------
  const float ts = ScaleForDp(m, 32.f);
  SetColor(kOnSurface);
  TextCentered(cx, y, StageTitle(s), ts, true);
  y += LineHeight(m, ts) + m.Dp(6);

  const float ss = ScaleForDp(m, 17.f);
  std::string status = s.status_line.empty() ? "Do not turn off your device" : s.status_line;
  SetColor(kOnSurfaceVariant);
  TextCentered(cx, y, Ellipsize(status, lw - m.Dp(16), ss, m), ss);
  y += LineHeight(m, ss) + m.Dp(22);

  // ---- Wavy shape progress bar ------------------------------------------------------------
  const int bx = lx + m.Dp(8), bw = lw - m.Dp(16);
  DrawWavyProgress(m, bx, y, bw, s.progress, s.determinate || s.erasing, s.anim_s);
  y += m.Dp(34);

  const float cs = ScaleForDp(m, 16.f);
  SetColor(kOnSurfaceVariant);
  Text(bx, y, "Elapsed " + FormatDuration(s.elapsed_s), cs);
  std::string eta;
  if (s.determinate && s.progress > 0.03f && s.progress < 0.999f) {
    eta = "~" + FormatDuration(s.elapsed_s * (1.0 - s.progress) / s.progress) + " left";
  } else if (s.determinate && s.progress >= 0.999f) {
    eta = "Almost done";
  } else {
    eta = "Estimating...";
  }
  Text(bx + bw - TextWidth(eta, cs), y, eta, cs);
  y += LineHeight(m, cs) + m.Dp(20);

  if (!show_details) return;

  // ---- Stage chips ------------------------------------------------------------------------
  int dy = land ? top + bar_h + m.Dp(24) : y;
  static const char* kStageNames[3] = { "Verify", "Install", "Finish" };
  const int chip_gap = m.Dp(8), chip_h = m.Dp(44);
  const int chip_w = (rw - 2 * chip_gap) / 3;
  const float chs = ScaleForDp(m, 16.f);
  for (int i = 0; i < 3; ++i) {
    const int cx0 = rx + i * (chip_w + chip_gap);
    const int idx = i + 1;
    const bool done = idx < static_cast<int>(stage);
    const bool active = idx == static_cast<int>(stage);
    SetColor(active ? kPrimary : done ? kTertiaryContainer : kSurfaceContainer);
    FillPill(cx0, dy, chip_w, chip_h);
    SetColor(active ? kOnPrimary : done ? kOnTertiaryContainer : kOnSurfaceVariant);
    const int tw = TextWidth(kStageNames[i], chs);
    const int dot = done ? m.Dp(14) : 0;  // room for the check dot
    const int tx = cx0 + (chip_w - tw - dot) / 2 + dot;
    if (done) {
      SetColor(kTertiary);
      gr_fill_circle(tx - m.Dp(9), dy + chip_h / 2, m.Dp(4.5f));
      SetColor(kOnTertiaryContainer);
    }
    Text(tx, dy + (chip_h - LineHeight(m, chs)) / 2, kStageNames[i], chs, active);
  }
  dy += chip_h + m.Dp(18);

  // ---- Details card -----------------------------------------------------------------------
  std::vector<Row> rows;
  if (!s.info.package_name.empty()) rows.push_back({ "Package", s.info.package_name });
  if (s.info.package_size > 0) rows.push_back({ "Size", FormatBytes(s.info.package_size) });
  if (!s.info.ota_type.empty()) rows.push_back({ "Type", OtaTypeLabel(s.info.ota_type) });
  if (!s.info.post_build.empty()) {
    rows.push_back({ "Build", s.info.pre_build.empty() ? s.info.post_build
                                                       : s.info.pre_build + " -> " + s.info.post_build });
  }
  if (!s.info.security_patch.empty()) rows.push_back({ "Security patch", s.info.security_patch });
  if (!s.info.device.empty()) rows.push_back({ "Device", s.info.device });
  if (rows.empty()) return;

  const float hs = ScaleForDp(m, 15.f), ls = ScaleForDp(m, 15.5f), vs = ScaleForDp(m, 16.5f);
  const int pad = m.Dp(16), row_h = m.Dp(42);
  const int head_h = LineHeight(m, hs) + m.Dp(10);
  const int bottom_limit = m.height - mh;
  // Drop the least important rows if the card would not fit on a small screen.
  while (!rows.empty() && dy + 2 * pad + head_h + static_cast<int>(rows.size()) * row_h > bottom_limit) {
    rows.pop_back();
  }
  if (rows.empty()) return;
  const int card_h = 2 * pad + head_h + static_cast<int>(rows.size()) * row_h;

  SetColor(kSurfaceContainer);
  FillRound(rx, dy, rw, card_h, m.Dp(28));
  SetColor(kPrimary);
  Text(rx + pad + m.Dp(4), dy + pad, "UPDATE DETAILS", hs, true);

  int ry = dy + pad + head_h;
  for (size_t i = 0; i < rows.size(); ++i) {
    const int tx = rx + pad + m.Dp(4), tr = rx + rw - pad - m.Dp(4);
    const int ty = ry + (row_h - LineHeight(m, ls)) / 2;
    if (i > 0) {
      SetColor(kOutlineVariant, 150);
      gr_fill(rx + pad, ry, rx + rw - pad, ry + std::max(1, m.Dp(0.6f)));
    }
    SetColor(kOnSurfaceVariant);
    Text(tx, ty, rows[i].label, ls);
    const int label_w = TextWidth(rows[i].label, ls);
    std::string v = Ellipsize(rows[i].value, tr - tx - label_w - m.Dp(14), vs, m);
    SetColor(kOnSurface);
    Text(tr - TextWidth(v, vs), ry + (row_h - LineHeight(m, vs)) / 2, v, vs, true);
    ry += row_h;
  }
}

}  // namespace md3
