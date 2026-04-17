#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>

namespace app_config {
constexpr uint32_t DRAW_INTERVAL_MS = 33;
constexpr float COMPLEMENTARY_ALPHA = 0.96f;
constexpr float MAX_FRAME_DT_SEC = 0.1f;
constexpr float PROJECTION_DISTANCE = 3.2f;
constexpr float CUBE_SIZE = 0.95f;
constexpr float GRAVITY_REFERENCE = 1.0f;
constexpr float GYRO_BAR_MAX_DPS = 180.0f;
}

struct Vec3 {
  float x;
  float y;
  float z;
};

struct Point2D {
  int16_t x;
  int16_t y;
};

struct Edge {
  uint8_t a;
  uint8_t b;
};

constexpr Vec3 kCubeVertices[8] = {
    {-app_config::CUBE_SIZE, -app_config::CUBE_SIZE, -app_config::CUBE_SIZE},
    { app_config::CUBE_SIZE, -app_config::CUBE_SIZE, -app_config::CUBE_SIZE},
    { app_config::CUBE_SIZE,  app_config::CUBE_SIZE, -app_config::CUBE_SIZE},
    {-app_config::CUBE_SIZE,  app_config::CUBE_SIZE, -app_config::CUBE_SIZE},
    {-app_config::CUBE_SIZE, -app_config::CUBE_SIZE,  app_config::CUBE_SIZE},
    { app_config::CUBE_SIZE, -app_config::CUBE_SIZE,  app_config::CUBE_SIZE},
    { app_config::CUBE_SIZE,  app_config::CUBE_SIZE,  app_config::CUBE_SIZE},
    {-app_config::CUBE_SIZE,  app_config::CUBE_SIZE,  app_config::CUBE_SIZE},
};

constexpr Edge kCubeEdges[12] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
};

M5Canvas g_canvas(&M5Cardputer.Display);
bool g_imu_ready = false;
bool g_needs_redraw = true;
uint32_t g_last_draw_ms = 0;
uint32_t g_last_imu_usec = 0;

float g_roll_deg = 0.0f;
float g_pitch_deg = 0.0f;
float g_yaw_deg = 0.0f;

float g_accel_x = 0.0f;
float g_accel_y = 0.0f;
float g_accel_z = 0.0f;
float g_gyro_x = 0.0f;
float g_gyro_y = 0.0f;
float g_gyro_z = 0.0f;

float clamp_value(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

float wrap_angle_deg(float angle_deg) {
  while (angle_deg > 180.0f) {
    angle_deg -= 360.0f;
  }
  while (angle_deg < -180.0f) {
    angle_deg += 360.0f;
  }
  return angle_deg;
}

const char* imu_name(m5::imu_t imu_type) {
  switch (imu_type) {
    case m5::imu_sh200q:
      return "SH200Q";
    case m5::imu_mpu6050:
      return "MPU6050";
    case m5::imu_mpu6886:
      return "MPU6886";
    case m5::imu_mpu9250:
      return "MPU9250";
    case m5::imu_bmi270:
      return "BMI270";
    case m5::imu_none:
      return "NONE";
    default:
      return "UNKNOWN";
  }
}

const char* charging_label(m5::Power_Class::is_charging_t charging_state) {
  switch (charging_state) {
    case m5::Power_Class::is_charging_t::is_charging:
      return "CHG";
    case m5::Power_Class::is_charging_t::is_discharging:
      return "BAT";
    default:
      return "UNK";
  }
}

void draw_battery_status(int16_t width) {
  auto& display = g_canvas;
  const int battery_level = M5Cardputer.Power.getBatteryLevel();
  const auto charging_state = M5Cardputer.Power.isCharging();
  const int capped_level = battery_level < 0 ? 0 : (battery_level > 100 ? 100 : battery_level);
  const int icon_x = width - 24;
  const int icon_y = 3;
  const int icon_w = 16;
  const int icon_h = 8;
  const int fill_w = (icon_w - 4) * capped_level / 100;
  const uint16_t fill_color = capped_level > 20 ? TFT_GREENYELLOW : TFT_ORANGE;

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(width - 58, 0);
  display.printf("%3d%%", capped_level);
  display.drawRect(icon_x, icon_y, icon_w, icon_h, TFT_WHITE);
  display.fillRect(icon_x + icon_w, icon_y + 2, 2, icon_h - 4, TFT_WHITE);
  if (fill_w > 0) {
    display.fillRect(icon_x + 2, icon_y + 2, fill_w, icon_h - 4, fill_color);
  }

  const uint16_t status_color =
      charging_state == m5::Power_Class::is_charging_t::is_charging ? TFT_GREENYELLOW :
      charging_state == m5::Power_Class::is_charging_t::is_discharging ? TFT_DARKGREY :
      TFT_ORANGE;
  display.setTextColor(status_color, TFT_BLACK);
  display.setCursor(width - 58, 9);
  display.print(charging_label(charging_state));
}

Vec3 rotate_vector(const Vec3& input, float roll_rad, float pitch_rad, float yaw_rad) {
  const float cx = cosf(roll_rad);
  const float sx = sinf(roll_rad);
  const float cy = cosf(pitch_rad);
  const float sy = sinf(pitch_rad);
  const float cz = cosf(yaw_rad);
  const float sz = sinf(yaw_rad);

  Vec3 v = input;

  const float y1 = v.y * cx - v.z * sx;
  const float z1 = v.y * sx + v.z * cx;
  v.y = y1;
  v.z = z1;

  const float x2 = v.x * cy + v.z * sy;
  const float z2 = -v.x * sy + v.z * cy;
  v.x = x2;
  v.z = z2;

  const float x3 = v.x * cz - v.y * sz;
  const float y3 = v.x * sz + v.y * cz;
  v.x = x3;
  v.y = y3;

  return v;
}

Point2D project_point(const Vec3& point, int16_t center_x, int16_t center_y, float scale) {
  const float depth = point.z + app_config::PROJECTION_DISTANCE;
  const float perspective = scale / depth;
  Point2D projected = {
      static_cast<int16_t>(center_x + point.x * perspective),
      static_cast<int16_t>(center_y - point.y * perspective),
  };
  return projected;
}

void draw_cube(int16_t center_x, int16_t center_y, float scale) {
  auto& display = g_canvas;

  // The screen is fixed to the device, so the model needs the inverse attitude
  // to appear to move in the same direction as the physical unit.
  const float roll_rad = -g_roll_deg * DEG_TO_RAD;
  const float pitch_rad = -g_pitch_deg * DEG_TO_RAD;
  const float yaw_rad = -g_yaw_deg * DEG_TO_RAD;

  Vec3 rotated[8];
  Point2D projected[8];

  for (size_t i = 0; i < 8; ++i) {
    rotated[i] = rotate_vector(kCubeVertices[i], roll_rad, pitch_rad, yaw_rad);
    projected[i] = project_point(rotated[i], center_x, center_y, scale);
  }

  display.drawCircle(center_x, center_y, 3, TFT_DARKGREY);
  display.drawFastHLine(center_x - 48, center_y, 96, TFT_DARKGREY);
  display.drawFastVLine(center_x, center_y - 48, 96, TFT_DARKGREY);

  for (const auto& edge : kCubeEdges) {
    const float avg_depth = (rotated[edge.a].z + rotated[edge.b].z) * 0.5f;
    const uint16_t color = avg_depth > 0.0f ? TFT_CYAN : TFT_DARKGREY;
    display.drawLine(
        projected[edge.a].x,
        projected[edge.a].y,
        projected[edge.b].x,
        projected[edge.b].y,
        color);
  }

  const Vec3 accel_vector = rotate_vector(
      {g_accel_x, g_accel_y, g_accel_z},
      roll_rad,
      pitch_rad,
      yaw_rad);
  const Vec3 accel_tip = {
      accel_vector.x * 0.6f,
      accel_vector.y * 0.6f,
      accel_vector.z * 0.6f,
  };
  const Point2D accel_projected = project_point(accel_tip, center_x, center_y, scale * 0.95f);
  display.drawLine(center_x, center_y, accel_projected.x, accel_projected.y, TFT_RED);
  display.fillCircle(accel_projected.x, accel_projected.y, 3, TFT_RED);
}

void draw_gyro_bars(int16_t x, int16_t y, int16_t w, int16_t h) {
  auto& display = g_canvas;
  const float gyro_values[3] = {g_gyro_x, g_gyro_y, g_gyro_z};
  const uint16_t colors[3] = {TFT_RED, TFT_GREEN, TFT_CYAN};
  const char labels[3] = {'X', 'Y', 'Z'};
  const int16_t row_h = h / 3;
  const int16_t bar_origin = x + 18;
  const int16_t bar_w = w - 24;
  const int16_t half_w = bar_w / 2;

  for (int i = 0; i < 3; ++i) {
    const int16_t row_y = y + row_h * i;
    const int16_t mid_x = bar_origin + half_w;
    const float normalized = clamp_value(
        gyro_values[i] / app_config::GYRO_BAR_MAX_DPS,
        -1.0f,
        1.0f);
    const int16_t fill_w = static_cast<int16_t>(normalized * half_w);

    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(x, row_y + 2);
    display.printf("%c", labels[i]);
    display.drawRect(bar_origin, row_y + 2, bar_w, row_h - 4, TFT_DARKGREY);
    display.drawFastVLine(mid_x, row_y + 3, row_h - 6, TFT_DARKGREY);
    if (fill_w >= 0) {
      display.fillRect(mid_x, row_y + 4, fill_w, row_h - 8, colors[i]);
    } else {
      display.fillRect(mid_x + fill_w, row_y + 4, -fill_w, row_h - 8, colors[i]);
    }
  }
}

void draw_ui() {
  auto& display = g_canvas;
  const int16_t width = display.width();
  const int16_t height = display.height();
  const int16_t cube_center_x = 72;
  const int16_t cube_center_y = 72;

  display.fillScreen(TFT_BLACK);
  display.setTextFont(1);
  display.setTextSize(1);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setCursor(0, 0);
  display.print("IMU 3D Visualizer");
  draw_battery_status(width);

  display.setTextColor(g_imu_ready ? TFT_YELLOW : TFT_ORANGE, TFT_BLACK);
  display.setCursor(0, 10);
  display.printf("IMU: %s", imu_name(M5.Imu.getType()));

  display.drawFastHLine(0, 18, width, TFT_DARKGREY);
  display.drawFastVLine(144, 18, height - 32, TFT_DARKGREY);
  display.drawFastHLine(144, 86, width - 144, TFT_DARKGREY);

  if (g_imu_ready) {
    draw_cube(cube_center_x, cube_center_y, 68.0f);

    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(150, 24);
    display.printf("Roll : %6.1f", g_roll_deg);
    display.setCursor(150, 36);
    display.printf("Pitch: %6.1f", g_pitch_deg);
    display.setCursor(150, 48);
    display.printf("Yaw  : %6.1f", g_yaw_deg);
    display.setCursor(150, 64);
    display.printf("Acc  : %4.2fg", sqrtf(g_accel_x * g_accel_x + g_accel_y * g_accel_y + g_accel_z * g_accel_z));

    draw_gyro_bars(150, 90, width - 154, 40);

    display.setCursor(4, 118);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.printf("A:(%+.2f,%+.2f,%+.2f)g", g_accel_x, g_accel_y, g_accel_z);
  } else {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.setCursor(12, 50);
    display.println("No internal IMU detected.");
    display.setCursor(12, 64);
    display.println("This demo requires Cardputer-ADV.");
  }

  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(4, height - 10);
  display.print("BtnA: reset yaw");
  M5Cardputer.Display.startWrite();
  display.pushSprite(&M5Cardputer.Display, 0, 0);
  M5Cardputer.Display.endWrite();
}

void update_orientation(const m5::IMU_Class::imu_data_t& data) {
  g_accel_x = data.accel.x;
  g_accel_y = data.accel.y;
  g_accel_z = data.accel.z;
  g_gyro_x = data.gyro.x;
  g_gyro_y = data.gyro.y;
  g_gyro_z = data.gyro.z;

  const float accel_roll_deg = atan2f(g_accel_y, g_accel_z) * RAD_TO_DEG;
  const float accel_pitch_deg = atan2f(
      -g_accel_x,
      sqrtf(g_accel_y * g_accel_y + g_accel_z * g_accel_z)) * RAD_TO_DEG;

  if (g_last_imu_usec == 0) {
    g_roll_deg = accel_roll_deg;
    g_pitch_deg = accel_pitch_deg;
    g_yaw_deg = 0.0f;
    g_last_imu_usec = data.usec;
    g_needs_redraw = true;
    return;
  }

  float dt = static_cast<float>(data.usec - g_last_imu_usec) / 1000000.0f;
  g_last_imu_usec = data.usec;
  dt = clamp_value(dt, 0.0f, app_config::MAX_FRAME_DT_SEC);

  g_roll_deg += g_gyro_x * dt;
  g_pitch_deg += g_gyro_y * dt;
  g_yaw_deg += g_gyro_z * dt;

  g_roll_deg = app_config::COMPLEMENTARY_ALPHA * g_roll_deg +
               (1.0f - app_config::COMPLEMENTARY_ALPHA) * accel_roll_deg;
  g_pitch_deg = app_config::COMPLEMENTARY_ALPHA * g_pitch_deg +
                (1.0f - app_config::COMPLEMENTARY_ALPHA) * accel_pitch_deg;

  g_roll_deg = wrap_angle_deg(g_roll_deg);
  g_pitch_deg = wrap_angle_deg(g_pitch_deg);
  g_yaw_deg = wrap_angle_deg(g_yaw_deg);
  g_needs_redraw = true;
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  cfg.output_power = true;
  cfg.internal_imu = true;
  M5Cardputer.begin(cfg, true);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);
  g_canvas.setColorDepth(16);
  g_canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);

  g_imu_ready = M5.Imu.getType() != m5::imu_none;
  Serial.println("Cardputer ADV IMU 3D demo started");
  Serial.printf("Detected IMU: %s\n", imu_name(M5.Imu.getType()));

  if (g_imu_ready) {
    M5.Imu.loadOffsetFromNVS();
  }

  draw_ui();
  g_needs_redraw = false;
  g_last_draw_ms = millis();
}

void loop() {
  M5Cardputer.update();

  if (g_imu_ready && M5.Imu.update()) {
    update_orientation(M5.Imu.getImuData());
  }

  if (M5Cardputer.BtnA.wasClicked()) {
    g_yaw_deg = 0.0f;
    g_needs_redraw = true;
  }

  const uint32_t now = millis();
  if (now - g_last_draw_ms >= app_config::DRAW_INTERVAL_MS) {
    g_needs_redraw = true;
  }

  if (g_needs_redraw) {
    draw_ui();
    g_needs_redraw = false;
    g_last_draw_ms = now;
  }
}
