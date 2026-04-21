#include <Arduino.h>
#include <M5Cardputer.h>

#include <cmath>

namespace app_config {
constexpr int16_t SCREEN_W = 240;
constexpr int16_t SCREEN_H = 135;
constexpr int16_t PLAYFIELD_X = 0;
constexpr int16_t PLAYFIELD_Y = 0;
constexpr int16_t PLAYFIELD_W = 160;
constexpr int16_t PLAYFIELD_H = 135;
constexpr int16_t PLAYFIELD_RIGHT = PLAYFIELD_X + PLAYFIELD_W;
constexpr int16_t PLAYFIELD_BOTTOM = PLAYFIELD_Y + PLAYFIELD_H;
constexpr int16_t SIDEBAR_X = 164;
constexpr int16_t SIDEBAR_W = SCREEN_W - SIDEBAR_X;

constexpr uint32_t UPDATE_MIN_MS = 8;
constexpr uint32_t DRAW_INTERVAL_MS = 33;

constexpr uint8_t MAX_PLAYER_SHOTS = 40;
constexpr uint8_t MAX_ENEMY_BULLETS = 160;
constexpr uint8_t MAX_ENEMIES = 10;

constexpr float PLAYER_RADIUS = 2.0f;
constexpr float PLAYER_GRAZE_RADIUS = 8.0f;
constexpr float PLAYER_SPEED = 94.0f;
constexpr float PLAYER_FOCUS_SPEED = 48.0f;
constexpr uint16_t PLAYER_FIRE_MS = 80;
constexpr uint16_t PLAYER_FOCUS_FIRE_MS = 65;
constexpr uint16_t PLAYER_INVULN_MS = 1700;
constexpr uint16_t BOMB_ACTIVE_MS = 700;

constexpr uint16_t OPENING_TIME_MS = 12000;
constexpr uint16_t MID_TIME_MS = 22000;

constexpr uint16_t BOSS_HP = 140;
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
  StageClear,
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
  bool shoot_held = false;
  bool focus_held = false;
  bool bomb_held = false;
  bool bomb_pressed = false;
  bool start_held = false;
  bool start_pressed = false;
};

struct PlayerShot {
  Vec2 pos;
  Vec2 vel;
  uint16_t ttl_ms = 0;
  uint8_t damage = 1;
  bool active = false;
};

struct EnemyBullet {
  Vec2 pos;
  Vec2 vel;
  float radius = 3.0f;
  uint16_t ttl_ms = 0;
  uint8_t style = 0;
  bool grazed = false;
  bool active = false;
};

struct Enemy {
  Vec2 pos;
  Vec2 vel;
  float radius = 8.0f;
  uint8_t hp = 1;
  uint8_t kind = 0;
  uint16_t fire_ms = 0;
  float phase = 0.0f;
  bool active = false;
};

struct BossState {
  Vec2 pos;
  uint16_t hp = app_config::BOSS_HP;
  uint16_t max_hp = app_config::BOSS_HP;
  uint16_t pattern_ms = 0;
  uint16_t sub_pattern_ms = 0;
  uint8_t phase = 0;
  float angle = 0.0f;
  bool active = false;
  bool arrived = false;
};

struct GameState {
  GameMode mode = GameMode::Title;
  Vec2 player_pos = {app_config::PLAYFIELD_X + app_config::PLAYFIELD_W * 0.5f,
                     app_config::PLAYFIELD_BOTTOM - 18.0f};
  uint32_t score = 0;
  uint32_t graze = 0;
  uint32_t high_score = 0;
  uint16_t stage_ms = 0;
  uint16_t fire_ms = 0;
  uint16_t invuln_ms = 0;
  uint16_t bomb_ms = 0;
  uint16_t overlay_ms = 0;
  uint16_t spawn_ms = 0;
  uint8_t lives = 3;
  uint8_t bombs = 3;
  uint8_t power = 1;
  uint16_t frame = 0;
  BossState boss;
};

M5Canvas g_canvas(&M5Cardputer.Display);
GameState g_game;
PlayerShot g_player_shots[app_config::MAX_PLAYER_SHOTS];
EnemyBullet g_enemy_bullets[app_config::MAX_ENEMY_BULLETS];
Enemy g_enemies[app_config::MAX_ENEMIES];
InputState g_prev_input;
uint32_t g_last_update_ms = 0;
uint32_t g_last_draw_ms = 0;

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

float positive_mod(float value, float period) {
  while (value < 0.0f) {
    value += period;
  }
  while (value >= period) {
    value -= period;
  }
  return value;
}

uint16_t timer_subtract(uint16_t timer, uint16_t delta_ms) {
  return timer > delta_ms ? static_cast<uint16_t>(timer - delta_ms) : 0;
}

Vec2 make_vec(float x, float y) {
  Vec2 v;
  v.x = x;
  v.y = y;
  return v;
}

Vec2 add_vec(Vec2 a, Vec2 b) {
  return make_vec(a.x + b.x, a.y + b.y);
}

Vec2 sub_vec(Vec2 a, Vec2 b) {
  return make_vec(a.x - b.x, a.y - b.y);
}

Vec2 scale_vec(Vec2 v, float scale) {
  return make_vec(v.x * scale, v.y * scale);
}

float length_sq(Vec2 v) {
  return v.x * v.x + v.y * v.y;
}

float length(Vec2 v) {
  return sqrtf(length_sq(v));
}

Vec2 normalize_or_zero(Vec2 v) {
  const float len = length(v);
  if (len <= 0.0001f) {
    return make_vec(0.0f, 0.0f);
  }
  return scale_vec(v, 1.0f / len);
}

Vec2 rotate_vec(Vec2 v, float radians) {
  const float c = cosf(radians);
  const float s = sinf(radians);
  return make_vec(v.x * c - v.y * s, v.x * s + v.y * c);
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
  const bool bomb_held = contains_char_key(status, 'd') || contains_char_key(status, 'D');
  const bool enter_held = status.enter || contains_hid_key(status, keyboard_hid::ENTER);

  InputState input;
  input.left_held = contains_hid_key(status, keyboard_hid::LEFT) || contains_char_key(status, ',');
  input.right_held = contains_hid_key(status, keyboard_hid::RIGHT) || contains_char_key(status, '/');
  input.up_held = contains_hid_key(status, keyboard_hid::UP) || contains_char_key(status, ';');
  input.down_held = contains_hid_key(status, keyboard_hid::DOWN) || contains_char_key(status, '.');
  input.shoot_held = contains_char_key(status, 'a') || contains_char_key(status, 'A');
  input.focus_held = contains_char_key(status, 's') || contains_char_key(status, 'S');
  input.bomb_held = bomb_held;
  input.start_held = enter_held;
  input.bomb_pressed = bomb_held && !g_prev_input.bomb_held;
  input.start_pressed = M5Cardputer.BtnA.wasClicked() || (enter_held && !g_prev_input.start_held);
  g_prev_input = input;
  return input;
}

void clear_entities() {
  for (PlayerShot& shot : g_player_shots) {
    shot.active = false;
  }
  for (EnemyBullet& bullet : g_enemy_bullets) {
    bullet.active = false;
  }
  for (Enemy& enemy : g_enemies) {
    enemy.active = false;
  }
}

bool spawn_player_shot(Vec2 pos, Vec2 vel, uint8_t damage) {
  for (PlayerShot& shot : g_player_shots) {
    if (!shot.active) {
      shot.active = true;
      shot.pos = pos;
      shot.vel = vel;
      shot.ttl_ms = 1200;
      shot.damage = damage;
      return true;
    }
  }
  return false;
}

bool spawn_enemy_bullet(Vec2 pos, Vec2 vel, float radius, uint16_t ttl_ms, uint8_t style) {
  for (EnemyBullet& bullet : g_enemy_bullets) {
    if (!bullet.active) {
      bullet.active = true;
      bullet.pos = pos;
      bullet.vel = vel;
      bullet.radius = radius;
      bullet.ttl_ms = ttl_ms;
      bullet.style = style;
      bullet.grazed = false;
      return true;
    }
  }
  return false;
}

bool spawn_enemy(uint8_t kind, Vec2 pos, Vec2 vel, uint8_t hp, uint16_t fire_ms) {
  for (Enemy& enemy : g_enemies) {
    if (!enemy.active) {
      enemy.active = true;
      enemy.kind = kind;
      enemy.pos = pos;
      enemy.vel = vel;
      enemy.hp = hp;
      enemy.fire_ms = fire_ms;
      enemy.phase = static_cast<float>(random(0, 628)) / 100.0f;
      enemy.radius = kind == 0 ? 7.0f : 8.5f;
      return true;
    }
  }
  return false;
}

void add_score(uint32_t value) {
  g_game.score += value;
  if (g_game.score > g_game.high_score) {
    g_game.high_score = g_game.score;
  }
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

void clear_enemy_bullets() {
  for (EnemyBullet& bullet : g_enemy_bullets) {
    bullet.active = false;
  }
}

void start_run() {
  clear_entities();
  g_game.mode = GameMode::Playing;
  g_game.player_pos = make_vec(app_config::PLAYFIELD_X + app_config::PLAYFIELD_W * 0.5f,
                               app_config::PLAYFIELD_BOTTOM - 18.0f);
  g_game.score = 0;
  g_game.graze = 0;
  g_game.stage_ms = 0;
  g_game.fire_ms = 0;
  g_game.invuln_ms = 900;
  g_game.bomb_ms = 0;
  g_game.overlay_ms = 1000;
  g_game.spawn_ms = 300;
  g_game.lives = 3;
  g_game.bombs = 3;
  g_game.power = 1;
  g_game.frame = 0;
  g_game.boss = BossState();
  g_game.boss.max_hp = app_config::BOSS_HP;
  g_game.boss.hp = app_config::BOSS_HP;
}

void activate_bomb() {
  if (g_game.bombs == 0 || g_game.mode != GameMode::Playing || g_game.bomb_ms > 0) {
    return;
  }
  --g_game.bombs;
  g_game.bomb_ms = app_config::BOMB_ACTIVE_MS;
  clear_enemy_bullets();
  for (Enemy& enemy : g_enemies) {
    if (enemy.active) {
      if (enemy.hp <= 2) {
        enemy.active = false;
        add_score(180);
      } else {
        enemy.hp = static_cast<uint8_t>(enemy.hp - 2);
      }
    }
  }
  if (g_game.boss.active) {
    if (g_game.boss.hp <= 12) {
      g_game.boss.hp = 0;
    } else {
      g_game.boss.hp = static_cast<uint16_t>(g_game.boss.hp - 12);
    }
  }
  add_score(500);
}

void player_hit() {
  if (g_game.invuln_ms > 0 || g_game.bomb_ms > 0 || g_game.mode != GameMode::Playing) {
    return;
  }
  clear_enemy_bullets();
  if (g_game.lives <= 1) {
    g_game.lives = 0;
    g_game.mode = GameMode::GameOver;
    g_game.overlay_ms = 0;
    return;
  }
  --g_game.lives;
  g_game.invuln_ms = app_config::PLAYER_INVULN_MS;
  if (g_game.power > 1) {
    --g_game.power;
  }
}

void spawn_player_pattern(bool focus_held) {
  const float fire_speed = 190.0f;
  const int base_damage = focus_held ? 2 : 1;
  spawn_player_shot(add_vec(g_game.player_pos, make_vec(0.0f, -6.0f)), make_vec(0.0f, -fire_speed), base_damage);
  if (g_game.power >= 2) {
    spawn_player_shot(add_vec(g_game.player_pos, make_vec(-5.0f, -5.0f)),
                      make_vec(focus_held ? -8.0f : -24.0f, -fire_speed), 1);
    spawn_player_shot(add_vec(g_game.player_pos, make_vec(5.0f, -5.0f)),
                      make_vec(focus_held ? 8.0f : 24.0f, -fire_speed), 1);
  }
  if (g_game.power >= 3) {
    spawn_player_shot(add_vec(g_game.player_pos, make_vec(-9.0f, -2.0f)),
                      make_vec(focus_held ? -18.0f : -45.0f, -fire_speed + 8.0f), 1);
    spawn_player_shot(add_vec(g_game.player_pos, make_vec(9.0f, -2.0f)),
                      make_vec(focus_held ? 18.0f : 45.0f, -fire_speed + 8.0f), 1);
  }
  if (g_game.power >= 4 && !focus_held) {
    spawn_player_shot(add_vec(g_game.player_pos, make_vec(-12.0f, -1.0f)), make_vec(-62.0f, -170.0f), 1);
    spawn_player_shot(add_vec(g_game.player_pos, make_vec(12.0f, -1.0f)), make_vec(62.0f, -170.0f), 1);
  }
}

void fire_aimed_fan(Vec2 origin, Vec2 target, int count, float spread_deg, float speed, uint8_t style) {
  const Vec2 base_dir = normalize_or_zero(sub_vec(target, origin));
  const float base_angle = atan2f(base_dir.y, base_dir.x);
  const float spread_rad = spread_deg * (kPi / 180.0f);
  for (int i = 0; i < count; ++i) {
    float t = 0.0f;
    if (count > 1) {
      t = static_cast<float>(i) / static_cast<float>(count - 1);
    }
    const float angle = base_angle - spread_rad * 0.5f + spread_rad * t;
    spawn_enemy_bullet(origin, make_vec(cosf(angle) * speed, sinf(angle) * speed), style == 2 ? 2.2f : 3.0f, 2400,
                       style);
  }
}

void fire_radial(Vec2 origin, int count, float speed, float phase, uint8_t style) {
  for (int i = 0; i < count; ++i) {
    const float angle = phase + (2.0f * kPi * static_cast<float>(i) / static_cast<float>(count));
    spawn_enemy_bullet(origin, make_vec(cosf(angle) * speed, sinf(angle) * speed), style == 1 ? 2.5f : 3.4f, 2800,
                       style);
  }
}

void spawn_opening_wave() {
  const bool from_left = ((g_game.stage_ms / 900) % 2) == 0;
  const float y = static_cast<float>(app_config::PLAYFIELD_Y + 18 + random(0, 34));
  const float x = from_left ? static_cast<float>(app_config::PLAYFIELD_X + 18)
                            : static_cast<float>(app_config::PLAYFIELD_RIGHT - 18);
  const float vx = from_left ? 34.0f : -34.0f;
  spawn_enemy(0, make_vec(x, y), make_vec(vx, 13.0f), 3, static_cast<uint16_t>(550 + random(0, 220)));
  spawn_enemy(0, make_vec(x, y + 18.0f), make_vec(vx * 0.8f, 18.0f), 3, static_cast<uint16_t>(700 + random(0, 220)));
}

void spawn_mid_wave() {
  const float center_x = static_cast<float>(app_config::PLAYFIELD_X + 20 + random(0, app_config::PLAYFIELD_W - 40));
  spawn_enemy(1, make_vec(center_x - 26.0f, app_config::PLAYFIELD_Y - 8.0f), make_vec(14.0f, 33.0f), 4, 680);
  spawn_enemy(1, make_vec(center_x, app_config::PLAYFIELD_Y - 14.0f), make_vec(0.0f, 36.0f), 5, 520);
  spawn_enemy(1, make_vec(center_x + 26.0f, app_config::PLAYFIELD_Y - 8.0f), make_vec(-14.0f, 33.0f), 4, 680);
}

void spawn_boss() {
  g_game.boss = BossState();
  g_game.boss.active = true;
  g_game.boss.pos = make_vec(app_config::PLAYFIELD_X + app_config::PLAYFIELD_W * 0.5f, -22.0f);
  g_game.boss.hp = app_config::BOSS_HP;
  g_game.boss.max_hp = app_config::BOSS_HP;
  g_game.boss.pattern_ms = 280;
  g_game.boss.sub_pattern_ms = 520;
  g_game.boss.phase = 0;
  g_game.boss.arrived = false;
}

void update_stage_script(uint16_t delta_ms) {
  g_game.stage_ms = static_cast<uint16_t>(g_game.stage_ms + delta_ms);
  if (g_game.boss.active) {
    return;
  }

  g_game.spawn_ms = timer_subtract(g_game.spawn_ms, delta_ms);
  if (g_game.stage_ms < app_config::OPENING_TIME_MS) {
    if (g_game.spawn_ms == 0) {
      spawn_opening_wave();
      g_game.spawn_ms = 900;
    }
  } else if (g_game.stage_ms < app_config::MID_TIME_MS) {
    if (g_game.spawn_ms == 0) {
      spawn_mid_wave();
      g_game.spawn_ms = 1300;
    }
  } else if (active_enemy_count() == 0 && !g_game.boss.active) {
    spawn_boss();
    g_game.overlay_ms = 1200;
  }
}

void destroy_enemy(Enemy& enemy) {
  enemy.active = false;
  add_score(enemy.kind == 0 ? 220 : 360);
  if (g_game.power < 4 && (random(0, 4) != 0)) {
    ++g_game.power;
  }
}

void update_player_shots(uint16_t delta_ms, float delta_s) {
  for (PlayerShot& shot : g_player_shots) {
    if (!shot.active) {
      continue;
    }
    shot.ttl_ms = timer_subtract(shot.ttl_ms, delta_ms);
    if (shot.ttl_ms == 0) {
      shot.active = false;
      continue;
    }
    shot.pos = add_vec(shot.pos, scale_vec(shot.vel, delta_s));
    if (shot.pos.y < app_config::PLAYFIELD_Y - 8.0f || shot.pos.x < app_config::PLAYFIELD_X - 8.0f ||
        shot.pos.x > app_config::PLAYFIELD_RIGHT + 8.0f) {
      shot.active = false;
      continue;
    }
    for (Enemy& enemy : g_enemies) {
      if (!enemy.active) {
        continue;
      }
      const float hit_radius = enemy.radius + 2.0f;
      if (length_sq(sub_vec(enemy.pos, shot.pos)) <= hit_radius * hit_radius) {
        shot.active = false;
        if (shot.damage >= enemy.hp) {
          destroy_enemy(enemy);
        } else {
          enemy.hp = static_cast<uint8_t>(enemy.hp - shot.damage);
        }
        break;
      }
    }
    if (!shot.active) {
      continue;
    }
    if (g_game.boss.active) {
      const float boss_hit_radius = 14.0f;
      if (length_sq(sub_vec(g_game.boss.pos, shot.pos)) <= boss_hit_radius * boss_hit_radius) {
        shot.active = false;
        if (shot.damage >= g_game.boss.hp) {
          g_game.boss.hp = 0;
        } else {
          g_game.boss.hp = static_cast<uint16_t>(g_game.boss.hp - shot.damage);
        }
        add_score(6);
      }
    }
  }
}

void update_enemies(uint16_t delta_ms, float delta_s) {
  for (Enemy& enemy : g_enemies) {
    if (!enemy.active) {
      continue;
    }
    enemy.fire_ms = timer_subtract(enemy.fire_ms, delta_ms);
    enemy.phase += delta_s * 2.2f;
    if (enemy.kind == 0) {
      enemy.pos = add_vec(enemy.pos, scale_vec(enemy.vel, delta_s));
      enemy.pos.y += sinf(enemy.phase) * 10.0f * delta_s;
      if (enemy.fire_ms == 0 && enemy.pos.y > app_config::PLAYFIELD_Y + 16.0f) {
        fire_aimed_fan(enemy.pos, g_game.player_pos, 3, 18.0f, 50.0f, 0);
        enemy.fire_ms = 900;
      }
    } else {
      enemy.pos = add_vec(enemy.pos, scale_vec(enemy.vel, delta_s));
      enemy.pos.x += sinf(enemy.phase) * 18.0f * delta_s;
      if (enemy.fire_ms == 0 && enemy.pos.y > app_config::PLAYFIELD_Y + 10.0f) {
        fire_aimed_fan(enemy.pos, g_game.player_pos, 5, 42.0f, 56.0f, 1);
        enemy.fire_ms = 820;
      }
    }
    if (enemy.pos.y > app_config::PLAYFIELD_BOTTOM + 18.0f || enemy.pos.x < app_config::PLAYFIELD_X - 24.0f ||
        enemy.pos.x > app_config::PLAYFIELD_RIGHT + 24.0f) {
      enemy.active = false;
    }
  }
}

void update_boss(uint16_t delta_ms, float delta_s) {
  if (!g_game.boss.active) {
    return;
  }

  if (!g_game.boss.arrived) {
    g_game.boss.pos.y += 42.0f * delta_s;
    if (g_game.boss.pos.y >= app_config::PLAYFIELD_Y + 24.0f) {
      g_game.boss.pos.y = app_config::PLAYFIELD_Y + 24.0f;
      g_game.boss.arrived = true;
    }
    return;
  }

  if (g_game.boss.hp == 0) {
    g_game.boss.active = false;
    clear_enemy_bullets();
    g_game.mode = GameMode::StageClear;
    g_game.overlay_ms = 0;
    add_score(5000);
    return;
  }

  g_game.boss.phase = g_game.boss.hp > (app_config::BOSS_HP / 2) ? 0 : 1;
  g_game.boss.angle += delta_s * (g_game.boss.phase == 0 ? 1.7f : 2.4f);
  g_game.boss.pos.x = app_config::PLAYFIELD_X + app_config::PLAYFIELD_W * 0.5f +
                      sinf(g_game.boss.angle * 0.9f) * (g_game.boss.phase == 0 ? 38.0f : 54.0f);

  g_game.boss.pattern_ms = timer_subtract(g_game.boss.pattern_ms, delta_ms);
  g_game.boss.sub_pattern_ms = timer_subtract(g_game.boss.sub_pattern_ms, delta_ms);

  if (g_game.boss.phase == 0) {
    if (g_game.boss.pattern_ms == 0) {
      fire_radial(g_game.boss.pos, 10, 46.0f, g_game.boss.angle, 0);
      g_game.boss.pattern_ms = 360;
    }
    if (g_game.boss.sub_pattern_ms == 0) {
      fire_aimed_fan(g_game.boss.pos, g_game.player_pos, 5, 36.0f, 64.0f, 1);
      g_game.boss.sub_pattern_ms = 800;
    }
  } else {
    if (g_game.boss.pattern_ms == 0) {
      fire_radial(g_game.boss.pos, 14, 54.0f, g_game.boss.angle, 1);
      fire_radial(g_game.boss.pos, 14, 42.0f, -g_game.boss.angle * 0.7f, 2);
      g_game.boss.pattern_ms = 300;
    }
    if (g_game.boss.sub_pattern_ms == 0) {
      fire_aimed_fan(g_game.boss.pos, g_game.player_pos, 7, 62.0f, 70.0f, 2);
      g_game.boss.sub_pattern_ms = 680;
    }
  }
}

void update_enemy_bullets(uint16_t delta_ms, float delta_s) {
  for (EnemyBullet& bullet : g_enemy_bullets) {
    if (!bullet.active) {
      continue;
    }
    bullet.ttl_ms = timer_subtract(bullet.ttl_ms, delta_ms);
    if (bullet.ttl_ms == 0) {
      bullet.active = false;
      continue;
    }
    bullet.pos = add_vec(bullet.pos, scale_vec(bullet.vel, delta_s));
    if (bullet.pos.x < app_config::PLAYFIELD_X - 10.0f || bullet.pos.x > app_config::PLAYFIELD_RIGHT + 10.0f ||
        bullet.pos.y < app_config::PLAYFIELD_Y - 10.0f || bullet.pos.y > app_config::PLAYFIELD_BOTTOM + 10.0f) {
      bullet.active = false;
      continue;
    }
    const float graze_radius = bullet.radius + app_config::PLAYER_GRAZE_RADIUS;
    if (!bullet.grazed && length_sq(sub_vec(bullet.pos, g_game.player_pos)) <= graze_radius * graze_radius) {
      bullet.grazed = true;
      ++g_game.graze;
      add_score(8);
    }
    const float hit_radius = bullet.radius + app_config::PLAYER_RADIUS;
    if (length_sq(sub_vec(bullet.pos, g_game.player_pos)) <= hit_radius * hit_radius) {
      player_hit();
      bullet.active = false;
    }
    if (g_game.bomb_ms > 0) {
      const float bomb_clear_radius = 22.0f;
      if (length_sq(sub_vec(bullet.pos, g_game.player_pos)) <= bomb_clear_radius * bomb_clear_radius) {
        bullet.active = false;
      }
    }
  }
}

void update_player(const InputState& input, uint16_t delta_ms, float delta_s) {
  g_game.fire_ms = timer_subtract(g_game.fire_ms, delta_ms);
  g_game.invuln_ms = timer_subtract(g_game.invuln_ms, delta_ms);
  g_game.bomb_ms = timer_subtract(g_game.bomb_ms, delta_ms);
  g_game.overlay_ms = timer_subtract(g_game.overlay_ms, delta_ms);

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

  const float speed = input.focus_held ? app_config::PLAYER_FOCUS_SPEED : app_config::PLAYER_SPEED;
  g_game.player_pos = add_vec(g_game.player_pos, scale_vec(normalize_or_zero(move), speed * delta_s));

  g_game.player_pos.x = clampf(g_game.player_pos.x, app_config::PLAYFIELD_X + 8.0f, app_config::PLAYFIELD_RIGHT - 8.0f);
  g_game.player_pos.y = clampf(g_game.player_pos.y, app_config::PLAYFIELD_Y + 10.0f, app_config::PLAYFIELD_BOTTOM - 10.0f);

  if (input.bomb_pressed) {
    activate_bomb();
  }

  if (input.shoot_held && g_game.fire_ms == 0) {
    spawn_player_pattern(input.focus_held);
    g_game.fire_ms = input.focus_held ? app_config::PLAYER_FOCUS_FIRE_MS : app_config::PLAYER_FIRE_MS;
  }
}

void update_game(const InputState& input, uint16_t delta_ms) {
  if (input.start_pressed) {
    if (g_game.mode == GameMode::Title || g_game.mode == GameMode::GameOver || g_game.mode == GameMode::StageClear) {
      start_run();
      return;
    }
  }

  if (g_game.mode != GameMode::Playing) {
    return;
  }

  ++g_game.frame;
  const float delta_s = static_cast<float>(delta_ms) * 0.001f;

  update_player(input, delta_ms, delta_s);
  update_stage_script(delta_ms);
  update_enemies(delta_ms, delta_s);
  update_boss(delta_ms, delta_s);
  update_player_shots(delta_ms, delta_s);
  update_enemy_bullets(delta_ms, delta_s);
}

void draw_playfield_background(M5Canvas& canvas) {
  for (int y = 0; y < app_config::SCREEN_H; ++y) {
    const uint8_t r = static_cast<uint8_t>(8 + (y * 18) / app_config::SCREEN_H);
    const uint8_t g = static_cast<uint8_t>(6 + (y * 22) / app_config::SCREEN_H);
    const uint8_t b = static_cast<uint8_t>(22 + (y * 42) / app_config::SCREEN_H);
    canvas.drawFastHLine(0, y, app_config::SCREEN_W, rgb565(r, g, b));
  }

  const float far_offset_y = static_cast<float>(g_game.frame) * 0.65f;
  const float far_offset_x = sinf(static_cast<float>(g_game.frame) * 0.02f) * 6.0f;
  const float far_cell = 18.0f;
  const int far_cells_x = app_config::PLAYFIELD_W / static_cast<int>(far_cell) + 5;
  const int far_cells_y = app_config::PLAYFIELD_H / static_cast<int>(far_cell) + 5;
  const float far_shift_x = positive_mod(far_offset_x, far_cell);
  const float far_shift_y = positive_mod(far_offset_y, far_cell);
  for (int gy = -2; gy < far_cells_y; ++gy) {
    for (int gx = -2; gx < far_cells_x; ++gx) {
      const int cx = gx + static_cast<int>(far_offset_x / far_cell);
      const int cy = gy - static_cast<int>(far_offset_y / far_cell);
      const uint32_t hash = hash2d(cx, cy, 0x31u);
      if ((hash & 3u) != 0u) {
        continue;
      }
      const int16_t x = static_cast<int16_t>(
          lroundf(app_config::PLAYFIELD_X + gx * far_cell - far_shift_x + 4.0f + static_cast<float>(hash % 9u)));
      const int16_t y = static_cast<int16_t>(lroundf(gy * far_cell + far_shift_y + 2.0f +
                                                     static_cast<float>((hash >> 9) % 12u)));
      if ((hash & 15u) == 5u) {
        canvas.fillCircle(x, y, 2, rgb565(118, 190, 255));
      } else {
        canvas.drawPixel(x, y, rgb565(98, 176, 255));
      }
    }
  }

  const float mid_offset_y = static_cast<float>(g_game.frame) * 1.4f;
  const float mid_offset_x = sinf(static_cast<float>(g_game.frame) * 0.015f) * 12.0f;
  const float mid_cell = 34.0f;
  const int mid_cells_x = app_config::PLAYFIELD_W / static_cast<int>(mid_cell) + 5;
  const int mid_cells_y = app_config::PLAYFIELD_H / static_cast<int>(mid_cell) + 5;
  const float mid_shift_x = positive_mod(mid_offset_x, mid_cell);
  const float mid_shift_y = positive_mod(mid_offset_y, mid_cell);
  for (int gy = -2; gy < mid_cells_y; ++gy) {
    for (int gx = -2; gx < mid_cells_x; ++gx) {
      const int cx = gx + static_cast<int>(mid_offset_x / mid_cell);
      const int cy = gy - static_cast<int>(mid_offset_y / mid_cell);
      const uint32_t hash = hash2d(cx, cy, 0x81u);
      if ((hash & 7u) > 2u) {
        continue;
      }
      const int16_t x = static_cast<int16_t>(lroundf(app_config::PLAYFIELD_X + gx * mid_cell - mid_shift_x));
      const int16_t y = static_cast<int16_t>(lroundf(gy * mid_cell + mid_shift_y));
      const int16_t width = static_cast<int16_t>(12 + static_cast<int>((hash >> 4) % 16u));
      const int16_t height = static_cast<int16_t>(6 + static_cast<int>((hash >> 11) % 12u));
      canvas.drawRect(x, y, width, height, rgb565(54, 102, 148));
      canvas.drawFastHLine(x + 2, y + height / 2, width - 4, rgb565(106, 208, 255));
      canvas.drawFastVLine(x + width / 2, y + 1, height - 2, rgb565(76, 156, 212));
    }
  }

  const int pulse = static_cast<int>(sinf(static_cast<float>(g_game.frame) * 0.08f) * 8.0f);
  for (int y = 0; y < app_config::PLAYFIELD_H; y += 12) {
    const uint16_t line = (y % 24 == 0) ? rgb565(36, 24, 70) : rgb565(24, 16, 50);
    canvas.drawFastHLine(app_config::PLAYFIELD_X, y, app_config::PLAYFIELD_W, line);
  }
  for (int x = app_config::PLAYFIELD_X; x < app_config::PLAYFIELD_RIGHT; x += 16) {
    canvas.drawFastVLine(x, app_config::PLAYFIELD_Y, app_config::PLAYFIELD_H, rgb565(22, 14, 46));
  }

  for (int i = 0; i < 5; ++i) {
    const int16_t glow_x = static_cast<int16_t>(18 + i * 30 + pulse);
    canvas.drawFastVLine(glow_x, app_config::PLAYFIELD_Y, app_config::PLAYFIELD_H, rgb565(26, 36, 78));
  }

  canvas.fillRect(app_config::PLAYFIELD_RIGHT, 0, app_config::SIDEBAR_X - app_config::PLAYFIELD_RIGHT,
                  app_config::SCREEN_H, rgb565(16, 8, 28));
  canvas.drawFastVLine(app_config::PLAYFIELD_RIGHT, 0, app_config::SCREEN_H, rgb565(255, 220, 255));
  canvas.drawFastVLine(app_config::SIDEBAR_X - 1, 0, app_config::SCREEN_H, rgb565(96, 54, 132));
}

void draw_sidebar(M5Canvas& canvas) {
  for (int y = 0; y < app_config::SCREEN_H; ++y) {
    const uint8_t r = static_cast<uint8_t>(18 + (y * 14) / app_config::SCREEN_H);
    const uint8_t g = static_cast<uint8_t>(10 + (y * 10) / app_config::SCREEN_H);
    const uint8_t b = static_cast<uint8_t>(28 + (y * 26) / app_config::SCREEN_H);
    canvas.drawFastHLine(app_config::SIDEBAR_X, y, app_config::SIDEBAR_W, rgb565(r, g, b));
  }

  for (int y = 8; y < app_config::SCREEN_H; y += 18) {
    canvas.drawFastHLine(app_config::SIDEBAR_X + 4, y, app_config::SIDEBAR_W - 8, rgb565(44, 24, 66));
  }
  for (int x = app_config::SIDEBAR_X + 8; x < app_config::SCREEN_W; x += 18) {
    canvas.drawFastVLine(x, 4, app_config::SCREEN_H - 8, rgb565(36, 20, 58));
  }

  canvas.drawRect(app_config::SIDEBAR_X, 0, app_config::SIDEBAR_W, app_config::SCREEN_H, rgb565(168, 112, 220));
  canvas.fillRect(app_config::SIDEBAR_X + 4, 4, app_config::SIDEBAR_W - 8, 12, rgb565(48, 20, 78));
  canvas.setTextColor(rgb565(250, 228, 255), rgb565(48, 20, 78));
  canvas.setCursor(app_config::SIDEBAR_X + 10, 7);
  canvas.print("DANMAKU");

  canvas.drawRoundRect(app_config::SIDEBAR_X + 4, 20, app_config::SIDEBAR_W - 8, 86, 4, rgb565(114, 78, 164));
  canvas.drawRoundRect(app_config::SIDEBAR_X + 4, 108, app_config::SIDEBAR_W - 8, 23, 4, rgb565(114, 78, 164));

  canvas.setTextColor(rgb565(235, 220, 255), rgb565(20, 10, 30));
  canvas.setCursor(app_config::SIDEBAR_X + 6, 24);
  canvas.printf("SC %lu", static_cast<unsigned long>(g_game.score));
  canvas.setCursor(app_config::SIDEBAR_X + 6, 38);
  canvas.printf("HI %lu", static_cast<unsigned long>(g_game.high_score));
  canvas.setCursor(app_config::SIDEBAR_X + 6, 54);
  canvas.printf("LIFE %u", g_game.lives);
  canvas.setCursor(app_config::SIDEBAR_X + 6, 68);
  canvas.printf("BOMB %u", g_game.bombs);
  canvas.setCursor(app_config::SIDEBAR_X + 6, 82);
  canvas.printf("PWR  %u", g_game.power);
  canvas.setCursor(app_config::SIDEBAR_X + 6, 96);
  canvas.printf("GRZ %lu", static_cast<unsigned long>(g_game.graze));

  if (g_game.mode == GameMode::Playing && g_game.boss.active) {
    canvas.setCursor(app_config::SIDEBAR_X + 6, 111);
    canvas.print("BOSS");
    canvas.drawRect(app_config::SIDEBAR_X + 6, 121, app_config::SIDEBAR_W - 12, 8, rgb565(220, 168, 255));
    const int fill_w = static_cast<int>((app_config::SIDEBAR_W - 14) * g_game.boss.hp / g_game.boss.max_hp);
    canvas.fillRect(app_config::SIDEBAR_X + 7, 122, fill_w, 6, rgb565(255, 92, 170));
  } else {
    canvas.setCursor(app_config::SIDEBAR_X + 6, 112);
    if (g_game.mode == GameMode::Playing) {
      canvas.printf("STG %u", g_game.stage_ms / 1000);
    }
  }
}

void draw_player_shots(M5Canvas& canvas) {
  for (const PlayerShot& shot : g_player_shots) {
    if (!shot.active) {
      continue;
    }
    canvas.drawFastVLine(static_cast<int16_t>(shot.pos.x), static_cast<int16_t>(shot.pos.y) - 2, 5,
                         rgb565(180, 255, 252));
  }
}

void draw_enemy_bullets(M5Canvas& canvas) {
  for (const EnemyBullet& bullet : g_enemy_bullets) {
    if (!bullet.active) {
      continue;
    }
    const int16_t x = static_cast<int16_t>(lroundf(bullet.pos.x));
    const int16_t y = static_cast<int16_t>(lroundf(bullet.pos.y));
    if (bullet.style == 0) {
      canvas.fillCircle(x, y, static_cast<int16_t>(bullet.radius), rgb565(255, 92, 128));
    } else if (bullet.style == 1) {
      canvas.fillCircle(x, y, static_cast<int16_t>(bullet.radius), rgb565(168, 208, 255));
      canvas.drawCircle(x, y, static_cast<int16_t>(bullet.radius + 1), rgb565(240, 252, 255));
    } else {
      canvas.fillRoundRect(x - 2, y - 1, 5, 3, 1, rgb565(255, 226, 96));
      canvas.drawFastHLine(x - 4, y, 9, rgb565(255, 246, 188));
    }
  }
}

void draw_enemies(M5Canvas& canvas) {
  for (const Enemy& enemy : g_enemies) {
    if (!enemy.active) {
      continue;
    }
    const int16_t x = static_cast<int16_t>(lroundf(enemy.pos.x));
    const int16_t y = static_cast<int16_t>(lroundf(enemy.pos.y));
    const int16_t r = static_cast<int16_t>(lroundf(enemy.radius));
    const uint16_t body = enemy.kind == 0 ? rgb565(255, 138, 176) : rgb565(255, 198, 116);
    canvas.fillCircle(x, y, r, body);
    canvas.fillTriangle(x, y + r + 1, x - r - 1, y - 1, x + r + 1, y - 1, rgb565(110, 24, 48));
    canvas.drawCircle(x, y, r + 1, rgb565(255, 236, 244));
  }
}

void draw_boss(M5Canvas& canvas) {
  if (!g_game.boss.active) {
    return;
  }
  const int16_t x = static_cast<int16_t>(lroundf(g_game.boss.pos.x));
  const int16_t y = static_cast<int16_t>(lroundf(g_game.boss.pos.y));
  canvas.fillCircle(x, y, 12, rgb565(255, 120, 160));
  canvas.fillTriangle(x - 17, y + 3, x - 4, y - 6, x - 2, y + 10, rgb565(255, 240, 170));
  canvas.fillTriangle(x + 17, y + 3, x + 4, y - 6, x + 2, y + 10, rgb565(255, 240, 170));
  canvas.drawCircle(x, y, 14, rgb565(255, 240, 248));
  canvas.fillCircle(x - 4, y - 2, 1, rgb565(90, 14, 40));
  canvas.fillCircle(x + 4, y - 2, 1, rgb565(90, 14, 40));
}

void draw_player(M5Canvas& canvas, bool focus_held) {
  if (g_game.invuln_ms > 0 && ((g_game.invuln_ms / 80) % 2) == 0) {
    return;
  }
  const int16_t x = static_cast<int16_t>(lroundf(g_game.player_pos.x));
  const int16_t y = static_cast<int16_t>(lroundf(g_game.player_pos.y));
  canvas.fillTriangle(x, y - 6, x - 5, y + 5, x + 5, y + 5, rgb565(255, 246, 244));
  canvas.fillTriangle(x, y - 8, x - 3, y - 1, x + 3, y - 1, rgb565(255, 92, 120));
  canvas.drawCircle(x, y, 1, rgb565(255, 80, 120));
  if (g_game.power >= 3) {
    canvas.fillCircle(x - 10, y - 2, 2, rgb565(150, 220, 255));
    canvas.fillCircle(x + 10, y - 2, 2, rgb565(150, 220, 255));
  }
  if (focus_held) {
    canvas.drawCircle(x, y, 3, rgb565(120, 255, 248));
    canvas.drawCircle(x, y, 5, rgb565(70, 180, 214));
  }
  if (g_game.bomb_ms > 0) {
    const int16_t radius = static_cast<int16_t>(10 + ((app_config::BOMB_ACTIVE_MS - g_game.bomb_ms) / 40) % 12);
    canvas.drawCircle(x, y, radius, rgb565(255, 255, 160));
    canvas.drawCircle(x, y, radius + 1, rgb565(255, 170, 210));
  }
}

void draw_overlay(M5Canvas& canvas, const InputState& input) {
  if (g_game.mode == GameMode::Title) {
    canvas.fillRoundRect(18, 18, 124, 98, 6, rgb565(18, 8, 28));
    canvas.drawRoundRect(18, 18, 124, 98, 6, rgb565(214, 164, 255));
    canvas.setTextColor(rgb565(248, 234, 255), rgb565(18, 8, 28));
    canvas.setCursor(31, 28);
    canvas.print("DANMAKU SHRINE");
    canvas.setCursor(30, 43);
    canvas.print(";/., move");
    canvas.setCursor(30, 55);
    canvas.print("A shot");
    canvas.setCursor(30, 67);
    canvas.print("S focus");
    canvas.setCursor(30, 79);
    canvas.print("D bomb");
    canvas.setCursor(30, 96);
    canvas.print("Enter / BtnA");
  } else if (g_game.mode == GameMode::GameOver) {
    canvas.fillRoundRect(28, 42, 104, 46, 6, rgb565(34, 10, 20));
    canvas.drawRoundRect(28, 42, 104, 46, 6, rgb565(255, 124, 170));
    canvas.setTextColor(rgb565(255, 228, 236), rgb565(34, 10, 20));
    canvas.setCursor(55, 54);
    canvas.print("GAME OVER");
    canvas.setCursor(39, 70);
    canvas.print("Enter / BtnA");
  } else if (g_game.mode == GameMode::StageClear) {
    canvas.fillRoundRect(24, 36, 112, 54, 6, rgb565(18, 22, 28));
    canvas.drawRoundRect(24, 36, 112, 54, 6, rgb565(164, 255, 220));
    canvas.setTextColor(rgb565(234, 255, 246), rgb565(18, 22, 28));
    canvas.setCursor(49, 49);
    canvas.print("STAGE CLEAR");
    canvas.setCursor(37, 65);
    canvas.print("Enter / BtnA");
  } else if (g_game.overlay_ms > 0) {
    canvas.fillRoundRect(37, 8, 88, 16, 4, rgb565(26, 8, 36));
    canvas.drawRoundRect(37, 8, 88, 16, 4, rgb565(255, 208, 126));
    canvas.setTextColor(rgb565(255, 238, 180), rgb565(26, 8, 36));
    canvas.setCursor(53, 13);
    if (g_game.boss.active) {
      canvas.print("BOSS ATTACK");
    } else if (g_game.stage_ms < app_config::OPENING_TIME_MS) {
      canvas.print("OPENING WAVE");
    } else {
      canvas.print("MID WAVE");
    }
  }
  (void)input;
}

void draw_scene(const InputState& input) {
  draw_playfield_background(g_canvas);
  draw_player_shots(g_canvas);
  draw_enemy_bullets(g_canvas);
  draw_enemies(g_canvas);
  draw_boss(g_canvas);
  draw_player(g_canvas, input.focus_held);
  g_canvas.drawRect(app_config::PLAYFIELD_X, app_config::PLAYFIELD_Y, app_config::PLAYFIELD_W, app_config::PLAYFIELD_H,
                    rgb565(255, 226, 255));
  draw_sidebar(g_canvas);
  draw_overlay(g_canvas, input);
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

  clear_entities();
  g_last_update_ms = millis();
  g_last_draw_ms = g_last_update_ms;
  Serial.println("Cardputer ADV Danmaku demo started");
}

void loop() {
  M5Cardputer.update();
  const InputState input = read_input();
  const uint32_t now = millis();

  uint16_t delta_ms = static_cast<uint16_t>(now - g_last_update_ms);
  if (delta_ms >= app_config::UPDATE_MIN_MS) {
    if (delta_ms > 40) {
      delta_ms = 40;
    }
    update_game(input, delta_ms);
    g_last_update_ms = now;
  }

  if (now - g_last_draw_ms >= app_config::DRAW_INTERVAL_MS) {
    draw_scene(input);
    M5Cardputer.Display.startWrite();
    g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
    M5Cardputer.Display.endWrite();
    g_last_draw_ms = now;
  }
}
