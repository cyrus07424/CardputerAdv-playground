#include <Arduino.h>
#include <M5Cardputer.h>

namespace app_config {
inline int display_width() { return M5Cardputer.Display.width(); }
inline int display_height() { return M5Cardputer.Display.height(); }
#define SCREEN_W display_width()
#define SCREEN_H display_height()
constexpr uint32_t FRAME_INTERVAL_MS = 33;
constexpr float PLAYER_RADIUS = 7.0f;
constexpr int BUBBLE_COUNT = 18;
constexpr int FIX_SHIFT = 10;
constexpr int FIX_SCALE = 1 << FIX_SHIFT;
constexpr int SWIM_WAIT_FRAMES = 8;
constexpr int GRAVITY_STEP = 32;
constexpr int FRICTION_X = 8;
constexpr int GROUND_FRICTION_X = 96;
constexpr int GROUND_STOP_THRESHOLD = 96;
constexpr int MAX_SPEED = 0x800;
constexpr int SWIM_XM[3] = {-0x200, 0x200, 0};
constexpr int SWIM_YM[3] = {-0x200, -0x200, -0x2D4};
}

struct InputState {
  bool left = false;
  bool right = false;
  bool spout = false;
};

struct Bubble {
  float x;
  float y;
  float speed;
  uint8_t radius;
};

struct SpritePoint {
  int8_t x;
  int8_t y;
};

bool contains_hid_key(const Keyboard_Class::KeysState& status, uint8_t hid_key) {
  for (const auto raw_hid_key : status.hid_keys) {
    if ((raw_hid_key & ~SHIFT) == hid_key) {
      return true;
    }
  }
  return false;
}

class IkachanPortDemo {
 public:
  void begin() {
    reset_player();
    for (int i = 0; i < app_config::BUBBLE_COUNT; ++i) {
      reset_bubble(bubbles_[i], true);
    }
  }

  void update(const InputState& input) {
    const bool spout_trigger = input.spout && !prev_spout_;
    direction_ = 2;
    if (input.left && !input.right) {
      direction_ = 0;
    } else if (input.right && !input.left) {
      direction_ = 1;
    }

    if (swim_wait_ > 0) {
      --swim_wait_;
    }
    if (spout_flash_ > 0) {
      --spout_flash_;
    }

    if (spout_trigger && swim_wait_ == 0) {
      player_vx_ += app_config::SWIM_XM[direction_];
      player_vy_ += app_config::SWIM_YM[direction_];
      swim_wait_ = app_config::SWIM_WAIT_FRAMES;
      spout_flash_ = 2;
    }

    if (player_vy_ < app_config::MAX_SPEED) {
      player_vy_ += app_config::GRAVITY_STEP;
    }

    clamp_speed(player_vx_);
    clamp_speed(player_vy_);

    if (player_vx_ > 0) {
      player_vx_ -= app_config::FRICTION_X;
      if (player_vx_ < 0) {
        player_vx_ = 0;
      }
    } else if (player_vx_ < 0) {
      player_vx_ += app_config::FRICTION_X;
      if (player_vx_ > 0) {
        player_vx_ = 0;
      }
    }

    if (player_vx_ >= app_config::FRICTION_X || player_vx_ <= -app_config::FRICTION_X) {
      player_x_ += player_vx_;
    }
    player_y_ += player_vy_;

    bounce_in_bounds();
    if (on_ground_) {
      if (player_vx_ > 0) {
        player_vx_ -= app_config::GROUND_FRICTION_X;
        if (player_vx_ < 0) {
          player_vx_ = 0;
        }
      } else if (player_vx_ < 0) {
        player_vx_ += app_config::GROUND_FRICTION_X;
        if (player_vx_ > 0) {
          player_vx_ = 0;
        }
      }

      if (player_vx_ > -app_config::GROUND_STOP_THRESHOLD && player_vx_ < app_config::GROUND_STOP_THRESHOLD) {
        player_vx_ = 0;
      }
    }
    update_bubbles(spout_trigger);
    prev_spout_ = input.spout;
    ++frame_counter_;
  }

  void draw(M5Canvas& canvas, const InputState& input) const {
    canvas.fillScreen(TFT_NAVY);
    draw_background(canvas);
    draw_bubbles(canvas);
    draw_player(canvas, input);
    draw_hud(canvas, input);
  }

 private:
  static constexpr SpritePoint kFrontSprite_[50] = {
      {0, -5},
      {-1, -4}, {0, -4}, {1, -4},
      {-2, -3}, {-1, -3}, {0, -3}, {1, -3}, {2, -3},
      {-2, -2}, {-1, -2}, {0, -2}, {1, -2}, {2, -2},
      {-3, -1}, {-2, -1}, {-1, -1}, {0, -1}, {1, -1}, {2, -1}, {3, -1},
      {-2, 0}, {-1, 0}, {0, 0}, {1, 0}, {2, 0},
      {-2, 1}, {-1, 1}, {0, 1}, {1, 1}, {2, 1},
      {-2, 2}, {-1, 2}, {0, 2}, {1, 2}, {2, 2},
      {-3, 3}, {-2, 3}, {-1, 3}, {0, 3}, {1, 3}, {2, 3}, {3, 3},
      {-3, 4}, {-1, 4}, {1, 4}, {3, 4},
      {-3, 5}, {0, 5}, {3, 5},
  };

  static int16_t scaled_round(int16_t value) {
    constexpr int kCos45Scaled = 181;
    constexpr int kScale = 256;
    if (value >= 0) {
      return static_cast<int16_t>((value * kCos45Scaled + kScale / 2) / kScale);
    }
    return static_cast<int16_t>((value * kCos45Scaled - kScale / 2) / kScale);
  }

  static void draw_sprite_dot(M5Canvas& canvas, int16_t x, int16_t y) {
    canvas.fillRect(x - 1, y - 1, 2, 2, TFT_WHITE);
  }

  static void draw_front_sprite(M5Canvas& canvas, int16_t px, int16_t py) {
    for (const SpritePoint point : kFrontSprite_) {
      draw_sprite_dot(canvas, px + point.x, py + point.y);
    }
  }

  static void draw_rotated_sprite(M5Canvas& canvas, int16_t px, int16_t py, bool clockwise) {
    for (const SpritePoint point : kFrontSprite_) {
      const int16_t rotated_x =
          clockwise ? scaled_round(point.x - point.y) : scaled_round(point.x + point.y);
      const int16_t rotated_y =
          clockwise ? scaled_round(point.x + point.y) : scaled_round(-point.x + point.y);
      draw_sprite_dot(canvas, px + rotated_x, py + rotated_y);
    }
  }

  void reset_player() {
    player_x_ = (app_config::SCREEN_W / 2) << app_config::FIX_SHIFT;
    player_y_ = (app_config::SCREEN_H / 2) << app_config::FIX_SHIFT;
    player_vx_ = 0;
    player_vy_ = 0;
    direction_ = 2;
    on_ground_ = false;
    frame_counter_ = 0;
  }

  void reset_bubble(Bubble& bubble, bool random_y) {
    bubble.x = static_cast<float>(random(6, app_config::SCREEN_W - 6));
    bubble.y = random_y ? static_cast<float>(random(0, app_config::SCREEN_H))
                        : static_cast<float>(app_config::SCREEN_H + random(4, 26));
    bubble.speed = static_cast<float>(random(10, 28)) / 10.0f;
    bubble.radius = static_cast<uint8_t>(random(1, 3));
  }

  static void clamp_speed(int32_t& value) {
    if (value > app_config::MAX_SPEED) {
      value = app_config::MAX_SPEED;
    } else if (value < -app_config::MAX_SPEED) {
      value = -app_config::MAX_SPEED;
    }
  }

  void bounce_in_bounds() {
    const int32_t radius = static_cast<int32_t>(app_config::PLAYER_RADIUS * app_config::FIX_SCALE);
    on_ground_ = false;
    if (player_x_ < radius) {
      player_x_ = radius;
      player_vx_ = 0;
    } else if (player_x_ > (app_config::SCREEN_W << app_config::FIX_SHIFT) - radius) {
      player_x_ = (app_config::SCREEN_W << app_config::FIX_SHIFT) - radius;
      player_vx_ = 0;
    }

    if (player_y_ < (HEADER_H() << app_config::FIX_SHIFT) + radius) {
      player_y_ = (HEADER_H() << app_config::FIX_SHIFT) + radius;
      if (player_vy_ < 0) {
        player_vy_ = 0;
      }
    } else if (player_y_ > ((app_config::SCREEN_H - 10) << app_config::FIX_SHIFT) - radius) {
      player_y_ = ((app_config::SCREEN_H - 10) << app_config::FIX_SHIFT) - radius;
      on_ground_ = true;
      if (player_vy_ > 0) {
        player_vy_ = 0;
      }
    }
  }

  void update_bubbles(bool spout) {
    for (int i = 0; i < app_config::BUBBLE_COUNT; ++i) {
      bubbles_[i].y -= bubbles_[i].speed * (spout ? 1.6f : 1.0f);
      if (bubbles_[i].y < -4.0f) {
        reset_bubble(bubbles_[i], false);
      }
    }
  }

  void draw_background(M5Canvas& canvas) const {
    canvas.fillRect(0, 0, canvas.width(), 13, TFT_DARKCYAN);
    canvas.fillRect(0, canvas.height() - 10, canvas.width(), 10, TFT_DARKGREEN);
    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(4, 3);
    canvas.print("Ikachan Cardputer Port");
  }

  void draw_bubbles(M5Canvas& canvas) const {
    for (const Bubble& bubble : bubbles_) {
      canvas.drawCircle(static_cast<int16_t>(bubble.x), static_cast<int16_t>(bubble.y), bubble.radius, TFT_CYAN);
    }
  }

  void draw_player(M5Canvas& canvas, const InputState& input) const {
    const int16_t px = static_cast<int16_t>(player_x_ >> app_config::FIX_SHIFT);
    const int16_t py = static_cast<int16_t>(player_y_ >> app_config::FIX_SHIFT);
    if (input.left && !input.right) {
      draw_rotated_sprite(canvas, px, py, false);
    } else if (input.right && !input.left) {
      draw_rotated_sprite(canvas, px, py, true);
    } else {
      draw_front_sprite(canvas, px, py);
    }
  }

  void draw_hud(M5Canvas& canvas, const InputState& input) const {
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(4, 18);
    canvas.print("Ikachan-like spout inertia");

    canvas.setTextColor(TFT_CYAN);
    canvas.setCursor(4, 29);
    canvas.printf("XM:%4ld YM:%4ld", static_cast<long>(player_vx_), static_cast<long>(player_vy_));

    canvas.setTextColor(spout_flash_ > 0 ? TFT_ORANGE : TFT_LIGHTGREY);
    canvas.setCursor(4, 40);
    if (input.left && !input.right) {
      canvas.printf("POSE LU SW:%d", swim_wait_);
    } else if (input.right && !input.left) {
      canvas.printf("POSE RU SW:%d", swim_wait_);
    } else {
      canvas.printf("POSE UP SW:%d", swim_wait_);
    }

    canvas.setTextColor(TFT_WHITE);
    canvas.setCursor(4, 124);
    canvas.print(", / face   A spout");
  }

  static constexpr int HEADER_H() {
    return 13;
  }

  Bubble bubbles_[app_config::BUBBLE_COUNT];
  int32_t player_x_ = 0;
  int32_t player_y_ = 0;
  int32_t player_vx_ = 0;
  int32_t player_vy_ = 0;
  uint8_t direction_ = 2;
  bool on_ground_ = false;
  bool prev_spout_ = false;
  int swim_wait_ = 0;
  int spout_flash_ = 0;
  uint32_t frame_counter_ = 0;
};

M5Canvas g_canvas(&M5Cardputer.Display);
IkachanPortDemo g_demo;
uint32_t g_last_frame_ms = 0;

void update_keyboard_state() {
  M5Cardputer.update();
}

InputState read_input() {
  const auto& status = M5Cardputer.Keyboard.keysState();
  InputState input;
  input.left = contains_hid_key(status, 0x36);
  input.right = contains_hid_key(status, 0x38);
  input.spout = contains_hid_key(status, 0x04);
  return input;
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, true);

  randomSeed(static_cast<uint32_t>(micros()));

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);

  g_canvas.setColorDepth(16);
  g_canvas.createSprite(app_config::SCREEN_W, app_config::SCREEN_H);
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);

  g_demo.begin();
  g_last_frame_ms = millis();
  Serial.println("Cardputer ADV Ikachan clean-room port demo started");
}

void loop() {
  update_keyboard_state();
  const InputState input = read_input();

  const uint32_t now = millis();
  if (now - g_last_frame_ms >= app_config::FRAME_INTERVAL_MS) {
    g_demo.update(input);
    g_demo.draw(g_canvas, input);
    M5Cardputer.Display.startWrite();
    g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
    M5Cardputer.Display.endWrite();
    g_last_frame_ms = now;
  }
}
