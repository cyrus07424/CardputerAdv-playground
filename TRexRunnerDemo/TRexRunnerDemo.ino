#include <Arduino.h>
#include <M5Cardputer.h>

#include "TRexSprites.h"

namespace app_config {
inline int display_width() { return M5Cardputer.Display.width(); }
inline int display_height() { return M5Cardputer.Display.height(); }
#define SCREEN_W display_width()
#define SCREEN_H display_height()
constexpr uint32_t FRAME_INTERVAL_MS = 16;
constexpr float FPS = 60.0f;
constexpr float MS_PER_FRAME = 1000.0f / FPS;
constexpr float ACCELERATION = 0.001f;
constexpr float BG_CLOUD_SPEED = 0.2f;
constexpr int BOTTOM_PAD = 10;
constexpr int CLEAR_TIME = 3000;
constexpr float GAP_COEFFICIENT = 0.6f;
constexpr float GRAVITY = 0.6f;
constexpr float INITIAL_JUMP_VELOCITY = 12.0f;
constexpr float MAX_SPEED = 13.0f;
constexpr int MAX_CLOUDS = 6;
constexpr int MAX_OBSTACLE_DUPLICATION = 2;
constexpr int MAX_OBSTACLES = 6;
constexpr float SPEED = 1.0f;
constexpr float SPEED_DROP_COEFFICIENT = 3.0f;
constexpr int MAX_DISTANCE_UNITS = 5;
constexpr float DISTANCE_COEFFICIENT = 0.025f;
constexpr uint32_t BLINK_TIMING = 7000;
constexpr uint8_t HID_A = 0x04;
constexpr uint8_t HID_SEMICOLON = 0x33;
constexpr uint8_t HID_PERIOD = 0x37;
constexpr uint8_t HID_ENTER = 0x28;
constexpr int PLAYFIELD_TOP = 17;
constexpr int FLOOR_Y = PLAYFIELD_TOP + 84;
constexpr int HORIZON_Y = FLOOR_Y;
}  // namespace app_config

enum class Phase : uint8_t {
  Waiting,
  Playing,
  GameOver,
};

enum class TrexAnimState : uint8_t {
  Waiting,
  Running,
  Jumping,
  Ducking,
  Crashed,
};

enum class ObstacleId : uint8_t {
  SmallCactus,
  LargeCactus,
  Pterodactyl,
};

struct InputState {
  bool jump_pressed = false;
  bool jump_held = false;
  bool duck_held = false;
  bool restart_pressed = false;
};

struct CollisionBox {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};

struct ObstacleType {
  ObstacleId id;
  int16_t width;
  int16_t height;
  int16_t y_pos[3];
  uint8_t y_pos_count;
  float multiple_speed;
  float min_speed;
  int16_t min_gap;
  float speed_offset;
  uint8_t num_frames;
  uint16_t frame_rate_ms;
  const CollisionBox* collision_boxes;
  uint8_t collision_box_count;
};

struct Trex {
  float x_pos = 50.0f;
  float y_pos = 0.0f;
  float ground_y_pos = 0.0f;
  float min_jump_height = 0.0f;
  float jump_velocity = 0.0f;
  bool jumping = false;
  bool ducking = false;
  bool speed_drop = false;
  bool reached_min_height = false;
  uint32_t jump_count = 0;
  TrexAnimState state = TrexAnimState::Waiting;
  uint32_t anim_timer = 0;
  uint8_t anim_frame = 0;
  uint32_t blink_timer = 0;
  uint32_t blink_delay = app_config::BLINK_TIMING;

  void reset() {
    y_pos = ground_y_pos;
    jump_velocity = 0.0f;
    jumping = false;
    ducking = false;
    speed_drop = false;
    reached_min_height = false;
    state = TrexAnimState::Running;
    anim_timer = 0;
    anim_frame = 0;
  }
};

struct Obstacle {
  bool active = false;
  const ObstacleType* type = nullptr;
  float x_pos = 0.0f;
  int16_t y_pos = 0;
  int16_t width = 0;
  uint8_t size = 1;
  int16_t gap = 0;
  float speed_offset = 0.0f;
  uint8_t current_frame = 0;
  uint16_t timer = 0;
  bool following_created = false;
  CollisionBox collision_boxes[6];
  uint8_t collision_box_count = 0;
};

struct Cloud {
  bool active = false;
  float x_pos = 0.0f;
  int16_t y_pos = 0;
  int16_t gap = 0;
};

bool contains_hid_key(const Keyboard_Class::KeysState& status, uint8_t hid_key) {
  for (const auto raw_hid_key : status.hid_keys) {
    if ((raw_hid_key & ~SHIFT) == hid_key) {
      return true;
    }
  }
  return false;
}

constexpr CollisionBox kTrexRunningBoxes[] = {
    {11, 0, 9, 6},
    {8, 6, 12, 6},
    {6, 12, 14, 5},
    {6, 17, 13, 5},
    {6, 22, 4, 4},
    {14, 22, 4, 4},
};

constexpr CollisionBox kTrexDuckingBoxes[] = {
    {8, 2, 20, 8},
    {5, 10, 24, 5},
    {22, 13, 8, 4},
};

constexpr CollisionBox kSmallCactusBoxes[] = {
    {0, 5, 4, 20},
    {3, 0, 5, 26},
    {8, 3, 5, 11},
};

constexpr CollisionBox kLargeCactusBoxes[] = {
    {0, 9, 5, 29},
    {6, 0, 5, 37},
    {10, 8, 8, 29},
};

constexpr CollisionBox kPteroBoxes[] = {
    {11, 11, 12, 4},
    {14, 16, 18, 5},
    {2, 11, 3, 2},
    {5, 8, 3, 5},
    {8, 6, 5, 7},
};

constexpr ObstacleType kObstacleTypes[] = {
    {ObstacleId::SmallCactus, 17, 35, {0, 0, 0}, 1, 4.0f, 0.0f, 120, 0.0f, 1, 0,
     kSmallCactusBoxes, 3},
    {ObstacleId::LargeCactus, 25, 50, {0, 0, 0}, 1, 7.0f, 0.0f, 120, 0.0f, 1, 0,
     kLargeCactusBoxes, 3},
    {ObstacleId::Pterodactyl, 46, 40, {73, 58, 43}, 3, 999.0f, 8.5f, 150, 0.8f, 2,
     static_cast<uint16_t>(1000 / 6), kPteroBoxes, 5},
};

class TRexRunnerDemo {
 public:
  void begin() {
    reset_all();
    last_update_ms_ = millis();
  }

  void update(const InputState& input) {
    const uint32_t now = millis();
    uint32_t delta_ms = now - last_update_ms_;
    if (delta_ms > 50) {
      delta_ms = 50;
    }
    last_update_ms_ = now;

    update_trex_animation(delta_ms);
    update_waiting_blink(delta_ms);

    if (phase_ == Phase::Waiting) {
      if (input.jump_pressed) {
        phase_ = Phase::Playing;
        trex_.state = TrexAnimState::Running;
        start_jump();
      }
    } else if (phase_ == Phase::GameOver) {
      update_clouds(delta_ms);
      update_horizon(delta_ms, 0.0f);
      if (input.restart_pressed) {
        reset_all();
      }
      prev_input_ = input;
      return;
    } else {
      update_playing(input, delta_ms);
    }

    prev_input_ = input;
  }

  void draw(M5Canvas& canvas) const {
    canvas.fillScreen(TFT_WHITE);
    draw_horizon(canvas);
    draw_clouds(canvas);
    draw_obstacles(canvas);
    draw_trex(canvas);
    draw_score(canvas);

    if (phase_ == Phase::Waiting) {
      draw_centered(canvas, "T-REX RUNNER", 26, TFT_BLACK);
      draw_centered(canvas, "A / ; jump", 42, TFT_DARKGREY);
      draw_centered(canvas, ". duck", 54, TFT_DARKGREY);
      draw_centered(canvas, "press jump to start", 72, TFT_BLACK);
    } else if (phase_ == Phase::GameOver) {
      draw_centered(canvas, "GAME OVER", 34, TFT_RED);
      draw_centered(canvas, "A / Enter restart", 50, TFT_BLACK);
    }
  }

 private:
  void reset_all() {
    phase_ = Phase::Waiting;
    current_speed_ = app_config::SPEED;
    distance_ran_ = 0.0f;
    running_time_ = 0;
    obstacle_count_ = 0;
    cloud_count_ = 0;
    obstacle_history_len_ = 0;
    horizon_scroll_ = 0.0f;
    high_score_ = max(high_score_, static_cast<uint32_t>(distance_ran_));
    trex_.ground_y_pos = static_cast<float>(app_config::FLOOR_Y - trex_sprites::TREX_JUMP_SPRITE.height);
    trex_.min_jump_height = trex_.ground_y_pos - 30.0f;
    trex_.x_pos = 50.0f;
    trex_.y_pos = trex_.ground_y_pos;
    trex_.jump_velocity = 0.0f;
    trex_.jumping = false;
    trex_.ducking = false;
    trex_.speed_drop = false;
    trex_.reached_min_height = false;
    trex_.jump_count = 0;
    trex_.state = TrexAnimState::Waiting;
    trex_.anim_timer = 0;
    trex_.anim_frame = 0;
    trex_.blink_timer = 0;
    trex_.blink_delay = app_config::BLINK_TIMING + random(0, app_config::BLINK_TIMING);
    for (auto& obstacle : obstacles_) {
      obstacle = Obstacle{};
    }
    for (auto& cloud : clouds_) {
      cloud = Cloud{};
    }
  }

  void update_playing(const InputState& input, uint32_t delta_ms) {
    if (input.jump_pressed && !trex_.jumping && !trex_.ducking) {
      start_jump();
    }

    if (input.duck_held) {
      if (trex_.jumping) {
        trex_.speed_drop = true;
      } else {
        set_duck(true);
      }
    } else {
      trex_.speed_drop = false;
      set_duck(false);
    }

    if (prev_input_.jump_held && !input.jump_held) {
      end_jump();
    }

    if (trex_.jumping) {
      update_jump(delta_ms);
    }

    running_time_ += delta_ms;
    const bool has_obstacles = running_time_ > app_config::CLEAR_TIME;

    update_horizon(delta_ms, current_speed_);
    update_clouds(delta_ms);
    if (has_obstacles) {
      update_obstacles(delta_ms);
    }

    if (!check_collision()) {
      distance_ran_ += current_speed_ * static_cast<float>(delta_ms) / app_config::MS_PER_FRAME;
      if (current_speed_ < app_config::MAX_SPEED) {
        current_speed_ += app_config::ACCELERATION;
      }
      high_score_ = max(high_score_, get_actual_distance());
    } else {
      phase_ = Phase::GameOver;
      trex_.state = TrexAnimState::Crashed;
      high_score_ = max(high_score_, get_actual_distance());
    }
  }

  void update_waiting_blink(uint32_t delta_ms) {
    if (phase_ != Phase::Waiting) {
      return;
    }

    trex_.blink_timer += delta_ms;
    if (trex_.blink_timer >= trex_.blink_delay) {
      trex_.blink_timer = 0;
      trex_.anim_frame = (trex_.anim_frame + 1) % 2;
      if (trex_.anim_frame == 0) {
        trex_.blink_delay = app_config::BLINK_TIMING + random(0, app_config::BLINK_TIMING);
      } else {
        trex_.blink_delay = 160;
      }
    }
  }

  void update_trex_animation(uint32_t delta_ms) {
    if (phase_ == Phase::Waiting || trex_.state == TrexAnimState::Jumping || trex_.state == TrexAnimState::Crashed) {
      return;
    }

    const uint16_t frame_rate = trex_.state == TrexAnimState::Running ? static_cast<uint16_t>(1000 / 12)
                                                                      : static_cast<uint16_t>(1000 / 8);
    trex_.anim_timer += delta_ms;
    if (trex_.anim_timer >= frame_rate) {
      trex_.anim_timer = 0;
      trex_.anim_frame = (trex_.anim_frame + 1) % 2;
    }
  }

  void start_jump() {
    if (trex_.jumping) {
      return;
    }
    trex_.state = TrexAnimState::Jumping;
    trex_.jump_velocity = -10.0f - (current_speed_ / 10.0f);
    trex_.jumping = true;
    trex_.reached_min_height = false;
    trex_.speed_drop = false;
    set_duck(false);
  }

  void end_jump() {
    if (trex_.reached_min_height && trex_.jump_velocity < -5.0f) {
      trex_.jump_velocity = -5.0f;
    }
  }

  void update_jump(uint32_t delta_ms) {
    const float frames_elapsed = static_cast<float>(delta_ms) / app_config::MS_PER_FRAME;
    if (trex_.speed_drop) {
      trex_.y_pos += roundf(trex_.jump_velocity * app_config::SPEED_DROP_COEFFICIENT * frames_elapsed);
    } else {
      trex_.y_pos += roundf(trex_.jump_velocity * frames_elapsed);
    }

    trex_.jump_velocity += app_config::GRAVITY * frames_elapsed;

    if (trex_.y_pos < trex_.min_jump_height || trex_.speed_drop) {
      trex_.reached_min_height = true;
    }

    if (trex_.y_pos < trex_.ground_y_pos - 30.0f || trex_.speed_drop) {
      end_jump();
    }

    if (trex_.y_pos > trex_.ground_y_pos) {
      trex_.y_pos = trex_.ground_y_pos;
      trex_.jump_velocity = 0.0f;
      trex_.jumping = false;
      trex_.speed_drop = false;
      ++trex_.jump_count;
      trex_.state = trex_.ducking ? TrexAnimState::Ducking : TrexAnimState::Running;
    }
  }

  void set_duck(bool enabled) {
    trex_.ducking = enabled;
    if (!trex_.jumping) {
      trex_.state = enabled ? TrexAnimState::Ducking : TrexAnimState::Running;
    }
  }

  void update_horizon(uint32_t delta_ms, float speed) {
    horizon_scroll_ += floorf((speed * app_config::FPS / 1000.0f) * static_cast<float>(delta_ms));
    while (horizon_scroll_ >= trex_sprites::HORIZON_SPRITE.width) {
      horizon_scroll_ -= trex_sprites::HORIZON_SPRITE.width;
    }
  }

  void update_clouds(uint32_t delta_ms) {
    const float cloud_speed = app_config::BG_CLOUD_SPEED / 1000.0f * static_cast<float>(delta_ms) * current_speed_;
    for (int i = 0; i < cloud_count_;) {
      clouds_[i].x_pos -= cloud_speed;
      if (clouds_[i].x_pos + trex_sprites::CLOUD_SPRITE.width <= 0) {
        remove_cloud(i);
      } else {
        ++i;
      }
    }

    if (cloud_count_ == 0) {
      add_cloud();
      return;
    }

    const Cloud& last_cloud = clouds_[cloud_count_ - 1];
    if (cloud_count_ < app_config::MAX_CLOUDS &&
        (last_cloud.x_pos > static_cast<float>(app_config::SCREEN_W) - last_cloud.gap) &&
        0.5f > static_cast<float>(random(1000)) / 1000.0f) {
      add_cloud();
    }
  }

  void add_cloud() {
    if (cloud_count_ >= app_config::MAX_CLOUDS) {
      return;
    }
    Cloud cloud;
    cloud.active = true;
    cloud.x_pos = static_cast<float>(app_config::SCREEN_W);
    cloud.y_pos = random(app_config::PLAYFIELD_TOP + 4, app_config::PLAYFIELD_TOP + 37);
    cloud.gap = random(100, 401);
    clouds_[cloud_count_++] = cloud;
  }

  void remove_cloud(int index) {
    for (int i = index; i + 1 < cloud_count_; ++i) {
      clouds_[i] = clouds_[i + 1];
    }
    --cloud_count_;
  }

  void update_obstacles(uint32_t delta_ms) {
    for (int i = 0; i < obstacle_count_;) {
      update_obstacle(obstacles_[i], delta_ms);
      if (!obstacles_[i].active) {
        remove_obstacle(i);
      } else {
        ++i;
      }
    }

    if (obstacle_count_ > 0) {
      Obstacle& last = obstacles_[obstacle_count_ - 1];
      if (!last.following_created && is_obstacle_visible(last) &&
          (last.x_pos + last.width + last.gap) < app_config::SCREEN_W) {
        add_new_obstacle(current_speed_);
        last.following_created = true;
      }
    } else {
      add_new_obstacle(current_speed_);
    }
  }

  void update_obstacle(Obstacle& obstacle, uint32_t delta_ms) {
    float speed = current_speed_;
    if (obstacle.type->speed_offset != 0.0f) {
      speed += obstacle.speed_offset;
    }
    obstacle.x_pos -= floorf((speed * app_config::FPS / 1000.0f) * static_cast<float>(delta_ms));

    if (obstacle.type->num_frames > 1) {
      obstacle.timer += delta_ms;
      if (obstacle.timer >= obstacle.type->frame_rate_ms) {
        obstacle.current_frame = obstacle.current_frame == obstacle.type->num_frames - 1 ? 0 : obstacle.current_frame + 1;
        obstacle.timer = 0;
      }
    }

    if (obstacle.x_pos + obstacle.width <= 0) {
      obstacle.active = false;
    }
  }

  void add_new_obstacle(float current_speed) {
    if (obstacle_count_ >= app_config::MAX_OBSTACLES) {
      return;
    }

    const ObstacleType* obstacle_type = nullptr;
    for (int attempts = 0; attempts < 8; ++attempts) {
      const ObstacleType& candidate = kObstacleTypes[random(0, 3)];
      if (!duplicate_obstacle_check(candidate.id) && current_speed >= candidate.min_speed) {
        obstacle_type = &candidate;
        break;
      }
    }
    if (!obstacle_type) {
      return;
    }

    Obstacle obstacle;
    obstacle.active = true;
    obstacle.type = obstacle_type;
    obstacle.size = static_cast<uint8_t>(random(1, 4));
    if (obstacle.size > 1 && obstacle_type->multiple_speed > current_speed) {
      obstacle.size = 1;
    }
    obstacle.width = obstacle_type->width * obstacle.size;
    if (obstacle_type->id == ObstacleId::Pterodactyl) {
      obstacle.width = obstacle_type->width;
    }
    obstacle.x_pos = static_cast<float>(app_config::SCREEN_W + obstacle_type->width);
    const BitmapSprite& sprite = sprite_for(obstacle_type->id, obstacle.size, 0);
    obstacle.y_pos = obstacle_type->id == ObstacleId::Pterodactyl
                         ? obstacle_type->y_pos[random(0, obstacle_type->y_pos_count)]
                         : app_config::FLOOR_Y - sprite.height;
    obstacle.collision_box_count = obstacle_type->collision_box_count;
    for (uint8_t i = 0; i < obstacle.collision_box_count; ++i) {
      obstacle.collision_boxes[i] = obstacle_type->collision_boxes[i];
    }
    if (obstacle.size > 1 && obstacle.collision_box_count >= 3) {
      obstacle.collision_boxes[1].width =
          obstacle.width - obstacle.collision_boxes[0].width - obstacle.collision_boxes[2].width;
      obstacle.collision_boxes[2].x = obstacle.width - obstacle.collision_boxes[2].width;
    }
    if (obstacle_type->speed_offset != 0.0f) {
      obstacle.speed_offset = random(0, 2) == 0 ? obstacle_type->speed_offset : -obstacle_type->speed_offset;
    }
    obstacle.gap = get_obstacle_gap(obstacle, current_speed);
    obstacles_[obstacle_count_++] = obstacle;
    push_obstacle_history(obstacle_type->id);
  }

  int get_obstacle_gap(const Obstacle& obstacle, float speed) const {
    const int min_gap = roundf(obstacle.width * speed + obstacle.type->min_gap * app_config::GAP_COEFFICIENT);
    const int max_gap = roundf(min_gap * 1.5f);
    return random(min_gap, max_gap + 1);
  }

  bool duplicate_obstacle_check(ObstacleId next_type) const {
    int duplicate_count = 0;
    for (uint8_t i = 0; i < obstacle_history_len_; ++i) {
      duplicate_count = obstacle_history_[i] == next_type ? duplicate_count + 1 : 0;
    }
    return duplicate_count >= app_config::MAX_OBSTACLE_DUPLICATION;
  }

  void push_obstacle_history(ObstacleId id) {
    for (int i = min<int>(obstacle_history_len_, app_config::MAX_OBSTACLE_DUPLICATION - 1); i > 0; --i) {
      obstacle_history_[i] = obstacle_history_[i - 1];
    }
    obstacle_history_[0] = id;
    obstacle_history_len_ = min<uint8_t>(app_config::MAX_OBSTACLE_DUPLICATION, obstacle_history_len_ + 1);
  }

  void remove_obstacle(int index) {
    for (int i = index; i + 1 < obstacle_count_; ++i) {
      obstacles_[i] = obstacles_[i + 1];
    }
    --obstacle_count_;
  }

  bool is_obstacle_visible(const Obstacle& obstacle) const {
    return obstacle.x_pos + obstacle.width > 0.0f;
  }

  static bool box_compare(const CollisionBox& a, const CollisionBox& b) {
    return a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y;
  }

  bool check_collision() const {
    if (obstacle_count_ == 0) {
      return false;
    }

    const Obstacle& obstacle = obstacles_[0];
    const BitmapSprite& obstacle_bitmap = obstacle_sprite(obstacle);
    CollisionBox trex_outer = {
        static_cast<int16_t>(trex_.x_pos + 1),
        static_cast<int16_t>(current_trex_y() + 1),
        static_cast<int16_t>(current_trex_width() - 2),
        static_cast<int16_t>(current_trex_height() - 2),
    };
    CollisionBox obstacle_outer = {
        static_cast<int16_t>(obstacle.x_pos + 1),
        static_cast<int16_t>(obstacle.y_pos + 1),
        static_cast<int16_t>(obstacle_bitmap.width - 2),
        static_cast<int16_t>(obstacle_bitmap.height - 2),
    };

    if (!box_compare(trex_outer, obstacle_outer)) {
      return false;
    }

    const CollisionBox* trex_boxes = trex_.ducking ? kTrexDuckingBoxes : kTrexRunningBoxes;
    const uint8_t trex_box_count = trex_.ducking ? 1 : 6;
    for (uint8_t t = 0; t < trex_box_count; ++t) {
      CollisionBox adjusted_trex = {
          static_cast<int16_t>(trex_outer.x + trex_boxes[t].x),
          static_cast<int16_t>(trex_outer.y + trex_boxes[t].y),
          trex_boxes[t].width,
          trex_boxes[t].height,
      };
      for (uint8_t i = 0; i < obstacle.collision_box_count; ++i) {
        CollisionBox adjusted_obstacle = {
            static_cast<int16_t>(obstacle_outer.x + obstacle.collision_boxes[i].x),
            static_cast<int16_t>(obstacle_outer.y + obstacle.collision_boxes[i].y),
            obstacle.collision_boxes[i].width,
            obstacle.collision_boxes[i].height,
        };
        if (box_compare(adjusted_trex, adjusted_obstacle)) {
          return true;
        }
      }
    }

    return false;
  }

  int current_trex_y() const {
    return static_cast<int>(trex_.ducking && !trex_.jumping
                                ? trex_.ground_y_pos +
                                      (trex_sprites::TREX_JUMP_SPRITE.height - trex_sprites::TREX_DUCK_0_SPRITE.height)
                                : trex_.y_pos);
  }

  int current_trex_width() const {
    return current_trex_sprite().width;
  }

  int current_trex_height() const {
    return current_trex_sprite().height;
  }

  const BitmapSprite& current_trex_sprite() const {
    if (phase_ == Phase::GameOver) {
      return trex_sprites::TREX_WAIT_SPRITE;
    }
    if (phase_ == Phase::Waiting) {
      return trex_.anim_frame == 0 ? trex_sprites::TREX_WAIT_SPRITE : trex_sprites::TREX_JUMP_SPRITE;
    }
    if (trex_.jumping) {
      return trex_sprites::TREX_JUMP_SPRITE;
    }
    if (trex_.ducking) {
      return trex_.anim_frame == 0 ? trex_sprites::TREX_DUCK_0_SPRITE : trex_sprites::TREX_DUCK_1_SPRITE;
    }
    return trex_.anim_frame == 0 ? trex_sprites::TREX_RUN_0_SPRITE : trex_sprites::TREX_RUN_1_SPRITE;
  }

  const BitmapSprite& sprite_for(ObstacleId id, uint8_t size, uint8_t frame) const {
    if (id == ObstacleId::SmallCactus) {
      if (size == 1) return trex_sprites::CACTUS_SMALL_1_SPRITE;
      if (size == 2) return trex_sprites::CACTUS_SMALL_2_SPRITE;
      return trex_sprites::CACTUS_SMALL_3_SPRITE;
    }
    if (id == ObstacleId::LargeCactus) {
      if (size == 1) return trex_sprites::CACTUS_LARGE_1_SPRITE;
      if (size == 2) return trex_sprites::CACTUS_LARGE_2_SPRITE;
      return trex_sprites::CACTUS_LARGE_3_SPRITE;
    }
    return frame == 0 ? trex_sprites::PTERO_0_SPRITE : trex_sprites::PTERO_1_SPRITE;
  }

  const BitmapSprite& obstacle_sprite(const Obstacle& obstacle) const {
    return sprite_for(obstacle.type->id, obstacle.size, obstacle.current_frame);
  }

  static void draw_centered(M5Canvas& canvas, const char* text, int y, uint16_t color) {
    canvas.setTextColor(color);
    canvas.setCursor((canvas.width() - canvas.textWidth(text)) / 2, y);
    canvas.print(text);
  }

  void draw_horizon(M5Canvas& canvas) const {
    canvas.drawXBitmap(-static_cast<int>(horizon_scroll_), app_config::HORIZON_Y, trex_sprites::HORIZON_SPRITE.data,
                       trex_sprites::HORIZON_SPRITE.width, trex_sprites::HORIZON_SPRITE.height, TFT_DARKGREY);
    canvas.drawXBitmap(-static_cast<int>(horizon_scroll_) + trex_sprites::HORIZON_SPRITE.width, app_config::HORIZON_Y,
                       trex_sprites::HORIZON_SPRITE.data, trex_sprites::HORIZON_SPRITE.width,
                       trex_sprites::HORIZON_SPRITE.height, TFT_DARKGREY);
  }

  void draw_clouds(M5Canvas& canvas) const {
    for (int i = 0; i < cloud_count_; ++i) {
      canvas.drawXBitmap(static_cast<int>(clouds_[i].x_pos), clouds_[i].y_pos, trex_sprites::CLOUD_SPRITE.data,
                         trex_sprites::CLOUD_SPRITE.width, trex_sprites::CLOUD_SPRITE.height, TFT_LIGHTGREY);
    }
  }

  void draw_obstacles(M5Canvas& canvas) const {
    for (int i = 0; i < obstacle_count_; ++i) {
      const BitmapSprite& sprite = obstacle_sprite(obstacles_[i]);
      canvas.drawXBitmap(static_cast<int>(obstacles_[i].x_pos), obstacles_[i].y_pos, sprite.data, sprite.width,
                         sprite.height, TFT_BLACK);
    }
  }

  void draw_trex(M5Canvas& canvas) const {
    const BitmapSprite& sprite = current_trex_sprite();
    canvas.drawXBitmap(static_cast<int>(trex_.x_pos), current_trex_y(), sprite.data, sprite.width, sprite.height,
                       TFT_BLACK);
  }

  uint32_t get_actual_distance() const {
    return static_cast<uint32_t>(ceilf(distance_ran_ * app_config::DISTANCE_COEFFICIENT));
  }

  void draw_score(M5Canvas& canvas) const {
    char score_text[32];
    snprintf(score_text, sizeof(score_text), "HI %05lu %05lu",
             static_cast<unsigned long>(high_score_),
             static_cast<unsigned long>(get_actual_distance()));
    canvas.setTextColor(TFT_BLACK);
    canvas.setCursor(app_config::SCREEN_W - canvas.textWidth(score_text) - 6, 4);
    canvas.print(score_text);
  }

  Phase phase_ = Phase::Waiting;
  Trex trex_{};
  Obstacle obstacles_[app_config::MAX_OBSTACLES];
  Cloud clouds_[app_config::MAX_CLOUDS];
  uint8_t obstacle_count_ = 0;
  uint8_t cloud_count_ = 0;
  ObstacleId obstacle_history_[app_config::MAX_OBSTACLE_DUPLICATION];
  uint8_t obstacle_history_len_ = 0;
  InputState prev_input_{};
  float current_speed_ = app_config::SPEED;
  float distance_ran_ = 0.0f;
  uint32_t running_time_ = 0;
  uint32_t high_score_ = 0;
  uint32_t last_update_ms_ = 0;
  float horizon_scroll_ = 0.0f;
};

M5Canvas g_canvas(&M5Cardputer.Display);
TRexRunnerDemo g_demo;
uint32_t g_last_frame_ms = 0;
InputState g_prev_input;

InputState read_input() {
  const auto& status = M5Cardputer.Keyboard.keysState();
  const bool jump_held = contains_hid_key(status, app_config::HID_A) ||
                         contains_hid_key(status, app_config::HID_SEMICOLON);

  InputState input;
  input.jump_held = jump_held;
  input.duck_held = contains_hid_key(status, app_config::HID_PERIOD);
  input.jump_pressed = input.jump_held && !g_prev_input.jump_held;
  const bool enter_held = contains_hid_key(status, app_config::HID_ENTER);
  input.restart_pressed = M5Cardputer.BtnA.wasClicked() || (enter_held && !g_prev_input.restart_pressed);
  g_prev_input = input;
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
  Serial.println("Cardputer ADV T-Rex runner port started");
}

void loop() {
  M5Cardputer.update();
  const InputState input = read_input();
  g_demo.update(input);

  const uint32_t now = millis();
  if (now - g_last_frame_ms >= app_config::FRAME_INTERVAL_MS) {
    g_demo.draw(g_canvas);
    M5Cardputer.Display.startWrite();
    g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
    M5Cardputer.Display.endWrite();
    g_last_frame_ms = now;
  }
}
