#include <Arduino.h>
#include <M5Cardputer.h>

#include <cmath>

namespace app_config {
inline int display_width() { return M5Cardputer.Display.width(); }
inline int display_height() { return M5Cardputer.Display.height(); }
#define SCREEN_W display_width()
#define SCREEN_H display_height()
constexpr int HORIZON_Y = 34;
constexpr uint32_t FRAME_INTERVAL_MS = 33;

constexpr int TRACK_SEGMENTS = 480;
constexpr int DRAW_DISTANCE = 72;
constexpr int TRAFFIC_COUNT = 7;

constexpr float ROAD_HALF_WIDTH = 170.0f;
constexpr float SHOULDER_SCALE = 0.23f;
constexpr float CAMERA_HEIGHT = 92.0f;
constexpr float HILL_SCALE = 2.4f;

constexpr float MAX_SPEED = 0.92f;
constexpr float ACCELERATION = 0.024f;
constexpr float BRAKE_DECELERATION = 0.040f;
constexpr float COAST_DECELERATION = 0.008f;
constexpr float OFFROAD_DECELERATION = 0.020f;
constexpr float MIN_RECOVER_SPEED = 0.18f;
constexpr float MAX_PLAYER_X = 1.40f;
constexpr float ROAD_EDGE_X = 1.00f;
constexpr float CENTERING_ASSIST = 0.018f;
constexpr float OFFROAD_CENTERING_ASSIST = 0.030f;
constexpr float STEERING_BASE = 0.016f;
constexpr float STEERING_SPEED_SCALE = 0.020f;
constexpr float STEERING_EDGE_BOOST = 0.055f;
constexpr float STEERING_EDGE_SPEED_BOOST = 0.050f;

constexpr float STAGE_DISTANCE = 1200.0f;
constexpr float STAGE_TIME_S = 60.0f;
constexpr int CRASH_FRAMES = 28;

constexpr uint8_t SCENERY_PALM = 0;
constexpr uint8_t SCENERY_CITY = 1;
constexpr uint8_t SCENERY_ROCK = 2;
constexpr uint8_t SCENERY_GOAL = 3;
}

namespace keyboard_hid {
constexpr uint8_t RIGHT = 0x4F;
constexpr uint8_t LEFT = 0x50;
constexpr uint8_t DOWN = 0x51;
constexpr uint8_t UP = 0x52;
constexpr uint8_t ENTER = 0x28;
}

enum class GameMode : uint8_t {
  Title,
  Playing,
  Crash,
  GameOver,
  Goal,
};

struct InputState {
  bool left_held = false;
  bool right_held = false;
  bool accel_held = false;
  bool brake_held = false;
  bool start_pressed = false;
  bool mode_toggle_pressed = false;
  bool any_drive_input = false;
};

struct RoadSegment {
  float curve = 0.0f;
  float height = 0.0f;
  uint8_t scenery = app_config::SCENERY_PALM;
};

struct TrafficCar {
  float z = 0.0f;
  float lane = 0.0f;
  float speed = 0.0f;
  uint16_t body_color = TFT_RED;
};

struct ProjectedSegment {
  float center_x = 0.0f;
  float y = 0.0f;
  float road_w = 0.0f;
  float shoulder_w = 0.0f;
  uint8_t scenery = app_config::SCENERY_PALM;
};

struct GameState {
  GameMode mode = GameMode::Title;
  float player_z = 0.0f;
  float player_x = 0.0f;
  float speed = 0.0f;
  float distance_left = app_config::STAGE_DISTANCE;
  float time_left_s = app_config::STAGE_TIME_S;
  float crash_push = 0.0f;
  int crash_timer = 0;
  int gear = 1;
  bool auto_drive = false;
  uint32_t score = 0;
  uint32_t best_score = 0;
  uint32_t frame = 0;
  TrafficCar traffic[app_config::TRAFFIC_COUNT];
};

M5Canvas g_canvas(&M5Cardputer.Display);
RoadSegment g_track[app_config::TRACK_SEGMENTS];
ProjectedSegment g_projected[app_config::DRAW_DISTANCE + 2];
GameState g_game;
InputState g_prev_input;
bool g_prev_enter_held = false;
uint32_t g_last_frame_ms = 0;

float lerp_float(float a, float b, float t) {
  return a + (b - a) * t;
}

float smoothstep(float t) {
  return t * t * (3.0f - 2.0f * t);
}

float clamp_float(float value, float minimum, float maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

uint32_t max_u32(uint32_t a, uint32_t b) {
  return a > b ? a : b;
}

int max_int(int a, int b) {
  return a > b ? a : b;
}

int min_int(int a, int b) {
  return a < b ? a : b;
}

int16_t max_i16(int16_t a, int16_t b) {
  return a > b ? a : b;
}

float max_float_value(float a, float b) {
  return a > b ? a : b;
}

float wrap_track(float z) {
  while (z < 0.0f) {
    z += static_cast<float>(app_config::TRACK_SEGMENTS);
  }
  while (z >= static_cast<float>(app_config::TRACK_SEGMENTS)) {
    z -= static_cast<float>(app_config::TRACK_SEGMENTS);
  }
  return z;
}

float forward_distance(float target_z, float origin_z) {
  float delta = target_z - origin_z;
  while (delta < 0.0f) {
    delta += static_cast<float>(app_config::TRACK_SEGMENTS);
  }
  while (delta >= static_cast<float>(app_config::TRACK_SEGMENTS)) {
    delta -= static_cast<float>(app_config::TRACK_SEGMENTS);
  }
  return delta;
}

void fill_quad(M5Canvas& canvas,
               float x1,
               float y1,
               float x2,
               float y2,
               float x3,
               float y3,
               float x4,
               float y4,
               uint16_t color) {
  canvas.fillTriangle(static_cast<int16_t>(x1), static_cast<int16_t>(y1), static_cast<int16_t>(x2),
                      static_cast<int16_t>(y2), static_cast<int16_t>(x3), static_cast<int16_t>(y3),
                      color);
  canvas.fillTriangle(static_cast<int16_t>(x1), static_cast<int16_t>(y1), static_cast<int16_t>(x3),
                      static_cast<int16_t>(y3), static_cast<int16_t>(x4), static_cast<int16_t>(y4),
                      color);
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

  InputState input;
  input.left_held = contains_hid_key(status, keyboard_hid::LEFT) || contains_char_key(status, ',');
  input.right_held = contains_hid_key(status, keyboard_hid::RIGHT) || contains_char_key(status, '/');
  input.accel_held = contains_hid_key(status, keyboard_hid::UP) || contains_char_key(status, ';') ||
                     contains_char_key(status, 'a') || contains_char_key(status, 'A');
  input.brake_held = contains_hid_key(status, keyboard_hid::DOWN) || contains_char_key(status, '.');

  const bool enter_held = contains_hid_key(status, keyboard_hid::ENTER);
  input.start_pressed = M5Cardputer.BtnA.wasClicked() || (enter_held && !g_prev_enter_held);
  input.mode_toggle_pressed = enter_held && !g_prev_enter_held;
  input.any_drive_input =
      input.left_held || input.right_held || input.accel_held || input.brake_held || enter_held;
  g_prev_enter_held = enter_held;
  g_prev_input = input;
  return input;
}

void append_track_section(int& write_index,
                          int length,
                          float curve_start,
                          float curve_end,
                          float hill_delta_start,
                          float hill_delta_end,
                          uint8_t scenery,
                          float& running_height) {
  for (int i = 0; i < length && write_index < app_config::TRACK_SEGMENTS; ++i) {
    const float t = length <= 1 ? 1.0f : static_cast<float>(i) / static_cast<float>(length - 1);
    const float eased = smoothstep(t);
    const float hill_delta = lerp_float(hill_delta_start, hill_delta_end, eased);
    running_height += hill_delta;
    g_track[write_index].curve = lerp_float(curve_start, curve_end, eased);
    g_track[write_index].height = running_height;
    g_track[write_index].scenery = scenery;
    ++write_index;
  }
}

void build_track() {
  float running_height = 0.0f;
  int write_index = 0;

  append_track_section(write_index, 32, 0.000f, 0.000f, 0.00f, 0.00f, app_config::SCENERY_PALM,
                       running_height);
  append_track_section(write_index, 32, 0.000f, 0.010f, 0.00f, 0.12f, app_config::SCENERY_PALM,
                       running_height);
  append_track_section(write_index, 32, 0.010f, 0.010f, 0.12f, -0.08f, app_config::SCENERY_PALM,
                       running_height);
  append_track_section(write_index, 32, 0.010f, 0.000f, -0.08f, -0.28f, app_config::SCENERY_ROCK,
                       running_height);
  append_track_section(write_index, 32, 0.000f, -0.007f, -0.28f, 0.18f, app_config::SCENERY_CITY,
                       running_height);
  append_track_section(write_index, 32, -0.007f, -0.014f, 0.18f, 0.28f, app_config::SCENERY_CITY,
                       running_height);
  append_track_section(write_index, 32, -0.014f, -0.004f, 0.28f, 0.00f, app_config::SCENERY_CITY,
                       running_height);
  append_track_section(write_index, 32, -0.004f, 0.000f, 0.00f, -0.18f, app_config::SCENERY_ROCK,
                       running_height);
  append_track_section(write_index, 32, 0.000f, 0.000f, -0.18f, -0.22f, app_config::SCENERY_PALM,
                       running_height);
  append_track_section(write_index, 32, 0.000f, 0.009f, -0.22f, 0.10f, app_config::SCENERY_PALM,
                       running_height);
  append_track_section(write_index, 32, 0.009f, 0.014f, 0.10f, 0.18f, app_config::SCENERY_ROCK,
                       running_height);
  append_track_section(write_index, 32, 0.014f, 0.004f, 0.18f, -0.04f, app_config::SCENERY_ROCK,
                       running_height);
  append_track_section(write_index, 32, 0.004f, -0.004f, -0.04f, 0.04f, app_config::SCENERY_CITY,
                       running_height);
  append_track_section(write_index, 32, -0.004f, -0.008f, 0.04f, 0.12f, app_config::SCENERY_CITY,
                       running_height);
  append_track_section(write_index, 32, -0.008f, 0.000f, 0.12f, 0.00f, app_config::SCENERY_GOAL,
                       running_height);

  while (write_index < app_config::TRACK_SEGMENTS) {
    g_track[write_index].curve = 0.0f;
    g_track[write_index].height = running_height;
    g_track[write_index].scenery = app_config::SCENERY_GOAL;
    ++write_index;
  }
}

uint16_t pick_traffic_color() {
  static const uint16_t kColors[] = {TFT_RED,     TFT_YELLOW, TFT_CYAN,    TFT_ORANGE,
                                     TFT_MAGENTA, TFT_GREEN,  TFT_SKYBLUE, TFT_PINK};
  return kColors[random(0, static_cast<int>(sizeof(kColors) / sizeof(kColors[0])))];
}

void respawn_traffic(TrafficCar& car, float base_z, float spacing) {
  car.z = wrap_track(base_z + spacing + static_cast<float>(random(0, 42)));
  car.lane = -0.62f + static_cast<float>(random(0, 3)) * 0.62f;
  car.speed = 0.26f + static_cast<float>(random(0, 40)) / 100.0f;
  car.body_color = pick_traffic_color();
}

void reset_stage() {
  g_game.mode = GameMode::Title;
  g_game.player_z = 0.0f;
  g_game.player_x = 0.0f;
  g_game.speed = 0.0f;
  g_game.distance_left = app_config::STAGE_DISTANCE;
  g_game.time_left_s = app_config::STAGE_TIME_S;
  g_game.crash_push = 0.0f;
  g_game.crash_timer = 0;
  g_game.gear = 1;
  g_game.auto_drive = false;
  g_game.score = 0;
  g_game.frame = 0;

  for (int i = 0; i < app_config::TRAFFIC_COUNT; ++i) {
    respawn_traffic(g_game.traffic[i], g_game.player_z, 18.0f + static_cast<float>(i) * 10.0f);
  }
}

void begin_run() {
  g_game.mode = GameMode::Playing;
  g_game.player_z = 0.0f;
  g_game.player_x = 0.0f;
  g_game.speed = 0.18f;
  g_game.distance_left = app_config::STAGE_DISTANCE;
  g_game.time_left_s = app_config::STAGE_TIME_S;
  g_game.crash_push = 0.0f;
  g_game.crash_timer = 0;
  g_game.gear = 1;
  g_game.auto_drive = false;
  g_game.score = 0;
  g_game.frame = 0;

  for (int i = 0; i < app_config::TRAFFIC_COUNT; ++i) {
    respawn_traffic(g_game.traffic[i], g_game.player_z, 20.0f + static_cast<float>(i) * 12.0f);
  }
}

float current_track_curve() {
  const int segment_index = static_cast<int>(g_game.player_z) % app_config::TRACK_SEGMENTS;
  return g_track[segment_index].curve;
}

float current_track_height() {
  const int segment_index = static_cast<int>(g_game.player_z) % app_config::TRACK_SEGMENTS;
  return g_track[segment_index].height;
}

void update_progress_state() {
  g_game.speed = clamp_float(g_game.speed, 0.0f, app_config::MAX_SPEED);
  g_game.player_x = clamp_float(g_game.player_x, -app_config::MAX_PLAYER_X, app_config::MAX_PLAYER_X);
  g_game.player_z = wrap_track(g_game.player_z + g_game.speed);
  g_game.distance_left -= g_game.speed;
  g_game.time_left_s -= static_cast<float>(app_config::FRAME_INTERVAL_MS) / 1000.0f;
  g_game.score += static_cast<uint32_t>(g_game.speed * 42.0f);

  const float speed_ratio = g_game.speed / app_config::MAX_SPEED;
  if (speed_ratio > 0.82f) {
    g_game.gear = 5;
  } else if (speed_ratio > 0.62f) {
    g_game.gear = 4;
  } else if (speed_ratio > 0.42f) {
    g_game.gear = 3;
  } else if (speed_ratio > 0.22f) {
    g_game.gear = 2;
  } else {
    g_game.gear = 1;
  }
}

float auto_lane_penalty(float lane_target) {
  float penalty = fabsf(lane_target) * 2.2f;

  for (int i = 0; i < app_config::TRAFFIC_COUNT; ++i) {
    const TrafficCar& car = g_game.traffic[i];
    const float distance = forward_distance(car.z, g_game.player_z);
    if (distance < 1.0f || distance > 18.0f) {
      continue;
    }

    const float lane_gap = fabsf(car.lane - lane_target);
    if (lane_gap < 0.46f) {
      penalty += (18.5f - distance) * (0.46f - lane_gap) * 7.5f;
    }
  }

  return penalty;
}

void update_auto_drive() {
  static const float kLaneTargets[] = {0.0f, -0.55f, 0.55f, -0.92f, 0.92f};
  float best_lane = 0.0f;
  float best_penalty = auto_lane_penalty(best_lane);
  float nearest_hazard_distance = 99.0f;

  for (const float lane_target : kLaneTargets) {
    const float penalty = auto_lane_penalty(lane_target);
    if (penalty < best_penalty) {
      best_penalty = penalty;
      best_lane = lane_target;
    }
  }

  for (int i = 0; i < app_config::TRAFFIC_COUNT; ++i) {
    const TrafficCar& car = g_game.traffic[i];
    const float distance = forward_distance(car.z, g_game.player_z);
    if (distance < nearest_hazard_distance && fabsf(car.lane - best_lane) < 0.42f) {
      nearest_hazard_distance = distance;
    }
  }

  float desired_speed = 0.64f;
  if (fabsf(current_track_curve()) > 0.010f) {
    desired_speed = 0.54f;
  }
  if (fabsf(g_game.player_x) > 0.75f) {
    desired_speed = 0.48f;
  }
  if (nearest_hazard_distance < 6.5f) {
    desired_speed = 0.42f;
  }

  if (g_game.speed < desired_speed - 0.02f) {
    g_game.speed += app_config::ACCELERATION * 0.92f;
  } else if (g_game.speed > desired_speed + 0.02f) {
    g_game.speed -= app_config::BRAKE_DECELERATION * 0.65f;
  } else {
    g_game.speed -= app_config::COAST_DECELERATION * 0.10f;
  }

  const float lane_delta = best_lane - g_game.player_x;
  const float auto_pull =
      clamp_float(lane_delta * (0.24f + fabsf(g_game.player_x) * 0.18f), -0.18f, 0.18f);
  g_game.player_x += auto_pull;
  g_game.player_x -= current_track_curve() * (11.0f + g_game.speed * 24.0f);
  g_game.player_x -= g_game.player_x * 0.090f;

  update_progress_state();
}

void update_player_motion(const InputState& input) {
  if (input.accel_held) {
    g_game.speed += app_config::ACCELERATION;
  } else if (input.brake_held) {
    g_game.speed -= app_config::BRAKE_DECELERATION;
  } else {
    g_game.speed -= app_config::COAST_DECELERATION;
  }

  if (fabsf(g_game.player_x) > app_config::ROAD_EDGE_X) {
    g_game.speed -= app_config::OFFROAD_DECELERATION;
  }

  g_game.speed = clamp_float(g_game.speed, 0.0f, app_config::MAX_SPEED);

  const bool steering_input = input.left_held != input.right_held;
  const float center_distance =
      clamp_float(fabsf(g_game.player_x) / app_config::ROAD_EDGE_X, 0.0f, 1.35f);
  float steering = app_config::STEERING_BASE + g_game.speed * app_config::STEERING_SPEED_SCALE +
                   center_distance * (app_config::STEERING_EDGE_BOOST +
                                      g_game.speed * app_config::STEERING_EDGE_SPEED_BOOST);
  if (fabsf(g_game.player_x) > app_config::ROAD_EDGE_X) {
    steering += 0.018f;
  }

  if (input.left_held && !input.right_held) {
    g_game.player_x -= steering;
  } else if (input.right_held && !input.left_held) {
    g_game.player_x += steering;
  }

  g_game.player_x -= current_track_curve() * (16.0f + g_game.speed * 34.0f);
  float centering_strength =
      app_config::CENTERING_ASSIST + g_game.speed * 0.022f +
      (fabsf(g_game.player_x) > app_config::ROAD_EDGE_X ? app_config::OFFROAD_CENTERING_ASSIST : 0.0f);
  if (steering_input) {
    centering_strength *= 0.42f;
  }
  g_game.player_x -= g_game.player_x * centering_strength;
  update_progress_state();
}

void trigger_crash(float lane) {
  g_game.mode = GameMode::Crash;
  g_game.crash_timer = app_config::CRASH_FRAMES;
  g_game.crash_push = lane >= g_game.player_x ? -1.0f : 1.0f;
  g_game.speed *= 0.45f;
}

void update_traffic() {
  for (int i = 0; i < app_config::TRAFFIC_COUNT; ++i) {
    TrafficCar& car = g_game.traffic[i];
    car.z = wrap_track(car.z + car.speed);

    const float distance = forward_distance(car.z, g_game.player_z);
    if (distance > static_cast<float>(app_config::TRACK_SEGMENTS) * 0.75f) {
      respawn_traffic(car, g_game.player_z,
                      static_cast<float>(app_config::DRAW_DISTANCE + 8 + random(0, 26)));
      g_game.score += 250;
      continue;
    }

    if (!g_game.auto_drive && distance > 0.45f && distance < 1.75f &&
        fabsf(car.lane - g_game.player_x) < 0.24f) {
      trigger_crash(car.lane);
    }
  }
}

void update_crash() {
  if (g_game.crash_timer > 0) {
    --g_game.crash_timer;
  }
  g_game.speed *= 0.90f;
  g_game.player_x += g_game.crash_push * 0.055f;
  g_game.player_x = clamp_float(g_game.player_x, -app_config::MAX_PLAYER_X, app_config::MAX_PLAYER_X);
  g_game.player_z = wrap_track(g_game.player_z + g_game.speed * 0.55f);

  if (g_game.crash_timer <= 0) {
    g_game.mode = GameMode::Playing;
    g_game.speed = clamp_float(g_game.speed, app_config::MIN_RECOVER_SPEED, app_config::MAX_SPEED);
    g_game.player_x *= 0.55f;
  }
}

void update_game(const InputState& input) {
  if (g_game.mode == GameMode::Title) {
    if (input.start_pressed || input.any_drive_input) {
      begin_run();
    }
    return;
  }

  if (g_game.mode == GameMode::GameOver || g_game.mode == GameMode::Goal) {
    if (input.start_pressed || input.any_drive_input) {
      begin_run();
    }
    return;
  }

  if (g_game.mode == GameMode::Playing && input.mode_toggle_pressed) {
    g_game.auto_drive = !g_game.auto_drive;
  }

  if (g_game.mode == GameMode::Crash) {
    update_crash();
  } else if (g_game.auto_drive) {
    update_auto_drive();
  } else {
    update_player_motion(input);
    update_traffic();
  }

  if (g_game.auto_drive) {
    update_traffic();
  }

  if (g_game.distance_left <= 0.0f) {
    g_game.mode = GameMode::Goal;
    g_game.best_score = max_u32(g_game.best_score, g_game.score);
  } else if (g_game.time_left_s <= 0.0f) {
    g_game.mode = GameMode::GameOver;
    g_game.best_score = max_u32(g_game.best_score, g_game.score);
  }

  ++g_game.frame;
}

void draw_background(M5Canvas& canvas) {
  canvas.fillScreen(0x33B7);
  canvas.fillCircle(194, 20, 13, 0xFDB6);
  canvas.fillRect(0, app_config::HORIZON_Y - 4, app_config::SCREEN_W, 8, 0x7E9F);

  const int shift = static_cast<int>(g_game.player_z * 2.0f) % app_config::SCREEN_W;
  for (int i = -1; i <= 3; ++i) {
    const int base_x = i * 90 - shift;
    canvas.fillTriangle(base_x, app_config::HORIZON_Y + 8, base_x + 30, app_config::HORIZON_Y - 15,
                        base_x + 60, app_config::HORIZON_Y + 8, 0x64D4);
    canvas.fillTriangle(base_x + 36, app_config::HORIZON_Y + 8, base_x + 68,
                        app_config::HORIZON_Y - 10, base_x + 98, app_config::HORIZON_Y + 8, 0x4C31);
  }

  canvas.fillRect(0, app_config::HORIZON_Y + 2, app_config::SCREEN_W, 12, 0x2A8A);
}

void build_projection() {
  const int base_segment = static_cast<int>(g_game.player_z) % app_config::TRACK_SEGMENTS;
  const float base_fraction = g_game.player_z - floorf(g_game.player_z);
  const float base_height = current_track_height();

  float curve_accum = 0.0f;
  float curve_velocity = 0.0f;

  for (int step = 1; step <= app_config::DRAW_DISTANCE + 1; ++step) {
    const int segment_index = (base_segment + step) % app_config::TRACK_SEGMENTS;
    const RoadSegment& segment = g_track[segment_index];
    const float z = static_cast<float>(step) - base_fraction;
    const float perspective = 1.0f / z;

    curve_velocity += segment.curve;
    curve_accum += curve_velocity;

    g_projected[step].road_w = perspective * app_config::ROAD_HALF_WIDTH;
    g_projected[step].shoulder_w = g_projected[step].road_w * app_config::SHOULDER_SCALE;
    g_projected[step].center_x =
        (app_config::SCREEN_W * 0.5f) + curve_accum * g_projected[step].road_w * 1.45f -
        g_game.player_x * g_projected[step].road_w * 0.95f;
    g_projected[step].y =
        static_cast<float>(app_config::HORIZON_Y) + perspective * app_config::CAMERA_HEIGHT -
        (segment.height - base_height) * perspective * app_config::HILL_SCALE;
    g_projected[step].scenery = segment.scenery;
  }
}

void draw_decor(M5Canvas& canvas, int segment_index, const ProjectedSegment& segment, bool left_side) {
  if (segment.road_w < 7.0f) {
    return;
  }

  if ((segment_index + (left_side ? 0 : 3)) % 7 != 0) {
    return;
  }

  const float side = left_side ? -1.0f : 1.0f;
  const int16_t base_x = static_cast<int16_t>(segment.center_x + side * (segment.road_w + segment.shoulder_w * 2.0f));
  const int16_t base_y = static_cast<int16_t>(segment.y);
  const int16_t height = static_cast<int16_t>(segment.road_w * 1.15f);

  if (base_y < app_config::HORIZON_Y || base_y > app_config::SCREEN_H + 10) {
    return;
  }

  switch (segment.scenery) {
    case app_config::SCENERY_PALM: {
      canvas.fillRect(base_x - 1, base_y - height, 3, height, 0x9A60);
      canvas.fillTriangle(base_x, base_y - height - 10, base_x - 10, base_y - height + 2,
                          base_x + 3, base_y - height + 2, TFT_GREEN);
      canvas.fillTriangle(base_x, base_y - height - 10, base_x + 10, base_y - height + 2,
                          base_x - 3, base_y - height + 2, TFT_GREEN);
      canvas.fillCircle(base_x, base_y - height - 5, 3, 0x3666);
      break;
    }
    case app_config::SCENERY_CITY: {
      const int16_t width = max_i16(4, static_cast<int16_t>(segment.road_w * 0.24f));
      canvas.fillRect(base_x - width / 2, base_y - height, width, height, 0x8C71);
      canvas.drawFastHLine(base_x - width / 2, base_y - height + 4, width, TFT_YELLOW);
      canvas.drawFastHLine(base_x - width / 2, base_y - height + 9, width, TFT_YELLOW);
      break;
    }
    case app_config::SCENERY_ROCK: {
      canvas.fillTriangle(base_x - 6, base_y, base_x, base_y - height, base_x + 7, base_y, 0x7BEF);
      canvas.drawLine(base_x - 3, base_y - 4, base_x + 2, base_y - height / 2, 0xC638);
      break;
    }
    case app_config::SCENERY_GOAL: {
      canvas.fillRect(base_x - 1, base_y - height, 3, height, TFT_WHITE);
      canvas.fillRect(base_x + static_cast<int16_t>(side * 10.0f), base_y - height, 3, height, TFT_WHITE);
      canvas.fillRect(min_int(base_x - 1, base_x + static_cast<int16_t>(side * 10.0f)),
                      base_y - height - 3, 13, 4, TFT_RED);
      break;
    }
  }
}

void draw_road(M5Canvas& canvas) {
  build_projection();

  for (int step = app_config::DRAW_DISTANCE; step >= 2; --step) {
    const int segment_index =
        (static_cast<int>(g_game.player_z) + step) % app_config::TRACK_SEGMENTS;
    const ProjectedSegment& far_seg = g_projected[step];
    const ProjectedSegment& near_seg = g_projected[step - 1];

    const int16_t top_y = static_cast<int16_t>(far_seg.y);
    const int16_t bottom_y = static_cast<int16_t>(near_seg.y);
    if (bottom_y <= top_y) {
      continue;
    }
    if (top_y > app_config::SCREEN_H || bottom_y < app_config::HORIZON_Y) {
      continue;
    }

    const bool alternate = ((segment_index / 3) & 1) == 0;
    const uint16_t grass_color = alternate ? 0x2643 : 0x1D82;
    const uint16_t road_color = alternate ? 0x6B4D : 0x4A49;
    const uint16_t rumble_color = alternate ? TFT_WHITE : TFT_RED;
    const uint16_t lane_color = alternate ? 0xF7DE : 0xBDF7;

    canvas.fillRect(0, top_y, app_config::SCREEN_W, bottom_y - top_y + 1, grass_color);

    const float far_left_outer = far_seg.center_x - far_seg.road_w - far_seg.shoulder_w;
    const float far_left_inner = far_seg.center_x - far_seg.road_w;
    const float far_right_inner = far_seg.center_x + far_seg.road_w;
    const float far_right_outer = far_seg.center_x + far_seg.road_w + far_seg.shoulder_w;

    const float near_left_outer = near_seg.center_x - near_seg.road_w - near_seg.shoulder_w;
    const float near_left_inner = near_seg.center_x - near_seg.road_w;
    const float near_right_inner = near_seg.center_x + near_seg.road_w;
    const float near_right_outer = near_seg.center_x + near_seg.road_w + near_seg.shoulder_w;

    fill_quad(canvas, far_left_outer, top_y, far_left_inner, top_y, near_left_inner, bottom_y,
              near_left_outer, bottom_y, rumble_color);
    fill_quad(canvas, far_right_inner, top_y, far_right_outer, top_y, near_right_outer, bottom_y,
              near_right_inner, bottom_y, rumble_color);
    fill_quad(canvas, far_left_inner, top_y, far_right_inner, top_y, near_right_inner, bottom_y,
              near_left_inner, bottom_y, road_color);

    if ((segment_index % 4) != 0) {
      for (int lane = -1; lane <= 1; lane += 2) {
        const float lane_offset = static_cast<float>(lane) * 0.34f;
        const float far_lane = far_seg.center_x + far_seg.road_w * lane_offset;
        const float near_lane = near_seg.center_x + near_seg.road_w * lane_offset;
        const float far_lane_w = max_float_value(1.0f, far_seg.road_w * 0.022f);
        const float near_lane_w = max_float_value(1.0f, near_seg.road_w * 0.022f);
        fill_quad(canvas, far_lane - far_lane_w, top_y, far_lane + far_lane_w, top_y,
                  near_lane + near_lane_w, bottom_y, near_lane - near_lane_w, bottom_y,
                  lane_color);
      }
    }

    draw_decor(canvas, segment_index, near_seg, true);
    draw_decor(canvas, segment_index, near_seg, false);
  }
}

void draw_traffic_car(M5Canvas& canvas, const TrafficCar& car) {
  const float distance = forward_distance(car.z, g_game.player_z);
  if (distance < 2.0f || distance >= static_cast<float>(app_config::DRAW_DISTANCE - 1)) {
    return;
  }

  const int near_step = static_cast<int>(distance);
  const float fraction = distance - static_cast<float>(near_step);
  const ProjectedSegment& a = g_projected[near_step];
  const ProjectedSegment& b = g_projected[near_step + 1];

  const float road_w = lerp_float(a.road_w, b.road_w, fraction);
  const float center_x = lerp_float(a.center_x, b.center_x, fraction);
  const float screen_y = lerp_float(a.y, b.y, fraction);

  if (road_w < 4.0f || screen_y < app_config::HORIZON_Y || screen_y > app_config::SCREEN_H + 4) {
    return;
  }

  const int16_t car_w = max_i16(5, static_cast<int16_t>(road_w * 0.24f));
  const int16_t car_h = max_i16(6, static_cast<int16_t>(road_w * 0.20f));
  const int16_t x = static_cast<int16_t>(center_x + car.lane * road_w * 0.82f);
  const int16_t y = static_cast<int16_t>(screen_y - car_h);

  canvas.fillRect(x - car_w / 2, y, car_w, car_h, car.body_color);
  canvas.fillRect(x - car_w / 3, y + 1, (car_w * 2) / 3, max_i16(2, car_h / 3), 0x8D9B);
  canvas.fillRect(x - car_w / 2, y + car_h - 2, 2, 2, TFT_RED);
  canvas.fillRect(x + car_w / 2 - 2, y + car_h - 2, 2, 2, TFT_RED);
}

void draw_player_car(M5Canvas& canvas) {
  const bool flash = g_game.mode == GameMode::Crash && ((g_game.crash_timer / 2) & 1) == 0;
  const int16_t center_x =
      static_cast<int16_t>((app_config::SCREEN_W / 2) + g_game.player_x * 16.0f);
  const int16_t base_y = app_config::SCREEN_H - 18;
  const int16_t body_w = 42;
  const int16_t body_h = 18;

  canvas.fillEllipse(center_x, base_y + 12, 24, 5, 0x0841);
  canvas.fillRect(center_x - body_w / 2, base_y, body_w, body_h, flash ? TFT_ORANGE : TFT_RED);
  canvas.fillRect(center_x - 14, base_y - 8, 28, 10, flash ? TFT_YELLOW : 0x8D9B);
  canvas.fillTriangle(center_x - 18, base_y, center_x - 6, base_y - 8, center_x - 2, base_y,
                      flash ? 0xFD20 : 0xD800);
  canvas.fillTriangle(center_x + 18, base_y, center_x + 6, base_y - 8, center_x + 2, base_y,
                      flash ? 0xFD20 : 0xD800);
  canvas.fillRect(center_x - 20, base_y + 3, 5, 8, 0x18C3);
  canvas.fillRect(center_x + 15, base_y + 3, 5, 8, 0x18C3);
  canvas.fillRect(center_x - 17, base_y + body_h - 3, 4, 2, TFT_RED);
  canvas.fillRect(center_x + 13, base_y + body_h - 3, 4, 2, TFT_RED);

  const int16_t wheel_offset = 18;
  const int16_t wheel_y = base_y + body_h - 1;
  canvas.fillRect(center_x - wheel_offset - 4, wheel_y, 8, 4, TFT_BLACK);
  canvas.fillRect(center_x + wheel_offset - 4, wheel_y, 8, 4, TFT_BLACK);
}

void draw_hud(M5Canvas& canvas) {
  canvas.fillRect(0, 0, app_config::SCREEN_W, 14, 0x1082);
  canvas.setTextColor(TFT_WHITE, 0x1082);
  canvas.setCursor(4, 3);
  canvas.printf("SCORE %06lu", static_cast<unsigned long>(g_game.score));

  canvas.setCursor(118, 3);
  canvas.printf("SPD %3d", static_cast<int>((g_game.speed / app_config::MAX_SPEED) * 280.0f));

  canvas.setCursor(188, 3);
  canvas.printf("G%d", g_game.gear);

  canvas.fillRect(0, app_config::SCREEN_H - 11, app_config::SCREEN_W, 11, 0x18C3);
  canvas.setTextColor(TFT_WHITE, 0x18C3);
  canvas.setCursor(4, app_config::SCREEN_H - 9);
  canvas.printf("TIME %02d", max_int(0, static_cast<int>(g_game.time_left_s + 0.5f)));

  canvas.setCursor(70, app_config::SCREEN_H - 9);
  canvas.printf("DIST %03d", max_int(0, static_cast<int>(g_game.distance_left)));

  if (g_game.auto_drive) {
    canvas.setTextColor(TFT_CYAN, 0x18C3);
    canvas.setCursor(150, app_config::SCREEN_H - 9);
    canvas.print("AUTO");
  } else if (fabsf(g_game.player_x) > app_config::ROAD_EDGE_X) {
    canvas.setTextColor(TFT_YELLOW, 0x18C3);
    canvas.setCursor(154, app_config::SCREEN_H - 9);
    canvas.print("OFFROAD");
  } else if (g_game.mode == GameMode::Crash) {
    canvas.setTextColor(TFT_ORANGE, 0x18C3);
    canvas.setCursor(154, app_config::SCREEN_H - 9);
    canvas.print("CRASH");
  } else {
    canvas.setTextColor(TFT_GREENYELLOW, 0x18C3);
    canvas.setCursor(154, app_config::SCREEN_H - 9);
    canvas.print("CRUISE");
  }
}

void draw_title_overlay(M5Canvas& canvas) {
  canvas.fillRoundRect(18, 18, 204, 108, 8, 0x0000);
  canvas.drawRoundRect(18, 18, 204, 108, 8, TFT_ORANGE);
  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.setCursor(74, 34);
  canvas.print("OUTRUN ADV");
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(33, 52);
  canvas.print("Clean-room Cardputer port");
  canvas.setCursor(42, 66);
  canvas.print("Any drive key starts");
  canvas.setCursor(47, 78);
  canvas.print(";/A accel  . brake");
  canvas.setCursor(47, 90);
  canvas.print(", left   / right");
  canvas.setCursor(47, 102);
  canvas.print("Enter / BtnA too");
  canvas.setCursor(47, 114);
  canvas.print("Enter in run: AUTO");
}

void draw_end_overlay(M5Canvas& canvas, bool cleared) {
  canvas.fillRoundRect(34, 34, 172, 62, 8, TFT_BLACK);
  canvas.drawRoundRect(34, 34, 172, 62, 8, cleared ? TFT_GREENYELLOW : TFT_RED);
  canvas.setTextColor(cleared ? TFT_GREENYELLOW : TFT_RED, TFT_BLACK);
  canvas.setCursor(cleared ? 82 : 74, 44);
  canvas.print(cleared ? "STAGE CLEAR" : "GAME OVER");
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(56, 61);
  canvas.printf("SCORE %06lu", static_cast<unsigned long>(g_game.score));
  canvas.setCursor(58, 74);
  canvas.printf("BEST  %06lu", static_cast<unsigned long>(max_u32(g_game.best_score, g_game.score)));
  canvas.setCursor(52, 87);
  canvas.print("Any drive key retry");
}

void draw_scene(M5Canvas& canvas) {
  draw_background(canvas);
  draw_road(canvas);

  for (int i = 0; i < app_config::TRAFFIC_COUNT; ++i) {
    draw_traffic_car(canvas, g_game.traffic[i]);
  }

  draw_player_car(canvas);
  draw_hud(canvas);

  if (g_game.mode == GameMode::Title) {
    draw_title_overlay(canvas);
  } else if (g_game.mode == GameMode::GameOver) {
    draw_end_overlay(canvas, false);
  } else if (g_game.mode == GameMode::Goal) {
    draw_end_overlay(canvas, true);
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

  build_track();
  reset_stage();
  g_last_frame_ms = millis();

  Serial.println("Cardputer ADV OutRun clean-room demo started");
}

void loop() {
  M5Cardputer.update();
  const InputState input = read_input();

  const uint32_t now = millis();
  if (now - g_last_frame_ms < app_config::FRAME_INTERVAL_MS) {
    return;
  }

  update_game(input);
  draw_scene(g_canvas);

  M5Cardputer.Display.startWrite();
  g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
  M5Cardputer.Display.endWrite();

  g_last_frame_ms = now;
}
