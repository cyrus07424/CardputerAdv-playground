#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>
#include "PescadoCore.h"

namespace app_config {
constexpr int16_t SCREEN_W = 240;
constexpr int16_t SCREEN_H = 135;
constexpr int16_t RENDER_W = SCREEN_W;
constexpr int16_t RENDER_H = SCREEN_H;
constexpr uint8_t TEXTURE_SIZE = 8;

constexpr int WORLD_W = 32;
constexpr int WORLD_H = 18;
constexpr int WORLD_D = 32;
constexpr int WATER_LEVEL = 4;

constexpr uint32_t FRAME_INTERVAL_MS = 50;
constexpr float MAX_DT_SEC = 0.05f;
constexpr float CAMERA_HEIGHT = 1.55f;
constexpr float PLAYER_HEIGHT = 1.70f;
constexpr float PLAYER_RADIUS = 0.28f;
constexpr float WALK_SPEED = 2.8f;
constexpr float FLY_SPEED = 4.4f;
constexpr float GRAVITY = 15.0f;
constexpr float JUMP_VELOCITY = 5.2f;
constexpr float TURN_SPEED_DEG = 110.0f;
constexpr float LOOK_SPEED_DEG = 90.0f;
constexpr float H_FOV_DEG = 68.0f;
constexpr float RAY_MAX_DISTANCE = 24.0f;
constexpr float TARGET_MAX_DISTANCE = 6.5f;
constexpr int MAX_RAY_STEPS = 56;
constexpr uint32_t INITIAL_SEED = 1337U;
}  // namespace app_config

enum BlockType : uint8_t {
  BLOCK_AIR = 0,
  BLOCK_GRASS,
  BLOCK_DIRT,
  BLOCK_STONE,
  BLOCK_SAND,
  BLOCK_WATER,
  BLOCK_LOG,
  BLOCK_LEAVES,
  BLOCK_BRICKS,
  BLOCK_GLASS,
  BLOCK_BEDROCK,
  BLOCK_CACTUS,
  BLOCK_COUNT
};

using Vec3 = pescado_core::Vec3;
using Mat3 = pescado_core::Matrix3x3;

struct ColorRgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct RayHit {
  bool hit;
  uint8_t block;
  int x;
  int y;
  int z;
  int prev_x;
  int prev_y;
  int prev_z;
  int axis;
  int face_sign;
  float distance;
};

struct PlayerState {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float velocity_y = 0.0f;
  float yaw_deg = 0.0f;
  float pitch_deg = -8.0f;
  bool on_ground = false;
  bool fly_mode = false;
  uint8_t selected_slot = 0;
};

struct ResolutionMode {
  uint8_t pixel_step;
  const char* label;
};

struct InputLatch {
  bool move_forward = false;
  bool move_back = false;
  bool move_left = false;
  bool move_right = false;
  bool turn_left = false;
  bool turn_right = false;
  bool look_up = false;
  bool look_down = false;
  bool move_down = false;
  bool move_up = false;
  bool jump = false;
  bool break_block = false;
  bool place_block = false;
  bool cycle_block = false;
  bool toggle_footer = false;
  bool toggle_help = false;
  bool toggle_header = false;
  bool toggle_fly = false;
  bool reset_world = false;
};

M5Canvas g_canvas(&M5Cardputer.Display);
uint8_t g_world[app_config::WORLD_W][app_config::WORLD_H][app_config::WORLD_D];
float g_column_factor[app_config::RENDER_W];
float g_row_factor[app_config::RENDER_H];
ColorRgb g_face_textures[BLOCK_COUNT][3][app_config::TEXTURE_SIZE * app_config::TEXTURE_SIZE];
uint16_t g_sky_colors[app_config::RENDER_H];
uint16_t g_ground_colors[app_config::RENDER_H];
uint16_t g_scanline[app_config::RENDER_W];

PlayerState g_player;
RayHit g_target = {};
uint32_t g_seed = app_config::INITIAL_SEED;
uint32_t g_last_tick_ms = 0;
uint32_t g_last_draw_ms = 0;
uint32_t g_frame_counter = 0;
uint32_t g_last_fps_sample_ms = 0;
uint16_t g_fps = 0;
bool g_show_help = true;
bool g_show_header = true;
bool g_show_footer = true;
InputLatch g_latched_input;

bool g_prev_break = false;
bool g_prev_place = false;
bool g_prev_cycle = false;
bool g_prev_jump = false;
bool g_prev_fly_toggle = false;
bool g_prev_reset = false;
bool g_prev_help_toggle = false;
bool g_prev_header_toggle = false;
bool g_prev_footer_toggle = false;

constexpr uint8_t kSelectableBlocks[] = {
    BLOCK_GRASS,
    BLOCK_DIRT,
    BLOCK_STONE,
    BLOCK_SAND,
    BLOCK_LOG,
    BLOCK_LEAVES,
    BLOCK_BRICKS,
    BLOCK_GLASS,
    BLOCK_CACTUS,
};

constexpr ResolutionMode kResolutionModes[] = {
    {1, "240x135"},
    {2, "120x68"},
    {3, "80x45"},
    {4, "60x34"},
};
uint8_t g_motion_resolution_mode_index = 1;
uint8_t g_dynamic_motion_resolution_mode_index = 1;
uint32_t g_motion_resolution_until_ms = 0;
uint32_t g_last_frame_present_ms = 0;
uint16_t g_last_frame_interval_ms = 0;
uint8_t g_good_motion_frames = 0;
uint8_t g_forced_low_res_frames = 0;

float clampf(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

float lerpf(float a, float b, float t) {
  return a + (b - a) * t;
}

float smoothstepf(float t) {
  return t * t * (3.0f - 2.0f * t);
}

uint32_t hash_u32(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  value ^= value >> 16;
  return value;
}

uint32_t hash_coords(int x, int z, uint32_t seed) {
  uint32_t hx = static_cast<uint32_t>(x) * 374761393U;
  uint32_t hz = static_cast<uint32_t>(z) * 668265263U;
  return hash_u32(hx ^ (hz + seed * 1442695041U));
}

float hash01(int x, int z, uint32_t seed) {
  return static_cast<float>(hash_coords(x, z, seed) & 0x00FFFFFFU) / 16777215.0f;
}

float value_noise(float x, float z, uint32_t seed) {
  const int ix = static_cast<int>(floorf(x));
  const int iz = static_cast<int>(floorf(z));
  const float tx = x - ix;
  const float tz = z - iz;
  const float sx = smoothstepf(tx);
  const float sz = smoothstepf(tz);

  const float n00 = hash01(ix, iz, seed);
  const float n10 = hash01(ix + 1, iz, seed);
  const float n01 = hash01(ix, iz + 1, seed);
  const float n11 = hash01(ix + 1, iz + 1, seed);

  return lerpf(lerpf(n00, n10, sx), lerpf(n01, n11, sx), sz);
}

float fbm_noise(float x, float z, uint32_t seed) {
  float amplitude = 0.5f;
  float frequency = 1.0f;
  float total = 0.0f;
  float norm = 0.0f;
  for (int octave = 0; octave < 4; ++octave) {
    total += value_noise(x * frequency, z * frequency, seed + octave * 977U) * amplitude;
    norm += amplitude;
    amplitude *= 0.5f;
    frequency *= 2.0f;
  }
  if (norm <= 0.0f) {
    return 0.0f;
  }
  return total / norm;
}

Vec3 make_vec3(float x, float y, float z) {
  return Vec3(x, y, z);
}

Vec3 add_vec3(const Vec3& a, const Vec3& b) {
  return a + b;
}

Vec3 scale_vec3(const Vec3& v, float scale) {
  return v * scale;
}

Vec3 cross_vec3(const Vec3& a, const Vec3& b) {
  return pescado_core::CrossProduct(a, b);
}

float length_vec3(const Vec3& v) {
  return v.Magnitude();
}

Vec3 normalize_vec3(const Vec3& v) {
  return v.Normalized();
}

bool in_world(int x, int y, int z) {
  return x >= 0 && x < app_config::WORLD_W &&
         y >= 0 && y < app_config::WORLD_H &&
         z >= 0 && z < app_config::WORLD_D;
}

uint8_t get_block(int x, int y, int z) {
  if (!in_world(x, y, z)) {
    return BLOCK_BEDROCK;
  }
  return g_world[x][y][z];
}

void set_block(int x, int y, int z, uint8_t block) {
  if (in_world(x, y, z)) {
    g_world[x][y][z] = block;
  }
}

bool is_solid_block(uint8_t block) {
  return block != BLOCK_AIR && block != BLOCK_WATER;
}

const char* block_name(uint8_t block) {
  switch (block) {
    case BLOCK_GRASS: return "Grass";
    case BLOCK_DIRT: return "Dirt";
    case BLOCK_STONE: return "Stone";
    case BLOCK_SAND: return "Sand";
    case BLOCK_WATER: return "Water";
    case BLOCK_LOG: return "Log";
    case BLOCK_LEAVES: return "Leaves";
    case BLOCK_BRICKS: return "Bricks";
    case BLOCK_GLASS: return "Glass";
    case BLOCK_BEDROCK: return "Bedrock";
    case BLOCK_CACTUS: return "Cactus";
    default: return "Air";
  }
}

ColorRgb make_rgb(uint8_t r, uint8_t g, uint8_t b) {
  ColorRgb color = {r, g, b};
  return color;
}

ColorRgb shade_rgb(const ColorRgb& color, float factor) {
  return make_rgb(
      static_cast<uint8_t>(clampf(color.r * factor, 0.0f, 255.0f)),
      static_cast<uint8_t>(clampf(color.g * factor, 0.0f, 255.0f)),
      static_cast<uint8_t>(clampf(color.b * factor, 0.0f, 255.0f)));
}

uint16_t rgb_to_565(const ColorRgb& color) {
  return lgfx::v1::color565(color.r, color.g, color.b);
}

uint16_t output_color_565(const ColorRgb& color) {
  return rgb_to_565(color);
}

ColorRgb block_base_color(uint8_t block) {
  switch (block) {
    case BLOCK_GRASS: return make_rgb(120, 184, 72);
    case BLOCK_DIRT: return make_rgb(124, 84, 52);
    case BLOCK_STONE: return make_rgb(136, 136, 140);
    case BLOCK_SAND: return make_rgb(218, 206, 132);
    case BLOCK_WATER: return make_rgb(52, 110, 204);
    case BLOCK_LOG: return make_rgb(120, 92, 56);
    case BLOCK_LEAVES: return make_rgb(52, 142, 60);
    case BLOCK_BRICKS: return make_rgb(168, 76, 64);
    case BLOCK_GLASS: return make_rgb(164, 214, 224);
    case BLOCK_BEDROCK: return make_rgb(74, 74, 82);
    case BLOCK_CACTUS: return make_rgb(42, 154, 72);
    default: return make_rgb(0, 0, 0);
  }
}

constexpr uint8_t FACE_TEXTURE_TOP = 0;
constexpr uint8_t FACE_TEXTURE_SIDE = 1;
constexpr uint8_t FACE_TEXTURE_BOTTOM = 2;

float fracf_positive(float value) {
  return value - floorf(value);
}

uint8_t texture_noise(int block, int face, int x, int y) {
  return static_cast<uint8_t>(
      hash_u32(
          static_cast<uint32_t>(block * 92821 + face * 1237 + x * 97 + y * 57 + 17)) &
      0xFFU);
}

void set_texture_texel(uint8_t block, uint8_t face, int x, int y, const ColorRgb& color) {
  g_face_textures[block][face][y * app_config::TEXTURE_SIZE + x] = color;
}

void build_block_textures() {
  const ColorRgb white = make_rgb(255, 255, 255);
  const ColorRgb green_top = make_rgb(122, 196, 78);
  const ColorRgb dirt = make_rgb(124, 84, 52);
  const ColorRgb sand = make_rgb(218, 206, 132);
  const ColorRgb water = make_rgb(54, 110, 210);
  const ColorRgb water_highlight = make_rgb(120, 210, 255);
  const ColorRgb wood = make_rgb(120, 92, 56);
  const ColorRgb leaf = make_rgb(56, 144, 60);
  const ColorRgb mortar = make_rgb(164, 164, 170);
  const ColorRgb brick = make_rgb(168, 76, 64);
  const ColorRgb glass = make_rgb(110, 180, 200);
  const ColorRgb bedrock = make_rgb(74, 74, 82);
  const ColorRgb cactus = make_rgb(42, 154, 72);
  const ColorRgb cactus_dark = make_rgb(30, 108, 52);
  const ColorRgb cactus_core = make_rgb(104, 88, 48);

  for (int block = 0; block < BLOCK_COUNT; ++block) {
    for (int face = 0; face < 3; ++face) {
      for (int y = 0; y < app_config::TEXTURE_SIZE; ++y) {
        for (int x = 0; x < app_config::TEXTURE_SIZE; ++x) {
          const uint8_t noise = texture_noise(block, face, x, y);
          const float noise_factor = 0.82f + (static_cast<float>(noise) / 255.0f) * 0.30f;
          ColorRgb color = shade_rgb(block_base_color(static_cast<uint8_t>(block)), noise_factor);

          switch (block) {
            case BLOCK_GRASS:
              if (face == FACE_TEXTURE_TOP) {
                const bool patch = ((x ^ y) & 1) == 0;
                color = shade_rgb(green_top, patch ? 1.02f : 0.88f + noise_factor * 0.10f);
              } else if (face == FACE_TEXTURE_SIDE) {
                if (y < 2) {
                  color = shade_rgb(green_top, 0.90f + 0.04f * (2 - y));
                } else {
                  color = shade_rgb(dirt, 0.76f + (noise_factor - 0.82f) * 0.6f);
                }
              } else {
                color = shade_rgb(dirt, 0.80f + (noise_factor - 0.82f) * 0.8f);
              }
              break;
            case BLOCK_DIRT:
              color = shade_rgb(dirt, 0.74f + (static_cast<float>(noise) / 255.0f) * 0.35f);
              break;
            case BLOCK_STONE:
              color = shade_rgb(make_rgb(136, 136, 140), 0.68f + (static_cast<float>(noise) / 255.0f) * 0.42f);
              break;
            case BLOCK_SAND:
              color = shade_rgb(sand, 0.86f + (static_cast<float>((noise + y * 17) & 0xFFU) / 255.0f) * 0.20f);
              break;
            case BLOCK_WATER:
              color = shade_rgb(water, ((x + y) & 1) == 0 ? 0.82f : 0.66f);
              if ((y == 1 || y == 5) && x > 0 && x < 7) {
                color = shade_rgb(water_highlight, 0.92f);
              }
              break;
            case BLOCK_LOG:
              if (face == FACE_TEXTURE_SIDE) {
                const bool stripe = (x == 1 || x == 4 || x == 6);
                color = shade_rgb(wood, stripe ? 0.72f : 0.96f);
              } else {
                const int dx = x - 3;
                const int dy = y - 3;
                const int dist2 = dx * dx + dy * dy;
                color = shade_rgb(wood, 0.72f + (dist2 % 5) * 0.07f);
              }
              break;
            case BLOCK_LEAVES:
              color = shade_rgb(leaf, ((noise & 0x03U) == 0U) ? 0.60f : 0.92f);
              break;
            case BLOCK_BRICKS:
              if (y == 0 || y == 4 || x == 0) {
                color = shade_rgb(mortar, 0.50f);
              } else if (y > 4 && x == 4) {
                color = shade_rgb(mortar, 0.50f);
              } else {
                color = shade_rgb(brick, 0.75f + (static_cast<float>(noise) / 255.0f) * 0.25f);
              }
              break;
            case BLOCK_GLASS:
              color = shade_rgb(glass, 0.30f);
              if (x == 0 || y == 0 || x == 7 || y == 7 || x == y || x + y == 7) {
                color = shade_rgb(white, 0.62f);
              }
              break;
            case BLOCK_BEDROCK:
              color = shade_rgb(bedrock, 0.52f + (static_cast<float>(noise) / 255.0f) * 0.18f);
              break;
            case BLOCK_CACTUS:
              if (face == FACE_TEXTURE_SIDE) {
                const bool rib = (x == 1 || x == 4 || x == 6);
                color = shade_rgb(cactus, rib ? 0.64f : 0.90f);
                if (x == 0 || x == 7) {
                  color = shade_rgb(cactus_dark, 0.90f);
                }
              } else {
                color = shade_rgb(cactus, 0.86f);
                if (x == 3 || y == 3 || x == 4 || y == 4) {
                  color = shade_rgb(cactus_core, 0.60f);
                }
              }
              break;
            default:
              break;
          }

          set_texture_texel(static_cast<uint8_t>(block), static_cast<uint8_t>(face), x, y, color);
        }
      }
    }
  }
}

int top_solid_y(int x, int z) {
  for (int y = app_config::WORLD_H - 1; y >= 0; --y) {
    const uint8_t block = get_block(x, y, z);
    if (block != BLOCK_AIR && block != BLOCK_WATER) {
      return y;
    }
  }
  return 0;
}

void place_tree(int x, int y, int z) {
  if (y < app_config::WATER_LEVEL + 2 || y + 4 >= app_config::WORLD_H) {
    return;
  }
  for (int i = 1; i <= 3; ++i) {
    set_block(x, y + i, z, BLOCK_LOG);
  }
  for (int dx = -2; dx <= 2; ++dx) {
    for (int dz = -2; dz <= 2; ++dz) {
      for (int dy = 3; dy <= 4; ++dy) {
        if (abs(dx) + abs(dz) < 4) {
          set_block(x + dx, y + dy, z + dz, BLOCK_LEAVES);
        }
      }
    }
  }
}

void place_cactus(int x, int y, int z, int height) {
  if (y < app_config::WATER_LEVEL + 1 || y + height >= app_config::WORLD_H) {
    return;
  }
  for (int i = 1; i <= height; ++i) {
    set_block(x, y + i, z, BLOCK_CACTUS);
  }
}

void reset_world() {
  for (int x = 0; x < app_config::WORLD_W; ++x) {
    for (int y = 0; y < app_config::WORLD_H; ++y) {
      for (int z = 0; z < app_config::WORLD_D; ++z) {
        g_world[x][y][z] = BLOCK_AIR;
      }
    }
  }

  for (int x = 0; x < app_config::WORLD_W; ++x) {
    for (int z = 0; z < app_config::WORLD_D; ++z) {
      const float biome_noise = value_noise(x * 0.10f, z * 0.10f, g_seed ^ 0xA53CU);
      const bool desert = biome_noise > 0.60f;
      const float hills = fbm_noise(x * 0.12f, z * 0.12f, g_seed ^ 0x91E1U);
      const float detail = value_noise(x * 0.33f, z * 0.33f, g_seed ^ 0x44B9U);
      int height = 3 + static_cast<int>(hills * 7.0f + detail * 2.0f);
      height = static_cast<int>(clampf(height, 2.0f, app_config::WORLD_H - 3.0f));

      for (int y = 0; y <= height; ++y) {
        uint8_t block = BLOCK_STONE;
        if (y == 0) {
          block = BLOCK_BEDROCK;
        } else if (desert) {
          block = (y >= height - 3) ? BLOCK_SAND : BLOCK_STONE;
        } else if (y == height) {
          block = BLOCK_GRASS;
        } else if (y >= height - 2) {
          block = BLOCK_DIRT;
        }
        set_block(x, y, z, block);
      }

      if (height < app_config::WATER_LEVEL) {
        for (int y = height + 1; y <= app_config::WATER_LEVEL; ++y) {
          set_block(x, y, z, BLOCK_WATER);
        }
      }
    }
  }

  for (int x = 2; x < app_config::WORLD_W - 2; ++x) {
    for (int z = 2; z < app_config::WORLD_D - 2; ++z) {
      const int y = top_solid_y(x, z);
      const uint8_t top_block = get_block(x, y, z);
      if (top_block == BLOCK_GRASS) {
        if ((hash_coords(x, z, g_seed ^ 0x2222U) & 0xFFU) < 4U) {
          place_tree(x, y, z);
        }
      } else if (top_block == BLOCK_SAND) {
        const uint8_t roll = static_cast<uint8_t>(hash_coords(x, z, g_seed ^ 0x5555U) & 0x1FU);
        if (roll == 0U) {
          place_cactus(x, y, z, 2 + static_cast<int>(hash_coords(x, z, g_seed ^ 0x7777U) % 3U));
        }
      }
    }
  }

  int spawn_x = app_config::WORLD_W / 2;
  int spawn_z = app_config::WORLD_D / 2;
  int best_y = 0;
  for (int radius = 0; radius < 8; ++radius) {
    for (int dz = -radius; dz <= radius; ++dz) {
      for (int dx = -radius; dx <= radius; ++dx) {
        const int x = app_config::WORLD_W / 2 + dx;
        const int z = app_config::WORLD_D / 2 + dz;
        if (!in_world(x, 1, z)) {
          continue;
        }
        const int y = top_solid_y(x, z);
        const uint8_t top_block = get_block(x, y, z);
        if (top_block != BLOCK_WATER && top_block != BLOCK_LEAVES && top_block != BLOCK_CACTUS) {
          spawn_x = x;
          spawn_z = z;
          best_y = y;
          radius = 99;
          break;
        }
      }
    }
  }

  g_player.x = spawn_x + 0.5f;
  g_player.z = spawn_z + 0.5f;
  g_player.y = best_y + 1.01f;
  g_player.velocity_y = 0.0f;
  g_player.yaw_deg = 35.0f;
  g_player.pitch_deg = -8.0f;
  g_player.on_ground = true;
  g_target.hit = false;
}

Vec3 player_eye_position() {
  return make_vec3(g_player.x, g_player.y + app_config::CAMERA_HEIGHT, g_player.z);
}

Mat3 camera_rotation_matrix() {
  const float yaw_rad = g_player.yaw_deg * DEG_TO_RAD;
  const float pitch_rad = g_player.pitch_deg * DEG_TO_RAD;
  return Mat3::RotY(-yaw_rad) * Mat3::RotZ(pitch_rad);
}

Vec3 camera_forward() {
  return normalize_vec3(camera_rotation_matrix() * make_vec3(1.0f, 0.0f, 0.0f));
}

Vec3 camera_right() {
  return normalize_vec3(camera_rotation_matrix() * make_vec3(0.0f, 0.0f, 1.0f));
}

Vec3 camera_up() {
  return normalize_vec3(camera_rotation_matrix() * make_vec3(0.0f, 1.0f, 0.0f));
}

uint8_t active_resolution_mode_index() {
  if (g_forced_low_res_frames > 0) {
    return g_dynamic_motion_resolution_mode_index;
  }
  return millis() < g_motion_resolution_until_ms ? g_dynamic_motion_resolution_mode_index : 0;
}

void request_low_res_redraw(uint8_t frames = 1U) {
  if (millis() >= g_motion_resolution_until_ms) {
    g_dynamic_motion_resolution_mode_index = g_motion_resolution_mode_index;
    g_good_motion_frames = 0;
  }
  if (frames > g_forced_low_res_frames) {
    g_forced_low_res_frames = frames;
  }
}

void update_dynamic_motion_resolution() {
  if (millis() >= g_motion_resolution_until_ms) {
    g_dynamic_motion_resolution_mode_index = g_motion_resolution_mode_index;
    g_good_motion_frames = 0;
    return;
  }

  if (g_last_frame_interval_ms > 66U) {
    if (g_dynamic_motion_resolution_mode_index + 1 < sizeof(kResolutionModes) / sizeof(kResolutionModes[0])) {
      ++g_dynamic_motion_resolution_mode_index;
    }
    g_good_motion_frames = 0;
    return;
  }

  if (g_last_frame_interval_ms > 0U && g_last_frame_interval_ms < 58U) {
    if (++g_good_motion_frames >= 12U &&
        g_dynamic_motion_resolution_mode_index > g_motion_resolution_mode_index) {
      --g_dynamic_motion_resolution_mode_index;
      g_good_motion_frames = 0;
    }
  } else {
    g_good_motion_frames = 0;
  }
}

bool collides_aabb(float center_x, float feet_y, float center_z) {
  const float min_x = center_x - app_config::PLAYER_RADIUS;
  const float max_x = center_x + app_config::PLAYER_RADIUS;
  const float min_y = feet_y;
  const float max_y = feet_y + app_config::PLAYER_HEIGHT;
  const float min_z = center_z - app_config::PLAYER_RADIUS;
  const float max_z = center_z + app_config::PLAYER_RADIUS;

  const int start_x = static_cast<int>(floorf(min_x));
  const int end_x = static_cast<int>(floorf(max_x));
  const int start_y = static_cast<int>(floorf(min_y));
  const int end_y = static_cast<int>(floorf(max_y));
  const int start_z = static_cast<int>(floorf(min_z));
  const int end_z = static_cast<int>(floorf(max_z));

  for (int x = start_x; x <= end_x; ++x) {
    for (int y = start_y; y <= end_y; ++y) {
      for (int z = start_z; z <= end_z; ++z) {
        if (!in_world(x, y, z)) {
          return true;
        }
        if (is_solid_block(get_block(x, y, z))) {
          return true;
        }
      }
    }
  }
  return false;
}

void try_move_horizontal(float dx, float dz) {
  if (g_player.fly_mode) {
    g_player.x = clampf(g_player.x + dx, 1.1f, app_config::WORLD_W - 1.1f);
    g_player.z = clampf(g_player.z + dz, 1.1f, app_config::WORLD_D - 1.1f);
    return;
  }

  if (fabsf(dx) > 0.0001f) {
    const float test_x = g_player.x + dx;
    if (!collides_aabb(test_x, g_player.y, g_player.z)) {
      g_player.x = test_x;
    } else if (!collides_aabb(test_x, g_player.y + 1.02f, g_player.z)) {
      g_player.x = test_x;
      g_player.y += 1.0f;
    }
  }

  if (fabsf(dz) > 0.0001f) {
    const float test_z = g_player.z + dz;
    if (!collides_aabb(g_player.x, g_player.y, test_z)) {
      g_player.z = test_z;
    } else if (!collides_aabb(g_player.x, g_player.y + 1.02f, test_z)) {
      g_player.z = test_z;
      g_player.y += 1.0f;
    }
  }

  g_player.x = clampf(g_player.x, 1.1f, app_config::WORLD_W - 1.1f);
  g_player.z = clampf(g_player.z, 1.1f, app_config::WORLD_D - 1.1f);
}

void apply_vertical_motion(float dt) {
  if (g_player.fly_mode) {
    g_player.on_ground = false;
    return;
  }

  g_player.velocity_y -= app_config::GRAVITY * dt;
  float remaining = g_player.velocity_y * dt;
  const float step = remaining >= 0.0f ? 0.08f : -0.08f;
  g_player.on_ground = false;

  while (fabsf(remaining) > 0.001f) {
    const float delta = fabsf(remaining) > fabsf(step) ? step : remaining;
    if (collides_aabb(g_player.x, g_player.y + delta, g_player.z)) {
      if (delta < 0.0f) {
        g_player.on_ground = true;
      }
      g_player.velocity_y = 0.0f;
      break;
    }
    g_player.y += delta;
    remaining -= delta;
  }

  g_player.y = clampf(g_player.y, 1.01f, app_config::WORLD_H - app_config::PLAYER_HEIGHT - 0.1f);
}

Vec3 forward_vector() {
  return camera_forward();
}

RayHit cast_ray(const Vec3& origin, const Vec3& dir, float max_distance) {
  RayHit hit = {};
  hit.hit = false;

  int map_x = static_cast<int>(floorf(origin.x));
  int map_y = static_cast<int>(floorf(origin.y));
  int map_z = static_cast<int>(floorf(origin.z));

  const float inf = 1.0e9f;
  const float delta_x = fabsf(dir.x) < 0.00001f ? inf : fabsf(1.0f / dir.x);
  const float delta_y = fabsf(dir.y) < 0.00001f ? inf : fabsf(1.0f / dir.y);
  const float delta_z = fabsf(dir.z) < 0.00001f ? inf : fabsf(1.0f / dir.z);

  const int step_x = dir.x < 0.0f ? -1 : 1;
  const int step_y = dir.y < 0.0f ? -1 : 1;
  const int step_z = dir.z < 0.0f ? -1 : 1;

  float side_x = dir.x < 0.0f ? (origin.x - map_x) * delta_x : (map_x + 1.0f - origin.x) * delta_x;
  float side_y = dir.y < 0.0f ? (origin.y - map_y) * delta_y : (map_y + 1.0f - origin.y) * delta_y;
  float side_z = dir.z < 0.0f ? (origin.z - map_z) * delta_z : (map_z + 1.0f - origin.z) * delta_z;

  for (int step = 0; step < app_config::MAX_RAY_STEPS; ++step) {
    const int prev_x = map_x;
    const int prev_y = map_y;
    const int prev_z = map_z;
    int axis = 0;
    int face_sign = 0;
    float distance = 0.0f;

    if (side_x < side_y && side_x < side_z) {
      map_x += step_x;
      distance = side_x;
      side_x += delta_x;
      axis = 0;
      face_sign = -step_x;
    } else if (side_y < side_z) {
      map_y += step_y;
      distance = side_y;
      side_y += delta_y;
      axis = 1;
      face_sign = -step_y;
    } else {
      map_z += step_z;
      distance = side_z;
      side_z += delta_z;
      axis = 2;
      face_sign = -step_z;
    }

    if (distance > max_distance) {
      return hit;
    }

    if (!in_world(map_x, map_y, map_z)) {
      return hit;
    }

    const uint8_t block = get_block(map_x, map_y, map_z);
    if (block != BLOCK_AIR) {
      hit.hit = true;
      hit.block = block;
      hit.x = map_x;
      hit.y = map_y;
      hit.z = map_z;
      hit.prev_x = prev_x;
      hit.prev_y = prev_y;
      hit.prev_z = prev_z;
      hit.axis = axis;
      hit.face_sign = face_sign;
      hit.distance = distance;
      return hit;
    }
  }

  return hit;
}

bool key_pressed(const Keyboard_Class::KeysState& status, char lower) {
  const char upper =
      (lower >= 'a' && lower <= 'z') ? static_cast<char>(lower - 'a' + 'A') : lower;
  for (const char key : status.word) {
    if (key == lower || key == upper) {
      return true;
    }
  }
  return false;
}

bool motion_input_active(const Keyboard_Class::KeysState& status) {
  return key_pressed(status, 'w') || key_pressed(status, 'a') ||
         key_pressed(status, 's') || key_pressed(status, 'd') ||
         key_pressed(status, 'j') || key_pressed(status, 'l') ||
         key_pressed(status, 'i') || key_pressed(status, 'k') ||
         key_pressed(status, 'q') || key_pressed(status, 'e') ||
         key_pressed(status, 'g');
}

void latch_render_input(const Keyboard_Class::KeysState& status) {
  g_latched_input.move_forward |= key_pressed(status, 'w');
  g_latched_input.move_back |= key_pressed(status, 's');
  g_latched_input.move_left |= key_pressed(status, 'a');
  g_latched_input.move_right |= key_pressed(status, 'd');
  g_latched_input.turn_left |= key_pressed(status, 'j');
  g_latched_input.turn_right |= key_pressed(status, 'l');
  g_latched_input.look_up |= key_pressed(status, 'i');
  g_latched_input.look_down |= key_pressed(status, 'k');
  g_latched_input.move_down |= key_pressed(status, 'q');
  g_latched_input.move_up |= key_pressed(status, 'e');
  g_latched_input.jump |= key_pressed(status, 'g');
  g_latched_input.break_block |= key_pressed(status, 'z');
  g_latched_input.place_block |= key_pressed(status, 'x');
  g_latched_input.cycle_block |= key_pressed(status, 'c');
  g_latched_input.toggle_footer |= key_pressed(status, 'b');
  g_latched_input.toggle_help |= key_pressed(status, 'h');
  g_latched_input.toggle_header |= key_pressed(status, 'u');
  g_latched_input.toggle_fly |= key_pressed(status, 'f');
  g_latched_input.reset_world |= key_pressed(status, 'r');
}

void update_target_ray() {
  g_target = cast_ray(player_eye_position(), normalize_vec3(forward_vector()), app_config::TARGET_MAX_DISTANCE);
}

void break_target_block() {
  if (!g_target.hit) {
    return;
  }
  if (g_target.block == BLOCK_BEDROCK) {
    return;
  }
  set_block(g_target.x, g_target.y, g_target.z, BLOCK_AIR);
}

void place_selected_block() {
  if (!g_target.hit) {
    return;
  }
  if (!in_world(g_target.prev_x, g_target.prev_y, g_target.prev_z)) {
    return;
  }
  if (get_block(g_target.prev_x, g_target.prev_y, g_target.prev_z) != BLOCK_AIR) {
    return;
  }

  const uint8_t block = kSelectableBlocks[g_player.selected_slot];
  set_block(g_target.prev_x, g_target.prev_y, g_target.prev_z, block);
  if (!g_player.fly_mode && collides_aabb(g_player.x, g_player.y, g_player.z)) {
    set_block(g_target.prev_x, g_target.prev_y, g_target.prev_z, BLOCK_AIR);
  }
}

void update_game(float dt) {
  const auto status = M5Cardputer.Keyboard.keysState();
  const InputLatch latched = g_latched_input;
  g_latched_input = {};
  const float prev_x = g_player.x;
  const float prev_y = g_player.y;
  const float prev_z = g_player.z;
  const float prev_yaw = g_player.yaw_deg;
  const float prev_pitch = g_player.pitch_deg;

  const bool move_forward = key_pressed(status, 'w') || latched.move_forward;
  const bool move_back = key_pressed(status, 's') || latched.move_back;
  const bool move_left = key_pressed(status, 'a') || latched.move_left;
  const bool move_right = key_pressed(status, 'd') || latched.move_right;
  const bool turn_left = key_pressed(status, 'j') || latched.turn_left;
  const bool turn_right = key_pressed(status, 'l') || latched.turn_right;
  const bool look_up = key_pressed(status, 'i') || latched.look_up;
  const bool look_down = key_pressed(status, 'k') || latched.look_down;
  const bool move_down = key_pressed(status, 'q') || latched.move_down;
  const bool move_up = key_pressed(status, 'e') || latched.move_up;
  const bool jump_key = key_pressed(status, 'g') || latched.jump;
  const bool break_key = key_pressed(status, 'z') || latched.break_block;
  const bool place_key = key_pressed(status, 'x') || latched.place_block;
  const bool cycle_key = key_pressed(status, 'c') || latched.cycle_block;
  const bool footer_key = key_pressed(status, 'b') || latched.toggle_footer;
  const bool help_key = key_pressed(status, 'h') || latched.toggle_help;
  const bool header_key = key_pressed(status, 'u') || latched.toggle_header;
  const bool fly_toggle = key_pressed(status, 'f') || latched.toggle_fly;
  const bool reset_key = key_pressed(status, 'r') || latched.reset_world;

  g_player.yaw_deg +=
      ((turn_right ? 1.0f : 0.0f) - (turn_left ? 1.0f : 0.0f)) * app_config::TURN_SPEED_DEG * dt;
  g_player.pitch_deg += ((look_up ? 1.0f : 0.0f) - (look_down ? 1.0f : 0.0f)) * app_config::LOOK_SPEED_DEG * dt;
  g_player.pitch_deg = clampf(g_player.pitch_deg, -60.0f, 60.0f);

  if (fly_toggle && !g_prev_fly_toggle) {
    g_player.fly_mode = !g_player.fly_mode;
    g_player.velocity_y = 0.0f;
  }
  g_prev_fly_toggle = fly_toggle;

  if (reset_key && !g_prev_reset) {
    ++g_seed;
    reset_world();
  }
  g_prev_reset = reset_key;

  if (cycle_key && !g_prev_cycle) {
    g_player.selected_slot = (g_player.selected_slot + 1) % (sizeof(kSelectableBlocks) / sizeof(kSelectableBlocks[0]));
    request_low_res_redraw();
  }
  g_prev_cycle = cycle_key;

  if (help_key && !g_prev_help_toggle) {
    g_show_help = !g_show_help;
  }
  g_prev_help_toggle = help_key;

  if (footer_key && !g_prev_footer_toggle) {
    g_show_footer = !g_show_footer;
  }
  g_prev_footer_toggle = footer_key;

  if (header_key && !g_prev_header_toggle) {
    g_show_header = !g_show_header;
  }
  g_prev_header_toggle = header_key;
  const bool motion_input_requested =
      move_forward || move_back || move_left || move_right ||
      turn_left || turn_right || look_up || look_down ||
      move_up || move_down || jump_key;

  Vec3 move_dir = make_vec3(0.0f, 0.0f, 0.0f);
  Vec3 flat_forward = camera_forward();
  flat_forward.y = 0.0f;
  flat_forward = normalize_vec3(flat_forward);
  Vec3 flat_right = camera_right();
  flat_right.y = 0.0f;
  flat_right = normalize_vec3(flat_right);

  if (move_forward) {
    move_dir = add_vec3(move_dir, flat_forward);
  }
  if (move_back) {
    move_dir = add_vec3(move_dir, scale_vec3(flat_forward, -1.0f));
  }
  if (move_right) {
    move_dir = add_vec3(move_dir, flat_right);
  }
  if (move_left) {
    move_dir = add_vec3(move_dir, scale_vec3(flat_right, -1.0f));
  }
  move_dir = normalize_vec3(move_dir);

  const float speed = g_player.fly_mode ? app_config::FLY_SPEED : app_config::WALK_SPEED;
  try_move_horizontal(move_dir.x * speed * dt, move_dir.z * speed * dt);

  if (g_player.fly_mode) {
    float vertical = 0.0f;
    if (move_up) {
      vertical += 1.0f;
    }
    if (move_down) {
      vertical -= 1.0f;
    }
    g_player.y = clampf(g_player.y + vertical * app_config::FLY_SPEED * dt, 1.0f, app_config::WORLD_H - 2.0f);
  } else {
    if (jump_key && !g_prev_jump && g_player.on_ground) {
      g_player.velocity_y = app_config::JUMP_VELOCITY;
      g_player.on_ground = false;
    }
  }
  g_prev_jump = jump_key;

  apply_vertical_motion(dt);
  update_target_ray();

  const bool pose_changed =
      fabsf(g_player.x - prev_x) > 0.001f ||
      fabsf(g_player.y - prev_y) > 0.001f ||
      fabsf(g_player.z - prev_z) > 0.001f ||
      fabsf(g_player.yaw_deg - prev_yaw) > 0.001f ||
      fabsf(g_player.pitch_deg - prev_pitch) > 0.001f;
  if (motion_input_requested || pose_changed) {
    if (millis() >= g_motion_resolution_until_ms) {
      g_dynamic_motion_resolution_mode_index = g_motion_resolution_mode_index;
      g_good_motion_frames = 0;
    }
    g_motion_resolution_until_ms = millis() + 180U;
  }

  if (break_key && !g_prev_break) {
    break_target_block();
    update_target_ray();
    request_low_res_redraw();
  }
  g_prev_break = break_key;

  if (place_key && !g_prev_place) {
    place_selected_block();
    update_target_ray();
    request_low_res_redraw();
  }
  g_prev_place = place_key;
}

uint16_t background_color(int row, float dir_y) {
  if (dir_y >= 0.0f) {
    return g_sky_colors[row];
  }
  return g_ground_colors[row];
}

uint8_t face_texture_group_for_hit(const RayHit& hit) {
  if (hit.axis == 1) {
    return hit.face_sign > 0 ? FACE_TEXTURE_TOP : FACE_TEXTURE_BOTTOM;
  }
  return FACE_TEXTURE_SIDE;
}

uint16_t voxel_color_for_hit(const RayHit& hit, const Vec3& origin, const Vec3& dir) {
  const Vec3 impact = add_vec3(origin, scale_vec3(dir, hit.distance));
  float u = 0.0f;
  float v = 0.0f;

  if (hit.axis == 0) {
    u = hit.face_sign > 0 ? 1.0f - fracf_positive(impact.z) : fracf_positive(impact.z);
    v = 1.0f - fracf_positive(impact.y);
  } else if (hit.axis == 1) {
    u = fracf_positive(impact.x);
    v = hit.face_sign > 0 ? fracf_positive(impact.z) : 1.0f - fracf_positive(impact.z);
  } else {
    u = hit.face_sign > 0 ? fracf_positive(impact.x) : 1.0f - fracf_positive(impact.x);
    v = 1.0f - fracf_positive(impact.y);
  }

  const int tex_x = static_cast<int>(clampf(u * app_config::TEXTURE_SIZE, 0.0f, app_config::TEXTURE_SIZE - 1.0f));
  const int tex_y = static_cast<int>(clampf(v * app_config::TEXTURE_SIZE, 0.0f, app_config::TEXTURE_SIZE - 1.0f));
  const uint8_t face_group = face_texture_group_for_hit(hit);
  const ColorRgb texel =
      g_face_textures[hit.block][face_group][tex_y * app_config::TEXTURE_SIZE + tex_x];

  float light = 0.82f;
  if (face_group == FACE_TEXTURE_TOP) {
    light = 1.00f;
  } else if (face_group == FACE_TEXTURE_BOTTOM) {
    light = 0.66f;
  }

  light *= 1.0f - clampf(hit.distance / app_config::RAY_MAX_DISTANCE, 0.0f, 1.0f) * 0.36f;
  if (hit.block == BLOCK_WATER) {
    light *= 0.92f;
  }

  return output_color_565(shade_rgb(texel, light));
}

bool render_scene() {
  g_canvas.fillScreen(TFT_BLACK);

  const Vec3 forward = camera_forward();
  const Vec3 right = camera_right();
  const Vec3 up = camera_up();
  const Vec3 eye = player_eye_position();
  const ResolutionMode mode = kResolutionModes[active_resolution_mode_index()];
  const int render_w = (app_config::RENDER_W + mode.pixel_step - 1) / mode.pixel_step;
  const int render_h = (app_config::RENDER_H + mode.pixel_step - 1) / mode.pixel_step;
  const bool native_mode = mode.pixel_step == 1;

  if (native_mode) {
    g_canvas.setSwapBytes(true);
  }

  for (int py = 0; py < render_h; ++py) {
    if ((py & 0x03) == 0) {
      M5Cardputer.update();
      const auto status = M5Cardputer.Keyboard.keysState();
      latch_render_input(status);
      if (native_mode && motion_input_active(status)) {
        g_motion_resolution_until_ms = millis() + 180U;
        g_dynamic_motion_resolution_mode_index = g_motion_resolution_mode_index;
        g_good_motion_frames = 0;
        g_canvas.setSwapBytes(false);
        return false;
      }
    }

    const int y0 = py * mode.pixel_step;
    const int block_h = min<int>(mode.pixel_step, app_config::RENDER_H - y0);
    const int sample_y = min<int>(app_config::RENDER_H - 1, y0 + block_h / 2);

    for (int px = 0; px < render_w; ++px) {
      const int x0 = px * mode.pixel_step;
      const int block_w = min<int>(mode.pixel_step, app_config::RENDER_W - x0);
      const int sample_x = min<int>(app_config::RENDER_W - 1, x0 + block_w / 2);
      const Vec3 dir = add_vec3(
          forward,
          add_vec3(scale_vec3(right, g_column_factor[sample_x]), scale_vec3(up, g_row_factor[sample_y])));
      const RayHit hit = cast_ray(eye, dir, app_config::RAY_MAX_DISTANCE);
      const uint16_t color =
          hit.hit ? voxel_color_for_hit(hit, eye, dir) : background_color(sample_y, dir.y);

      if (mode.pixel_step == 1) {
        g_scanline[px] = color;
      } else {
        g_canvas.fillRect(x0, y0, block_w, block_h, color);
      }
    }
    if (native_mode) {
      g_canvas.pushImage(0, py, app_config::RENDER_W, 1, g_scanline);
    }
  }

  if (native_mode) {
    g_canvas.setSwapBytes(false);
  }
  return true;
}

void draw_hud() {
  const int16_t center_x = app_config::SCREEN_W / 2;
  const int16_t center_y = app_config::SCREEN_H / 2;
  const uint8_t selected_block = kSelectableBlocks[g_player.selected_slot];
  const uint8_t active_resolution_index = active_resolution_mode_index();

  if (g_show_header) {
    g_canvas.fillRect(0, 0, app_config::SCREEN_W, 26, TFT_BLACK);
    g_canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    g_canvas.setCursor(2, 2);
    g_canvas.printf(
        "ADV Minecraft  %s  FPS:%u",
        g_player.fly_mode ? "FLY" : "WALK",
        g_fps);
    g_canvas.setCursor(2, 10);
    g_canvas.printf(
        "Idle:%s Move:%s",
        kResolutionModes[0].label,
        kResolutionModes[g_dynamic_motion_resolution_mode_index].label);
    g_canvas.setCursor(2, 18);
    g_canvas.printf(
        "%s  Pos:%d,%d,%d",
        kResolutionModes[active_resolution_index].label,
        static_cast<int>(g_player.x),
        static_cast<int>(g_player.y),
        static_cast<int>(g_player.z));
  }

  g_canvas.setTextColor(TFT_WHITE);
  g_canvas.drawFastHLine(center_x - 5, center_y, 11, TFT_WHITE);
  g_canvas.drawFastVLine(center_x, center_y - 5, 11, TFT_WHITE);

  if (g_show_footer) {
    g_canvas.setCursor(2, app_config::SCREEN_H - 10);
    if (g_target.hit) {
      g_canvas.printf(
          "Target:%s @ %d,%d,%d",
          block_name(g_target.block),
          g_target.x,
          g_target.y,
          g_target.z);
    } else {
      g_canvas.print("Target:none");
    }

    g_canvas.setCursor(2, app_config::SCREEN_H - 18);
    g_canvas.printf(
        "Block:%s",
        block_name(selected_block));
  }

  if (!g_show_help) {
    return;
  }

  g_canvas.fillRect(12, 22, 216, 68, TFT_BLACK);
  g_canvas.drawRect(12, 22, 216, 68, TFT_DARKGREY);
  g_canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  g_canvas.setCursor(18, 28);
  g_canvas.print("WASD move  JL turn  IK look");
  g_canvas.setCursor(18, 40);
  g_canvas.print("G jump  F fly  QE up/down");
  g_canvas.setCursor(18, 52);
  g_canvas.print("Z break  X place  C block");
  g_canvas.setCursor(18, 64);
  g_canvas.print("R regen  H help");
  g_canvas.setCursor(18, 76);
  g_canvas.print("U header  B footer");
}

bool draw_frame() {
  if (!render_scene()) {
    return false;
  }
  draw_hud();
  M5Cardputer.Display.startWrite();
  g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
  M5Cardputer.Display.endWrite();
  return true;
}

void setup_palette() {
  build_block_textures();

  for (int row = 0; row < app_config::RENDER_H; ++row) {
    const float t = static_cast<float>(row) / static_cast<float>(app_config::RENDER_H - 1);
    g_sky_colors[row] = lgfx::v1::color565(
        static_cast<uint8_t>(18 + 34 * (1.0f - t)),
        static_cast<uint8_t>(80 + 80 * (1.0f - t)),
        static_cast<uint8_t>(120 + 90 * (1.0f - t)));
    g_ground_colors[row] = lgfx::v1::color565(
        static_cast<uint8_t>(24 + 22 * t),
        static_cast<uint8_t>(38 + 26 * t),
        static_cast<uint8_t>(20 + 18 * t));
  }

  const float v_fov = app_config::H_FOV_DEG *
                      (static_cast<float>(app_config::RENDER_H) / static_cast<float>(app_config::RENDER_W));
  const float tan_half_h = tanf(app_config::H_FOV_DEG * DEG_TO_RAD * 0.5f);
  const float tan_half_v = tanf(v_fov * DEG_TO_RAD * 0.5f);

  for (int x = 0; x < app_config::RENDER_W; ++x) {
    g_column_factor[x] =
        (((static_cast<float>(x) + 0.5f) / app_config::RENDER_W) - 0.5f) * 2.0f * tan_half_h;
  }
  for (int y = 0; y < app_config::RENDER_H; ++y) {
    g_row_factor[y] =
        (0.5f - ((static_cast<float>(y) + 0.5f) / app_config::RENDER_H)) * 2.0f * tan_half_v;
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
  g_canvas.setSwapBytes(false);
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);

  setup_palette();
  reset_world();
  update_target_ray();

  g_last_tick_ms = millis();
  g_last_draw_ms = g_last_tick_ms;
  g_last_fps_sample_ms = g_last_tick_ms;
  g_last_frame_present_ms = g_last_tick_ms;

  Serial.println("Cardputer ADV Minecraft demo started");
}

void loop() {
  M5Cardputer.update();

  const uint32_t now = millis();
  float dt = static_cast<float>(now - g_last_tick_ms) / 1000.0f;
  g_last_tick_ms = now;
  dt = clampf(dt, 0.0f, app_config::MAX_DT_SEC);

  update_game(dt);

  if (now - g_last_draw_ms >= app_config::FRAME_INTERVAL_MS) {
    if (draw_frame()) {
      if (g_forced_low_res_frames > 0) {
        --g_forced_low_res_frames;
      }
      const uint32_t frame_end_ms = millis();
      g_last_frame_interval_ms = static_cast<uint16_t>(frame_end_ms - g_last_frame_present_ms);
      g_last_frame_present_ms = frame_end_ms;
      g_last_draw_ms = frame_end_ms;
      ++g_frame_counter;
      update_dynamic_motion_resolution();
    }
  }

  if (now - g_last_fps_sample_ms >= 1000U) {
    g_fps = static_cast<uint16_t>(g_frame_counter);
    g_frame_counter = 0;
    g_last_fps_sample_ms = now;
  }
}
