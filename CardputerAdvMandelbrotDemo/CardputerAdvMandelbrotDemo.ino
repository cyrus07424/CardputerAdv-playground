#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>
#include <string.h>

namespace app_config {
constexpr int16_t SCREEN_W = 240;
constexpr int16_t SCREEN_H = 135;
constexpr int16_t HEADER_H = 17;
constexpr int16_t FOOTER_H = 28;
constexpr int16_t FRACTAL_X = 0;
constexpr int16_t FRACTAL_Y = HEADER_H;
constexpr int16_t FRACTAL_W = SCREEN_W;
constexpr int16_t FRACTAL_H = SCREEN_H - HEADER_H - FOOTER_H;
constexpr uint32_t INPUT_REPEAT_MS = 60;
constexpr uint8_t ROWS_PER_TICK = 4;
constexpr uint16_t INITIAL_ITERATIONS = 64;
constexpr uint16_t MIN_ITERATIONS = 24;
constexpr uint16_t MAX_ITERATIONS = 320;
constexpr uint16_t ITERATION_STEP = 8;
constexpr float INITIAL_CENTER_X = -0.55f;
constexpr float INITIAL_CENTER_Y = 0.0f;
constexpr float INITIAL_VIEW_WIDTH = 3.1f;
constexpr float MIN_VIEW_WIDTH = 0.00002f;
constexpr float MAX_VIEW_WIDTH = 4.0f;
constexpr float PAN_RATIO = 0.08f;
constexpr float ZOOM_IN_FACTOR = 0.82f;
constexpr float ZOOM_OUT_FACTOR = 1.22f;
}  // namespace app_config

namespace keyboard_hid {
constexpr uint8_t RIGHT = 0x4F;
constexpr uint8_t LEFT = 0x50;
constexpr uint8_t DOWN = 0x51;
constexpr uint8_t UP = 0x52;
}  // namespace keyboard_hid

M5Canvas g_canvas(&M5Cardputer.Display);
uint16_t g_fractal_pixels[app_config::FRACTAL_H][app_config::FRACTAL_W];
uint16_t g_prev_fractal_pixels[app_config::FRACTAL_H][app_config::FRACTAL_W];

float g_center_x = app_config::INITIAL_CENTER_X;
float g_center_y = app_config::INITIAL_CENTER_Y;
float g_view_width = app_config::INITIAL_VIEW_WIDTH;
uint16_t g_max_iterations = app_config::INITIAL_ITERATIONS;
uint8_t g_palette_index = 0;
uint16_t g_render_row = 0;
uint32_t g_last_input_repeat_ms = 0;
bool g_show_help = true;
bool g_frame_dirty = true;
bool g_prev_palette_pressed = false;
bool g_prev_help_pressed = false;

constexpr const char* kPaletteNames[] = {
    "Classic",
    "Fire",
    "Neon",
    "Mono",
};

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((static_cast<uint16_t>(r) & 0xF8u) << 8) |
         ((static_cast<uint16_t>(g) & 0xFCu) << 3) |
         (static_cast<uint16_t>(b) >> 3);
}

float clampf(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

float view_height_for(float view_width) {
  return view_width * static_cast<float>(app_config::FRACTAL_H) / static_cast<float>(app_config::FRACTAL_W);
}

bool contains_hid_key(const Keyboard_Class::KeysState& status, uint8_t key_code) {
  for (const auto key : status.hid_keys) {
    if ((key & ~SHIFT) == key_code) {
      return true;
    }
  }
  return false;
}

bool contains_char_key(const Keyboard_Class::KeysState& status, char key_code) {
  for (const auto key : status.word) {
    if (key == key_code) {
      return true;
    }
  }
  return false;
}

bool contains_alpha_key(const Keyboard_Class::KeysState& status, char lower_case) {
  return contains_char_key(status, lower_case) || contains_char_key(status, lower_case - ('a' - 'A'));
}

void reuse_previous_pixels(float old_center_x, float old_center_y, float old_view_width) {
  const float old_view_height = view_height_for(old_view_width);
  const float new_view_height = view_height_for(g_view_width);
  const float old_left = old_center_x - old_view_width * 0.5f;
  const float old_top = old_center_y - old_view_height * 0.5f;
  const float new_left = g_center_x - g_view_width * 0.5f;
  const float new_top = g_center_y - new_view_height * 0.5f;
  const float new_step_x = g_view_width / static_cast<float>(app_config::FRACTAL_W);
  const float new_step_y = new_view_height / static_cast<float>(app_config::FRACTAL_H);

  for (int16_t y = 0; y < app_config::FRACTAL_H; ++y) {
    const float cy = new_top + (static_cast<float>(y) + 0.5f) * new_step_y;
    const float src_yf =
        ((cy - old_top) / old_view_height) * static_cast<float>(app_config::FRACTAL_H) - 0.5f;
    const int16_t src_y = static_cast<int16_t>(src_yf + 0.5f);

    for (int16_t x = 0; x < app_config::FRACTAL_W; ++x) {
      uint16_t color = TFT_BLACK;

      if (src_y >= 0 && src_y < app_config::FRACTAL_H) {
        const float cx = new_left + (static_cast<float>(x) + 0.5f) * new_step_x;
        const float src_xf =
            ((cx - old_left) / old_view_width) * static_cast<float>(app_config::FRACTAL_W) - 0.5f;
        const int16_t src_x = static_cast<int16_t>(src_xf + 0.5f);
        if (src_x >= 0 && src_x < app_config::FRACTAL_W) {
          color = g_prev_fractal_pixels[src_y][src_x];
        }
      }

      g_fractal_pixels[y][x] = color;
      g_canvas.drawPixel(app_config::FRACTAL_X + x, app_config::FRACTAL_Y + y, color);
    }
  }
}

void invalidate_render(bool reuse_previous = false,
                       float old_center_x = 0.0f,
                       float old_center_y = 0.0f,
                       float old_view_width = app_config::INITIAL_VIEW_WIDTH) {
  g_render_row = 0;
  if (reuse_previous) {
    memcpy(g_prev_fractal_pixels, g_fractal_pixels, sizeof(g_fractal_pixels));
    reuse_previous_pixels(old_center_x, old_center_y, old_view_width);
  } else {
    memset(g_fractal_pixels, 0, sizeof(g_fractal_pixels));
    g_canvas.fillRect(
        app_config::FRACTAL_X,
        app_config::FRACTAL_Y,
        app_config::FRACTAL_W,
        app_config::FRACTAL_H,
        TFT_BLACK);
  }
  g_frame_dirty = true;
}

void reset_view() {
  g_center_x = app_config::INITIAL_CENTER_X;
  g_center_y = app_config::INITIAL_CENTER_Y;
  g_view_width = app_config::INITIAL_VIEW_WIDTH;
  g_max_iterations = app_config::INITIAL_ITERATIONS;
  invalidate_render();
}

uint16_t palette_color(float t, uint8_t palette_index) {
  const float clamped_t = clampf(t, 0.0f, 1.0f);
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  switch (palette_index % 4) {
    case 0: {
      const float inv = 1.0f - clamped_t;
      r = static_cast<uint8_t>(255.0f * clampf(9.0f * inv * clamped_t * clamped_t * clamped_t, 0.0f, 1.0f));
      g = static_cast<uint8_t>(255.0f * clampf(15.0f * inv * inv * clamped_t * clamped_t, 0.0f, 1.0f));
      b = static_cast<uint8_t>(255.0f * clampf(8.5f * inv * inv * inv * clamped_t, 0.0f, 1.0f));
      break;
    }
    case 1:
      r = static_cast<uint8_t>(255.0f * clampf(clamped_t * 1.3f, 0.0f, 1.0f));
      g = static_cast<uint8_t>(255.0f * clamped_t * clamped_t);
      b = static_cast<uint8_t>(180.0f * clamped_t * clamped_t * clamped_t);
      break;
    case 2:
      r = static_cast<uint8_t>(127.0f + 127.0f * sinf(6.28318f * (clamped_t + 0.00f)));
      g = static_cast<uint8_t>(127.0f + 127.0f * sinf(6.28318f * (clamped_t + 0.33f)));
      b = static_cast<uint8_t>(127.0f + 127.0f * sinf(6.28318f * (clamped_t + 0.66f)));
      break;
    default: {
      const uint8_t mono = static_cast<uint8_t>(255.0f * clamped_t);
      r = mono;
      g = mono;
      b = mono;
      break;
    }
  }

  return rgb565(r, g, b);
}

uint16_t mandelbrot_color(float cx, float cy) {
  const float x_minus_quarter = cx - 0.25f;
  const float q = x_minus_quarter * x_minus_quarter + cy * cy;
  if (q * (q + x_minus_quarter) <= 0.25f * cy * cy) {
    return TFT_BLACK;
  }

  const float bulb_x = cx + 1.0f;
  if (bulb_x * bulb_x + cy * cy <= 0.0625f) {
    return TFT_BLACK;
  }

  float zx = 0.0f;
  float zy = 0.0f;
  float zx2 = 0.0f;
  float zy2 = 0.0f;
  uint16_t iter = 0;

  while (iter < g_max_iterations && (zx2 + zy2) <= 4.0f) {
    zy = 2.0f * zx * zy + cy;
    zx = zx2 - zy2 + cx;
    zx2 = zx * zx;
    zy2 = zy * zy;
    ++iter;
  }

  if (iter >= g_max_iterations) {
    return TFT_BLACK;
  }

  const float magnitude_sq = zx2 + zy2;
  float smooth_iter = static_cast<float>(iter);
  if (magnitude_sq > 1.0f) {
    const float log_zn = 0.5f * logf(magnitude_sq);
    const float nu = logf(log_zn / logf(2.0f)) / logf(2.0f);
    smooth_iter = smooth_iter + 1.0f - nu;
  }

  return palette_color(smooth_iter / static_cast<float>(g_max_iterations), g_palette_index);
}

void render_rows(uint8_t row_budget) {
  if (g_render_row >= app_config::FRACTAL_H) {
    return;
  }

  const float aspect = static_cast<float>(app_config::FRACTAL_H) / static_cast<float>(app_config::FRACTAL_W);
  const float view_height = g_view_width * aspect;
  const float x0 = g_center_x - g_view_width * 0.5f;
  const float y0 = g_center_y - view_height * 0.5f;
  const float step_x = g_view_width / static_cast<float>(app_config::FRACTAL_W);
  const float step_y = view_height / static_cast<float>(app_config::FRACTAL_H);

  uint8_t rows_rendered = 0;
  while (rows_rendered < row_budget && g_render_row < app_config::FRACTAL_H) {
    const int16_t screen_y = app_config::FRACTAL_Y + static_cast<int16_t>(g_render_row);
    const float cy = y0 + (static_cast<float>(g_render_row) + 0.5f) * step_y;

    for (int16_t x = 0; x < app_config::FRACTAL_W; ++x) {
      const float cx = x0 + (static_cast<float>(x) + 0.5f) * step_x;
      const uint16_t color = mandelbrot_color(cx, cy);
      g_fractal_pixels[g_render_row][x] = color;
      g_canvas.drawPixel(app_config::FRACTAL_X + x, screen_y, color);
    }

    ++g_render_row;
    ++rows_rendered;
  }

  g_frame_dirty = true;
}

void draw_overlay() {
  g_canvas.fillRect(0, 0, app_config::SCREEN_W, app_config::HEADER_H, TFT_BLACK);
  g_canvas.fillRect(
      0,
      app_config::SCREEN_H - app_config::FOOTER_H,
      app_config::SCREEN_W,
      app_config::FOOTER_H,
      TFT_BLACK);
  g_canvas.drawFastHLine(0, app_config::HEADER_H - 1, app_config::SCREEN_W, TFT_DARKGREY);
  g_canvas.drawFastHLine(
      0,
      app_config::SCREEN_H - app_config::FOOTER_H,
      app_config::SCREEN_W,
      TFT_DARKGREY);

  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);
  g_canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  g_canvas.setCursor(2, 4);
  g_canvas.print("Mandelbrot Explorer");

  g_canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  g_canvas.setCursor(150, 4);
  if (g_render_row >= app_config::FRACTAL_H) {
    g_canvas.print("READY");
  } else {
    const uint16_t progress = static_cast<uint16_t>(
        (static_cast<uint32_t>(g_render_row) * 100u) / static_cast<uint32_t>(app_config::FRACTAL_H));
    g_canvas.printf("%3u%%", progress);
  }

  const int16_t footer_y = app_config::SCREEN_H - app_config::FOOTER_H + 3;
  const float zoom = app_config::INITIAL_VIEW_WIDTH / g_view_width;
  g_canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  g_canvas.setCursor(2, footer_y);
  g_canvas.printf(
      "Zoom:%5.1fx It:%3u Pal:%s",
      zoom,
      g_max_iterations,
      kPaletteNames[g_palette_index % 4]);

  if (g_show_help) {
    g_canvas.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    g_canvas.setCursor(2, footer_y + 9);
    g_canvas.print("Arrows/,/;.:pan  A/Z:zoom  Q/W:iter");
    g_canvas.setCursor(2, footer_y + 18);
    g_canvas.print("P:palette  Enter:help  BtnA:reset");
  } else {
    g_canvas.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g_canvas.setCursor(2, footer_y + 9);
    g_canvas.print("Enter: show help");
    g_canvas.setCursor(2, footer_y + 18);
    g_canvas.printf("Center:(%.3f, %.3f)", g_center_x, g_center_y);
  }
}

void present() {
  if (!g_frame_dirty) {
    return;
  }

  draw_overlay();
  g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
  g_frame_dirty = false;
}

void apply_continuous_controls(const Keyboard_Class::KeysState& status) {
  const uint32_t now = millis();
  if (now - g_last_input_repeat_ms < app_config::INPUT_REPEAT_MS) {
    return;
  }

  const bool pan_left = contains_hid_key(status, keyboard_hid::LEFT) || contains_char_key(status, ',');
  const bool pan_right = contains_hid_key(status, keyboard_hid::RIGHT) || contains_char_key(status, '/');
  const bool pan_up = contains_hid_key(status, keyboard_hid::UP) || contains_char_key(status, ';');
  const bool pan_down = contains_hid_key(status, keyboard_hid::DOWN) || contains_char_key(status, '.');
  const bool zoom_in = contains_alpha_key(status, 'a');
  const bool zoom_out = contains_alpha_key(status, 'z');
  const bool iter_down = contains_alpha_key(status, 'q');
  const bool iter_up = contains_alpha_key(status, 'w');

  const float old_center_x = g_center_x;
  const float old_center_y = g_center_y;
  const float old_view_width = g_view_width;
  float next_center_x = g_center_x;
  float next_center_y = g_center_y;
  float next_view_width = g_view_width;
  uint16_t next_iterations = g_max_iterations;
  const float pan_step = g_view_width * app_config::PAN_RATIO;

  if (pan_left && !pan_right) {
    next_center_x -= pan_step;
  } else if (pan_right && !pan_left) {
    next_center_x += pan_step;
  }

  if (pan_up && !pan_down) {
    next_center_y -= pan_step * static_cast<float>(app_config::FRACTAL_H) /
                     static_cast<float>(app_config::FRACTAL_W);
  } else if (pan_down && !pan_up) {
    next_center_y += pan_step * static_cast<float>(app_config::FRACTAL_H) /
                     static_cast<float>(app_config::FRACTAL_W);
  }

  if (zoom_in && !zoom_out) {
    next_view_width *= app_config::ZOOM_IN_FACTOR;
    next_view_width = clampf(next_view_width, app_config::MIN_VIEW_WIDTH, app_config::MAX_VIEW_WIDTH);
  } else if (zoom_out && !zoom_in) {
    next_view_width *= app_config::ZOOM_OUT_FACTOR;
    next_view_width = clampf(next_view_width, app_config::MIN_VIEW_WIDTH, app_config::MAX_VIEW_WIDTH);
  }

  if (iter_up && !iter_down && next_iterations + app_config::ITERATION_STEP <= app_config::MAX_ITERATIONS) {
    next_iterations += app_config::ITERATION_STEP;
  } else if (iter_down && !iter_up &&
             next_iterations >= app_config::MIN_ITERATIONS + app_config::ITERATION_STEP) {
    next_iterations -= app_config::ITERATION_STEP;
  }

  const bool view_changed =
      next_center_x != g_center_x || next_center_y != g_center_y || next_view_width != g_view_width;
  const bool detail_changed = next_iterations != g_max_iterations;

  if (view_changed || detail_changed) {
    g_center_x = next_center_x;
    g_center_y = next_center_y;
    g_view_width = next_view_width;
    g_max_iterations = next_iterations;
    invalidate_render(view_changed && !detail_changed, old_center_x, old_center_y, old_view_width);
    g_last_input_repeat_ms = now;
  }
}

void handle_input() {
  const auto status = M5Cardputer.Keyboard.keysState();

  apply_continuous_controls(status);

  const bool palette_pressed = contains_alpha_key(status, 'p');
  if (palette_pressed && !g_prev_palette_pressed) {
    g_palette_index = (g_palette_index + 1) % 4;
    invalidate_render();
  }
  g_prev_palette_pressed = palette_pressed;

  const bool help_pressed = status.enter;
  if (help_pressed && !g_prev_help_pressed) {
    g_show_help = !g_show_help;
    g_frame_dirty = true;
  }
  g_prev_help_pressed = help_pressed;

  if (M5Cardputer.BtnA.wasClicked()) {
    reset_view();
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, true);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);

  g_canvas.setColorDepth(16);
  g_canvas.createSprite(app_config::SCREEN_W, app_config::SCREEN_H);
  g_canvas.fillScreen(TFT_BLACK);

  reset_view();
  present();
}

void loop() {
  M5Cardputer.update();

  handle_input();
  render_rows(app_config::ROWS_PER_TICK);
  present();
}
