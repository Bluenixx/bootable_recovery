// Renders the MD3 Expressive recovery screens on the host using the real recovery_ui/md3_ui.cpp.
// usage: md3_preview <font.raw> <out_prefix> [logo.rgb logo_w logo_h]
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <vector>

static std::vector<uint8_t> ReadAll(const char* path) {
  std::vector<uint8_t> v;
  FILE* f = fopen(path, "rb");
  if (!f) return v;
  uint8_t buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) v.insert(v.end(), buf, buf + n);
  fclose(f);
  return v;
}

#include "mock_minui.h"
#include "recovery_ui/md3_ui.h"

static md3::Metrics MakeMetrics(int w, int h, float dp) {
  md3::Metrics m;
  m.width = w; m.height = h; m.dp = dp;
  m.char_w = 18; m.char_h = 32;
  return m;
}

static const GRSurface* g_wordmark = nullptr;
static md3::InstallScreenState OtaState(float progress, md3::OtaStage stage, double elapsed, const char* status, double anim = 3.0) {
  md3::InstallScreenState s;
  s.info.package_name = "bluenixx-26.1-20261025-OTA-lemonade.zip";
  s.info.package_size = 1973141504LL;
  s.info.ota_type = "AB";
  s.info.device = "lemonade";
  s.info.pre_build = "20261018";
  s.info.post_build = "20261025";
  s.info.security_patch = "2026-10-05";
  s.stage = stage; s.determinate = true; s.progress = progress; s.elapsed_s = elapsed;
  s.anim_s = anim; s.status_line = status; s.battery = 78; s.charging = true; s.wordmark = g_wordmark;
  return s;
}


// Mirrors ScreenRecoveryUI::draw_menu_and_text_buffer_locked + TextMenu::DrawItems.
static void DrawMenuScene(const md3::Metrics& m, const GRSurface* logo, int selection, int prev,
                          float anim_t, bool back_arrow_title, const std::vector<std::string>& items,
                          const std::vector<std::string>& titles) {
  using namespace md3;
  SetColor(palette::kSurface);
  gr_clear();
  const int lw = gr_get_width(logo), lh = gr_get_height(logo);
  gr_blit(logo, 0, 0, lw, lh, (m.width - lw) / 2, m.margin_h);
  DrawBatteryChip(m, m.width - m.margin_w - m.Dp(16), m.margin_h + m.Dp(14), 78, true);
  int y = m.margin_h + lh;
  const int x = m.margin_w + 24;
  SetColor(palette::kOnSurface);
  for (const auto& t : titles) { gr_text(gr_sys_font(), x, y, t.c_str(), false); y += m.char_h + 4; }
  const int padding = m.char_h, spacing = 8;
  const int bar_h = padding * 2 + m.char_h, pitch = bar_h + spacing;
  const int bx = padding + x, bw = m.width - (padding + x) * 2;
  const int first_y = y + 12;
  SetColor(palette::kSurfaceContainer);
  for (size_t i = 0; i < items.size(); ++i) gr_fill_round(bx, first_y + i * pitch, bx + bw, first_y + i * pitch + bar_h, m.Dp(12));
  float py = first_y + selection * pitch;
  if (prev >= 0) py = Lerp(first_y + prev * pitch, py, EaseOutBack(anim_t));
  SetColor(palette::kPrimary);
  gr_fill_round(bx, (int)(py + 0.5f), bx + bw, (int)(py + 0.5f) + bar_h, bar_h / 2);
  for (size_t i = 0; i < items.size(); ++i) {
    SetColor((int)i == selection ? palette::kOnPrimary : palette::kOnSurface);
    gr_text(gr_sys_font(), padding * 2 + x, first_y + i * pitch + padding, items[i].c_str(), false);
  }
}

int main(int argc, char** argv) {
  if (argc < 3) return 1;
  std::string prefix = argv[2];
  const int W = 1080, H = 2400;
  mock_init(W, H);
  if (!mock_load_font(argv[1], 1728, 64)) { fprintf(stderr, "font load failed\n"); return 1; }
  md3::Metrics m = MakeMetrics(W, H, 2.75f);

  auto logo_raw = ReadAll("/tmp/md3_preview/logo.raw");
  auto wm_raw = ReadAll("/tmp/md3_preview/wm.raw");
  auto logo = mock_make_rgb_surface(logo_raw.data(), 768, 480);
  auto wordmark = mock_make_alpha_surface(wm_raw.data(), 321, 62);

  g_wordmark = wordmark.get();
  struct Scene { const char* name; md3::InstallScreenState s; };
  std::vector<Scene> scenes;
  scenes.push_back({ "install_42", OtaState(0.42f, md3::OtaStage::INSTALLING, 134, "Patching system_a (3/9)...", 3.0) });
  scenes.push_back({ "install_verify_8", OtaState(0.08f, md3::OtaStage::VERIFYING, 9, "Verifying update package...", 5.2) });
  scenes.push_back({ "install_97", OtaState(0.97f, md3::OtaStage::FINALIZING, 412, "Writing slot metadata...", 7.4) });
  {
    md3::InstallScreenState e = OtaState(0.55f, md3::OtaStage::NONE, 20, "");
    e.erasing = true; e.info = md3::OtaInfo();
    scenes.push_back({ "erasing_55", e });
  }
  for (auto& sc : scenes) {
    md3::DrawInstallScreen(m, sc.s);
    mock_save_ppm((prefix + sc.name + ".ppm").c_str());
  }
  mock_init(W, H);
  DrawMenuScene(m, logo.get(), 1, 0, 0.55f, false,
                { "Apply update", "Factory reset", "Advanced", "Mount / unmount", "Reboot system now" },
                { "Bluenixx Recovery 26.1", "Android 17 - lemonade", "" });
  mock_save_ppm((prefix + "menu_main.ppm").c_str());
  // Landscape variant (tablet / landscape device).
  mock_init(2400, 1080);
  md3::Metrics ml = MakeMetrics(2400, 1080, 2.0f);
  md3::DrawInstallScreen(ml, OtaState(0.42f, md3::OtaStage::INSTALLING, 134, "Patching system_a (3/9)...", 3.0));
  mock_save_ppm((prefix + "install_landscape.ppm").c_str());
  return 0;
}
