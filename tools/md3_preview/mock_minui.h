#pragma once
#include <memory>
#include "minui/minui.h"
void mock_init(int w, int h);
bool mock_load_font(const char* raw_path, int w, int h);
void mock_save_ppm(const char* path);
std::unique_ptr<GRSurface> mock_make_rgb_surface(const uint8_t* rgb, int w, int h);
std::unique_ptr<GRSurface> mock_make_alpha_surface(const uint8_t* a, int w, int h);
