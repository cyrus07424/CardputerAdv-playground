#include <Arduino.h>
#include <M5Cardputer.h>

#include <cmath>

namespace app_config {
inline int16_t display_width() { return M5Cardputer.Display.width(); }
inline int16_t display_height() { return M5Cardputer.Display.height(); }
#define SCREEN_W display_width()
#define SCREEN_H display_height()
constexpr uint32_t FRAME_INTERVAL_MS = 33;

constexpr float WORLD_W = 640.0f;
constexpr float WORLD_H = 360.0f;

constexpr float PLAYER_RADIUS = 6.0f;
constexpr float PLAYER_MAX_SPEED = 82.0f;
constexpr float PLAYER_ACCEL = 520.0f;

constexpr uint8_t PLAYER_MAX_HP = 5;
constexpr uint16_t PLAYER_INVULN_MS = 720;
constexpr uint16_t PLAYER_MUZZLE_MS = 70;

constexpr uint8_t MAX_ENEMIES = 18;
constexpr uint8_t MAX_BULLETS = 56;

constexpr float AUTO_TARGET_RANGE = 150.0f;
constexpr uint16_t AUTO_FIRE_INTERVAL_MS = 150;
constexpr float PLAYER_BULLET_SPEED = 210.0f;
constexpr float PLAYER_BULLET_RADIUS = 2.5f;
constexpr float ENEMY_BULLET_SPEED = 92.0f;
constexpr float ENEMY_BULLET_RADIUS = 2.5f;

constexpr uint16_t WAVE_INTRO_MS = 850;
constexpr uint16_t WAVE_CLEAR_MS = 1300;
}  // namespace app_config

namespace keyboard_hid {
constexpr uint8_t RIGHT = 0x4F;
constexpr uint8_t LEFT = 0x50;
constexpr uint8_t DOWN = 0x51;
constexpr uint8_t UP = 0x52;
constexpr uint8_t ENTER = 0x28;
}  // namespace keyboard_hid

constexpr float kPi = 3.14159265f;

enum class GameMode : uint8_t {
  Title,
  Playing,
  GameOver,
};

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct InputState {
  bool left_held = false;
  bool right_held = false;
  bool up_held = false;
  bool down_held = false;
  bool start_pressed = false;
};

struct Bullet {
  Vec2 pos;
  Vec2 vel;
  uint16_t ttl_ms = 0;
  float radius = 2.0f;
  uint8_t power = 1;
  bool friendly = false;
  bool active = false;
};

struct Enemy {
  Vec2 pos;
  Vec2 vel;
  float radius = 7.0f;
  float phase = 0.0f;
  uint16_t fire_timer_ms = 0;
  uint16_t hit_flash_ms = 0;
  uint8_t hp = 1;
  uint8_t kind = 0;
  bool active = false;
};

struct PlayerState {
  Vec2 pos;
  Vec2 vel;
  uint16_t fire_timer_ms = 0;
  uint16_t invuln_ms = 0;
  uint16_t muzzle_flash_ms = 0;
  uint8_t hp = app_config::PLAYER_MAX_HP;
};

struct GameState {
  GameMode mode = GameMode::Title;
  PlayerState player;
  float camera_x = app_config::SCREEN_W * 0.5f;
  float camera_y = app_config::SCREEN_H * 0.5f;
  uint32_t score = 0;
  uint16_t wave = 0;
  uint16_t kills = 0;
  uint16_t wave_intro_ms = 0;
  uint16_t wave_clear_ms = 0;
  int8_t locked_enemy = -1;
  uint32_t frame = 0;
};

M5Canvas g_canvas(&M5Cardputer.Display);
GameState g_game;
Enemy g_enemies[app_config::MAX_ENEMIES];
Bullet g_bullets[app_config::MAX_BULLETS];
bool g_prev_enter_held = false;
bool g_start_press_latched = false;
uint32_t g_last_frame_ms = 0;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((static_cast<uint16_t>(r) & 0xF8u) << 8) |
         ((static_cast<uint16_t>(g) & 0xFCu) << 3) |
         (static_cast<uint16_t>(b) >> 3);
}

float clampf(float value, float minimum, float maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

uint16_t timer_subtract(uint16_t timer, uint16_t delta_ms) {
  return timer > delta_ms ? static_cast<uint16_t>(timer - delta_ms) : 0;
}

Vec2 make_vec(float x, float y) {
  Vec2 value;
  value.x = x;
  value.y = y;
  return value;
}

Vec2 add_vec(Vec2 a, Vec2 b) {
  return make_vec(a.x + b.x, a.y + b.y);
}

Vec2 sub_vec(Vec2 a, Vec2 b) {
  return make_vec(a.x - b.x, a.y - b.y);
}

Vec2 scale_vec(Vec2 value, float scale) {
  return make_vec(value.x * scale, value.y * scale);
}

float vec_length_sq(Vec2 value) {
  return value.x * value.x + value.y * value.y;
}

float vec_length(Vec2 value) {
  return sqrtf(vec_length_sq(value));
}

Vec2 normalize_or_zero(Vec2 value) {
  const float length = vec_length(value);
  if (length <= 0.0001f) {
    return make_vec(0.0f, 0.0f);
  }
  return scale_vec(value, 1.0f / length);
}

Vec2 rotate_vec(Vec2 value, float radians) {
  const float c = cosf(radians);
  const float s = sinf(radians);
  return make_vec(value.x * c - value.y * s, value.x * s + value.y * c);
}

float approach_float(float current, float target, float step) {
  if (current < target) {
    current += step;
    if (current > target) {
      current = target;
    }
  } else if (current > target) {
    current -= step;
    if (current < target) {
      current = target;
    }
  }
  return current;
}

float positive_mod(float value, float period) {
  while (value < 0.0f) {
    value += period;
  }
  while (value >= period) {
    value -= period;
  }
  return value;
}

uint32_t mix_bits(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

uint32_t hash2d(int32_t x, int32_t y, uint32_t seed) {
  return mix_bits(static_cast<uint32_t>(x) * 0x1f123bb5U ^
                  static_cast<uint32_t>(y) * 0x59d2f15dU ^ seed);
}

bool contains_hid_key(const Keyboard_Class::KeysState& status, uint8_t hid_key) {
  for (const auto raw_hid_key : status.hid_keys) {
    if ((raw_hid_key & ~SHIFT) == hid_key) {
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

InputState read_input() {
  const auto& status = M5Cardputer.Keyboard.keysState();
  const bool enter_held = status.enter || contains_hid_key(status, keyboard_hid::ENTER);

  InputState input;
  input.left_held = contains_hid_key(status, keyboard_hid::LEFT) || contains_char_key(status, ',') ||
                    contains_char_key(status, 'a') || contains_char_key(status, 'A');
  input.right_held = contains_hid_key(status, keyboard_hid::RIGHT) || contains_char_key(status, '/') ||
                     contains_char_key(status, 'd') || contains_char_key(status, 'D');
  input.up_held = contains_hid_key(status, keyboard_hid::UP) || contains_char_key(status, ';') ||
                  contains_char_key(status, 'w') || contains_char_key(status, 'W');
  input.down_held = contains_hid_key(status, keyboard_hid::DOWN) || contains_char_key(status, '.') ||
                    contains_char_key(status, 's') || contains_char_key(status, 'S');
  input.start_pressed = M5Cardputer.BtnA.wasClicked() || (enter_held && !g_prev_enter_held);
  g_prev_enter_held = enter_held;
  return input;
}

float camera_left() {
  return g_game.camera_x - app_config::SCREEN_W * 0.5f;
}

float camera_top() {
  return g_game.camera_y - app_config::SCREEN_H * 0.5f;
}

void world_to_screen(const Vec2& world, int16_t& sx, int16_t& sy) {
  sx = static_cast<int16_t>(lroundf(world.x - camera_left()));
  sy = static_cast<int16_t>(lroundf(world.y - camera_top()));
}

bool on_screen(int16_t sx, int16_t sy, int16_t margin) {
  return sx >= -margin && sx < app_config::SCREEN_W + margin && sy >= -margin &&
         sy < app_config::SCREEN_H + margin;
}

void clear_bullets() {
  for (Bullet& bullet : g_bullets) {
    bullet.active = false;
  }
}

void clear_enemies() {
  for (Enemy& enemy : g_enemies) {
    enemy.active = false;
  }
}

void refresh_camera() {
  g_game.camera_x =
      clampf(g_game.player.pos.x, app_config::SCREEN_W * 0.5f, app_config::WORLD_W - app_config::SCREEN_W * 0.5f);
  g_game.camera_y =
      clampf(g_game.player.pos.y, app_config::SCREEN_H * 0.5f, app_config::WORLD_H - app_config::SCREEN_H * 0.5f);
}

bool spawn_bullet(Vec2 pos, Vec2 velocity, bool friendly, uint16_t ttl_ms, uint8_t power, float radius) {
  for (Bullet& bullet : g_bullets) {
    if (!bullet.active) {
      bullet.active = true;
      bullet.friendly = friendly;
      bullet.pos = pos;
      bullet.vel = velocity;
      bullet.ttl_ms = ttl_ms;
      bullet.power = power;
      bullet.radius = radius;
      return true;
    }
  }
  return false;
}

float enemy_speed(uint8_t kind) {
  switch (kind) {
    case 0:
      return 30.0f;
    case 1:
      return 24.0f;
    case 2:
      return 18.0f;
  }
  return 20.0f;
}

uint16_t enemy_fire_delay(uint8_t kind) {
  switch (kind) {
    case 0:
      return static_cast<uint16_t>(1100 + random(0, 300));
    case 1:
      return static_cast<uint16_t>(850 + random(0, 260));
    case 2:
      return static_cast<uint16_t>(620 + random(0, 160));
  }
  return 900;
}

uint8_t enemy_base_hp(uint8_t kind) {
  switch (kind) {
    case 0:
      return 2;
    case 1:
      return 3;
    case 2:
      return 12;
  }
  return 2;
}

float enemy_radius(uint8_t kind) {
  switch (kind) {
    case 0:
      return 7.0f;
    case 1:
      return 8.5f;
    case 2:
      return 13.0f;
  }
  return 7.0f;
}

uint32_t enemy_score(uint8_t kind) {
  switch (kind) {
    case 0:
      return 60;
    case 1:
      return 95;
    case 2:
      return 380;
  }
  return 50;
}

bool spawn_enemy(Vec2 pos, uint8_t kind) {
  for (Enemy& enemy : g_enemies) {
    if (!enemy.active) {
      enemy.active = true;
      enemy.kind = kind;
      enemy.hp = enemy_base_hp(kind);
      enemy.radius = enemy_radius(kind);
      enemy.phase = static_cast<float>(random(0, 628)) / 100.0f;
      enemy.fire_timer_ms = static_cast<uint16_t>(220 + random(0, 280));
      enemy.hit_flash_ms = 0;
      enemy.pos.x = clampf(pos.x, enemy.radius + 6.0f, app_config::WORLD_W - enemy.radius - 6.0f);
      enemy.pos.y = clampf(pos.y, enemy.radius + 6.0f, app_config::WORLD_H - enemy.radius - 6.0f);
      enemy.vel = make_vec(0.0f, 0.0f);
      return true;
    }
  }
  return false;
}

uint8_t active_enemy_count() {
  uint8_t count = 0;
  for (const Enemy& enemy : g_enemies) {
    if (enemy.active) {
      ++count;
    }
  }
  return count;
}

void begin_wave() {
  ++g_game.wave;
  g_game.wave_intro_ms = app_config::WAVE_INTRO_MS;
  g_game.wave_clear_ms = 0;

  const bool boss_wave = (g_game.wave % 4) == 0;
  int enemy_count = 3 + static_cast<int>(g_game.wave);
  if (enemy_count > 12) {
    enemy_count = 12;
  }

  float base_angle = static_cast<float>(random(0, 628)) / 100.0f;
  if (boss_wave) {
    spawn_enemy(add_vec(g_game.player.pos, make_vec(cosf(base_angle) * 122.0f, sinf(base_angle) * 122.0f)), 2);
    enemy_count = 4 + static_cast<int>(g_game.wave / 2);
    if (enemy_count > 8) {
      enemy_count = 8;
    }
  }

  for (int i = 0; i < enemy_count; ++i) {
    const float angle = base_angle + (2.0f * kPi * static_cast<float>(i) / static_cast<float>(enemy_count)) +
                        static_cast<float>(random(-45, 46)) * 0.01f;
    const float distance = static_cast<float>(random(96, 148));
    const Vec2 offset = make_vec(cosf(angle) * distance, sinf(angle) * distance);
    const uint8_t kind = boss_wave ? static_cast<uint8_t>(i % 2) : static_cast<uint8_t>((i + g_game.wave) % 2);
    spawn_enemy(add_vec(g_game.player.pos, offset), kind);
  }
}

void reset_player() {
  g_game.player.pos = make_vec(app_config::WORLD_W * 0.5f, app_config::WORLD_H * 0.5f);
  g_game.player.vel = make_vec(0.0f, 0.0f);
  g_game.player.hp = app_config::PLAYER_MAX_HP;
  g_game.player.fire_timer_ms = 0;
  g_game.player.invuln_ms = 0;
  g_game.player.muzzle_flash_ms = 0;
}

void start_game() {
  g_game = GameState();
  g_game.mode = GameMode::Playing;
  g_game.score = 0;
  g_game.wave = 0;
  g_game.kills = 0;
  g_game.frame = 0;
  g_game.locked_enemy = -1;
  reset_player();
  clear_enemies();
  clear_bullets();
  refresh_camera();
  begin_wave();
}

void apply_player_damage(uint8_t damage, Vec2 knockback_dir) {
  if (g_game.player.invuln_ms > 0 || g_game.mode != GameMode::Playing) {
    return;
  }

  if (damage >= g_game.player.hp) {
    g_game.player.hp = 0;
  } else {
    g_game.player.hp = static_cast<uint8_t>(g_game.player.hp - damage);
  }
  g_game.player.invuln_ms = app_config::PLAYER_INVULN_MS;
  g_game.player.vel = add_vec(g_game.player.vel, scale_vec(knockback_dir, 105.0f));
  if (g_game.player.hp == 0) {
    g_game.mode = GameMode::GameOver;
    g_game.locked_enemy = -1;
  }
}

void update_player(const InputState& input, float delta_s) {
  Vec2 move = make_vec(0.0f, 0.0f);
  if (input.left_held && !input.right_held) {
    move.x -= 1.0f;
  } else if (input.right_held && !input.left_held) {
    move.x += 1.0f;
  }
  if (input.up_held && !input.down_held) {
    move.y -= 1.0f;
  } else if (input.down_held && !input.up_held) {
    move.y += 1.0f;
  }

  const Vec2 desired = scale_vec(normalize_or_zero(move), app_config::PLAYER_MAX_SPEED);
  const float accel_step = app_config::PLAYER_ACCEL * delta_s;
  g_game.player.vel.x = approach_float(g_game.player.vel.x, desired.x, accel_step);
  g_game.player.vel.y = approach_float(g_game.player.vel.y, desired.y, accel_step);

  g_game.player.pos = add_vec(g_game.player.pos, scale_vec(g_game.player.vel, delta_s));

  const float min_x = app_config::PLAYER_RADIUS + 4.0f;
  const float min_y = app_config::PLAYER_RADIUS + 4.0f;
  const float max_x = app_config::WORLD_W - app_config::PLAYER_RADIUS - 4.0f;
  const float max_y = app_config::WORLD_H - app_config::PLAYER_RADIUS - 4.0f;
  if (g_game.player.pos.x < min_x) {
    g_game.player.pos.x = min_x;
    g_game.player.vel.x = 0.0f;
  } else if (g_game.player.pos.x > max_x) {
    g_game.player.pos.x = max_x;
    g_game.player.vel.x = 0.0f;
  }
  if (g_game.player.pos.y < min_y) {
    g_game.player.pos.y = min_y;
    g_game.player.vel.y = 0.0f;
  } else if (g_game.player.pos.y > max_y) {
    g_game.player.pos.y = max_y;
    g_game.player.vel.y = 0.0f;
  }
}

int8_t find_lock_target() {
  float best_dist_sq = app_config::AUTO_TARGET_RANGE * app_config::AUTO_TARGET_RANGE;
  int8_t best_index = -1;
  for (int8_t i = 0; i < app_config::MAX_ENEMIES; ++i) {
    const Enemy& enemy = g_enemies[i];
    if (!enemy.active) {
      continue;
    }
    const Vec2 delta = sub_vec(enemy.pos, g_game.player.pos);
    const float dist_sq = vec_length_sq(delta);
    if (dist_sq > best_dist_sq) {
      continue;
    }
    int16_t sx = 0;
    int16_t sy = 0;
    world_to_screen(enemy.pos, sx, sy);
    if (!on_screen(sx, sy, 12)) {
      continue;
    }
    best_dist_sq = dist_sq;
    best_index = i;
  }
  return best_index;
}

void update_auto_fire(uint16_t delta_ms) {
  g_game.locked_enemy = find_lock_target();
  g_game.player.fire_timer_ms = timer_subtract(g_game.player.fire_timer_ms, delta_ms);
  if (g_game.player.muzzle_flash_ms > 0) {
    g_game.player.muzzle_flash_ms = timer_subtract(g_game.player.muzzle_flash_ms, delta_ms);
  }

  if (g_game.locked_enemy < 0 || g_game.player.fire_timer_ms > 0) {
    return;
  }

  Enemy& target = g_enemies[g_game.locked_enemy];
  Vec2 aim = sub_vec(target.pos, g_game.player.pos);
  const float distance = vec_length(aim);
  if (distance <= 0.001f) {
    return;
  }
  aim = sub_vec(add_vec(target.pos, scale_vec(target.vel, distance / app_config::PLAYER_BULLET_SPEED)),
                g_game.player.pos);
  const Vec2 dir = normalize_or_zero(aim);
  if (spawn_bullet(add_vec(g_game.player.pos, scale_vec(dir, app_config::PLAYER_RADIUS + 4.0f)),
                   scale_vec(dir, app_config::PLAYER_BULLET_SPEED), true, 900, 1,
                   app_config::PLAYER_BULLET_RADIUS)) {
    g_game.player.fire_timer_ms = app_config::AUTO_FIRE_INTERVAL_MS;
    g_game.player.muzzle_flash_ms = app_config::PLAYER_MUZZLE_MS;
  }
}

int8_t find_enemy_near_bullet(const Bullet& bullet, float max_dist_sq) {
  int8_t best = -1;
  for (int8_t i = 0; i < app_config::MAX_ENEMIES; ++i) {
    const Enemy& enemy = g_enemies[i];
    if (!enemy.active) {
      continue;
    }
    const float dist_sq = vec_length_sq(sub_vec(enemy.pos, bullet.pos));
    if (dist_sq < max_dist_sq) {
      max_dist_sq = dist_sq;
      best = i;
    }
  }
  return best;
}

void destroy_enemy(Enemy& enemy) {
  enemy.active = false;
  g_game.score += enemy_score(enemy.kind) + static_cast<uint32_t>(g_game.wave) * 12U;
  ++g_game.kills;
}

void update_bullets(uint16_t delta_ms, float delta_s) {
  for (Bullet& bullet : g_bullets) {
    if (!bullet.active) {
      continue;
    }

    bullet.ttl_ms = timer_subtract(bullet.ttl_ms, delta_ms);
    if (bullet.ttl_ms == 0) {
      bullet.active = false;
      continue;
    }

    if (bullet.friendly) {
      const int8_t target_index = find_enemy_near_bullet(bullet, 38.0f * 38.0f);
      if (target_index >= 0) {
        const Vec2 desired = normalize_or_zero(sub_vec(g_enemies[target_index].pos, bullet.pos));
        const Vec2 current = normalize_or_zero(bullet.vel);
        bullet.vel =
            scale_vec(normalize_or_zero(add_vec(scale_vec(current, 0.82f), scale_vec(desired, 0.18f))),
                      app_config::PLAYER_BULLET_SPEED);
      }
    }

    bullet.pos = add_vec(bullet.pos, scale_vec(bullet.vel, delta_s));

    if (bullet.pos.x < -16.0f || bullet.pos.x > app_config::WORLD_W + 16.0f || bullet.pos.y < -16.0f ||
        bullet.pos.y > app_config::WORLD_H + 16.0f) {
      bullet.active = false;
      continue;
    }

    if (bullet.friendly) {
      for (Enemy& enemy : g_enemies) {
        if (!enemy.active) {
          continue;
        }
        const float hit_radius = bullet.radius + enemy.radius;
        if (vec_length_sq(sub_vec(enemy.pos, bullet.pos)) <= hit_radius * hit_radius) {
          bullet.active = false;
          enemy.hit_flash_ms = 90;
          if (bullet.power >= enemy.hp) {
            destroy_enemy(enemy);
          } else {
            enemy.hp = static_cast<uint8_t>(enemy.hp - bullet.power);
          }
          break;
        }
      }
    } else {
      const float hit_radius = bullet.radius + app_config::PLAYER_RADIUS;
      if (vec_length_sq(sub_vec(g_game.player.pos, bullet.pos)) <= hit_radius * hit_radius) {
        bullet.active = false;
        apply_player_damage(1, normalize_or_zero(sub_vec(g_game.player.pos, bullet.pos)));
      }
    }
  }
}

void update_enemy_fire(Enemy& enemy, Vec2 to_player) {
  const Vec2 dir = normalize_or_zero(to_player);
  if (enemy.kind == 2) {
    for (int i = -1; i <= 1; ++i) {
      const Vec2 shot_dir = rotate_vec(dir, static_cast<float>(i) * 0.20f);
      spawn_bullet(add_vec(enemy.pos, scale_vec(shot_dir, enemy.radius + 2.0f)),
                   scale_vec(shot_dir, app_config::ENEMY_BULLET_SPEED), false, 1600, 1,
                   app_config::ENEMY_BULLET_RADIUS);
    }
  } else {
    spawn_bullet(add_vec(enemy.pos, scale_vec(dir, enemy.radius + 2.0f)),
                 scale_vec(dir, app_config::ENEMY_BULLET_SPEED), false, 1500, 1,
                 app_config::ENEMY_BULLET_RADIUS);
  }
  enemy.fire_timer_ms = enemy_fire_delay(enemy.kind);
}

void update_enemies(uint16_t delta_ms, float delta_s, uint32_t now_ms) {
  for (Enemy& enemy : g_enemies) {
    if (!enemy.active) {
      continue;
    }

    enemy.fire_timer_ms = timer_subtract(enemy.fire_timer_ms, delta_ms);
    enemy.hit_flash_ms = timer_subtract(enemy.hit_flash_ms, delta_ms);

    const Vec2 to_player = sub_vec(g_game.player.pos, enemy.pos);
    const float distance = vec_length(to_player);
    Vec2 dir = normalize_or_zero(to_player);
    if (distance <= 0.0001f) {
      dir = make_vec(0.0f, 1.0f);
    }

    const Vec2 tangent = make_vec(-dir.y, dir.x);
    const float sway = sinf(static_cast<float>(now_ms) * 0.0043f + enemy.phase);
    float strafe = 0.0f;
    if (enemy.kind == 0) {
      strafe = 0.35f * sway;
    } else if (enemy.kind == 1) {
      strafe = 0.55f * sway;
    } else {
      strafe = 0.20f * sway;
    }

    Vec2 move_dir = add_vec(dir, scale_vec(tangent, strafe));
    if (distance < 34.0f) {
      move_dir = add_vec(scale_vec(dir, -0.35f), scale_vec(tangent, strafe * 1.2f));
    }
    move_dir = normalize_or_zero(move_dir);
    enemy.vel = scale_vec(move_dir, enemy_speed(enemy.kind));
    enemy.pos = add_vec(enemy.pos, scale_vec(enemy.vel, delta_s));

    enemy.pos.x = clampf(enemy.pos.x, enemy.radius + 4.0f, app_config::WORLD_W - enemy.radius - 4.0f);
    enemy.pos.y = clampf(enemy.pos.y, enemy.radius + 4.0f, app_config::WORLD_H - enemy.radius - 4.0f);

    if (distance < 122.0f && enemy.fire_timer_ms == 0) {
      update_enemy_fire(enemy, to_player);
    }

    const float collide_radius = enemy.radius + app_config::PLAYER_RADIUS + 1.5f;
    if (vec_length_sq(sub_vec(enemy.pos, g_game.player.pos)) <= collide_radius * collide_radius) {
      apply_player_damage(enemy.kind == 2 ? 2 : 1, normalize_or_zero(sub_vec(g_game.player.pos, enemy.pos)));
      enemy.pos = add_vec(enemy.pos, scale_vec(normalize_or_zero(sub_vec(enemy.pos, g_game.player.pos)), 6.0f));
    }
  }
}

void update_wave_state(uint16_t delta_ms) {
  g_game.wave_intro_ms = timer_subtract(g_game.wave_intro_ms, delta_ms);

  if (active_enemy_count() == 0 && g_game.mode == GameMode::Playing) {
    if (g_game.wave_clear_ms == 0) {
      g_game.wave_clear_ms = app_config::WAVE_CLEAR_MS;
      g_game.score += 80U + static_cast<uint32_t>(g_game.wave) * 18U;
    } else {
      g_game.wave_clear_ms = timer_subtract(g_game.wave_clear_ms, delta_ms);
      if (g_game.wave_clear_ms == 0) {
        begin_wave();
      }
    }
  } else if (active_enemy_count() > 0) {
    g_game.wave_clear_ms = 0;
  }
}

void update_game(const InputState& input, uint16_t delta_ms) {
  ++g_game.frame;
  if (input.start_pressed && (g_game.mode == GameMode::Title || g_game.mode == GameMode::GameOver)) {
    start_game();
    return;
  }

  if (g_game.mode != GameMode::Playing) {
    return;
  }

  const float delta_s = static_cast<float>(delta_ms) * 0.001f;
  const uint32_t now_ms = millis();

  g_game.player.invuln_ms = timer_subtract(g_game.player.invuln_ms, delta_ms);
  update_player(input, delta_s);
  refresh_camera();
  update_auto_fire(delta_ms);
  update_enemies(delta_ms, delta_s, now_ms);
  update_bullets(delta_ms, delta_s);
  update_wave_state(delta_ms);
}

void draw_gradient_background(M5Canvas& canvas) {
  for (int y = 0; y < app_config::SCREEN_H; ++y) {
    const uint8_t r = static_cast<uint8_t>(10 + (y * 18) / app_config::SCREEN_H);
    const uint8_t g = static_cast<uint8_t>(14 + (y * 46) / app_config::SCREEN_H);
    const uint8_t b = static_cast<uint8_t>(26 + (y * 44) / app_config::SCREEN_H);
    canvas.drawFastHLine(0, y, app_config::SCREEN_W, rgb565(r, g, b));
  }
}

void draw_far_parallax(M5Canvas& canvas) {
  const float factor = 0.16f;
  const float cell = 18.0f;
  const float offset_x = g_game.camera_x * factor;
  const float offset_y = g_game.camera_y * factor;
  const int start_cell_x = static_cast<int>(floorf(offset_x / cell)) - 2;
  const int start_cell_y = static_cast<int>(floorf(offset_y / cell)) - 2;
  const int cells_x = app_config::SCREEN_W / static_cast<int>(cell) + 5;
  const int cells_y = app_config::SCREEN_H / static_cast<int>(cell) + 5;
  const float shift_x = positive_mod(offset_x, cell);
  const float shift_y = positive_mod(offset_y, cell);

  for (int gy = 0; gy < cells_y; ++gy) {
    for (int gx = 0; gx < cells_x; ++gx) {
      const int cx = start_cell_x + gx;
      const int cy = start_cell_y + gy;
      const uint32_t hash = hash2d(cx, cy, 0x3au);
      if ((hash & 3u) != 0u) {
        continue;
      }
      const int16_t px = static_cast<int16_t>(lroundf(gx * cell - shift_x + 3.0f + static_cast<float>(hash % 11u)));
      const int16_t py =
          static_cast<int16_t>(lroundf(gy * cell - shift_y + 2.0f + static_cast<float>((hash >> 8) % 10u)));
      if ((hash & 31u) == 7u) {
        canvas.fillCircle(px, py, 2, rgb565(118, 190, 255));
      } else {
        canvas.drawPixel(px, py, rgb565(96, 165, 255));
      }
    }
  }
}

void draw_mid_parallax(M5Canvas& canvas) {
  const float factor = 0.38f;
  const float cell = 42.0f;
  const float offset_x = g_game.camera_x * factor;
  const float offset_y = g_game.camera_y * factor;
  const int start_cell_x = static_cast<int>(floorf(offset_x / cell)) - 2;
  const int start_cell_y = static_cast<int>(floorf(offset_y / cell)) - 2;
  const int cells_x = app_config::SCREEN_W / static_cast<int>(cell) + 5;
  const int cells_y = app_config::SCREEN_H / static_cast<int>(cell) + 5;
  const float shift_x = positive_mod(offset_x, cell);
  const float shift_y = positive_mod(offset_y, cell);

  for (int gy = 0; gy < cells_y; ++gy) {
    for (int gx = 0; gx < cells_x; ++gx) {
      const int cx = start_cell_x + gx;
      const int cy = start_cell_y + gy;
      const uint32_t hash = hash2d(cx, cy, 0x81u);
      if ((hash & 7u) > 2u) {
        continue;
      }
      const int16_t px = static_cast<int16_t>(lroundf(gx * cell - shift_x));
      const int16_t py = static_cast<int16_t>(lroundf(gy * cell - shift_y));
      const int16_t width = static_cast<int16_t>(12 + static_cast<int>((hash >> 4) % 18u));
      const int16_t height = static_cast<int16_t>(7 + static_cast<int>((hash >> 10) % 13u));
      canvas.drawRect(px, py, width, height, rgb565(42, 88, 118));
      canvas.drawFastHLine(px + 2, py + height / 2, width - 4, rgb565(82, 172, 214));
      canvas.drawFastVLine(px + width / 2, py + 2, height - 4, rgb565(62, 128, 164));
    }
  }
}

void draw_floor(M5Canvas& canvas) {
  const float left = camera_left();
  const float top = camera_top();
  const int cell = 24;
  const int start_x = static_cast<int>(floorf(left / static_cast<float>(cell))) - 1;
  const int start_y = static_cast<int>(floorf(top / static_cast<float>(cell))) - 1;
  const int end_x = start_x + app_config::SCREEN_W / cell + 4;
  const int end_y = start_y + app_config::SCREEN_H / cell + 4;

  for (int gy = start_y; gy <= end_y; ++gy) {
    const int16_t sy = static_cast<int16_t>(lroundf(gy * cell - top));
    const uint16_t line_color = ((gy & 3) == 0) ? rgb565(24, 96, 94) : rgb565(18, 56, 60);
    canvas.drawFastHLine(0, sy, app_config::SCREEN_W, line_color);
  }
  for (int gx = start_x; gx <= end_x; ++gx) {
    const int16_t sx = static_cast<int16_t>(lroundf(gx * cell - left));
    const uint16_t line_color = ((gx & 3) == 0) ? rgb565(24, 96, 94) : rgb565(18, 56, 60);
    canvas.drawFastVLine(sx, 0, app_config::SCREEN_H, line_color);
  }

  for (int gy = start_y; gy <= end_y; ++gy) {
    for (int gx = start_x; gx <= end_x; ++gx) {
      const uint32_t hash = hash2d(gx, gy, 0x55u);
      if ((hash & 15u) != 3u) {
        continue;
      }
      const int16_t sx = static_cast<int16_t>(lroundf(gx * cell - left + 4.0f));
      const int16_t sy = static_cast<int16_t>(lroundf(gy * cell - top + 4.0f));
      canvas.drawRoundRect(sx, sy, 16, 16, 3, rgb565(36, 126, 104));
      canvas.drawFastHLine(sx + 3, sy + 8, 10, rgb565(90, 230, 164));
    }
  }

  const int16_t border_x = static_cast<int16_t>(lroundf(-left));
  const int16_t border_y = static_cast<int16_t>(lroundf(-top));
  canvas.drawRect(border_x, border_y, static_cast<int16_t>(app_config::WORLD_W),
                  static_cast<int16_t>(app_config::WORLD_H), rgb565(90, 210, 168));
}

void draw_enemy(M5Canvas& canvas, const Enemy& enemy) {
  int16_t sx = 0;
  int16_t sy = 0;
  world_to_screen(enemy.pos, sx, sy);
  if (!on_screen(sx, sy, 16)) {
    return;
  }

  const bool flash = enemy.hit_flash_ms > 0 && ((enemy.hit_flash_ms / 30) % 2) == 0;
  const uint16_t body = flash ? TFT_WHITE
                              : (enemy.kind == 2 ? rgb565(255, 88, 106)
                                                 : (enemy.kind == 1 ? rgb565(255, 176, 92) : rgb565(255, 120, 74)));
  const int16_t r = static_cast<int16_t>(lroundf(enemy.radius));
  if (enemy.kind == 2) {
    canvas.drawCircle(sx, sy, r + 2, rgb565(255, 214, 148));
    canvas.fillCircle(sx, sy, r, body);
    canvas.drawFastHLine(sx - r + 3, sy, (r - 3) * 2, rgb565(70, 10, 20));
    canvas.drawFastVLine(sx, sy - r + 3, (r - 3) * 2, rgb565(70, 10, 20));
  } else if (enemy.kind == 1) {
    canvas.fillRoundRect(sx - r, sy - r + 1, r * 2, r * 2 - 2, 3, body);
    canvas.fillCircle(sx - r + 3, sy - 1, 2, rgb565(80, 28, 20));
    canvas.fillCircle(sx + r - 3, sy - 1, 2, rgb565(80, 28, 20));
  } else {
    canvas.fillTriangle(sx, sy - r - 1, sx + r + 1, sy, sx, sy + r + 1, body);
    canvas.fillTriangle(sx, sy - r - 1, sx - r - 1, sy, sx, sy + r + 1, body);
  }
}

void draw_player(M5Canvas& canvas) {
  if (g_game.player.invuln_ms > 0 && ((g_game.player.invuln_ms / 70) % 2) == 0) {
    return;
  }

  Vec2 facing = make_vec(0.0f, -1.0f);
  if (g_game.locked_enemy >= 0 && g_enemies[g_game.locked_enemy].active) {
    facing = normalize_or_zero(sub_vec(g_enemies[g_game.locked_enemy].pos, g_game.player.pos));
  } else if (vec_length_sq(g_game.player.vel) > 4.0f) {
    facing = normalize_or_zero(g_game.player.vel);
  }
  const Vec2 right = make_vec(-facing.y, facing.x);

  int16_t sx = 0;
  int16_t sy = 0;
  world_to_screen(g_game.player.pos, sx, sy);

  const Vec2 nose = add_vec(g_game.player.pos, scale_vec(facing, 9.0f));
  const Vec2 left_wing = add_vec(add_vec(g_game.player.pos, scale_vec(facing, -5.0f)), scale_vec(right, 6.0f));
  const Vec2 right_wing =
      add_vec(add_vec(g_game.player.pos, scale_vec(facing, -5.0f)), scale_vec(right, -6.0f));
  const Vec2 tail = add_vec(g_game.player.pos, scale_vec(facing, -8.0f));

  int16_t nx = 0;
  int16_t ny = 0;
  int16_t lx = 0;
  int16_t ly = 0;
  int16_t rx = 0;
  int16_t ry = 0;
  int16_t tx = 0;
  int16_t ty = 0;
  world_to_screen(nose, nx, ny);
  world_to_screen(left_wing, lx, ly);
  world_to_screen(right_wing, rx, ry);
  world_to_screen(tail, tx, ty);

  canvas.fillTriangle(nx, ny, lx, ly, rx, ry, rgb565(116, 255, 214));
  canvas.fillTriangle(tx, ty, lx, ly, rx, ry, rgb565(30, 120, 108));
  canvas.drawCircle(sx, sy, 2, rgb565(220, 255, 245));

  if (g_game.player.muzzle_flash_ms > 0) {
    const Vec2 muzzle = add_vec(g_game.player.pos, scale_vec(facing, 12.0f));
    int16_t mx = 0;
    int16_t my = 0;
    world_to_screen(muzzle, mx, my);
    canvas.fillCircle(mx, my, 2, rgb565(255, 232, 150));
  }
}

void draw_bullets(M5Canvas& canvas) {
  for (const Bullet& bullet : g_bullets) {
    if (!bullet.active) {
      continue;
    }
    int16_t sx = 0;
    int16_t sy = 0;
    world_to_screen(bullet.pos, sx, sy);
    if (!on_screen(sx, sy, 4)) {
      continue;
    }
    const uint16_t color = bullet.friendly ? rgb565(164, 255, 210) : rgb565(255, 120, 120);
    canvas.fillCircle(sx, sy, static_cast<int16_t>(lroundf(bullet.radius)), color);
  }
}

void draw_lock_on(M5Canvas& canvas) {
  if (g_game.locked_enemy < 0 || !g_enemies[g_game.locked_enemy].active) {
    return;
  }
  int16_t sx = 0;
  int16_t sy = 0;
  world_to_screen(g_enemies[g_game.locked_enemy].pos, sx, sy);
  if (!on_screen(sx, sy, 16)) {
    return;
  }
  const int16_t radius = static_cast<int16_t>(lroundf(g_enemies[g_game.locked_enemy].radius)) + 6;
  const int16_t pulse = static_cast<int16_t>((g_game.frame / 2) % 3);
  const uint16_t color = rgb565(210, 255, 150);
  canvas.drawFastHLine(sx - radius, sy - radius, 5 + pulse, color);
  canvas.drawFastVLine(sx - radius, sy - radius, 5 + pulse, color);
  canvas.drawFastHLine(sx + radius - 4 - pulse, sy - radius, 5 + pulse, color);
  canvas.drawFastVLine(sx + radius, sy - radius, 5 + pulse, color);
  canvas.drawFastHLine(sx - radius, sy + radius, 5 + pulse, color);
  canvas.drawFastVLine(sx - radius, sy + radius - 4 - pulse, 5 + pulse, color);
  canvas.drawFastHLine(sx + radius - 4 - pulse, sy + radius, 5 + pulse, color);
  canvas.drawFastVLine(sx + radius, sy + radius - 4 - pulse, 5 + pulse, color);
}

void draw_hud(M5Canvas& canvas) {
  canvas.fillRect(0, 0, app_config::SCREEN_W, 12, rgb565(4, 16, 20));
  canvas.setTextColor(rgb565(230, 255, 244), rgb565(4, 16, 20));
  canvas.setCursor(4, 2);
  canvas.printf("HP:%u  W:%u  EN:%u", g_game.player.hp, g_game.wave, active_enemy_count());
  canvas.setCursor(134, 2);
  canvas.printf("SC:%lu", static_cast<unsigned long>(g_game.score));

  canvas.fillRect(0, app_config::SCREEN_H - 10, app_config::SCREEN_W, 10, rgb565(4, 16, 20));
  canvas.setCursor(4, app_config::SCREEN_H - 8);
  if (g_game.locked_enemy >= 0 && g_enemies[g_game.locked_enemy].active) {
    canvas.printf("LOCK HP:%u", g_enemies[g_game.locked_enemy].hp);
  } else {
    canvas.print("LOCK SEARCH");
  }
  canvas.setCursor(150, app_config::SCREEN_H - 8);
  canvas.printf("K:%u", g_game.kills);
}

void draw_wave_banner(M5Canvas& canvas) {
  if (g_game.wave_intro_ms > 0) {
    canvas.fillRoundRect(73, 18, 94, 18, 4, rgb565(8, 28, 34));
    canvas.drawRoundRect(73, 18, 94, 18, 4, rgb565(128, 255, 220));
    canvas.setTextColor(rgb565(222, 255, 238), rgb565(8, 28, 34));
    canvas.setCursor(92, 24);
    canvas.printf("WAVE %u", g_game.wave);
  } else if (g_game.wave_clear_ms > 0) {
    canvas.fillRoundRect(60, 18, 120, 18, 4, rgb565(8, 28, 34));
    canvas.drawRoundRect(60, 18, 120, 18, 4, rgb565(255, 226, 120));
    canvas.setTextColor(rgb565(255, 244, 188), rgb565(8, 28, 34));
    canvas.setCursor(89, 24);
    canvas.print("AREA CLEAR");
  }
}

void draw_title_overlay(M5Canvas& canvas) {
  canvas.fillRoundRect(24, 18, 192, 96, 8, rgb565(6, 18, 22));
  canvas.drawRoundRect(24, 18, 192, 96, 8, rgb565(118, 236, 214));
  canvas.setTextColor(rgb565(218, 255, 244), rgb565(6, 18, 22));
  canvas.setCursor(54, 30);
  canvas.print("TOP-DOWN SHOOTER");
  canvas.setCursor(45, 46);
  canvas.print("Cursor / WASD : move");
  canvas.setCursor(45, 58);
  canvas.print("Auto lock + auto fire");
  canvas.setCursor(45, 70);
  canvas.print("Parallax cyber arena");
  canvas.setCursor(45, 82);
  canvas.print("Enter / BtnA : start");
  canvas.setCursor(40, 98);
  canvas.print("Inspired by a Unity shooter");
}

void draw_game_over_overlay(M5Canvas& canvas) {
  canvas.fillRoundRect(34, 30, 172, 72, 8, rgb565(22, 6, 10));
  canvas.drawRoundRect(34, 30, 172, 72, 8, rgb565(255, 120, 132));
  canvas.setTextColor(rgb565(255, 226, 232), rgb565(22, 6, 10));
  canvas.setCursor(86, 43);
  canvas.print("GAME OVER");
  canvas.setCursor(64, 59);
  canvas.printf("Wave %u  Score %lu", g_game.wave, static_cast<unsigned long>(g_game.score));
  canvas.setCursor(58, 75);
  canvas.printf("Kills %u   Enter/BtnA", g_game.kills);
}

void draw_scene(M5Canvas& canvas) {
  draw_gradient_background(canvas);
  draw_far_parallax(canvas);
  draw_mid_parallax(canvas);
  draw_floor(canvas);
  draw_bullets(canvas);
  for (const Enemy& enemy : g_enemies) {
    draw_enemy(canvas, enemy);
  }
  draw_lock_on(canvas);
  draw_player(canvas);
  draw_hud(canvas);
  draw_wave_banner(canvas);
  if (g_game.mode == GameMode::Title) {
    draw_title_overlay(canvas);
  } else if (g_game.mode == GameMode::GameOver) {
    draw_game_over_overlay(canvas);
  }
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

  reset_player();
  refresh_camera();
  clear_enemies();
  clear_bullets();

  g_last_frame_ms = millis();
  Serial.println("Cardputer ADV Top-Down Shooter demo started");
}

void loop() {
  M5Cardputer.update();
  const InputState input = read_input();
  g_start_press_latched = g_start_press_latched || input.start_pressed;

  const uint32_t now = millis();
  if (now - g_last_frame_ms < app_config::FRAME_INTERVAL_MS) {
    return;
  }

  const uint16_t delta_ms = static_cast<uint16_t>(clampf(static_cast<float>(now - g_last_frame_ms), 1.0f, 50.0f));
  InputState frame_input = input;
  frame_input.start_pressed = g_start_press_latched;
  g_start_press_latched = false;

  update_game(frame_input, delta_ms);
  draw_scene(g_canvas);

  M5Cardputer.Display.startWrite();
  g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
  M5Cardputer.Display.endWrite();

  g_last_frame_ms = now;
}
