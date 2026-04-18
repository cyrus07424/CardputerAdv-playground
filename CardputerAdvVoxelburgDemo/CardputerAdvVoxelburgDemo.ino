#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>
#include <string.h>

namespace app_config {
constexpr int16_t SCREEN_W = 240;
constexpr int16_t SCREEN_H = 135;
constexpr int16_t RENDER_COLUMNS = 120;
constexpr int16_t COLUMN_W = SCREEN_W / RENDER_COLUMNS;
constexpr int16_t WORLD_SIZE = 128;
constexpr int16_t WORLD_MASK = WORLD_SIZE - 1;
constexpr int16_t WATER_LEVEL = 12;
constexpr float WORLD_UNIT = 1.0f;
constexpr float FLIGHT_HEIGHT = 16.0f;
constexpr float POD_HEIGHT = 8.5f;
constexpr float POD_RADIUS = 2.2f;
constexpr float CAMERA_EYE_OFFSET = 2.0f;
constexpr float FOV_DEG = 72.0f;
constexpr float FAR_DISTANCE = 82.0f;
constexpr float NEAR_DISTANCE = 1.5f;
constexpr float TURN_SPEED_DEG = 86.0f;
constexpr float PITCH_SPEED = 34.0f;
constexpr float MAX_PITCH_OFFSET = 20.0f;
constexpr float MIN_PITCH_OFFSET = -16.0f;
constexpr float STRAFE_SPEED = 9.5f;
constexpr float SHOT_COOLDOWN_MS = 260.0f;
constexpr float SHOT_MAX_AIM_PX = 18.0f;
constexpr float HIT_FLASH_MS = 140.0f;
constexpr float MUZZLE_FLASH_MS = 70.0f;
constexpr float DODGE_WINDOW_MS = 350.0f;
constexpr uint32_t FRAME_INTERVAL_MS = 33;
constexpr uint8_t MAX_ENEMIES = 16;
constexpr uint8_t MAX_HEALTH = 5;
constexpr uint8_t POD_HP = 3;
constexpr int16_t SKY_BANDS = 18;
}  // namespace app_config

namespace keyboard_hid {
constexpr uint8_t RIGHT = 0x4F;
constexpr uint8_t LEFT = 0x50;
constexpr uint8_t DOWN = 0x51;
constexpr uint8_t UP = 0x52;
constexpr uint8_t ENTER = 0x28;
}  // namespace keyboard_hid

enum class GameMode : uint8_t {
  Title,
  Playing,
  Victory,
  GameOver,
};

enum class Difficulty : uint8_t {
  Easy = 0,
  Normal,
  Hard,
  Count,
};

enum class PlayStyle : uint8_t {
  Mission = 0,
  Endless,
};

struct DifficultyConfig {
  const char* name;
  uint8_t pod_count;
  uint16_t enemy_interval_ms;
  uint16_t enemy_charge_ms;
  float thrust_speed;
  uint8_t damage;
  uint16_t score_bonus;
};

struct InputState {
  bool turn_left = false;
  bool turn_right = false;
  bool pitch_up = false;
  bool pitch_down = false;
  bool thrust = false;
  bool strafe_left = false;
  bool strafe_right = false;
  bool shoot = false;
  bool start = false;
  bool restart = false;
  bool help = false;
  bool difficulty_left = false;
  bool difficulty_right = false;
  bool style_toggle = false;
};

struct EnemyPod {
  float x = 0.0f;
  float y = 0.0f;
  uint8_t hp = app_config::POD_HP;
  bool alive = false;
  bool visible = false;
  bool charging = false;
  uint32_t last_attack_ms = 0;
  uint32_t charge_start_ms = 0;
  float distance = 0.0f;
  float screen_x = 0.0f;
  float screen_y = 0.0f;
  float screen_radius = 0.0f;
};

struct PlayerState {
  float x = 0.0f;
  float y = 0.0f;
  float yaw_deg = 0.0f;
  float pitch_offset = 0.0f;
  float altitude = 0.0f;
  uint8_t health = app_config::MAX_HEALTH;
};

M5Canvas g_canvas(&M5Cardputer.Display);
uint8_t g_height_map[app_config::WORLD_SIZE][app_config::WORLD_SIZE];
uint16_t g_color_map[app_config::WORLD_SIZE][app_config::WORLD_SIZE];
EnemyPod g_enemies[app_config::MAX_ENEMIES];

const DifficultyConfig kDifficulties[] = {
    {"EASY", 8, 3600, 1250, 7.8f, 1, 450},
    {"NORMAL", 12, 2800, 1000, 8.6f, 1, 800},
    {"HARD", 16, 2100, 850, 9.4f, 2, 1300},
};

GameMode g_mode = GameMode::Title;
Difficulty g_selected_difficulty = Difficulty::Normal;
PlayStyle g_selected_style = PlayStyle::Mission;
PlayerState g_player;
InputState g_prev_input;

uint32_t g_seed = 0;
uint32_t g_last_frame_ms = 0;
uint32_t g_round_start_ms = 0;
uint32_t g_last_shot_ms = 0;
uint32_t g_last_damage_ms = 0;
uint32_t g_last_muzzle_flash_ms = 0;
uint32_t g_last_strafe_ms = 0;
uint16_t g_destroyed_pods = 0;
uint16_t g_total_kills = 0;
uint16_t g_total_score = 0;
uint16_t g_wave_index = 1;
bool g_show_help = true;

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

int clampi(int value, int min_value, int max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
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

uint32_t hash2d(uint32_t x, uint32_t y, uint32_t seed) {
  return mix_bits(x * 0x1f123bb5U ^ y * 0x5f356495U ^ seed);
}

float hash_unit(int x, int y, uint32_t seed) {
  return static_cast<float>(hash2d(static_cast<uint32_t>(x), static_cast<uint32_t>(y), seed) & 0xFFFFU) /
         65535.0f;
}

float smoothstep(float t) {
  return t * t * (3.0f - 2.0f * t);
}

float value_noise(float x, float y, uint32_t seed) {
  const int ix = static_cast<int>(floorf(x));
  const int iy = static_cast<int>(floorf(y));
  const float fx = x - static_cast<float>(ix);
  const float fy = y - static_cast<float>(iy);
  const float tx = smoothstep(fx);
  const float ty = smoothstep(fy);

  const float v00 = hash_unit(ix, iy, seed);
  const float v10 = hash_unit(ix + 1, iy, seed);
  const float v01 = hash_unit(ix, iy + 1, seed);
  const float v11 = hash_unit(ix + 1, iy + 1, seed);

  const float a = v00 + (v10 - v00) * tx;
  const float b = v01 + (v11 - v01) * tx;
  return a + (b - a) * ty;
}

float fractal_noise(float x, float y, uint32_t seed, uint8_t octaves, float persistence) {
  float total = 0.0f;
  float amplitude = 1.0f;
  float norm = 0.0f;
  float freq = 1.0f;
  for (uint8_t i = 0; i < octaves; ++i) {
    total += (value_noise(x * freq, y * freq, seed + i * 31U) * 2.0f - 1.0f) * amplitude;
    norm += amplitude;
    amplitude *= persistence;
    freq *= 2.0f;
  }
  if (norm <= 0.0f) {
    return 0.0f;
  }
  return total / norm;
}

int wrap_index(int value) {
  return value & app_config::WORLD_MASK;
}

float wrap_position(float value) {
  while (value < 0.0f) {
    value += static_cast<float>(app_config::WORLD_SIZE);
  }
  while (value >= static_cast<float>(app_config::WORLD_SIZE)) {
    value -= static_cast<float>(app_config::WORLD_SIZE);
  }
  return value;
}

float shortest_delta(float from, float to) {
  float delta = to - from;
  const float half = static_cast<float>(app_config::WORLD_SIZE) * 0.5f;
  while (delta > half) {
    delta -= static_cast<float>(app_config::WORLD_SIZE);
  }
  while (delta < -half) {
    delta += static_cast<float>(app_config::WORLD_SIZE);
  }
  return delta;
}

uint8_t cell_height(int x, int y) {
  return g_height_map[wrap_index(y)][wrap_index(x)];
}

float sample_height(float x, float y) {
  const float wx = wrap_position(x);
  const float wy = wrap_position(y);
  const int ix = static_cast<int>(floorf(wx));
  const int iy = static_cast<int>(floorf(wy));
  const float fx = wx - static_cast<float>(ix);
  const float fy = wy - static_cast<float>(iy);

  const float h00 = static_cast<float>(cell_height(ix, iy));
  const float h10 = static_cast<float>(cell_height(ix + 1, iy));
  const float h01 = static_cast<float>(cell_height(ix, iy + 1));
  const float h11 = static_cast<float>(cell_height(ix + 1, iy + 1));

  const float a = h00 + (h10 - h00) * fx;
  const float b = h01 + (h11 - h01) * fx;
  return a + (b - a) * fy;
}

uint16_t sample_color(float x, float y) {
  const int ix = static_cast<int>(floorf(wrap_position(x)));
  const int iy = static_cast<int>(floorf(wrap_position(y)));
  return g_color_map[wrap_index(iy)][wrap_index(ix)];
}

uint16_t shade_color(uint16_t color, float factor) {
  factor = clampf(factor, 0.15f, 1.0f);
  uint8_t r = static_cast<uint8_t>(((color >> 11) & 0x1F) * 255 / 31);
  uint8_t g = static_cast<uint8_t>(((color >> 5) & 0x3F) * 255 / 63);
  uint8_t b = static_cast<uint8_t>((color & 0x1F) * 255 / 31);
  r = static_cast<uint8_t>(clampf(r * factor, 0.0f, 255.0f));
  g = static_cast<uint8_t>(clampf(g * factor, 0.0f, 255.0f));
  b = static_cast<uint8_t>(clampf(b * factor, 0.0f, 255.0f));
  return rgb565(r, g, b);
}

bool contains_hid_key(const Keyboard_Class::KeysState& status, uint8_t key_code) {
  for (const auto key : status.hid_keys) {
    if ((key & ~SHIFT) == key_code) {
      return true;
    }
  }
  return false;
}

bool key_pressed(const Keyboard_Class::KeysState& status, char lower_case) {
  const char upper_case = lower_case - ('a' - 'A');
  for (const auto key : status.word) {
    if (key == lower_case || key == upper_case) {
      return true;
    }
  }
  return false;
}

InputState read_input() {
  const auto status = M5Cardputer.Keyboard.keysState();
  InputState input;
  input.turn_left = contains_hid_key(status, keyboard_hid::LEFT) || key_pressed(status, ',');
  input.turn_right = contains_hid_key(status, keyboard_hid::RIGHT) || key_pressed(status, '/');
  input.pitch_up = contains_hid_key(status, keyboard_hid::UP) || key_pressed(status, ';');
  input.pitch_down = contains_hid_key(status, keyboard_hid::DOWN) || key_pressed(status, '.');
  input.thrust = key_pressed(status, 'a');
  input.strafe_left = key_pressed(status, 'q');
  input.strafe_right = key_pressed(status, 'e');
  input.shoot = key_pressed(status, 's');
  input.start = contains_hid_key(status, keyboard_hid::ENTER);
  input.restart = key_pressed(status, 'r');
  input.help = key_pressed(status, 'h');
  input.difficulty_left = key_pressed(status, 'z');
  input.difficulty_right = key_pressed(status, 'x');
  input.style_toggle = key_pressed(status, 'c');
  return input;
}

const DifficultyConfig& active_difficulty() {
  return kDifficulties[static_cast<uint8_t>(g_selected_difficulty)];
}

bool input_pressed(bool current, bool previous) {
  return current && !previous;
}

bool endless_mode() {
  return g_selected_style == PlayStyle::Endless;
}

bool is_local_peak(int x, int y) {
  const uint8_t center = cell_height(x, y);
  for (int dy = -2; dy <= 2; ++dy) {
    for (int dx = -2; dx <= 2; ++dx) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      if (cell_height(x + dx, y + dy) > center) {
        return false;
      }
    }
  }
  return true;
}

float distance_sq(float x0, float y0, float x1, float y1) {
  const float dx = shortest_delta(x0, x1);
  const float dy = shortest_delta(y0, y1);
  return dx * dx + dy * dy;
}

void clear_enemies() {
  for (auto& enemy : g_enemies) {
    enemy = {};
  }
}

void generate_world(uint32_t seed) {
  clear_enemies();
  for (int y = 0; y < app_config::WORLD_SIZE; ++y) {
    for (int x = 0; x < app_config::WORLD_SIZE; ++x) {
      const float nx = static_cast<float>(x) / static_cast<float>(app_config::WORLD_SIZE);
      const float ny = static_cast<float>(y) / static_cast<float>(app_config::WORLD_SIZE);
      const float continent = fractal_noise(nx * 1.8f + 4.2f, ny * 1.8f - 1.6f, seed + 11U, 4, 0.55f);
      const float hills = fractal_noise(nx * 4.5f - 3.0f, ny * 4.5f + 8.0f, seed + 39U, 3, 0.5f);
      const float detail = fractal_noise(nx * 9.0f + 1.0f, ny * 9.0f + 2.0f, seed + 97U, 3, 0.45f);
      const float ridges = 1.0f - fabsf(value_noise(nx * 6.0f + 12.0f, ny * 6.0f + 7.0f, seed + 131U) * 2.0f - 1.0f);
      float height = 18.0f + continent * 12.0f + hills * 8.5f + detail * 4.0f + ridges * 6.0f;
      if (continent < -0.18f) {
        height -= 8.0f;
      }
      if (continent < -0.32f) {
        height -= 6.0f;
      }
      const int cell = clampi(static_cast<int>(height + 0.5f), 4, 54);
      g_height_map[y][x] = static_cast<uint8_t>(cell);
    }
  }

  for (int y = 0; y < app_config::WORLD_SIZE; ++y) {
    for (int x = 0; x < app_config::WORLD_SIZE; ++x) {
      const int h = cell_height(x, y);
      const int slope = max(
          abs(cell_height(x + 1, y) - cell_height(x - 1, y)),
          abs(cell_height(x, y + 1) - cell_height(x, y - 1)));
      uint16_t color = TFT_GREEN;
      if (h <= app_config::WATER_LEVEL - 2) {
        color = rgb565(20, 70, 142);
      } else if (h <= app_config::WATER_LEVEL + 1) {
        color = rgb565(198, 174, 108);
      } else if (h >= 46) {
        color = slope > 2 ? rgb565(210, 214, 220) : rgb565(236, 240, 244);
      } else if (h >= 38) {
        color = rgb565(112, 114, 120);
      } else if (slope >= 5) {
        color = rgb565(108, 92, 64);
      } else if (h >= 28) {
        color = rgb565(74, 134, 74);
      } else {
        color = rgb565(52, 158, 62);
      }
      g_color_map[y][x] = color;
    }
  }
}

void place_enemy_pods(uint32_t seed, uint8_t pod_count) {
  uint8_t placed = 0;
  uint16_t attempts = 0;
  const float min_spacing_sq = 15.0f * 15.0f;
  while (placed < pod_count && attempts < 8000) {
    const uint32_t h = hash2d(attempts + 17U, attempts * 13U + 5U, seed + 0x92U);
    const int x = static_cast<int>(h & app_config::WORLD_MASK);
    const int y = static_cast<int>((h >> 8) & app_config::WORLD_MASK);
    ++attempts;

    if (cell_height(x, y) <= app_config::WATER_LEVEL + 4) {
      continue;
    }
    if (!is_local_peak(x, y)) {
      continue;
    }

    bool too_close = false;
    for (uint8_t i = 0; i < placed; ++i) {
      if (distance_sq(g_enemies[i].x, g_enemies[i].y, static_cast<float>(x), static_cast<float>(y)) < min_spacing_sq) {
        too_close = true;
        break;
      }
    }
    if (too_close) {
      continue;
    }

    g_enemies[placed].x = static_cast<float>(x) + 0.5f;
    g_enemies[placed].y = static_cast<float>(y) + 0.5f;
    g_enemies[placed].hp = app_config::POD_HP;
    g_enemies[placed].alive = true;
    g_enemies[placed].last_attack_ms = millis() + placed * 160U;
    ++placed;
  }

  for (; placed < pod_count; ++placed) {
    const int x = (13 * placed + 23) & app_config::WORLD_MASK;
    const int y = (29 * placed + 41) & app_config::WORLD_MASK;
    g_enemies[placed].x = static_cast<float>(x) + 0.5f;
    g_enemies[placed].y = static_cast<float>(y) + 0.5f;
    g_enemies[placed].hp = app_config::POD_HP;
    g_enemies[placed].alive = true;
    g_enemies[placed].last_attack_ms = millis();
  }
}

void place_player_spawn(uint32_t seed) {
  float best_score = -1.0f;
  float best_x = 64.0f;
  float best_y = 64.0f;

  for (uint16_t i = 0; i < 320; ++i) {
    const uint32_t h = hash2d(i + 3U, i * 17U + 9U, seed + 0x211U);
    const int x = static_cast<int>(h & app_config::WORLD_MASK);
    const int y = static_cast<int>((h >> 7) & app_config::WORLD_MASK);
    const float terrain = static_cast<float>(cell_height(x, y));
    if (terrain <= app_config::WATER_LEVEL + 1) {
      continue;
    }

    float nearest_enemy_sq = 1e9f;
    for (const auto& enemy : g_enemies) {
      if (!enemy.alive) {
        continue;
      }
      nearest_enemy_sq = min(nearest_enemy_sq, distance_sq(static_cast<float>(x), static_cast<float>(y), enemy.x, enemy.y));
    }
    const float score = nearest_enemy_sq + terrain * 3.0f;
    if (score > best_score) {
      best_score = score;
      best_x = static_cast<float>(x) + 0.5f;
      best_y = static_cast<float>(y) + 0.5f;
    }
  }

  g_player.x = best_x;
  g_player.y = best_y;

  float best_dist = -1.0f;
  float target_x = best_x;
  float target_y = best_y;
  for (const auto& enemy : g_enemies) {
    if (!enemy.alive) {
      continue;
    }
    const float dsq = distance_sq(best_x, best_y, enemy.x, enemy.y);
    if (dsq > best_dist) {
      best_dist = dsq;
      target_x = enemy.x;
      target_y = enemy.y;
    }
  }
  g_player.yaw_deg = atan2f(shortest_delta(best_y, target_y), shortest_delta(best_x, target_x)) * RAD_TO_DEG;
}

void start_wave(uint32_t seed) {
  clear_enemies();
  place_enemy_pods(seed, active_difficulty().pod_count);
  g_destroyed_pods = 0;
}

void reset_round() {
  g_seed = esp_random() ^ millis();
  generate_world(g_seed);
  start_wave(g_seed);
  place_player_spawn(g_seed);
  g_player.pitch_offset = -4.0f;
  g_player.health = app_config::MAX_HEALTH;
  g_player.altitude = sample_height(g_player.x, g_player.y) + app_config::FLIGHT_HEIGHT;
  g_total_kills = 0;
  g_last_shot_ms = 0;
  g_last_damage_ms = 0;
  g_last_muzzle_flash_ms = 0;
  g_last_strafe_ms = 0;
  g_round_start_ms = millis();
  g_total_score = 0;
  g_wave_index = 1;
  g_mode = GameMode::Playing;
}

float horizon_y() {
  return clampf(static_cast<float>(app_config::SCREEN_H) * 0.45f + g_player.pitch_offset, 26.0f, 92.0f);
}

bool line_of_sight_to(float target_x, float target_y, float target_height) {
  const float dx = shortest_delta(g_player.x, target_x);
  const float dy = shortest_delta(g_player.y, target_y);
  const float dist = sqrtf(dx * dx + dy * dy);
  if (dist < 1.0f) {
    return true;
  }

  const int steps = max(4, static_cast<int>(dist * 1.4f));
  const float eye_height = g_player.altitude + app_config::CAMERA_EYE_OFFSET;
  for (int i = 1; i < steps; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(steps);
    const float sx = wrap_position(g_player.x + dx * t);
    const float sy = wrap_position(g_player.y + dy * t);
    const float sz = eye_height + (target_height - eye_height) * t;
    if (sample_height(sx, sy) + 1.5f > sz) {
      return false;
    }
  }
  return true;
}

void update_enemy_visibility() {
  const float yaw_rad = g_player.yaw_deg * DEG_TO_RAD;
  const float sin_yaw = sinf(yaw_rad);
  const float cos_yaw = cosf(yaw_rad);
  const float focal = (static_cast<float>(app_config::SCREEN_W) * 0.5f) /
                      tanf(app_config::FOV_DEG * DEG_TO_RAD * 0.5f);
  const float horizon = horizon_y();

  for (auto& enemy : g_enemies) {
    enemy.visible = false;
    enemy.screen_radius = 0.0f;
    if (!enemy.alive) {
      enemy.charging = false;
      continue;
    }

    const float dx = shortest_delta(g_player.x, enemy.x);
    const float dy = shortest_delta(g_player.y, enemy.y);
    const float forward = dx * cos_yaw + dy * sin_yaw;
    const float side = -dx * sin_yaw + dy * cos_yaw;
    if (forward <= 1.0f || forward > app_config::FAR_DISTANCE) {
      enemy.charging = false;
      continue;
    }

    const float target_height = sample_height(enemy.x, enemy.y) + app_config::POD_HEIGHT;
    if (!line_of_sight_to(enemy.x, enemy.y, target_height)) {
      enemy.charging = false;
      continue;
    }

    enemy.distance = sqrtf(dx * dx + dy * dy);
    enemy.screen_x = static_cast<float>(app_config::SCREEN_W) * 0.5f + side * focal / forward;
    enemy.screen_y = horizon + (g_player.altitude - target_height) * focal / forward;
    enemy.screen_radius = clampf((app_config::POD_RADIUS * focal) / forward, 4.0f, 18.0f);
    enemy.visible = enemy.screen_x >= -18.0f && enemy.screen_x <= app_config::SCREEN_W + 18.0f &&
                    enemy.screen_y >= -18.0f && enemy.screen_y <= app_config::SCREEN_H + 18.0f;
    if (!enemy.visible) {
      enemy.charging = false;
    }
  }
}

void update_enemy_attacks(uint32_t now_ms) {
  const DifficultyConfig& config = active_difficulty();
  for (auto& enemy : g_enemies) {
    if (!enemy.alive || !enemy.visible) {
      continue;
    }

    if (!enemy.charging) {
      if (now_ms - enemy.last_attack_ms >= config.enemy_interval_ms) {
        enemy.charging = true;
        enemy.charge_start_ms = now_ms;
      }
      continue;
    }

    if (now_ms - enemy.charge_start_ms < config.enemy_charge_ms) {
      continue;
    }

    enemy.charging = false;
    enemy.last_attack_ms = now_ms;
    if (now_ms - g_last_strafe_ms <= app_config::DODGE_WINDOW_MS) {
      continue;
    }

    g_last_damage_ms = now_ms;
    if (endless_mode()) {
      continue;
    }

    if (g_player.health > config.damage) {
      g_player.health -= config.damage;
    } else {
      g_player.health = 0;
    }
    if (g_player.health == 0) {
      g_mode = GameMode::GameOver;
    }
  }
}

void attempt_shot(uint32_t now_ms) {
  if (!endless_mode() && now_ms - g_last_shot_ms < app_config::SHOT_COOLDOWN_MS) {
    return;
  }
  g_last_shot_ms = now_ms;
  g_last_muzzle_flash_ms = now_ms;

  int best_index = -1;
  float best_score = 1e9f;
  const float center_x = static_cast<float>(app_config::SCREEN_W) * 0.5f;
  const float center_y = static_cast<float>(app_config::SCREEN_H) * 0.5f;

  for (uint8_t i = 0; i < active_difficulty().pod_count; ++i) {
    auto& enemy = g_enemies[i];
    if (!enemy.alive || !enemy.visible) {
      continue;
    }
    const float dx = enemy.screen_x - center_x;
    const float dy = enemy.screen_y - center_y;
    const float radius = enemy.screen_radius + app_config::SHOT_MAX_AIM_PX;
    const float dist_sq = dx * dx + dy * dy;
    if (dist_sq <= radius * radius) {
      const float score = dist_sq + enemy.distance * 18.0f;
      if (score < best_score) {
        best_score = score;
        best_index = i;
      }
    }
  }

  if (best_index < 0) {
    return;
  }

  auto& enemy = g_enemies[best_index];
  if (enemy.hp > 1) {
    --enemy.hp;
    enemy.last_attack_ms = now_ms;
    enemy.charging = false;
    return;
  }

  enemy.hp = 0;
  enemy.alive = false;
  enemy.visible = false;
  enemy.charging = false;
  ++g_destroyed_pods;
  ++g_total_kills;

  if (g_destroyed_pods >= active_difficulty().pod_count) {
    if (endless_mode()) {
      ++g_wave_index;
      start_wave(g_seed ^ now_ms ^ (static_cast<uint32_t>(g_wave_index) << 8));
      update_enemy_visibility();
      return;
    }
    const uint32_t elapsed_sec = (now_ms - g_round_start_ms) / 1000U;
    const uint16_t time_bonus = static_cast<uint16_t>(max(0, 900 - static_cast<int>(elapsed_sec) * 10));
    g_total_score = active_difficulty().score_bonus + time_bonus + g_player.health * 120;
    g_mode = GameMode::Victory;
  }
}

void update_playing(const InputState& input, float dt, uint32_t now_ms) {
  const DifficultyConfig& config = active_difficulty();
  if (input.turn_left) {
    g_player.yaw_deg -= app_config::TURN_SPEED_DEG * dt;
  }
  if (input.turn_right) {
    g_player.yaw_deg += app_config::TURN_SPEED_DEG * dt;
  }
  if (input.pitch_up) {
    g_player.pitch_offset -= app_config::PITCH_SPEED * dt;
  }
  if (input.pitch_down) {
    g_player.pitch_offset += app_config::PITCH_SPEED * dt;
  }
  g_player.pitch_offset = clampf(
      g_player.pitch_offset, app_config::MIN_PITCH_OFFSET, app_config::MAX_PITCH_OFFSET);

  const float yaw_rad = g_player.yaw_deg * DEG_TO_RAD;
  const float cos_yaw = cosf(yaw_rad);
  const float sin_yaw = sinf(yaw_rad);

  if (input.thrust) {
    g_player.x = wrap_position(g_player.x + cos_yaw * config.thrust_speed * dt);
    g_player.y = wrap_position(g_player.y + sin_yaw * config.thrust_speed * dt);
  }
  if (input.strafe_left) {
    g_player.x = wrap_position(g_player.x + sin_yaw * app_config::STRAFE_SPEED * dt);
    g_player.y = wrap_position(g_player.y - cos_yaw * app_config::STRAFE_SPEED * dt);
    g_last_strafe_ms = now_ms;
  }
  if (input.strafe_right) {
    g_player.x = wrap_position(g_player.x - sin_yaw * app_config::STRAFE_SPEED * dt);
    g_player.y = wrap_position(g_player.y + cos_yaw * app_config::STRAFE_SPEED * dt);
    g_last_strafe_ms = now_ms;
  }

  g_player.altitude = sample_height(g_player.x, g_player.y) + app_config::FLIGHT_HEIGHT;
  update_enemy_visibility();
  if ((endless_mode() && input.shoot) || input_pressed(input.shoot, g_prev_input.shoot)) {
    attempt_shot(now_ms);
  }
  update_enemy_attacks(now_ms);

  if (input_pressed(input.restart, g_prev_input.restart)) {
    reset_round();
  }
}

void update_title(const InputState& input) {
  if (input_pressed(input.difficulty_left, g_prev_input.difficulty_left)) {
    const int value = static_cast<int>(g_selected_difficulty);
    g_selected_difficulty = static_cast<Difficulty>((value + static_cast<int>(Difficulty::Count) - 1) %
                                                    static_cast<int>(Difficulty::Count));
  }
  if (input_pressed(input.difficulty_right, g_prev_input.difficulty_right)) {
    const int value = static_cast<int>(g_selected_difficulty);
    g_selected_difficulty =
        static_cast<Difficulty>((value + 1) % static_cast<int>(Difficulty::Count));
  }
  if (input_pressed(input.style_toggle, g_prev_input.style_toggle)) {
    g_selected_style =
        g_selected_style == PlayStyle::Mission ? PlayStyle::Endless : PlayStyle::Mission;
  }
  if (input_pressed(input.start, g_prev_input.start)) {
    reset_round();
  }
}

void update_end_screen(const InputState& input) {
  if (input_pressed(input.start, g_prev_input.start) || input_pressed(input.restart, g_prev_input.restart)) {
    g_mode = GameMode::Title;
  }
}

void draw_sky() {
  for (int i = 0; i < app_config::SKY_BANDS; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(app_config::SKY_BANDS - 1);
    const uint16_t color = rgb565(
        static_cast<uint8_t>(28 + 20 * (1.0f - t)),
        static_cast<uint8_t>(96 + 70 * (1.0f - t)),
        static_cast<uint8_t>(132 + 80 * (1.0f - t)));
    const int16_t y0 = (i * app_config::SCREEN_H) / app_config::SKY_BANDS;
    const int16_t y1 = ((i + 1) * app_config::SCREEN_H) / app_config::SKY_BANDS;
    g_canvas.fillRect(0, y0, app_config::SCREEN_W, y1 - y0, color);
  }
}

void render_terrain() {
  draw_sky();

  const float horizon = horizon_y();
  const float half_fov = app_config::FOV_DEG * DEG_TO_RAD * 0.5f;
  const float vertical_focal = (static_cast<float>(app_config::SCREEN_H) * 0.66f) / tanf(half_fov);

  for (int col = 0; col < app_config::RENDER_COLUMNS; ++col) {
    const float u = ((static_cast<float>(col) + 0.5f) / static_cast<float>(app_config::RENDER_COLUMNS) - 0.5f) *
                    2.0f * tanf(half_fov);
    const float ray_angle = g_player.yaw_deg * DEG_TO_RAD + atanf(u);
    const float dir_x = cosf(ray_angle);
    const float dir_y = sinf(ray_angle);
    int max_top = app_config::SCREEN_H;

    for (float dist = app_config::NEAR_DISTANCE; dist < app_config::FAR_DISTANCE && max_top > 0;
         dist += max(0.9f, dist * 0.045f)) {
      const float sample_x = g_player.x + dir_x * dist;
      const float sample_y = g_player.y + dir_y * dist;
      const float terrain_h = sample_height(sample_x, sample_y);
      const int projected_top =
          clampi(static_cast<int>(horizon + (g_player.altitude - terrain_h) * vertical_focal / dist),
                 -20,
                 app_config::SCREEN_H);
      if (projected_top >= max_top) {
        continue;
      }

      const uint16_t base = sample_color(sample_x, sample_y);
      const float fog = 1.0f - dist / app_config::FAR_DISTANCE * 0.72f;
      const uint16_t shaded = shade_color(base, fog);
      const int draw_y = max(0, projected_top);
      const int draw_h = max_top - draw_y;
      if (draw_h > 0) {
        g_canvas.fillRect(col * app_config::COLUMN_W, draw_y, app_config::COLUMN_W, draw_h, shaded);
      }
      max_top = projected_top;
    }
  }
}

void draw_pod(const EnemyPod& enemy, uint32_t now_ms) {
  if (!enemy.alive || !enemy.visible) {
    return;
  }

  const int radius = static_cast<int>(enemy.screen_radius);
  const int cx = static_cast<int>(enemy.screen_x);
  const int cy = static_cast<int>(enemy.screen_y);
  const uint16_t body_color = enemy.hp == 1 ? rgb565(240, 110, 80) : rgb565(212, 74, 58);
  const uint16_t glow_color = enemy.charging ? rgb565(255, 210, 96) : rgb565(160, 206, 255);
  const int body_h = radius * 2 + 4;
  const int body_w = radius + 4;

  if (enemy.charging) {
    const float charge_t = clampf(
        static_cast<float>(now_ms - enemy.charge_start_ms) /
            static_cast<float>(active_difficulty().enemy_charge_ms),
        0.0f,
        1.0f);
    g_canvas.drawCircle(cx, cy - radius - 2, radius + 2 + static_cast<int>(charge_t * 3.0f), glow_color);
    g_canvas.drawCircle(cx, cy - radius - 2, radius + 5 + static_cast<int>(charge_t * 3.0f), glow_color);
  }

  g_canvas.fillRect(cx - body_w / 2, cy - body_h, body_w, body_h, body_color);
  g_canvas.drawRect(cx - body_w / 2, cy - body_h, body_w, body_h, TFT_BLACK);
  g_canvas.fillCircle(cx, cy - body_h, radius, body_color);
  g_canvas.drawCircle(cx, cy - body_h, radius, TFT_BLACK);
  g_canvas.fillCircle(cx, cy - body_h, max(1, radius / 3), glow_color);
}

void draw_pods(uint32_t now_ms) {
  const uint8_t count = active_difficulty().pod_count;
  for (uint8_t i = 0; i < count; ++i) {
    draw_pod(g_enemies[i], now_ms);
  }
}

void draw_reticle(uint32_t now_ms) {
  const int cx = app_config::SCREEN_W / 2;
  const int cy = app_config::SCREEN_H / 2;
  const uint16_t color = (now_ms - g_last_muzzle_flash_ms <= app_config::MUZZLE_FLASH_MS) ? TFT_ORANGE : TFT_WHITE;
  g_canvas.drawFastHLine(cx - 9, cy, 6, color);
  g_canvas.drawFastHLine(cx + 4, cy, 6, color);
  g_canvas.drawFastVLine(cx, cy - 9, 6, color);
  g_canvas.drawFastVLine(cx, cy + 4, 6, color);
  g_canvas.drawCircle(cx, cy, 10, shade_color(color, 0.7f));
}

void draw_hud(uint32_t now_ms) {
  char line[64];
  g_canvas.fillRect(0, 0, app_config::SCREEN_W, 14, TFT_BLACK);
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);
  g_canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  if (endless_mode()) {
    snprintf(line, sizeof(line), "%s ENDLESS  W%u  K%u",
             active_difficulty().name,
             static_cast<unsigned>(g_wave_index),
             static_cast<unsigned>(g_total_kills));
  } else {
    snprintf(line, sizeof(line), "%s  POD %u/%u  HP %u",
             active_difficulty().name,
             static_cast<unsigned>(g_destroyed_pods),
             static_cast<unsigned>(active_difficulty().pod_count),
             static_cast<unsigned>(g_player.health));
  }
  g_canvas.setCursor(4, 3);
  g_canvas.print(line);

  const uint32_t elapsed_sec = (now_ms - g_round_start_ms) / 1000U;
  snprintf(line, sizeof(line), "%lus  YAW %03d",
           static_cast<unsigned long>(elapsed_sec),
           static_cast<int>(fmodf(g_player.yaw_deg + 3600.0f, 360.0f)));
  g_canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  g_canvas.setCursor(146, 3);
  g_canvas.print(line);

  if (now_ms - g_last_damage_ms <= app_config::HIT_FLASH_MS) {
    g_canvas.drawRect(0, 0, app_config::SCREEN_W, app_config::SCREEN_H, TFT_RED);
    g_canvas.drawRect(1, 1, app_config::SCREEN_W - 2, app_config::SCREEN_H - 2, TFT_RED);
  }
}

void draw_help_overlay() {
  if (!g_show_help || g_mode != GameMode::Playing) {
    return;
  }

  g_canvas.fillRoundRect(8, 80, 224, 46, 4, TFT_BLACK);
  g_canvas.drawRoundRect(8, 80, 224, 46, 4, TFT_DARKGREY);
  g_canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  g_canvas.setCursor(14, 86);
  g_canvas.print("A thrust  S fire  Q/E strafe");
  g_canvas.setCursor(14, 96);
  g_canvas.print(";/., aim/turn  C title mode");
  g_canvas.setCursor(14, 106);
  g_canvas.print("R restart  H help  Z/X diff");
  g_canvas.setCursor(14, 116);
  g_canvas.print(endless_mode() ? "ENDLESS: no game over / no shot cap"
                                : "Strafe during charge to dodge");
}

void draw_title_screen() {
  render_terrain();
  g_canvas.fillRoundRect(20, 14, 200, 100, 6, TFT_BLACK);
  g_canvas.drawRoundRect(20, 14, 200, 100, 6, TFT_ORANGE);

  g_canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(2);
  g_canvas.setCursor(34, 26);
  g_canvas.print("VOXELBURG");
  g_canvas.setTextSize(1);
  g_canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  g_canvas.setCursor(38, 52);
  g_canvas.print("Cardputer-ADV clean-room port");
  g_canvas.setCursor(38, 64);
  g_canvas.print(g_selected_style == PlayStyle::Mission ? "Destroy all defense pods."
                                                        : "Free-flight with endless combat.");
  g_canvas.setCursor(38, 74);
  g_canvas.print(g_selected_style == PlayStyle::Mission ? "Dodge charged return fire."
                                                        : "No game over, no shot cooldown.");
  g_canvas.setCursor(38, 88);
  g_canvas.print("Z/X difficulty: ");
  g_canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  g_canvas.print(active_difficulty().name);
  g_canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  g_canvas.setCursor(38, 98);
  g_canvas.print("C mode: ");
  g_canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  g_canvas.print(g_selected_style == PlayStyle::Mission ? "MISSION" : "ENDLESS");
  g_canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  g_canvas.setCursor(38, 108);
  g_canvas.print("Enter to launch");
  g_canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  g_canvas.setCursor(18, 118);
  g_canvas.print("Controls: A thrust / S fire / ;. ,/ aim");
}

void draw_result_overlay(const char* title, uint16_t accent) {
  g_canvas.fillRoundRect(30, 24, 180, 82, 6, TFT_BLACK);
  g_canvas.drawRoundRect(30, 24, 180, 82, 6, accent);

  const uint32_t elapsed_sec = (millis() - g_round_start_ms) / 1000U;
  char line[48];

  g_canvas.setTextFont(1);
  g_canvas.setTextSize(2);
  g_canvas.setTextColor(accent, TFT_BLACK);
  g_canvas.setCursor(56, 34);
  g_canvas.print(title);

  g_canvas.setTextSize(1);
  g_canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  snprintf(line, sizeof(line), "Pods cleared: %u/%u",
           static_cast<unsigned>(g_destroyed_pods),
           static_cast<unsigned>(active_difficulty().pod_count));
  g_canvas.setCursor(48, 60);
  g_canvas.print(line);
  snprintf(line, sizeof(line), "Time: %lus  HP: %u",
           static_cast<unsigned long>(elapsed_sec),
           static_cast<unsigned>(g_player.health));
  g_canvas.setCursor(48, 72);
  g_canvas.print(line);
  snprintf(line, sizeof(line), "Score: %u", static_cast<unsigned>(g_total_score));
  g_canvas.setCursor(48, 84);
  g_canvas.print(line);
  g_canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  g_canvas.setCursor(48, 96);
  g_canvas.print("Enter or R to return");
}

void draw_frame() {
  render_terrain();
  const uint32_t now_ms = millis();
  draw_pods(now_ms);
  draw_reticle(now_ms);
  draw_hud(now_ms);
  draw_help_overlay();

  if (g_mode == GameMode::Victory) {
    draw_result_overlay("MISSION CLEAR", TFT_GREENYELLOW);
  } else if (g_mode == GameMode::GameOver) {
    draw_result_overlay("SHOT DOWN", TFT_RED);
  }
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(TFT_BLACK);

  g_canvas.setColorDepth(16);
  g_canvas.createSprite(app_config::SCREEN_W, app_config::SCREEN_H);
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);

  g_seed = esp_random();
  generate_world(g_seed);
  start_wave(g_seed);
  place_player_spawn(g_seed);
  g_player.altitude = sample_height(g_player.x, g_player.y) + app_config::FLIGHT_HEIGHT;
  g_last_frame_ms = millis();
}

void loop() {
  M5Cardputer.update();

  const uint32_t now_ms = millis();
  if (now_ms - g_last_frame_ms < app_config::FRAME_INTERVAL_MS) {
    return;
  }
  const float dt = min(0.05f, static_cast<float>(now_ms - g_last_frame_ms) / 1000.0f);
  g_last_frame_ms = now_ms;

  const InputState input = read_input();
  if (input_pressed(input.help, g_prev_input.help)) {
    g_show_help = !g_show_help;
  }

  switch (g_mode) {
    case GameMode::Title:
      update_title(input);
      draw_title_screen();
      break;
    case GameMode::Playing:
      update_playing(input, dt, now_ms);
      draw_frame();
      break;
    case GameMode::Victory:
    case GameMode::GameOver:
      update_end_screen(input);
      draw_frame();
      break;
  }

  g_canvas.pushSprite(0, 0);
  g_prev_input = input;
}
