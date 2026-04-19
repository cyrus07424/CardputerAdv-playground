#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>

namespace app_config {
constexpr int16_t SCREEN_W = 240;
constexpr int16_t SCREEN_H = 135;
constexpr int16_t MAP_X = 4;
constexpr int16_t MAP_Y = 6;
constexpr int16_t TILE_SIZE = 4;
constexpr int16_t MAP_W = 28;
constexpr int16_t MAP_H = 31;
constexpr int16_t MAP_PIXEL_W = MAP_W * TILE_SIZE;
constexpr int16_t MAP_PIXEL_H = MAP_H * TILE_SIZE;
constexpr int16_t PANEL_X = 122;
constexpr uint32_t STEP_MS = 16;
constexpr uint32_t DRAW_MS = 33;
constexpr float PACMAN_SPEED = 0.40f;
constexpr float GHOST_SPEED_FAST = 0.35f;
constexpr float GHOST_SPEED_SLOW = 0.25f;
constexpr int HIT_MARGIN = 1;
constexpr int LEFT_TIME = 150;
constexpr int POWER_TICKS = 300;
constexpr int BLINK_TICKS = 120;
constexpr int READY_TICKS = 120;
constexpr int PACMAN_COUNT = 4;
constexpr int ACTOR_COUNT = PACMAN_COUNT + 1;
}  // namespace app_config

namespace keyboard_hid {
constexpr uint8_t RIGHT = 0x4F;
constexpr uint8_t LEFT = 0x50;
constexpr uint8_t DOWN = 0x51;
constexpr uint8_t UP = 0x52;
}  // namespace keyboard_hid

enum Direction : uint8_t {
  DIR_UP = 0,
  DIR_RIGHT = 1,
  DIR_DOWN = 2,
  DIR_LEFT = 3,
};

enum GameState : int8_t {
  STATE_GAMEOVER = 0,
  STATE_PLAYING = 1,
  STATE_READY = 2,
  STATE_TITLE = -1,
};

M5Canvas g_canvas(&M5Cardputer.Display);

int g_frame_count = 0;
int g_power_ticks = 0;
int g_stage = 1;
int g_score = 10000;
int g_left_time = app_config::LEFT_TIME;
int g_ready_ticks = app_config::READY_TICKS;
int g_ghost_color = 1;
GameState g_game_state = STATE_TITLE;
uint32_t g_last_step_ms = 0;
uint32_t g_last_draw_ms = 0;

float g_char_x[app_config::ACTOR_COUNT] = {};
float g_char_y[app_config::ACTOR_COUNT] = {};
Direction g_char_dir[app_config::ACTOR_COUNT] = {};
bool g_char_moving[app_config::ACTOR_COUNT] = {};
int g_char_alive[app_config::ACTOR_COUNT] = {};
int g_next_dir_behavior[app_config::ACTOR_COUNT] = {};
int g_frame_wait[app_config::ACTOR_COUNT] = {};

bool g_prev_start_pressed = false;
bool g_input_up = false;
bool g_input_right = false;
bool g_input_down = false;
bool g_input_left = false;

char g_map[app_config::MAP_H][app_config::MAP_W + 1] = {};
const char* const kDefaultMap[app_config::MAP_H] = {
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBB",
    "BCCCCCCCCCCCCBBCCCCCCCCCCCCB",
    "BCBBBBCBBBBBCBBCBBBBBCBBBBCB",
    "BDBBBBCBBBBBCBBCBBBBBCBBBBDB",
    "BCBBBBCBBBBBCBBCBBBBBCBBBBCB",
    "BCCCCCCCCCCCCCCCCCCCCCCCCCCB",
    "BCBBBBCBBCBBBBBBBBCBBCBBBBCB",
    "BCBBBBCBBCBBBBBBBBCBBCBBBBCB",
    "BCCCCCCBBCCCCBBCCCCBBCCCCCCB",
    "BBBBBBCBBBBBABBABBBBBCBBBBBB",
    "AAAAABCBBBBBABBABBBBBCBAAAAA",
    "AAAAABCBBAAAAAAAAAABBCBAAAAA",
    "AAAAABCBBABBBEEBBBABBCBAAAAA",
    "BBBBBBCBBABAAAAAABABBCBBBBBB",
    "BAAAAACAAABAAAAAABAAACAAAAAB",
    "BBBBBBCBBABAAAAAABABBCBBBBBB",
    "AAAAABCBBABBBBBBBBABBCBAAAAA",
    "AAAAABCBBAAAAAAAAAABBCBAAAAA",
    "AAAAABCBBABBBBBBBBABBCBAAAAA",
    "BBBBBBCBBABBBBBBBBABBCBBBBBB",
    "BCCCCCCCCCCCCBBCCCCCCCCCCCCB",
    "BCBBBBCBBBBBCBBCBBBBBCBBBBCB",
    "BCBBBBCBBBBBCBBCBBBBBCBBBBCB",
    "BDCCBBCCCCCCCCCCCCCCCCBBCCDB",
    "BBBCBBCBBCBBBBBBBBCBBCBBCBBB",
    "BBBCBBCBBCBBBBBBBBCBBCBBCBBB",
    "BCCCCCCBBCCCCBBCCCCBBCCCCCCB",
    "BCBBBBBBBBBBCBBCBBBBBBBBBBCB",
    "BCBBBBBBBBBBCBBCBBBBBBBBBBCB",
    "BCCCCCCCCCCCCCCCCCCCCCCCCCCB",
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBB",
};

const uint16_t kPacmanColors[app_config::PACMAN_COUNT] = {
    TFT_YELLOW,
    TFT_MAGENTA,
    TFT_ORANGE,
    TFT_GREENYELLOW,
};

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

bool rising_edge(bool pressed, bool& previous) {
  const bool triggered = pressed && !previous;
  previous = pressed;
  return triggered;
}

long random_dir() {
  return random(4);
}

float ghost_speed() {
  return g_power_ticks > 0 ? app_config::GHOST_SPEED_SLOW : app_config::GHOST_SPEED_FAST;
}

float snap_to_grid(float value) {
  return floorf((value + app_config::TILE_SIZE * 0.5f) / app_config::TILE_SIZE) * app_config::TILE_SIZE;
}

bool in_bounds(int tx, int ty) {
  return tx >= 0 && tx < app_config::MAP_W && ty >= 0 && ty < app_config::MAP_H;
}

char map_tile(int tx, int ty) {
  if (!in_bounds(tx, ty)) {
    return 'B';
  }
  return g_map[ty][tx];
}

void reset_map() {
  for (int y = 0; y < app_config::MAP_H; ++y) {
    memcpy(g_map[y], kDefaultMap[y], app_config::MAP_W + 1);
  }
}

void init_stage_state(bool full_reset) {
  g_power_ticks = 0;
  g_frame_count = 0;
  g_ready_ticks = app_config::READY_TICKS;
  g_left_time = app_config::LEFT_TIME - (app_config::LEFT_TIME / 10) * (g_stage - 1);
  if (g_left_time < 20) {
    g_left_time = 20;
  }

  for (int i = 1; i <= app_config::PACMAN_COUNT; ++i) {
    g_char_dir[i] = static_cast<Direction>(random_dir());
    g_char_moving[i] = true;
    g_char_alive[i] = app_config::BLINK_TICKS;
    g_next_dir_behavior[i] = static_cast<int>(random_dir());
    g_frame_wait[i] = 0;
  }

  g_char_x[1] = app_config::TILE_SIZE * 1.0f;
  g_char_y[1] = app_config::TILE_SIZE * 29.0f;
  g_char_x[2] = app_config::TILE_SIZE * 1.0f;
  g_char_y[2] = app_config::TILE_SIZE * 1.0f;
  g_char_x[3] = app_config::TILE_SIZE * 26.0f;
  g_char_y[3] = app_config::TILE_SIZE * 1.0f;
  g_char_x[4] = app_config::TILE_SIZE * 26.0f;
  g_char_y[4] = app_config::TILE_SIZE * 29.0f;

  g_char_x[0] = app_config::TILE_SIZE * 14.0f;
  g_char_y[0] = app_config::TILE_SIZE * 14.0f;
  g_char_dir[0] = DIR_UP;
  g_char_moving[0] = false;
  g_char_alive[0] = app_config::BLINK_TICKS;
  g_next_dir_behavior[0] = 0;
  g_frame_wait[0] = 0;
  g_ghost_color = static_cast<int>(random(1, app_config::PACMAN_COUNT + 1));

  reset_map();

  if (full_reset) {
    g_score = 10000;
  }

  g_game_state = STATE_READY;
}

void reset_game() {
  g_stage = 1;
  init_stage_state(true);
}

void advance_stage() {
  ++g_stage;
  g_score += 10000;
  init_stage_state(false);
}

bool is_wall_tile(char tile) {
  return tile == 'B';
}

bool hit_obj_check(float xf, float yf, int actor_index) {
  const int x = static_cast<int>(xf);
  const int y = static_cast<int>(yf);
  const int x0 = (x + app_config::HIT_MARGIN) / app_config::TILE_SIZE;
  const int y0 = (y + app_config::HIT_MARGIN) / app_config::TILE_SIZE;
  const int x1 = (x + app_config::TILE_SIZE - 1 - app_config::HIT_MARGIN) / app_config::TILE_SIZE;
  const int y1 = (y + app_config::TILE_SIZE - 1 - app_config::HIT_MARGIN) / app_config::TILE_SIZE;

  if (is_wall_tile(map_tile(x0, y0)) || is_wall_tile(map_tile(x1, y0)) ||
      is_wall_tile(map_tile(x0, y1)) || is_wall_tile(map_tile(x1, y1))) {
    return false;
  }

  if (actor_index != 0 && g_game_state == STATE_PLAYING) {
    const int tiles[4][2] = {{x0, y0}, {x1, y0}, {x0, y1}, {x1, y1}};
    for (const auto& tile_pos : tiles) {
      char& tile = g_map[tile_pos[1]][tile_pos[0]];
      if (tile == 'C') {
        tile = 'A';
      } else if (tile == 'D') {
        tile = 'A';
        g_power_ticks += app_config::POWER_TICKS;
      }
    }
  }

  return true;
}

bool wall_exist_check(float x, float y, Direction dir, float step) {
  const int center_tx = static_cast<int>(x + app_config::TILE_SIZE * 0.5f) / app_config::TILE_SIZE;
  const int center_ty = static_cast<int>(y + app_config::TILE_SIZE * 0.5f) / app_config::TILE_SIZE;
  int tx = center_tx;
  int ty = center_ty;

  switch (dir) {
    case DIR_UP:
      ty = static_cast<int>(y + app_config::TILE_SIZE * 0.5f - step) / app_config::TILE_SIZE - 1;
      break;
    case DIR_DOWN:
      ty = static_cast<int>(y + app_config::TILE_SIZE * 0.5f + step) / app_config::TILE_SIZE + 1;
      break;
    case DIR_LEFT:
      tx = static_cast<int>(x + app_config::TILE_SIZE * 0.5f - step) / app_config::TILE_SIZE - 1;
      break;
    case DIR_RIGHT:
      tx = static_cast<int>(x + app_config::TILE_SIZE * 0.5f + step) / app_config::TILE_SIZE + 1;
      break;
  }

  const char tile = map_tile(tx, ty);
  return tile != 'B' && tile != 'E';
}

void move_character(int actor_index, Direction dir, float speed) {
  float next_x = g_char_x[actor_index];
  float next_y = g_char_y[actor_index];

  switch (dir) {
    case DIR_UP:
      next_y -= speed;
      if (next_y >= app_config::TILE_SIZE && hit_obj_check(g_char_x[actor_index], next_y, actor_index)) {
        g_char_y[actor_index] = next_y;
        g_char_x[actor_index] = snap_to_grid(g_char_x[actor_index]);
      } else {
        g_char_y[actor_index] = snap_to_grid(g_char_y[actor_index]);
        g_char_moving[actor_index] = false;
      }
      break;
    case DIR_DOWN:
      next_y += speed;
      if (next_y <= app_config::MAP_PIXEL_H - app_config::TILE_SIZE && hit_obj_check(g_char_x[actor_index], next_y, actor_index)) {
        g_char_y[actor_index] = next_y;
        g_char_x[actor_index] = snap_to_grid(g_char_x[actor_index]);
      } else {
        g_char_y[actor_index] = snap_to_grid(g_char_y[actor_index]);
        g_char_moving[actor_index] = false;
      }
      break;
    case DIR_LEFT:
      next_x -= speed;
      if (next_x >= app_config::TILE_SIZE && hit_obj_check(next_x, g_char_y[actor_index], actor_index)) {
        g_char_x[actor_index] = next_x;
        g_char_y[actor_index] = snap_to_grid(g_char_y[actor_index]);
      } else {
        g_char_x[actor_index] = snap_to_grid(g_char_x[actor_index]);
        g_char_moving[actor_index] = false;
      }
      break;
    case DIR_RIGHT:
      next_x += speed;
      if (next_x <= app_config::MAP_PIXEL_W - app_config::TILE_SIZE && hit_obj_check(next_x, g_char_y[actor_index], actor_index)) {
        g_char_x[actor_index] = next_x;
        g_char_y[actor_index] = snap_to_grid(g_char_y[actor_index]);
      } else {
        g_char_x[actor_index] = snap_to_grid(g_char_x[actor_index]);
        g_char_moving[actor_index] = false;
      }
      break;
  }
}

bool overlap(float ax, float ay, float bx, float by) {
  return ((ax >= bx && ax <= bx + app_config::TILE_SIZE) || (bx >= ax && bx <= ax + app_config::TILE_SIZE)) &&
         ((ay >= by && ay <= by + app_config::TILE_SIZE) || (by >= ay && by <= ay + app_config::TILE_SIZE));
}

int count_dots() {
  int count = 0;
  for (int y = 0; y < app_config::MAP_H; ++y) {
    for (int x = 0; x < app_config::MAP_W; ++x) {
      if (g_map[y][x] == 'C' || g_map[y][x] == 'D') {
        ++count;
      }
    }
  }
  return count;
}

void update_blink_states() {
  for (int i = 1; i <= app_config::PACMAN_COUNT; ++i) {
    if (g_char_alive[i] != app_config::BLINK_TICKS && g_char_alive[i] > 0) {
      --g_char_alive[i];
      if (g_char_alive[i] == 0) {
        g_char_moving[i] = false;
      }
    }
  }

  if (g_char_alive[0] != app_config::BLINK_TICKS && g_char_alive[0] > 0) {
    --g_char_alive[0];
    if (g_char_alive[0] == 0) {
      g_char_moving[0] = false;
      g_game_state = STATE_GAMEOVER;
    }
  }
}

void hit_enemy_check() {
  for (int i = 1; i <= app_config::PACMAN_COUNT; ++i) {
    if (g_char_alive[i] == app_config::BLINK_TICKS && g_char_alive[0] != 0 &&
        overlap(g_char_x[0], g_char_y[0], g_char_x[i], g_char_y[i])) {
      if (g_power_ticks == 0) {
        g_char_alive[i] = app_config::BLINK_TICKS - 1;
      } else {
        g_char_alive[0] = app_config::BLINK_TICKS - 1;
      }
    }
  }
}

void try_set_ghost_direction(Direction dir) {
  const float speed = ghost_speed();
  float next_x = g_char_x[0];
  float next_y = g_char_y[0];

  switch (dir) {
    case DIR_UP:
      next_y -= speed;
      break;
    case DIR_RIGHT:
      next_x += speed;
      break;
    case DIR_DOWN:
      next_y += speed;
      break;
    case DIR_LEFT:
      next_x -= speed;
      break;
  }

  if (hit_obj_check(next_x, next_y, 0) && wall_exist_check(next_x, next_y, dir, speed)) {
    g_char_dir[0] = dir;
    g_char_moving[0] = true;
  }
}

void apply_player_input() {
  if (g_char_alive[0] != app_config::BLINK_TICKS) {
    return;
  }

  if (g_input_up && !g_input_down) {
    try_set_ghost_direction(DIR_UP);
  } else if (g_input_down && !g_input_up) {
    try_set_ghost_direction(DIR_DOWN);
  }

  if (g_input_left && !g_input_right) {
    try_set_ghost_direction(DIR_LEFT);
  } else if (g_input_right && !g_input_left) {
    try_set_ghost_direction(DIR_RIGHT);
  }
}

bool can_turn_pacman(int index, Direction dir) {
  switch (dir) {
    case DIR_UP:
      return wall_exist_check(g_char_x[index], g_char_y[index] - app_config::PACMAN_SPEED, DIR_UP, app_config::PACMAN_SPEED);
    case DIR_RIGHT:
      return wall_exist_check(g_char_x[index] + app_config::PACMAN_SPEED, g_char_y[index], DIR_RIGHT, app_config::PACMAN_SPEED);
    case DIR_DOWN:
      return wall_exist_check(g_char_x[index], g_char_y[index] + app_config::PACMAN_SPEED, DIR_DOWN, app_config::PACMAN_SPEED);
    case DIR_LEFT:
      return wall_exist_check(g_char_x[index] - app_config::PACMAN_SPEED, g_char_y[index], DIR_LEFT, app_config::PACMAN_SPEED);
  }
  return false;
}

void start_pacman_move(int index, Direction dir) {
  g_char_moving[index] = true;
  g_char_dir[index] = dir;
  g_frame_wait[index] = g_frame_count;
  g_next_dir_behavior[index] = static_cast<int>(random_dir());
}

void pacman_ai(int index) {
  if (g_frame_count - g_frame_wait[index] > 10) {
    Direction candidate = g_char_dir[index];
    if (g_next_dir_behavior[index] == 0 || g_next_dir_behavior[index] == 2) {
      candidate = static_cast<Direction>((static_cast<int>(g_char_dir[index]) + 3) % 4);
    } else {
      candidate = static_cast<Direction>((static_cast<int>(g_char_dir[index]) + 1) % 4);
    }

    if (can_turn_pacman(index, candidate)) {
      if (g_next_dir_behavior[index] >= 2) {
        g_char_dir[index] = candidate;
      }
      g_frame_wait[index] = g_frame_count;
      g_next_dir_behavior[index] = static_cast<int>(random_dir());
    }
  }

  int candidate = static_cast<int>(g_char_dir[index]);
  while (!g_char_moving[index] && (g_next_dir_behavior[index] % 2) == 1) {
    candidate = (candidate + 1) % 4;
    if (can_turn_pacman(index, static_cast<Direction>(candidate))) {
      start_pacman_move(index, static_cast<Direction>(candidate));
    }
  }

  while (!g_char_moving[index] && (g_next_dir_behavior[index] % 2) == 0) {
    candidate = (candidate + 3) % 4;
    if (can_turn_pacman(index, static_cast<Direction>(candidate))) {
      start_pacman_move(index, static_cast<Direction>(candidate));
    }
  }
}

void simulate_step() {
  ++g_frame_count;

  if (g_game_state == STATE_READY) {
    if (--g_ready_ticks <= 0) {
      g_game_state = STATE_PLAYING;
    }
    return;
  }

  if (g_game_state != STATE_PLAYING) {
    return;
  }

  if ((g_frame_count % 60) == 0) {
    --g_left_time;
    if (g_left_time <= 0) {
      g_game_state = STATE_GAMEOVER;
      g_char_moving[0] = false;
      return;
    }
  }

  if (g_score > 0) {
    --g_score;
  }
  if (g_power_ticks > 0) {
    --g_power_ticks;
  }

  int living_pacmen = 0;
  for (int i = 1; i <= app_config::PACMAN_COUNT; ++i) {
    if (g_char_alive[i] == app_config::BLINK_TICKS) {
      pacman_ai(i);
      ++living_pacmen;
    }
  }
  if (living_pacmen == 0) {
    advance_stage();
    return;
  }

  apply_player_input();

  if (g_char_moving[0]) {
    move_character(0, g_char_dir[0], ghost_speed());
  }
  for (int i = 1; i <= app_config::PACMAN_COUNT; ++i) {
    if (g_char_moving[i] && g_char_alive[i] == app_config::BLINK_TICKS) {
      move_character(i, g_char_dir[i], app_config::PACMAN_SPEED);
    }
  }

  if (count_dots() == 0) {
    g_game_state = STATE_GAMEOVER;
    g_char_moving[0] = false;
    return;
  }

  hit_enemy_check();
  update_blink_states();
}

bool should_draw_actor(int index) {
  return g_char_alive[index] == app_config::BLINK_TICKS || (g_char_alive[index] % 20) > 10;
}

void draw_map() {
  g_canvas.fillRect(app_config::MAP_X, app_config::MAP_Y, app_config::MAP_PIXEL_W, app_config::MAP_PIXEL_H, 0x0841);
  g_canvas.drawRect(app_config::MAP_X - 1, app_config::MAP_Y - 1, app_config::MAP_PIXEL_W + 2, app_config::MAP_PIXEL_H + 2, TFT_DARKGREY);

  for (int y = 0; y < app_config::MAP_H; ++y) {
    for (int x = 0; x < app_config::MAP_W; ++x) {
      const int16_t sx = app_config::MAP_X + x * app_config::TILE_SIZE;
      const int16_t sy = app_config::MAP_Y + y * app_config::TILE_SIZE;
      switch (g_map[y][x]) {
        case 'B':
          g_canvas.fillRect(sx, sy, app_config::TILE_SIZE, app_config::TILE_SIZE, TFT_BLUE);
          break;
        case 'C':
          g_canvas.fillCircle(sx + app_config::TILE_SIZE / 2, sy + app_config::TILE_SIZE / 2, 1, TFT_YELLOW);
          break;
        case 'D':
          g_canvas.fillCircle(sx + app_config::TILE_SIZE / 2, sy + app_config::TILE_SIZE / 2, 2, TFT_WHITE);
          break;
        case 'E':
          g_canvas.drawFastHLine(sx, sy + app_config::TILE_SIZE / 2, app_config::TILE_SIZE, TFT_MAGENTA);
          break;
        default:
          break;
      }
    }
  }
}

void draw_pacman(int index) {
  if (!should_draw_actor(index)) {
    return;
  }
  const int16_t sx = app_config::MAP_X + static_cast<int16_t>(roundf(g_char_x[index]));
  const int16_t sy = app_config::MAP_Y + static_cast<int16_t>(roundf(g_char_y[index]));
  g_canvas.fillCircle(sx + 2, sy + 2, 2, kPacmanColors[index - 1]);
  const uint16_t eye_color = TFT_BLACK;
  switch (g_char_dir[index]) {
    case DIR_UP:
      g_canvas.drawPixel(sx + 2, sy + 1, eye_color);
      break;
    case DIR_RIGHT:
      g_canvas.drawPixel(sx + 3, sy + 2, eye_color);
      break;
    case DIR_DOWN:
      g_canvas.drawPixel(sx + 2, sy + 3, eye_color);
      break;
    case DIR_LEFT:
      g_canvas.drawPixel(sx + 1, sy + 2, eye_color);
      break;
  }
}

void draw_ghost() {
  if (!should_draw_actor(0)) {
    return;
  }
  const int16_t sx = app_config::MAP_X + static_cast<int16_t>(roundf(g_char_x[0]));
  const int16_t sy = app_config::MAP_Y + static_cast<int16_t>(roundf(g_char_y[0]));
  const uint16_t body_color = g_power_ticks > 0 ? TFT_NAVY : kPacmanColors[g_ghost_color - 1];
  g_canvas.fillRoundRect(sx, sy, app_config::TILE_SIZE, app_config::TILE_SIZE, 1, body_color);
  g_canvas.drawPixel(sx + 1, sy + 1, TFT_WHITE);
  g_canvas.drawPixel(sx + 2, sy + 1, TFT_WHITE);
}

void draw_status_panel() {
  g_canvas.fillRect(app_config::PANEL_X, 0, app_config::SCREEN_W - app_config::PANEL_X, app_config::SCREEN_H, TFT_BLACK);
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);

  g_canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  g_canvas.setCursor(app_config::PANEL_X + 4, 6);
  g_canvas.print("GhostmaN");

  g_canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  if (g_game_state == STATE_TITLE) {
    g_canvas.setCursor(app_config::PANEL_X + 4, 24);
    g_canvas.print("Ghost vs Pacmen");
    g_canvas.setCursor(app_config::PANEL_X + 4, 36);
    g_canvas.print("Eat all 4 pacmen.");
    g_canvas.setCursor(app_config::PANEL_X + 4, 48);
    g_canvas.print("Power dot flips risk.");
    g_canvas.setCursor(app_config::PANEL_X + 4, 64);
    g_canvas.print("Up/;/W or Enter");
    g_canvas.setCursor(app_config::PANEL_X + 4, 76);
    g_canvas.print("BtnA: start");
  } else if (g_game_state == STATE_READY) {
    g_canvas.setCursor(app_config::PANEL_X + 4, 28);
    g_canvas.print("On your mark...");
    if ((g_ready_ticks / 15) % 2 == 0) {
      g_canvas.setCursor(app_config::PANEL_X + 4, 44);
      g_canvas.print("Start!!");
    }
  } else if (g_game_state == STATE_GAMEOVER) {
    g_canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
    g_canvas.setCursor(app_config::PANEL_X + 4, 24);
    g_canvas.print("Game Over");
    g_canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    g_canvas.setCursor(app_config::PANEL_X + 4, 40);
    g_canvas.printf("Stage %d", g_stage);
    g_canvas.setCursor(app_config::PANEL_X + 4, 52);
    g_canvas.printf("Score %d", g_score);
    g_canvas.setCursor(app_config::PANEL_X + 4, 68);
    g_canvas.print("Up/;/W or Enter");
    g_canvas.setCursor(app_config::PANEL_X + 4, 80);
    g_canvas.print("BtnA: retry");
  } else {
    g_canvas.setCursor(app_config::PANEL_X + 4, 22);
    g_canvas.printf("Stage %d", g_stage);
    g_canvas.setCursor(app_config::PANEL_X + 4, 34);
    g_canvas.printf("Time %d", g_left_time);
    g_canvas.setCursor(app_config::PANEL_X + 4, 46);
    g_canvas.printf("Score %d", g_score);
    g_canvas.setCursor(app_config::PANEL_X + 4, 58);
    g_canvas.printf("Dots %d", count_dots());
    g_canvas.setCursor(app_config::PANEL_X + 4, 70);
    g_canvas.printf("Power %s", g_power_ticks > 0 ? "ON" : "OFF");
    g_canvas.setCursor(app_config::PANEL_X + 4, 88);
    g_canvas.print("Move:");
    g_canvas.setCursor(app_config::PANEL_X + 4, 100);
    g_canvas.print("Arrow or ,/;.");
    g_canvas.setCursor(app_config::PANEL_X + 4, 112);
    g_canvas.print("Enter/BtnA start");
  }
}

void draw_ui() {
  g_canvas.fillScreen(TFT_BLACK);
  draw_map();
  for (int i = 1; i <= app_config::PACMAN_COUNT; ++i) {
    draw_pacman(i);
  }
  draw_ghost();
  draw_status_panel();
  g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
}

void handle_input() {
  const auto status = M5Cardputer.Keyboard.keysState();
  g_input_up = contains_hid_key(status, keyboard_hid::UP) || contains_char_key(status, ';') || contains_char_key(status, 'w') || contains_char_key(status, 'W');
  g_input_down = contains_hid_key(status, keyboard_hid::DOWN) || contains_char_key(status, '.') || contains_char_key(status, 's') || contains_char_key(status, 'S');
  g_input_left = contains_hid_key(status, keyboard_hid::LEFT) || contains_char_key(status, ',') || contains_char_key(status, 'a') || contains_char_key(status, 'A');
  g_input_right = contains_hid_key(status, keyboard_hid::RIGHT) || contains_char_key(status, '/') || contains_char_key(status, 'd') || contains_char_key(status, 'D');

  const bool start_pressed = g_input_up || status.enter || M5Cardputer.BtnA.wasClicked();
  if (rising_edge(start_pressed, g_prev_start_pressed) &&
      (g_game_state == STATE_TITLE || g_game_state == STATE_GAMEOVER)) {
    reset_game();
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
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);

  randomSeed(millis());
  reset_map();
  g_last_step_ms = millis();
  g_last_draw_ms = g_last_step_ms;
  draw_ui();
}

void loop() {
  M5Cardputer.update();
  handle_input();

  const uint32_t now = millis();
  while (now - g_last_step_ms >= app_config::STEP_MS) {
    simulate_step();
    g_last_step_ms += app_config::STEP_MS;
  }

  if (now - g_last_draw_ms >= app_config::DRAW_MS) {
    draw_ui();
    g_last_draw_ms = now;
  }
}
