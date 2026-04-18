#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>
#include <string.h>

namespace app_config {
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;
constexpr int HUD_H = 14;
constexpr int FOOTER_H = 13;
constexpr int PLAY_Y = HUD_H;
constexpr int PLAY_H = SCREEN_H - HUD_H - FOOTER_H;
constexpr int TILE_SIZE = 12;
constexpr int ROOM_COLS = SCREEN_W / TILE_SIZE;
constexpr int ROOM_ROWS = PLAY_H / TILE_SIZE;
constexpr uint32_t FRAME_INTERVAL_MS = 33;

constexpr float PLAYER_RADIUS = 4.5f;
constexpr float ENEMY_RADIUS = 5.0f;
constexpr float PROJECTILE_RADIUS = 2.6f;
constexpr float PLAYER_SPEED = 56.0f;
constexpr float DODGE_SPEED = 158.0f;
constexpr float PLAYER_SHOT_SPEED = 140.0f;
constexpr float ENEMY_SHOT_SPEED = 74.0f;
constexpr float DRONE_SPEED = 24.0f;
constexpr float DASHER_SPEED = 32.0f;
constexpr float DASHER_BURST_SPEED = 122.0f;
constexpr uint16_t PLAYER_SHOT_INTERVAL_MS = 150;
constexpr uint16_t PLAYER_INVULN_MS = 680;
constexpr uint16_t PLAYER_DODGE_MS = 170;
constexpr uint16_t PLAYER_DODGE_COOLDOWN_MS = 950;
constexpr uint16_t MESSAGE_MS = 1800;
constexpr int MAX_ENEMIES = 8;
constexpr int MAX_PROJECTILES = 22;
constexpr int MAX_PARTICLES = 40;

constexpr uint8_t HID_A = 0x04;
constexpr uint8_t HID_D = 0x07;
constexpr uint8_t HID_I = 0x0C;
constexpr uint8_t HID_J = 0x0D;
constexpr uint8_t HID_K = 0x0E;
constexpr uint8_t HID_L = 0x0F;
constexpr uint8_t HID_S = 0x16;
constexpr uint8_t HID_U = 0x18;
constexpr uint8_t HID_W = 0x1A;
constexpr uint8_t HID_ENTER = 0x28;

constexpr uint16_t COLOR_BG = 0x08C3;
constexpr uint16_t COLOR_PANEL = 0x18E4;
constexpr uint16_t COLOR_FLOOR_A = 0x18C3;
constexpr uint16_t COLOR_FLOOR_B = 0x1102;
constexpr uint16_t COLOR_WALL = 0x51C8;
constexpr uint16_t COLOR_WALL_HI = 0x72CF;
constexpr uint16_t COLOR_BARRIER = 0x2E7F;
constexpr uint16_t COLOR_PLAYER = 0xB69F;
constexpr uint16_t COLOR_PLAYER_ALT = 0xFD20;
constexpr uint16_t COLOR_SHADOW = 0x0000;
constexpr uint16_t COLOR_ENEMY_DRONE = 0xE1A8;
constexpr uint16_t COLOR_ENEMY_DASHER = 0xFCC8;
constexpr uint16_t COLOR_ENEMY_CORE = 0xFFFF;
constexpr uint16_t COLOR_BULLET_PLAYER = 0x87FF;
constexpr uint16_t COLOR_BULLET_ENEMY = 0xFD20;
constexpr uint16_t COLOR_PICKUP = 0xFFE0;
constexpr uint16_t COLOR_TRANSPORT = 0x57FF;
constexpr uint16_t COLOR_TRANSPORT_CORE = 0x9FFF;
constexpr uint16_t COLOR_TEXT = 0xFFFF;
constexpr uint16_t COLOR_MUTED = 0xAD55;
constexpr uint16_t COLOR_HURT = 0xF800;
constexpr uint16_t COLOR_GOOD = 0x87F0;
}  // namespace app_config

enum class Scene : uint8_t {
  Title,
  Playing,
  GameOver,
};

enum class EnemyKind : uint8_t {
  Drone,
  Dasher,
};

enum class Direction : uint8_t {
  North,
  South,
  West,
  East,
};

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct InputState {
  bool move_up = false;
  bool move_down = false;
  bool move_left = false;
  bool move_right = false;
  bool aim_up = false;
  bool aim_down = false;
  bool aim_left = false;
  bool aim_right = false;
  bool action_pressed = false;
  bool confirm_pressed = false;
};

struct RoomState {
  bool visited = false;
  bool cleared = false;
  bool key_room = false;
  bool exit_room = false;
  bool key_taken = false;
  uint8_t pattern = 0;
  uint8_t enemy_budget = 0;
  uint8_t seed = 0;
};

struct EnemyState {
  bool active = false;
  EnemyKind kind = EnemyKind::Drone;
  float x = 0.0f;
  float y = 0.0f;
  float vx = 0.0f;
  float vy = 0.0f;
  float dash_x = 0.0f;
  float dash_y = 0.0f;
  uint8_t hp = 0;
  uint16_t action_timer = 0;
  uint16_t attack_timer = 0;
  uint16_t flash_timer = 0;
};

struct ProjectileState {
  bool active = false;
  bool friendly = false;
  float x = 0.0f;
  float y = 0.0f;
  float vx = 0.0f;
  float vy = 0.0f;
  uint16_t life_ms = 0;
  uint8_t damage = 0;
};

struct ParticleState {
  bool active = false;
  float x = 0.0f;
  float y = 0.0f;
  float vx = 0.0f;
  float vy = 0.0f;
  uint16_t life_ms = 0;
  uint16_t color = 0;
};

struct PlayerState {
  float x = 0.0f;
  float y = 0.0f;
  float facing_x = 0.0f;
  float facing_y = -1.0f;
  float dodge_x = 0.0f;
  float dodge_y = -1.0f;
  uint8_t hp = 5;
  uint8_t max_hp = 5;
  uint16_t fire_timer = 0;
  uint16_t invuln_timer = 0;
  uint16_t dodge_timer = 0;
  uint16_t dodge_cooldown = 0;
  bool has_key = false;
};

bool contains_hid_key(const Keyboard_Class::KeysState& status, uint8_t hid_key) {
  for (const auto raw_hid_key : status.hid_keys) {
    if ((raw_hid_key & ~SHIFT) == hid_key) {
      return true;
    }
  }
  return false;
}

float clampf(const float value, const float min_value, const float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

float length_sq(const Vec2& value) {
  return value.x * value.x + value.y * value.y;
}

Vec2 normalize(const Vec2& value) {
  const float len_sq = length_sq(value);
  if (len_sq < 0.0001f) {
    return Vec2{};
  }
  const float inv_len = 1.0f / sqrtf(len_sq);
  return Vec2{value.x * inv_len, value.y * inv_len};
}

float distance_sq(float ax, float ay, float bx, float by) {
  const float dx = ax - bx;
  const float dy = ay - by;
  return dx * dx + dy * dy;
}

uint32_t mix_seed(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7FEB352DU;
  value ^= value >> 15;
  value *= 0x846CA68BU;
  value ^= value >> 16;
  return value;
}

uint32_t next_rng(uint32_t& state) {
  state = state * 1664525UL + 1013904223UL;
  return state;
}

class BlindJumpCardputerDemo {
 public:
  void begin() {
    scene_ = Scene::Title;
    high_floor_ = 1;
    message_[0] = '\0';
    message_timer_ = 0;
  }

  void update(const InputState& input, uint32_t delta_ms) {
    if (delta_ms > 50) {
      delta_ms = 50;
    }

    pulse_clock_ms_ += delta_ms;
    if (message_timer_ > delta_ms) {
      message_timer_ -= delta_ms;
    } else {
      message_timer_ = 0;
    }

    if (scene_ == Scene::Title) {
      if (input.confirm_pressed) {
        start_new_run();
      }
      return;
    }

    if (scene_ == Scene::GameOver) {
      if (input.confirm_pressed) {
        scene_ = Scene::Title;
        set_message("Press Enter or BtnA.");
      }
      return;
    }

    update_playing(input, delta_ms);
  }

  void draw(M5Canvas& canvas, const InputState& input) const {
    canvas.fillScreen(app_config::COLOR_BG);

    if (scene_ == Scene::Title) {
      draw_title(canvas);
      return;
    }

    draw_playfield(canvas);
    draw_room(canvas);
    draw_pickups(canvas);
    draw_particles(canvas);
    draw_projectiles(canvas, false);
    draw_enemies(canvas);
    draw_projectiles(canvas, true);
    draw_player(canvas);
    draw_minimap(canvas);
    draw_hud(canvas);
    draw_footer(canvas, input);

    if (scene_ == Scene::GameOver) {
      draw_game_over(canvas);
    }
  }

 private:
  void start_new_run() {
    floor_index_ = 1;
    score_ = 0;
    rooms_cleared_total_ = 0;
    player_ = PlayerState{};
    player_.x = app_config::SCREEN_W * 0.5f;
    player_.y = app_config::PLAY_Y + app_config::PLAY_H * 0.5f;
    rng_seed_ = mix_seed(static_cast<uint32_t>(micros()) ^ static_cast<uint32_t>(millis()));
    setup_floor(true);
    scene_ = Scene::Playing;
    set_message("Find the key.");
  }

  void setup_floor(bool reset_health) {
    if (reset_health) {
      player_.hp = player_.max_hp;
    } else if (player_.hp < player_.max_hp) {
      ++player_.hp;
    }

    player_.has_key = false;
    player_.fire_timer = 0;
    player_.invuln_timer = 0;
    player_.dodge_timer = 0;
    player_.dodge_cooldown = 0;
    player_.facing_x = 0.0f;
    player_.facing_y = -1.0f;
    current_room_x_ = 1;
    current_room_y_ = 1;
    clear_dynamic_entities();

    uint8_t key_index = 0;
    uint8_t exit_index = 0;
    do {
      key_index = static_cast<uint8_t>(next_rng(rng_seed_) % 9U);
    } while (key_index == 4);
    do {
      exit_index = static_cast<uint8_t>(next_rng(rng_seed_) % 9U);
    } while (exit_index == 4 || exit_index == key_index);

    for (int y = 0; y < 3; ++y) {
      for (int x = 0; x < 3; ++x) {
        RoomState& room = rooms_[y][x];
        room = RoomState{};
        const uint8_t index = static_cast<uint8_t>(y * 3 + x);
        const uint32_t room_seed = mix_seed((floor_index_ * 97U) + (index * 43U) + rng_seed_);
        room.seed = static_cast<uint8_t>(room_seed & 0xFFU);
        room.pattern = static_cast<uint8_t>((room_seed >> 8) % 7U);
        room.key_room = index == key_index;
        room.exit_room = index == exit_index;
        if (x == 1 && y == 1) {
          room.enemy_budget = 0;
          room.cleared = true;
        } else {
          uint8_t budget = static_cast<uint8_t>(1 + ((room_seed >> 12) % 3U));
          if (((room_seed >> 16) % 5U) == 0U) {
            budget = 0;
          }
          if (room.key_room && budget < 2) {
            budget = 2;
          }
          if (room.exit_room && budget < static_cast<uint8_t>(2 + floor_index_ / 2)) {
            budget = static_cast<uint8_t>(2 + floor_index_ / 2);
          }
          if (floor_index_ >= 3 && budget < 2 && ((room_seed >> 20) & 1U) != 0U) {
            budget = 2;
          }
          if (budget > 4) {
            budget = 4;
          }
          room.enemy_budget = budget;
          room.cleared = budget == 0;
        }
      }
    }

    rooms_[1][1].visited = true;
    place_player_room_center();
    enter_current_room();
  }

  RoomState& current_room() {
    return rooms_[current_room_y_][current_room_x_];
  }

  const RoomState& current_room() const {
    return rooms_[current_room_y_][current_room_x_];
  }

  void clear_dynamic_entities() {
    for (auto& enemy : enemies_) {
      enemy = EnemyState{};
    }
    for (auto& projectile : projectiles_) {
      projectile = ProjectileState{};
    }
    for (auto& particle : particles_) {
      particle = ParticleState{};
    }
  }

  void place_player_room_center() {
    player_.x = app_config::SCREEN_W * 0.5f;
    player_.y = app_config::PLAY_Y + app_config::PLAY_H * 0.5f;
  }

  bool room_has_live_enemies() const {
    for (const auto& enemy : enemies_) {
      if (enemy.active) {
        return true;
      }
    }
    return false;
  }

  void enter_current_room() {
    clear_dynamic_entities();
    RoomState& room = current_room();
    room.visited = true;
    if (!room.cleared) {
      spawn_room_enemies(room);
    }
  }

  void spawn_room_enemies(const RoomState& room) {
    uint32_t seed = mix_seed((floor_index_ * 131U) + (current_room_x_ * 23U) + (current_room_y_ * 61U) +
                             static_cast<uint32_t>(room.seed));
    for (uint8_t i = 0; i < room.enemy_budget; ++i) {
      Vec2 spawn = random_spawn_point(seed);
      EnemyKind kind = EnemyKind::Drone;
      if (floor_index_ >= 2 && ((next_rng(seed) >> 28) & 1U) != 0U) {
        kind = EnemyKind::Dasher;
      }
      if (room.exit_room && i == room.enemy_budget - 1 && floor_index_ >= 2) {
        kind = EnemyKind::Dasher;
      }
      spawn_enemy(kind, spawn.x, spawn.y);
    }
  }

  Vec2 random_spawn_point(uint32_t& seed) const {
    for (int attempt = 0; attempt < 40; ++attempt) {
      const int col = 2 + static_cast<int>(next_rng(seed) % static_cast<uint32_t>(app_config::ROOM_COLS - 4));
      const int row = 2 + static_cast<int>(next_rng(seed) % static_cast<uint32_t>(app_config::ROOM_ROWS - 4));
      const float x = col * app_config::TILE_SIZE + app_config::TILE_SIZE * 0.5f;
      const float y = app_config::PLAY_Y + row * app_config::TILE_SIZE + app_config::TILE_SIZE * 0.5f;
      if (!is_solid_circle(x, y, app_config::ENEMY_RADIUS) &&
          distance_sq(x, y, app_config::SCREEN_W * 0.5f, app_config::PLAY_Y + app_config::PLAY_H * 0.5f) > 1400.0f) {
        return Vec2{x, y};
      }
    }
    return Vec2{app_config::SCREEN_W * 0.5f, app_config::PLAY_Y + 24.0f};
  }

  void spawn_enemy(EnemyKind kind, float x, float y) {
    for (auto& enemy : enemies_) {
      if (!enemy.active) {
        enemy.active = true;
        enemy.kind = kind;
        enemy.x = x;
        enemy.y = y;
        enemy.vx = 0.0f;
        enemy.vy = 0.0f;
        enemy.dash_x = 0.0f;
        enemy.dash_y = 0.0f;
        enemy.hp = kind == EnemyKind::Drone ? 2 : 3;
        enemy.action_timer = kind == EnemyKind::Drone ? 0 : 500;
        enemy.attack_timer = 400 + static_cast<uint16_t>(random(0, 400));
        enemy.flash_timer = 0;
        return;
      }
    }
  }

  void update_playing(const InputState& input, uint32_t delta_ms) {
    if (player_.fire_timer > delta_ms) {
      player_.fire_timer -= delta_ms;
    } else {
      player_.fire_timer = 0;
    }
    if (player_.invuln_timer > delta_ms) {
      player_.invuln_timer -= delta_ms;
    } else {
      player_.invuln_timer = 0;
    }
    if (player_.dodge_timer > delta_ms) {
      player_.dodge_timer -= delta_ms;
    } else {
      player_.dodge_timer = 0;
    }
    if (player_.dodge_cooldown > delta_ms) {
      player_.dodge_cooldown -= delta_ms;
    } else {
      player_.dodge_cooldown = 0;
    }

    update_player(input, delta_ms);
    update_projectiles(delta_ms);
    update_enemies(delta_ms);
    update_particles(delta_ms);
    update_room_state(input);
  }

  void update_player(const InputState& input, uint32_t delta_ms) {
    Vec2 move_dir{};
    if (input.move_left) {
      move_dir.x -= 1.0f;
    }
    if (input.move_right) {
      move_dir.x += 1.0f;
    }
    if (input.move_up) {
      move_dir.y -= 1.0f;
    }
    if (input.move_down) {
      move_dir.y += 1.0f;
    }
    move_dir = normalize(move_dir);

    Vec2 aim_dir{};
    if (input.aim_left) {
      aim_dir.x -= 1.0f;
    }
    if (input.aim_right) {
      aim_dir.x += 1.0f;
    }
    if (input.aim_up) {
      aim_dir.y -= 1.0f;
    }
    if (input.aim_down) {
      aim_dir.y += 1.0f;
    }
    aim_dir = normalize(aim_dir);

    if (length_sq(aim_dir) > 0.0f) {
      player_.facing_x = aim_dir.x;
      player_.facing_y = aim_dir.y;
      if (player_.fire_timer == 0) {
        spawn_projectile(true, player_.x + aim_dir.x * 7.0f, player_.y + aim_dir.y * 7.0f,
                         aim_dir.x * app_config::PLAYER_SHOT_SPEED, aim_dir.y * app_config::PLAYER_SHOT_SPEED, 1,
                         620);
        player_.fire_timer = app_config::PLAYER_SHOT_INTERVAL_MS;
      }
    } else if (length_sq(move_dir) > 0.0f) {
      player_.facing_x = move_dir.x;
      player_.facing_y = move_dir.y;
    }

    const bool on_transporter = transporter_active() && distance_sq(player_.x, player_.y, app_config::SCREEN_W * 0.5f,
                                                                    app_config::PLAY_Y + app_config::PLAY_H * 0.5f) < 160.0f;
    if (input.action_pressed) {
      if (on_transporter) {
        ++floor_index_;
        score_ += 100;
        if (floor_index_ > high_floor_) {
          high_floor_ = floor_index_;
        }
        setup_floor(false);
        set_message("Warp complete.");
        return;
      }
      if (player_.dodge_cooldown == 0) {
        Vec2 dash_dir = length_sq(move_dir) > 0.0f ? move_dir : Vec2{player_.facing_x, player_.facing_y};
        dash_dir = normalize(dash_dir);
        if (length_sq(dash_dir) == 0.0f) {
          dash_dir = Vec2{0.0f, -1.0f};
        }
        player_.dodge_x = dash_dir.x;
        player_.dodge_y = dash_dir.y;
        player_.dodge_timer = app_config::PLAYER_DODGE_MS;
        player_.dodge_cooldown = app_config::PLAYER_DODGE_COOLDOWN_MS;
        player_.invuln_timer = app_config::PLAYER_DODGE_MS + 80U;
        spawn_particles(player_.x, player_.y, app_config::COLOR_TRANSPORT, 5, dash_dir, 42.0f);
      }
    }

    Vec2 velocity{};
    if (player_.dodge_timer > 0) {
      velocity.x = player_.dodge_x * app_config::DODGE_SPEED;
      velocity.y = player_.dodge_y * app_config::DODGE_SPEED;
    } else {
      velocity.x = move_dir.x * app_config::PLAYER_SPEED;
      velocity.y = move_dir.y * app_config::PLAYER_SPEED;
    }

    move_circle(player_.x, player_.y, velocity.x * (delta_ms / 1000.0f), velocity.y * (delta_ms / 1000.0f),
                app_config::PLAYER_RADIUS);
    handle_room_transition();
  }

  void spawn_projectile(bool friendly, float x, float y, float vx, float vy, uint8_t damage, uint16_t life_ms) {
    for (auto& projectile : projectiles_) {
      if (!projectile.active) {
        projectile.active = true;
        projectile.friendly = friendly;
        projectile.x = x;
        projectile.y = y;
        projectile.vx = vx;
        projectile.vy = vy;
        projectile.damage = damage;
        projectile.life_ms = life_ms;
        return;
      }
    }
  }

  void update_projectiles(uint32_t delta_ms) {
    for (auto& projectile : projectiles_) {
      if (!projectile.active) {
        continue;
      }

      if (projectile.life_ms > delta_ms) {
        projectile.life_ms -= delta_ms;
      } else {
        projectile.active = false;
        continue;
      }

      projectile.x += projectile.vx * (delta_ms / 1000.0f);
      projectile.y += projectile.vy * (delta_ms / 1000.0f);

      if (is_solid_circle(projectile.x, projectile.y, app_config::PROJECTILE_RADIUS)) {
        spawn_particles(projectile.x, projectile.y,
                        projectile.friendly ? app_config::COLOR_BULLET_PLAYER : app_config::COLOR_BULLET_ENEMY, 3,
                        Vec2{0.0f, 0.0f}, 18.0f);
        projectile.active = false;
        continue;
      }

      if (projectile.friendly) {
        for (auto& enemy : enemies_) {
          if (!enemy.active) {
            continue;
          }
          if (distance_sq(projectile.x, projectile.y, enemy.x, enemy.y) <
              (app_config::ENEMY_RADIUS + app_config::PROJECTILE_RADIUS + 1.0f) *
                  (app_config::ENEMY_RADIUS + app_config::PROJECTILE_RADIUS + 1.0f)) {
            hit_enemy(enemy, projectile.damage, Vec2{projectile.vx, projectile.vy});
            projectile.active = false;
            break;
          }
        }
      } else if (distance_sq(projectile.x, projectile.y, player_.x, player_.y) <
                 (app_config::PLAYER_RADIUS + app_config::PROJECTILE_RADIUS + 1.0f) *
                     (app_config::PLAYER_RADIUS + app_config::PROJECTILE_RADIUS + 1.0f)) {
        hurt_player(Vec2{projectile.vx, projectile.vy});
        projectile.active = false;
      }
    }
  }

  void update_enemies(uint32_t delta_ms) {
    for (auto& enemy : enemies_) {
      if (!enemy.active) {
        continue;
      }

      if (enemy.flash_timer > delta_ms) {
        enemy.flash_timer -= delta_ms;
      } else {
        enemy.flash_timer = 0;
      }

      Vec2 to_player{player_.x - enemy.x, player_.y - enemy.y};
      Vec2 player_dir = normalize(to_player);

      if (enemy.kind == EnemyKind::Drone) {
        enemy.attack_timer = enemy.attack_timer > delta_ms ? enemy.attack_timer - delta_ms : 0;
        const float desired_distance = 34.0f;
        const float dist_sq = distance_sq(enemy.x, enemy.y, player_.x, player_.y);
        Vec2 move{};
        if (dist_sq > desired_distance * desired_distance) {
          move = player_dir;
        } else if (dist_sq < 18.0f * 18.0f) {
          move = Vec2{-player_dir.x, -player_dir.y};
        } else {
          move = Vec2{-player_dir.y * 0.4f, player_dir.x * 0.4f};
        }
        move = normalize(move);
        move_circle(enemy.x, enemy.y, move.x * app_config::DRONE_SPEED * (delta_ms / 1000.0f),
                    move.y * app_config::DRONE_SPEED * (delta_ms / 1000.0f), app_config::ENEMY_RADIUS);

        if (enemy.attack_timer == 0 && dist_sq < 120.0f * 120.0f) {
          spawn_projectile(false, enemy.x + player_dir.x * 7.0f, enemy.y + player_dir.y * 7.0f,
                           player_dir.x * app_config::ENEMY_SHOT_SPEED, player_dir.y * app_config::ENEMY_SHOT_SPEED, 1,
                           900);
          enemy.attack_timer = static_cast<uint16_t>(760U + (random(0, 240)));
        }
      } else {
        enemy.attack_timer = enemy.attack_timer > delta_ms ? enemy.attack_timer - delta_ms : 0;
        if (enemy.action_timer > 0) {
          enemy.action_timer = enemy.action_timer > delta_ms ? enemy.action_timer - delta_ms : 0;
          if (enemy.action_timer < 180) {
            move_circle(enemy.x, enemy.y, enemy.dash_x * app_config::DASHER_BURST_SPEED * (delta_ms / 1000.0f),
                        enemy.dash_y * app_config::DASHER_BURST_SPEED * (delta_ms / 1000.0f), app_config::ENEMY_RADIUS);
          }
        } else {
          move_circle(enemy.x, enemy.y, player_dir.x * app_config::DASHER_SPEED * (delta_ms / 1000.0f),
                      player_dir.y * app_config::DASHER_SPEED * (delta_ms / 1000.0f), app_config::ENEMY_RADIUS);
          if (enemy.attack_timer == 0) {
            enemy.dash_x = player_dir.x;
            enemy.dash_y = player_dir.y;
            enemy.action_timer = 360;
            enemy.attack_timer = static_cast<uint16_t>(1050U + random(0, 260));
          }
        }
      }

      const float combined = app_config::PLAYER_RADIUS + app_config::ENEMY_RADIUS + 1.0f;
      if (distance_sq(enemy.x, enemy.y, player_.x, player_.y) < combined * combined) {
        if (player_.dodge_timer > 0) {
          hit_enemy(enemy, 1, Vec2{player_.dodge_x * 42.0f, player_.dodge_y * 42.0f});
        } else {
          hurt_player(Vec2{enemy.x - player_.x, enemy.y - player_.y});
        }
      }
    }
  }

  void update_particles(uint32_t delta_ms) {
    for (auto& particle : particles_) {
      if (!particle.active) {
        continue;
      }
      if (particle.life_ms > delta_ms) {
        particle.life_ms -= delta_ms;
      } else {
        particle.active = false;
        continue;
      }
      particle.x += particle.vx * (delta_ms / 1000.0f);
      particle.y += particle.vy * (delta_ms / 1000.0f);
      particle.vx *= 0.96f;
      particle.vy *= 0.96f;
    }
  }

  void update_room_state(const InputState&) {
    RoomState& room = current_room();

    if (!room.cleared && !room_has_live_enemies()) {
      room.cleared = true;
      ++rooms_cleared_total_;
      score_ += 20;
      spawn_particles(app_config::SCREEN_W * 0.5f, app_config::PLAY_Y + app_config::PLAY_H * 0.5f,
                      app_config::COLOR_GOOD, 10, Vec2{0.0f, -1.0f}, 34.0f);
      if (room.key_room && !room.key_taken) {
        set_message("Key room opened.");
      } else if (room.exit_room && !player_.has_key) {
        set_message("Need the key first.");
      } else if (room.exit_room && player_.has_key) {
        set_message("Transport online.");
      } else {
        set_message("Room clear.");
      }
    }

    if (key_pickup_visible()) {
      const float key_x = app_config::SCREEN_W * 0.5f;
      const float key_y = app_config::PLAY_Y + app_config::PLAY_H * 0.5f;
      if (distance_sq(player_.x, player_.y, key_x, key_y) < 110.0f) {
        current_room().key_taken = true;
        player_.has_key = true;
        score_ += 50;
        spawn_particles(key_x, key_y, app_config::COLOR_PICKUP, 12, Vec2{0.0f, -1.0f}, 38.0f);
        set_message("Gate key recovered.");
      }
    }
  }

  void hurt_player(const Vec2& knockback_hint) {
    if (player_.invuln_timer > 0) {
      return;
    }
    if (player_.hp > 0) {
      --player_.hp;
    }
    player_.invuln_timer = app_config::PLAYER_INVULN_MS;
    spawn_particles(player_.x, player_.y, app_config::COLOR_HURT, 10, normalize(knockback_hint), 36.0f);

    const Vec2 away = normalize(Vec2{-knockback_hint.x, -knockback_hint.y});
    move_circle(player_.x, player_.y, away.x * 6.0f, away.y * 6.0f, app_config::PLAYER_RADIUS);

    if (player_.hp == 0) {
      scene_ = Scene::GameOver;
      if (floor_index_ > high_floor_) {
        high_floor_ = floor_index_;
      }
      set_message("Run collapsed.");
    }
  }

  void hit_enemy(EnemyState& enemy, uint8_t damage, const Vec2& impulse_hint) {
    if (!enemy.active) {
      return;
    }
    enemy.flash_timer = 90;
    if (damage >= enemy.hp) {
      enemy.hp = 0;
      enemy.active = false;
      score_ += enemy.kind == EnemyKind::Drone ? 15 : 25;
      spawn_particles(enemy.x, enemy.y, enemy.kind == EnemyKind::Drone ? app_config::COLOR_ENEMY_DRONE
                                                                        : app_config::COLOR_ENEMY_DASHER,
                      8, normalize(impulse_hint), 30.0f);
      return;
    }
    enemy.hp -= damage;
    const Vec2 push = normalize(impulse_hint);
    move_circle(enemy.x, enemy.y, push.x * 4.0f, push.y * 4.0f, app_config::ENEMY_RADIUS);
  }

  void spawn_particles(float x, float y, uint16_t color, int count, const Vec2& bias, float speed) {
    for (int i = 0; i < count; ++i) {
      for (auto& particle : particles_) {
        if (!particle.active) {
          const float angle = static_cast<float>((next_rng(rng_seed_) % 628U)) / 100.0f;
          const float random_speed = speed * (0.35f + static_cast<float>(next_rng(rng_seed_) % 70U) / 100.0f);
          particle.active = true;
          particle.x = x;
          particle.y = y;
          particle.vx = cosf(angle) * random_speed + bias.x * speed * 0.5f;
          particle.vy = sinf(angle) * random_speed + bias.y * speed * 0.5f;
          particle.life_ms = static_cast<uint16_t>(220U + (next_rng(rng_seed_) % 180U));
          particle.color = color;
          break;
        }
      }
    }
  }

  bool key_pickup_visible() const {
    const RoomState& room = current_room();
    return room.key_room && room.cleared && !room.key_taken && !player_.has_key;
  }

  bool transporter_active() const {
    const RoomState& room = current_room();
    return room.exit_room && room.cleared && player_.has_key;
  }

  bool doorway_locked() const {
    return !current_room().cleared && room_has_live_enemies();
  }

  void handle_room_transition() {
    if (doorway_locked()) {
      return;
    }

    if (current_room_y_ > 0 && in_north_door(player_.x) &&
        player_.y - app_config::PLAYER_RADIUS <= app_config::PLAY_Y + 1.0f) {
      --current_room_y_;
      player_.x = app_config::SCREEN_W * 0.5f;
      player_.y = app_config::PLAY_Y + app_config::PLAY_H - app_config::PLAYER_RADIUS - 6.0f;
      enter_current_room();
      return;
    }
    if (current_room_y_ < 2 && in_north_door(player_.x) &&
        player_.y + app_config::PLAYER_RADIUS >= app_config::PLAY_Y + app_config::PLAY_H - 1.0f) {
      ++current_room_y_;
      player_.x = app_config::SCREEN_W * 0.5f;
      player_.y = app_config::PLAY_Y + app_config::PLAYER_RADIUS + 6.0f;
      enter_current_room();
      return;
    }
    if (current_room_x_ > 0 && in_side_door(player_.y) && player_.x - app_config::PLAYER_RADIUS <= 1.0f) {
      --current_room_x_;
      player_.x = app_config::SCREEN_W - app_config::PLAYER_RADIUS - 6.0f;
      player_.y = app_config::PLAY_Y + app_config::PLAY_H * 0.5f;
      enter_current_room();
      return;
    }
    if (current_room_x_ < 2 && in_side_door(player_.y) &&
        player_.x + app_config::PLAYER_RADIUS >= app_config::SCREEN_W - 1.0f) {
      ++current_room_x_;
      player_.x = app_config::PLAYER_RADIUS + 6.0f;
      player_.y = app_config::PLAY_Y + app_config::PLAY_H * 0.5f;
      enter_current_room();
    }
  }

  void move_circle(float& x, float& y, float dx, float dy, float radius) {
    const float next_x = x + dx;
    if (!is_solid_circle(next_x, y, radius)) {
      x = next_x;
    }
    const float next_y = y + dy;
    if (!is_solid_circle(x, next_y, radius)) {
      y = next_y;
    }

    x = clampf(x, radius, app_config::SCREEN_W - radius);
    y = clampf(y, app_config::PLAY_Y + radius, app_config::PLAY_Y + app_config::PLAY_H - radius);
  }

  bool in_north_door(float x) const {
    const int col = static_cast<int>(x / app_config::TILE_SIZE);
    return col >= 8 && col <= 11;
  }

  bool in_side_door(float y) const {
    const int row = static_cast<int>((y - app_config::PLAY_Y) / app_config::TILE_SIZE);
    return row >= 3 && row <= 5;
  }

  bool is_solid_circle(float x, float y, float radius) const {
    static constexpr float kSamples[8][2] = {
        {1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
        {0.7f, 0.7f}, {-0.7f, 0.7f}, {0.7f, -0.7f}, {-0.7f, -0.7f},
    };
    if (is_solid_point(x, y)) {
      return true;
    }
    for (const auto& sample : kSamples) {
      if (is_solid_point(x + sample[0] * radius, y + sample[1] * radius)) {
        return true;
      }
    }
    return false;
  }

  bool is_solid_point(float x, float y) const {
    if (x < 0.0f || x >= app_config::SCREEN_W || y < app_config::PLAY_Y || y >= app_config::PLAY_Y + app_config::PLAY_H) {
      return true;
    }

    const int col = static_cast<int>(x / app_config::TILE_SIZE);
    const int row = static_cast<int>((y - app_config::PLAY_Y) / app_config::TILE_SIZE);

    if (col <= 0) {
      return !door_open(Direction::West) || !in_side_door(y);
    }
    if (col >= app_config::ROOM_COLS - 1) {
      return !door_open(Direction::East) || !in_side_door(y);
    }
    if (row <= 0) {
      return !door_open(Direction::North) || !in_north_door(x);
    }
    if (row >= app_config::ROOM_ROWS - 1) {
      return !door_open(Direction::South) || !in_north_door(x);
    }

    return obstacle_solid(current_room().pattern, col, row);
  }

  bool door_open(Direction dir) const {
    if (doorway_locked()) {
      return false;
    }
    if (dir == Direction::North) {
      return current_room_y_ > 0;
    }
    if (dir == Direction::South) {
      return current_room_y_ < 2;
    }
    if (dir == Direction::West) {
      return current_room_x_ > 0;
    }
    return current_room_x_ < 2;
  }

  bool obstacle_solid(uint8_t pattern, int col, int row) const {
    switch (pattern % 7U) {
      case 0:
        return false;
      case 1:
        return col >= 8 && col <= 11 && row >= 3 && row <= 5;
      case 2:
        return ((col >= 5 && col <= 6) || (col >= 13 && col <= 14)) && ((row >= 2 && row <= 3) || (row >= 5 && row <= 6));
      case 3:
        return ((row == 2 || row == 6) && col >= 4 && col <= 15 && !(col >= 8 && col <= 11));
      case 4:
        return ((col == 5 || col == 14) && row >= 2 && row <= 6 && row != 4);
      case 5:
        return (row >= 2 && row <= 6) &&
               ((col == row + 4) || (col == (16 - row)));
      default:
        return (col >= 7 && col <= 12 && row == 4) || (row >= 3 && row <= 5 && col == 10);
    }
  }

  void set_message(const char* text) {
    strncpy(message_, text, sizeof(message_) - 1);
    message_[sizeof(message_) - 1] = '\0';
    message_timer_ = app_config::MESSAGE_MS;
  }

  void draw_title(M5Canvas& canvas) const {
    for (int y = 0; y < app_config::SCREEN_H; y += 9) {
      const uint16_t color = ((y / 9) & 1) == 0 ? app_config::COLOR_BG : app_config::COLOR_FLOOR_A;
      canvas.fillRect(0, y, app_config::SCREEN_W, 9, color);
    }

    for (int i = 0; i < 32; ++i) {
      const int sx = static_cast<int>((mix_seed(static_cast<uint32_t>(i * 113 + 9)) + pulse_clock_ms_ * (i + 3)) %
                                      app_config::SCREEN_W);
      const int sy = 14 + static_cast<int>((mix_seed(static_cast<uint32_t>(i * 59 + 7)) + pulse_clock_ms_ * (i + 1)) %
                                           (app_config::SCREEN_H - 28));
      canvas.drawPixel(sx, sy, i % 5 == 0 ? app_config::COLOR_TRANSPORT_CORE : app_config::COLOR_TEXT);
    }

    canvas.setTextColor(app_config::COLOR_TRANSPORT_CORE, app_config::COLOR_BG);
    canvas.setTextSize(2);
    canvas.setCursor(24, 20);
    canvas.print("BLIND JUMP");
    canvas.setTextSize(1);
    canvas.setTextColor(app_config::COLOR_MUTED, app_config::COLOR_BG);
    canvas.setCursor(56, 42);
    canvas.print("Cardputer clean-room port");

    canvas.fillRoundRect(26, 54, 188, 46, 6, app_config::COLOR_PANEL);
    canvas.drawRoundRect(26, 54, 188, 46, 6, app_config::COLOR_WALL_HI);
    canvas.setTextColor(app_config::COLOR_TEXT, app_config::COLOR_PANEL);
    canvas.setCursor(36, 63);
    canvas.print("WASD move");
    canvas.setCursor(36, 75);
    canvas.print("IJKL shoot");
    canvas.setCursor(36, 87);
    canvas.print("U / BtnA dodge or activate");

    canvas.setTextColor(app_config::COLOR_PICKUP, app_config::COLOR_BG);
    canvas.setCursor(42, 108);
    canvas.print("Enter / BtnA start");
    canvas.setTextColor(app_config::COLOR_MUTED, app_config::COLOR_BG);
    canvas.setCursor(48, 120);
    canvas.printf("Best floor %u", static_cast<unsigned>(high_floor_));
  }

  void draw_playfield(M5Canvas& canvas) const {
    canvas.fillRect(0, app_config::PLAY_Y, app_config::SCREEN_W, app_config::PLAY_H, app_config::COLOR_BG);
    for (int i = 0; i < 24; ++i) {
      const uint32_t base = mix_seed(static_cast<uint32_t>(i * 97U + floor_index_ * 43U + current_room_x_ * 11U +
                                                           current_room_y_ * 29U));
      const int sx = static_cast<int>(base % app_config::SCREEN_W);
      const int sy = app_config::PLAY_Y + static_cast<int>((base >> 9) % app_config::PLAY_H);
      canvas.drawPixel(sx, sy, ((base >> 3) & 1U) == 0U ? app_config::COLOR_MUTED : app_config::COLOR_TEXT);
    }
  }

  void draw_room(M5Canvas& canvas) const {
    for (int row = 0; row < app_config::ROOM_ROWS; ++row) {
      for (int col = 0; col < app_config::ROOM_COLS; ++col) {
        const int x = col * app_config::TILE_SIZE;
        const int y = app_config::PLAY_Y + row * app_config::TILE_SIZE;
        bool solid = false;
        if (col == 0) {
          solid = !door_open(Direction::West) || !(row >= 3 && row <= 5);
        } else if (col == app_config::ROOM_COLS - 1) {
          solid = !door_open(Direction::East) || !(row >= 3 && row <= 5);
        } else if (row == 0) {
          solid = !door_open(Direction::North) || !(col >= 8 && col <= 11);
        } else if (row == app_config::ROOM_ROWS - 1) {
          solid = !door_open(Direction::South) || !(col >= 8 && col <= 11);
        } else {
          solid = obstacle_solid(current_room().pattern, col, row);
        }

        if (solid) {
          canvas.fillRect(x, y, app_config::TILE_SIZE, app_config::TILE_SIZE, app_config::COLOR_WALL);
          canvas.drawFastHLine(x, y, app_config::TILE_SIZE, app_config::COLOR_WALL_HI);
          canvas.drawFastVLine(x, y, app_config::TILE_SIZE, app_config::COLOR_WALL_HI);
        } else {
          const uint16_t color = ((row + col) & 1) == 0 ? app_config::COLOR_FLOOR_A : app_config::COLOR_FLOOR_B;
          canvas.fillRect(x, y, app_config::TILE_SIZE, app_config::TILE_SIZE, color);
          if (((row + col + floor_index_) & 1) == 0) {
            canvas.drawPixel(x + 3, y + 3, app_config::COLOR_PANEL);
            canvas.drawPixel(x + 8, y + 7, app_config::COLOR_PANEL);
          }
        }
      }
    }

    if (doorway_locked()) {
      draw_barrier(canvas, Direction::North);
      draw_barrier(canvas, Direction::South);
      draw_barrier(canvas, Direction::West);
      draw_barrier(canvas, Direction::East);
    }

    if (transporter_active()) {
      const int cx = app_config::SCREEN_W / 2;
      const int cy = app_config::PLAY_Y + app_config::PLAY_H / 2;
      const int pulse = static_cast<int>((pulse_clock_ms_ / 70U) % 6U);
      canvas.drawCircle(cx, cy, 6 + pulse, app_config::COLOR_TRANSPORT);
      canvas.drawCircle(cx, cy, 12 + pulse, app_config::COLOR_TRANSPORT_CORE);
      canvas.fillCircle(cx, cy, 4, app_config::COLOR_TRANSPORT_CORE);
      canvas.drawFastVLine(cx, cy - 12, 24, app_config::COLOR_TRANSPORT);
      canvas.drawFastHLine(cx - 12, cy, 24, app_config::COLOR_TRANSPORT);
    }
  }

  void draw_barrier(M5Canvas& canvas, Direction dir) const {
    if (dir == Direction::North) {
      for (int x = 8 * app_config::TILE_SIZE; x < 12 * app_config::TILE_SIZE; x += 4) {
        canvas.drawFastVLine(x, app_config::PLAY_Y + 1, 10, app_config::COLOR_BARRIER);
      }
      return;
    }
    if (dir == Direction::South) {
      const int y = app_config::PLAY_Y + app_config::PLAY_H - 10;
      for (int x = 8 * app_config::TILE_SIZE; x < 12 * app_config::TILE_SIZE; x += 4) {
        canvas.drawFastVLine(x, y, 10, app_config::COLOR_BARRIER);
      }
      return;
    }
    if (dir == Direction::West) {
      for (int y = app_config::PLAY_Y + 3 * app_config::TILE_SIZE; y < app_config::PLAY_Y + 6 * app_config::TILE_SIZE;
           y += 4) {
        canvas.drawFastHLine(1, y, 10, app_config::COLOR_BARRIER);
      }
      return;
    }
    for (int y = app_config::PLAY_Y + 3 * app_config::TILE_SIZE; y < app_config::PLAY_Y + 6 * app_config::TILE_SIZE;
         y += 4) {
      canvas.drawFastHLine(app_config::SCREEN_W - 11, y, 10, app_config::COLOR_BARRIER);
    }
  }

  void draw_pickups(M5Canvas& canvas) const {
    if (!key_pickup_visible()) {
      return;
    }
    const int cx = app_config::SCREEN_W / 2;
    const int cy = app_config::PLAY_Y + app_config::PLAY_H / 2;
    const int bob = static_cast<int>((pulse_clock_ms_ / 130U) % 4U);
    canvas.drawLine(cx, cy - 8 - bob, cx + 5, cy - 3 - bob, app_config::COLOR_PICKUP);
    canvas.drawLine(cx + 5, cy - 3 - bob, cx, cy + 2 - bob, app_config::COLOR_PICKUP);
    canvas.drawLine(cx, cy + 2 - bob, cx - 5, cy - 3 - bob, app_config::COLOR_PICKUP);
    canvas.drawLine(cx - 5, cy - 3 - bob, cx, cy - 8 - bob, app_config::COLOR_PICKUP);
    canvas.drawFastVLine(cx, cy + 2 - bob, 8, app_config::COLOR_PICKUP);
    canvas.drawFastHLine(cx, cy + 8 - bob, 6, app_config::COLOR_PICKUP);
  }

  void draw_particles(M5Canvas& canvas) const {
    for (const auto& particle : particles_) {
      if (!particle.active) {
        continue;
      }
      canvas.fillRect(static_cast<int>(particle.x) - 1, static_cast<int>(particle.y) - 1, 2, 2, particle.color);
    }
  }

  void draw_projectiles(M5Canvas& canvas, bool friendly) const {
    for (const auto& projectile : projectiles_) {
      if (!projectile.active || projectile.friendly != friendly) {
        continue;
      }
      const uint16_t color = friendly ? app_config::COLOR_BULLET_PLAYER : app_config::COLOR_BULLET_ENEMY;
      canvas.fillCircle(static_cast<int>(projectile.x), static_cast<int>(projectile.y), 2, color);
      canvas.drawPixel(static_cast<int>(projectile.x + projectile.vx * 0.02f),
                       static_cast<int>(projectile.y + projectile.vy * 0.02f), app_config::COLOR_TEXT);
    }
  }

  void draw_enemies(M5Canvas& canvas) const {
    for (const auto& enemy : enemies_) {
      if (!enemy.active) {
        continue;
      }

      const uint16_t body_color = enemy.flash_timer > 0 ? app_config::COLOR_TEXT
                                                        : (enemy.kind == EnemyKind::Drone ? app_config::COLOR_ENEMY_DRONE
                                                                                          : app_config::COLOR_ENEMY_DASHER);
      const int px = static_cast<int>(enemy.x);
      const int py = static_cast<int>(enemy.y);
      canvas.fillCircle(px, py + 4, 5, app_config::COLOR_SHADOW);
      if (enemy.kind == EnemyKind::Drone) {
        canvas.fillCircle(px, py, 5, body_color);
        canvas.drawFastHLine(px - 4, py, 9, app_config::COLOR_ENEMY_CORE);
        canvas.drawPixel(px - 2, py - 2, app_config::COLOR_ENEMY_CORE);
        canvas.drawPixel(px + 2, py - 2, app_config::COLOR_ENEMY_CORE);
      } else {
        canvas.fillTriangle(px, py - 6, px - 6, py + 5, px + 6, py + 5, body_color);
        canvas.drawFastHLine(px - 2, py - 1, 5, app_config::COLOR_ENEMY_CORE);
      }
    }
  }

  void draw_player(M5Canvas& canvas) const {
    const bool flicker = player_.invuln_timer > 0 && ((pulse_clock_ms_ / 40U) & 1U) == 0U;
    if (flicker) {
      return;
    }

    const int px = static_cast<int>(player_.x);
    const int py = static_cast<int>(player_.y);
    canvas.fillCircle(px, py + 5, 6, app_config::COLOR_SHADOW);

    if (player_.dodge_timer > 0) {
      const int tx = static_cast<int>(player_.x - player_.dodge_x * 6.0f);
      const int ty = static_cast<int>(player_.y - player_.dodge_y * 6.0f);
      canvas.drawLine(tx, ty, px, py, app_config::COLOR_TRANSPORT);
    }

    canvas.fillRoundRect(px - 5, py - 6, 10, 12, 3, app_config::COLOR_PLAYER);
    canvas.fillCircle(px, py - 7, 3, app_config::COLOR_PLAYER_ALT);
    canvas.drawLine(px, py - 1, px + static_cast<int>(player_.facing_x * 8.0f),
                    py + static_cast<int>(player_.facing_y * 8.0f), app_config::COLOR_BULLET_PLAYER);
    canvas.drawPixel(px - 2, py - 7, app_config::COLOR_TEXT);
    canvas.drawPixel(px + 2, py - 7, app_config::COLOR_TEXT);
  }

  void draw_minimap(M5Canvas& canvas) const {
    const int origin_x = app_config::SCREEN_W - 32;
    const int origin_y = app_config::PLAY_Y + 3;
    canvas.fillRect(origin_x - 4, origin_y - 4, 30, 30, app_config::COLOR_PANEL);
    canvas.drawRect(origin_x - 4, origin_y - 4, 30, 30, app_config::COLOR_WALL_HI);

    for (int y = 0; y < 3; ++y) {
      for (int x = 0; x < 3; ++x) {
        const RoomState& room = rooms_[y][x];
        uint16_t color = room.visited ? app_config::COLOR_FLOOR_A : app_config::COLOR_BG;
        if (room.cleared && room.visited) {
          color = app_config::COLOR_GOOD;
        }
        if (x == current_room_x_ && y == current_room_y_) {
          color = app_config::COLOR_TRANSPORT_CORE;
        }
        canvas.fillRect(origin_x + x * 8, origin_y + y * 8, 6, 6, color);
        if (room.key_room && (room.visited || (x == current_room_x_ && y == current_room_y_)) && !room.key_taken &&
            !player_.has_key) {
          canvas.drawPixel(origin_x + x * 8 + 2, origin_y + y * 8 + 2, app_config::COLOR_PICKUP);
        }
        if (room.exit_room && (room.visited || (x == current_room_x_ && y == current_room_y_))) {
          canvas.drawRect(origin_x + x * 8, origin_y + y * 8, 6, 6, app_config::COLOR_TRANSPORT);
        }
      }
    }
  }

  void draw_hud(M5Canvas& canvas) const {
    canvas.fillRect(0, 0, app_config::SCREEN_W, app_config::HUD_H, app_config::COLOR_PANEL);
    canvas.drawFastHLine(0, app_config::HUD_H - 1, app_config::SCREEN_W, app_config::COLOR_WALL_HI);
    canvas.setTextColor(app_config::COLOR_TEXT, app_config::COLOR_PANEL);
    canvas.setCursor(4, 3);
    canvas.printf("F%u S%lu", static_cast<unsigned>(floor_index_), static_cast<unsigned long>(score_));

    int x = 74;
    for (uint8_t i = 0; i < player_.max_hp; ++i) {
      const uint16_t color = i < player_.hp ? app_config::COLOR_HURT : app_config::COLOR_MUTED;
      canvas.fillRect(x, 3, 6, 6, color);
      canvas.drawRect(x, 3, 6, 6, app_config::COLOR_TEXT);
      x += 8;
    }

    canvas.setCursor(120, 3);
    canvas.setTextColor(player_.dodge_cooldown == 0 ? app_config::COLOR_TRANSPORT_CORE : app_config::COLOR_MUTED,
                        app_config::COLOR_PANEL);
    canvas.print("DODGE");
    canvas.setTextColor(player_.has_key ? app_config::COLOR_PICKUP : app_config::COLOR_MUTED, app_config::COLOR_PANEL);
    canvas.setCursor(166, 3);
    canvas.print("KEY");
  }

  void draw_footer(M5Canvas& canvas, const InputState&) const {
    canvas.fillRect(0, app_config::SCREEN_H - app_config::FOOTER_H, app_config::SCREEN_W, app_config::FOOTER_H,
                    app_config::COLOR_PANEL);
    canvas.drawFastHLine(0, app_config::SCREEN_H - app_config::FOOTER_H, app_config::SCREEN_W,
                         app_config::COLOR_WALL_HI);
    canvas.setTextColor(message_timer_ > 0 ? app_config::COLOR_PICKUP : app_config::COLOR_MUTED, app_config::COLOR_PANEL);
    canvas.setCursor(4, app_config::SCREEN_H - 10);
    if (message_timer_ > 0) {
      canvas.print(message_);
    } else if (transporter_active()) {
      canvas.print("U / BtnA: warp");
    } else if (doorway_locked()) {
      canvas.print("Clear enemies to unlock doors");
    } else {
      canvas.print("WASD move  IJKL shoot  U dodge");
    }
  }

  void draw_game_over(M5Canvas& canvas) const {
    canvas.fillRect(46, 42, 148, 44, app_config::COLOR_PANEL);
    canvas.drawRect(46, 42, 148, 44, app_config::COLOR_WALL_HI);
    canvas.setTextColor(app_config::COLOR_HURT, app_config::COLOR_PANEL);
    canvas.setTextSize(2);
    canvas.setCursor(64, 48);
    canvas.print("GAME OVER");
    canvas.setTextSize(1);
    canvas.setTextColor(app_config::COLOR_TEXT, app_config::COLOR_PANEL);
    canvas.setCursor(60, 72);
    canvas.printf("Floor %u  Enter restart", static_cast<unsigned>(floor_index_));
  }

  Scene scene_ = Scene::Title;
  PlayerState player_;
  RoomState rooms_[3][3];
  EnemyState enemies_[app_config::MAX_ENEMIES];
  ProjectileState projectiles_[app_config::MAX_PROJECTILES];
  ParticleState particles_[app_config::MAX_PARTICLES];
  uint32_t rng_seed_ = 1;
  uint32_t pulse_clock_ms_ = 0;
  uint32_t score_ = 0;
  uint16_t floor_index_ = 1;
  uint16_t high_floor_ = 1;
  uint16_t rooms_cleared_total_ = 0;
  uint16_t message_timer_ = 0;
  int current_room_x_ = 1;
  int current_room_y_ = 1;
  char message_[40] = {};
};

M5Canvas g_canvas(&M5Cardputer.Display);
BlindJumpCardputerDemo g_demo;
uint32_t g_last_frame_ms = 0;
bool g_prev_u_held = false;
bool g_prev_enter_held = false;

InputState read_input() {
  const auto& status = M5Cardputer.Keyboard.keysState();
  const bool u_held = contains_hid_key(status, app_config::HID_U);
  const bool enter_held = contains_hid_key(status, app_config::HID_ENTER);

  InputState input;
  input.move_up = contains_hid_key(status, app_config::HID_W);
  input.move_down = contains_hid_key(status, app_config::HID_S);
  input.move_left = contains_hid_key(status, app_config::HID_A);
  input.move_right = contains_hid_key(status, app_config::HID_D);
  input.aim_up = contains_hid_key(status, app_config::HID_I);
  input.aim_down = contains_hid_key(status, app_config::HID_K);
  input.aim_left = contains_hid_key(status, app_config::HID_J);
  input.aim_right = contains_hid_key(status, app_config::HID_L);
  input.action_pressed = M5Cardputer.BtnA.wasClicked() || (u_held && !g_prev_u_held);
  input.confirm_pressed = input.action_pressed || (enter_held && !g_prev_enter_held);

  g_prev_u_held = u_held;
  g_prev_enter_held = enter_held;
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
  Serial.println("Cardputer ADV Blind Jump clean-room port demo started");
}

void loop() {
  M5Cardputer.update();
  const InputState input = read_input();

  const uint32_t now = millis();
  if (now - g_last_frame_ms >= app_config::FRAME_INTERVAL_MS) {
    const uint32_t delta_ms = now - g_last_frame_ms;
    g_demo.update(input, delta_ms);
    g_demo.draw(g_canvas, input);
    M5Cardputer.Display.startWrite();
    g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
    M5Cardputer.Display.endWrite();
    g_last_frame_ms = now;
  }
}
