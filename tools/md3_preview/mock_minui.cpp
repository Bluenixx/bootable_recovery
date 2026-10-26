// Host implementation of the subset of minui used by recovery_ui/md3_ui.cpp. The shape
// rasterizers are the real ones from minui/md3_shapes.h; only the pixel sink and the font atlas
// loader are replaced.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmath>
#include <vector>

#include "minui/minui.h"
#include "mock_minui.h"

std::unique_ptr<GRSurface> GRSurface::Create(size_t width, size_t height, size_t row_bytes,
                                             size_t pixel_bytes) {
  if (width == 0 || row_bytes == 0 || height == 0 || pixel_bytes == 0) return nullptr;
  auto result = std::unique_ptr<GRSurface>(new GRSurface(width, height, row_bytes, pixel_bytes));
  size_t data_size = row_bytes * height;
  result->data_size_ = (data_size + kSurfaceDataAlignment - 1) / kSurfaceDataAlignment *
                       kSurfaceDataAlignment;
  result->data_.reset(static_cast<uint8_t*>(aligned_alloc(kSurfaceDataAlignment, result->data_size_)));
  if (!result->data_) return nullptr;
  return result;
}

std::unique_ptr<GRSurface> GRSurface::Clone() const {
  auto r = GRSurface::Create(width, height, row_bytes, pixel_bytes);
  memcpy(r->data(), data(), data_size_);
  return r;
}

static std::unique_ptr<GRSurface> g_fb;
static GRFont g_font;
static std::unique_ptr<GRSurface> g_font_tex;
static uint8_t cr = 255, cg = 255, cb = 255, ca = 255;

void mock_init(int w, int h) {
  g_fb = GRSurface::Create(w, h, w * 4, 4);
  memset(g_fb->data(), 0, g_fb->data_size());
}

bool mock_load_font(const char* raw_path, int w, int h) {
  FILE* f = fopen(raw_path, "rb");
  if (!f) return false;
  g_font_tex = GRSurface::Create(w, h, w, 1);
  size_t n = fread(g_font_tex->data(), 1, w * h, f);
  fclose(f);
  g_font.texture = g_font_tex.get();
  g_font.char_width = w / 96;
  g_font.char_height = h / 2;
  return n == static_cast<size_t>(w * h);
}

void mock_save_ppm(const char* path) {
  FILE* f = fopen(path, "wb");
  fprintf(f, "P6\n%zu %zu\n255\n", g_fb->width, g_fb->height);
  for (size_t y = 0; y < g_fb->height; ++y) {
    const uint32_t* row = reinterpret_cast<const uint32_t*>(g_fb->data() + y * g_fb->row_bytes);
    for (size_t x = 0; x < g_fb->width; ++x) {
      uint8_t px[3] = { static_cast<uint8_t>(row[x] >> 16), static_cast<uint8_t>(row[x] >> 8),
                        static_cast<uint8_t>(row[x]) };
      fwrite(px, 1, 3, f);
    }
  }
  fclose(f);
}

std::unique_ptr<GRSurface> mock_make_rgb_surface(const uint8_t* rgb, int w, int h) {
  auto s = GRSurface::Create(w, h, w * 4, 4);
  uint32_t* p = reinterpret_cast<uint32_t*>(s->data());
  for (int i = 0; i < w * h; ++i)
    p[i] = 0xff000000u | (rgb[i * 3] << 16) | (rgb[i * 3 + 1] << 8) | rgb[i * 3 + 2];
  return s;
}

static inline void blend(int x, int y, uint32_t a) {
  if (x < 0 || y < 0 || x >= static_cast<int>(g_fb->width) || y >= static_cast<int>(g_fb->height)) return;
  a = a * ca / 255;
  if (!a) return;
  uint32_t* p = reinterpret_cast<uint32_t*>(g_fb->data() + y * g_fb->row_bytes) + x;
  uint32_t d = *p;
  uint32_t r = (((d >> 16) & 255) * (255 - a) + cr * a) / 255;
  uint32_t g = (((d >> 8) & 255) * (255 - a) + cg * a) / 255;
  uint32_t b = ((d & 255) * (255 - a) + cb * a) / 255;
  *p = 0xff000000u | (r << 16) | (g << 8) | b;
}
static void BlendCoverage(int x, int y, uint8_t c) { blend(x, y, c); }

void gr_color(unsigned char r, unsigned char g, unsigned char b, unsigned char a) { cr = r; cg = g; cb = b; ca = a; }
void gr_clear() {
  uint32_t v = 0xff000000u | (cr << 16) | (cg << 8) | cb;
  uint32_t* p = reinterpret_cast<uint32_t*>(g_fb->data());
  for (size_t i = 0; i < g_fb->width * g_fb->height; ++i) p[i] = v;
}
void gr_fill(int x1, int y1, int x2, int y2) {
  for (int y = y1; y < y2; ++y) for (int x = x1; x < x2; ++x) blend(x, y, 255);
}
void gr_fill_round(int x1, int y1, int x2, int y2, int r) { md3::FillRoundRect(x1, y1, x2, y2, r, BlendCoverage); }
void gr_fill_circle(int cx, int cy, int r) { gr_fill_round(cx - r, cy - r, cx + r, cy + r, r); }
void gr_fill_shape(int cx, int cy, int r, const GRShapeHarmonic* h, int n) { md3::FillShape(cx, cy, r, h, n, BlendCoverage); }
void gr_fill_wave(int x1, int x2, int cy, float amp, float wl, float ph, int th) { md3::FillWave(x1, x2, cy, amp, wl, ph, th, BlendCoverage); }
const GRFont* gr_sys_font() { return &g_font; }
const GRFont* gr_menu_font() { return &g_font; }
int gr_measure(const GRFont* f, const char* s) { return f->char_width * strlen(s); }
int gr_measure_scaled(const GRFont* f, const char* s, float sc) { return static_cast<int>(std::lround(f->char_width * sc)) * strlen(s); }
void gr_text(const GRFont* f, int x, int y, const char* s, bool bold) { gr_text_scaled(f, x, y, s, bold, 1.0f); }
int gr_text_scaled(const GRFont* font, int x, int y, const char* s, bool bold, float scale) {
  const int cw = font->char_width, ch = font->char_height;
  const int ow = static_cast<int>(std::lround(cw * scale)), oh = static_cast<int>(std::lround(ch * scale));
  const int sx = x;
  unsigned char c;
  while ((c = *s++)) {
    if (c < ' ' || c > '~') c = '?';
    const uint8_t* glyph = font->texture->data() + (c - ' ') * cw + (bold ? ch * font->texture->row_bytes : 0);
    for (int oy = 0; oy < oh; ++oy)
      for (int ox = 0; ox < ow; ++ox) {
        float a = md3::SampleAlphaBilinear(glyph, font->texture->row_bytes, cw, ch,
                                           (ox + 0.5f) * cw / ow - 0.5f, (oy + 0.5f) * ch / oh - 0.5f);
        if (a >= 1.f) blend(x + ox, y + oy, static_cast<uint8_t>(a));
      }
    x += ow;
  }
  return x - sx;
}
void gr_blit(const GRSurface* src, int sx, int sy, int w, int h, int dx, int dy) {
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      if (dx + x < 0 || dy + y < 0 || dx + x >= (int)g_fb->width || dy + y >= (int)g_fb->height) continue;
      reinterpret_cast<uint32_t*>(g_fb->data() + (dy + y) * g_fb->row_bytes)[dx + x] =
          reinterpret_cast<const uint32_t*>(src->data() + (sy + y) * src->row_bytes)[sx + x];
    }
}
unsigned int gr_get_width(const GRSurface* s) { return s ? s->width : 0; }
unsigned int gr_get_height(const GRSurface* s) { return s ? s->height : 0; }
void gr_texticon(int x, int y, const GRSurface* icon) {
  for (size_t j = 0; j < icon->height; ++j)
    for (size_t i = 0; i < icon->width; ++i) blend(x + i, y + j, icon->data()[j * icon->row_bytes + i]);
}
std::unique_ptr<GRSurface> mock_make_alpha_surface(const uint8_t* a, int w, int h) {
  auto s = GRSurface::Create(w, h, w, 1);
  memcpy(s->data(), a, w * h);
  return s;
}
