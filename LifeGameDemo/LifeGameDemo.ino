#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>
#include <memory>

namespace app_config {
constexpr uint32_t STEP_INTERVAL_MS = 55;
constexpr uint32_t STEP_INTERVAL_MIN_MS = 10;
constexpr uint32_t STEP_INTERVAL_MAX_MS = 300;
constexpr uint32_t STEP_INTERVAL_DELTA_MS = 5;
constexpr uint8_t RANDOM_FILL_PERCENT = 34;
constexpr uint16_t HUE_RANGE = 1536;
constexpr int16_t HUE_MUTATION = 40;
constexpr uint16_t DEAD = 0xFFFF;
inline int display_width() { return M5Cardputer.Display.width(); }
inline int display_height() { return M5Cardputer.Display.height(); }
}  // namespace app_config

static std::unique_ptr<uint16_t[]> g_hues;
static std::unique_ptr<uint16_t[]> g_next_hues;
static int g_cell_count = 0;

static bool g_paused = false;
static bool g_prev_enter = false;
static bool g_prev_space = false;
static bool g_prev_r = false;
static bool g_prev_up = false;
static bool g_prev_down = false;
static uint32_t g_step_interval_ms = app_config::STEP_INTERVAL_MS;
static uint32_t g_last_step_ms = 0;

inline int cell_index(int x, int y) {
  return y * app_config::display_width() + x;
}

bool is_alive(uint16_t hue) {
  return hue != app_config::DEAD;
}

int wrap_x(int x) {
  if (x < 0) {
    return app_config::display_width() - 1;
  }
  if (x >= app_config::display_width()) {
    return 0;
  }
  return x;
}

int wrap_y(int y) {
  if (y < 0) {
    return app_config::display_height() - 1;
  }
  if (y >= app_config::display_height()) {
    return 0;
  }
  return y;
}

uint16_t get_hue(int x, int y) {
  return g_hues[cell_index(wrap_x(x), wrap_y(y))];
}

uint16_t wrap_hue(int value) {
  while (value < 0) {
    value += app_config::HUE_RANGE;
  }
  while (value >= app_config::HUE_RANGE) {
    value -= app_config::HUE_RANGE;
  }
  return static_cast<uint16_t>(value);
}

uint16_t random_hue() {
  return static_cast<uint16_t>(random(app_config::HUE_RANGE));
}

uint16_t average_hue(const uint16_t* hues, int count) {
  float x = 0.0f;
  float y = 0.0f;
  for (int i = 0; i < count; ++i) {
    const float angle = static_cast<float>(hues[i]) * (2.0f * PI / static_cast<float>(app_config::HUE_RANGE));
    x += cosf(angle);
    y += sinf(angle);
  }

  x /= static_cast<float>(count);
  y /= static_cast<float>(count);
  const float angle = atan2f(y, x);
  const int hue = static_cast<int>(roundf(angle * static_cast<float>(app_config::HUE_RANGE) / (2.0f * PI)));
  return wrap_hue(hue);
}

uint16_t mutate_hue(uint16_t hue) {
  const int mutation = random(-app_config::HUE_MUTATION, app_config::HUE_MUTATION + 1);
  return wrap_hue(static_cast<int>(hue) + mutation);
}

uint16_t hue_to_rgb565(uint16_t hue) {
  const uint8_t segment = hue / 256;
  const uint8_t offset = hue % 256;
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;

  switch (segment) {
    case 0:
      r = 255;
      g = offset;
      b = 0;
      break;
    case 1:
      r = 255 - offset;
      g = 255;
      b = 0;
      break;
    case 2:
      r = 0;
      g = 255;
      b = offset;
      break;
    case 3:
      r = 0;
      g = 255 - offset;
      b = 255;
      break;
    case 4:
      r = offset;
      g = 0;
      b = 255;
      break;
    default:
      r = 255;
      g = 0;
      b = 255 - offset;
      break;
  }

  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void randomize_world() {
  for (int y = 0; y < app_config::display_height(); ++y) {
    for (int x = 0; x < app_config::display_width(); ++x) {
      const int index = cell_index(x, y);
      g_hues[index] = random(100) < app_config::RANDOM_FILL_PERCENT ? random_hue() : app_config::DEAD;
      g_next_hues[index] = app_config::DEAD;
    }
  }
}

void draw_full_world() {
  auto& display = M5Cardputer.Display;
  display.startWrite();
  display.fillScreen(TFT_BLACK);
  for (int y = 0; y < app_config::display_height(); ++y) {
    for (int x = 0; x < app_config::display_width(); ++x) {
      const uint16_t hue = g_hues[cell_index(x, y)];
      if (is_alive(hue)) {
        display.writePixel(x, y, hue_to_rgb565(hue));
      }
    }
  }
  display.endWrite();
}

void step_world() {
  auto& display = M5Cardputer.Display;
  display.startWrite();

  for (int y = 0; y < app_config::display_height(); ++y) {
    for (int x = 0; x < app_config::display_width(); ++x) {
      uint16_t neighbor_hues[8];
      int neighbor_count = 0;

      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          const uint16_t neighbor_hue = get_hue(x + dx, y + dy);
          if (is_alive(neighbor_hue)) {
            neighbor_hues[neighbor_count++] = neighbor_hue;
          }
        }
      }

      const int index = cell_index(x, y);
      const uint16_t current = g_hues[index];
      uint16_t next = app_config::DEAD;

      if (is_alive(current)) {
        next = (neighbor_count == 2 || neighbor_count == 3) ? current : app_config::DEAD;
      } else if (neighbor_count == 3) {
        next = mutate_hue(average_hue(neighbor_hues, neighbor_count));
      }

      g_next_hues[index] = next;
      if (next != current) {
        display.writePixel(x, y, is_alive(next) ? hue_to_rgb565(next) : TFT_BLACK);
      }
    }
  }

  display.endWrite();

  for (int i = 0; i < g_cell_count; ++i) {
    g_hues[i] = g_next_hues[i];
  }
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

bool contains_hid_key(const Keyboard_Class::KeysState& status, uint8_t hid_key) {
  for (const auto raw_hid_key : status.hid_keys) {
    if (raw_hid_key == hid_key) {
      return true;
    }
  }
  return false;
}

void handle_input() {
  if (M5Cardputer.BtnA.wasClicked()) {
    randomize_world();
    draw_full_world();
    g_last_step_ms = millis();
  }

  if (!M5Cardputer.Keyboard.isChange()) {
    return;
  }

  const auto status = M5Cardputer.Keyboard.keysState();
  const bool enter_pressed = status.enter;
  const bool space_pressed = contains_char_key(status, ' ');
  const bool randomize_pressed = contains_alpha_key(status, 'r');
  const bool up_pressed = contains_hid_key(status, ';');
  const bool down_pressed = contains_hid_key(status, '.');

  if ((enter_pressed && !g_prev_enter) || (space_pressed && !g_prev_space)) {
    g_paused = !g_paused;
  }

  if (randomize_pressed && !g_prev_r) {
    randomize_world();
    draw_full_world();
    g_last_step_ms = millis();
  }

  if (up_pressed && !g_prev_up) {
    if (g_step_interval_ms > app_config::STEP_INTERVAL_MIN_MS) {
      g_step_interval_ms -= app_config::STEP_INTERVAL_DELTA_MS;
      if (g_step_interval_ms < app_config::STEP_INTERVAL_MIN_MS) {
        g_step_interval_ms = app_config::STEP_INTERVAL_MIN_MS;
      }
    }
  }

  if (down_pressed && !g_prev_down) {
    if (g_step_interval_ms < app_config::STEP_INTERVAL_MAX_MS) {
      g_step_interval_ms += app_config::STEP_INTERVAL_DELTA_MS;
      if (g_step_interval_ms > app_config::STEP_INTERVAL_MAX_MS) {
        g_step_interval_ms = app_config::STEP_INTERVAL_MAX_MS;
      }
    }
  }

  g_prev_enter = enter_pressed;
  g_prev_space = space_pressed;
  g_prev_r = randomize_pressed;
  g_prev_up = up_pressed;
  g_prev_down = down_pressed;
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, true);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  g_cell_count = app_config::display_width() * app_config::display_height();
  g_hues = std::make_unique<uint16_t[]>(g_cell_count);
  g_next_hues = std::make_unique<uint16_t[]>(g_cell_count);

  randomSeed(static_cast<uint32_t>(micros()) ^ static_cast<uint32_t>(millis()));
  randomize_world();
  draw_full_world();
  g_last_step_ms = millis();
}

void loop() {
  M5Cardputer.update();
  handle_input();

  const uint32_t now = millis();
  if (!g_paused && now - g_last_step_ms >= g_step_interval_ms) {
    g_last_step_ms = now;
    step_world();
  }
}
