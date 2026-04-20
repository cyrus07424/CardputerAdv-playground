#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>

namespace app_config {
constexpr uint32_t DRAW_INTERVAL_MS = 33;
constexpr float COMPLEMENTARY_ALPHA = 0.94f;
constexpr float MAX_FRAME_DT_SEC = 0.1f;
constexpr float GAUGE_LIMIT_DEG = 12.0f;
constexpr float LEVEL_GOOD_DEG = 0.8f;
constexpr float LEVEL_NEAR_DEG = 2.0f;
}  // namespace app_config

constexpr uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kColorBg = color565(10, 12, 14);
constexpr uint16_t kColorCase = color565(26, 29, 34);
constexpr uint16_t kColorCaseEdge = color565(54, 60, 66);
constexpr uint16_t kColorKey = color565(38, 42, 48);
constexpr uint16_t kColorKeyEdge = color565(82, 88, 94);
constexpr uint16_t kColorTrack = color565(20, 23, 28);
constexpr uint16_t kColorTrackEdge = color565(64, 69, 74);
constexpr uint16_t kColorLegend = color565(228, 231, 235);
constexpr uint16_t kColorMuted = color565(140, 146, 153);
constexpr uint16_t kColorAccent = color565(255, 143, 62);
constexpr uint16_t kColorAccentDim = color565(132, 84, 46);
constexpr uint16_t kColorWarn = color565(255, 92, 64);
constexpr uint16_t kColorGood = color565(118, 255, 196);
constexpr uint16_t kColorBubble = color565(178, 255, 80);
constexpr uint16_t kColorBubbleEdge = color565(63, 109, 17);
constexpr uint16_t kColorBattery = color565(160, 250, 95);

M5Canvas g_canvas(&M5Cardputer.Display);
bool g_imu_ready = false;
bool g_needs_redraw = true;
uint32_t g_last_draw_ms = 0;
uint32_t g_last_imu_usec = 0;
uint32_t g_last_zero_ms = 0;

float g_roll_deg = 0.0f;
float g_pitch_deg = 0.0f;
float g_yaw_deg = 0.0f;
float g_zero_roll_deg = 0.0f;
float g_zero_pitch_deg = 0.0f;

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

float current_roll_deg() {
  return wrap_angle_deg(g_roll_deg - g_zero_roll_deg);
}

float current_pitch_deg() {
  return wrap_angle_deg(g_pitch_deg - g_zero_pitch_deg);
}

float current_tilt_deg() {
  const float roll = current_roll_deg();
  const float pitch = current_pitch_deg();
  return sqrtf(roll * roll + pitch * pitch);
}

uint16_t status_color(float tilt_deg) {
  if (tilt_deg <= app_config::LEVEL_GOOD_DEG) {
    return kColorGood;
  }
  if (tilt_deg <= app_config::LEVEL_NEAR_DEG) {
    return kColorAccent;
  }
  return kColorWarn;
}

const char* status_text(float tilt_deg) {
  if (millis() - g_last_zero_ms < 900) {
    return "ZEROED";
  }
  if (tilt_deg <= app_config::LEVEL_GOOD_DEG) {
    return "LEVEL";
  }
  if (tilt_deg <= app_config::LEVEL_NEAR_DEG) {
    return "NEAR";
  }
  return "TILT";
}

void draw_keycap(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t accent) {
  auto& display = g_canvas;
  display.fillRoundRect(x, y, w, h, 7, kColorKey);
  display.drawRoundRect(x, y, w, h, 7, kColorKeyEdge);
  display.fillRoundRect(x + 2, y + 2, w - 4, 4, 2, accent);
}

void draw_key_label(int16_t x, int16_t y, const char* label, const char* value, uint16_t accent) {
  auto& display = g_canvas;
  display.setTextColor(accent, kColorKey);
  display.setCursor(x + 8, y + 6);
  display.print(label);
  display.setTextColor(kColorLegend, kColorKey);
  display.setCursor(x + 8, y + 15);
  display.print(value);
}

void draw_battery_status(int16_t x, int16_t y) {
  auto& display = g_canvas;
  const int battery_level = M5Cardputer.Power.getBatteryLevel();
  const int capped_level = battery_level < 0 ? 0 : (battery_level > 100 ? 100 : battery_level);
  const auto charging_state = M5Cardputer.Power.isCharging();
  const uint16_t fill_color = capped_level > 20 ? kColorBattery : kColorWarn;

  display.setTextColor(kColorMuted, kColorCase);
  display.setCursor(x, y);
  display.printf("%s", charging_label(charging_state));
  display.setTextColor(kColorLegend, kColorCase);
  display.setCursor(x + 24, y);
  display.printf("%3d%%", capped_level);

  const int16_t icon_x = x + 58;
  const int16_t icon_y = y + 1;
  const int16_t icon_w = 16;
  const int16_t icon_h = 9;
  const int16_t fill_w = (icon_w - 4) * capped_level / 100;

  display.drawRect(icon_x, icon_y, icon_w, icon_h, kColorLegend);
  display.fillRect(icon_x + icon_w, icon_y + 2, 2, icon_h - 4, kColorLegend);
  if (fill_w > 0) {
    display.fillRect(icon_x + 2, icon_y + 2, fill_w, icon_h - 4, fill_color);
  }
}

void draw_status_pill(int16_t x, int16_t y, int16_t w, int16_t h, float tilt_deg) {
  auto& display = g_canvas;
  const uint16_t fill = status_color(tilt_deg);
  display.fillRoundRect(x, y, w, h, 8, fill);
  display.setTextColor(kColorBg, fill);
  display.setCursor(x + 11, y + 7);
  display.print(status_text(tilt_deg));
}

void draw_horizontal_gauge(int16_t x, int16_t y, int16_t w, int16_t h, float roll_deg) {
  auto& display = g_canvas;
  draw_keycap(x, y, w, h, kColorAccent);

  display.setTextColor(kColorMuted, kColorKey);
  display.setCursor(x + 10, y + 7);
  display.print("ROLL");

  const int16_t track_x = x + 12;
  const int16_t track_y = y + 22;
  const int16_t track_w = w - 24;
  const int16_t track_h = 18;
  display.fillRoundRect(track_x, track_y, track_w, track_h, 8, kColorTrack);
  display.drawRoundRect(track_x, track_y, track_w, track_h, 8, kColorTrackEdge);

  const int16_t center_x = track_x + track_w / 2;
  const int16_t center_y = track_y + track_h / 2;
  for (int i = -4; i <= 4; ++i) {
    const int16_t tick_x = center_x + i * (track_w - 22) / 8;
    const int16_t tick_h = (i == 0) ? 12 : (abs(i) == 4 ? 8 : 6);
    display.drawFastVLine(tick_x, center_y - tick_h / 2, tick_h, i == 0 ? kColorAccent : kColorTrackEdge);
  }

  display.drawFastHLine(center_x - 10, center_y, 20, kColorAccentDim);

  const float normalized = clamp_value(roll_deg / app_config::GAUGE_LIMIT_DEG, -1.0f, 1.0f);
  const int16_t travel = track_w / 2 - 12;
  const int16_t bubble_x = center_x + static_cast<int16_t>(normalized * travel);
  const uint16_t glow = fabsf(roll_deg) <= app_config::LEVEL_GOOD_DEG ? kColorGood :
                        fabsf(roll_deg) <= app_config::LEVEL_NEAR_DEG ? kColorAccent :
                        kColorWarn;

  display.fillCircle(bubble_x, center_y, 7, glow);
  display.drawCircle(bubble_x, center_y, 7, kColorBubbleEdge);
  display.fillCircle(bubble_x, center_y, 3, kColorBubble);
}

void draw_vertical_gauge(int16_t x, int16_t y, int16_t w, int16_t h, float pitch_deg) {
  auto& display = g_canvas;
  draw_keycap(x, y, w, h, kColorWarn);

  display.setTextColor(kColorMuted, kColorKey);
  display.setCursor(x + 7, y + 7);
  display.print("PITCH");

  const int16_t track_x = x + 9;
  const int16_t track_y = y + 17;
  const int16_t track_w = w - 18;
  const int16_t track_h = h - 23;
  display.fillRoundRect(track_x, track_y, track_w, track_h, 10, kColorTrack);
  display.drawRoundRect(track_x, track_y, track_w, track_h, 10, kColorTrackEdge);

  const int16_t center_x = track_x + track_w / 2;
  const int16_t center_y = track_y + track_h / 2;
  for (int i = -4; i <= 4; ++i) {
    const int16_t tick_y = center_y + i * (track_h - 20) / 8;
    const int16_t tick_w = (i == 0) ? 14 : (abs(i) == 4 ? 10 : 8);
    display.drawFastHLine(center_x - tick_w / 2, tick_y, tick_w, i == 0 ? kColorWarn : kColorTrackEdge);
  }

  const float normalized = clamp_value(pitch_deg / app_config::GAUGE_LIMIT_DEG, -1.0f, 1.0f);
  const int16_t travel = track_h / 2 - 10;
  const int16_t bubble_y = center_y + static_cast<int16_t>(normalized * travel);
  const uint16_t glow = fabsf(pitch_deg) <= app_config::LEVEL_GOOD_DEG ? kColorGood :
                        fabsf(pitch_deg) <= app_config::LEVEL_NEAR_DEG ? kColorAccent :
                        kColorWarn;

  display.fillCircle(center_x, bubble_y, 7, glow);
  display.drawCircle(center_x, bubble_y, 7, kColorBubbleEdge);
  display.fillCircle(center_x, bubble_y, 3, kColorBubble);
}

void draw_value_key(int16_t x, int16_t y, int16_t w, const char* label, const char* value, uint16_t accent) {
  draw_keycap(x, y, w, 26, accent);
  draw_key_label(x, y, label, value, accent);
}

void draw_shell() {
  auto& display = g_canvas;
  display.fillScreen(kColorBg);
  display.fillRoundRect(4, 4, 232, 127, 16, kColorCase);
  display.drawRoundRect(4, 4, 232, 127, 16, kColorCaseEdge);
  display.drawFastHLine(12, 38, 216, kColorCaseEdge);
  display.drawFastHLine(12, 104, 216, kColorCaseEdge);
  display.fillRoundRect(58, 123, 124, 4, 2, color565(18, 20, 23));
}

void draw_ui() {
  auto& display = g_canvas;
  display.setTextFont(1);
  display.setTextSize(1);

  draw_shell();
  draw_battery_status(152, 11);

  if (!g_imu_ready) {
    draw_keycap(14, 12, 92, 22, kColorWarn);
    draw_key_label(14, 12, "LEVEL", "IMU ERR", kColorWarn);
    display.setTextColor(kColorLegend, kColorCase);
    display.setCursor(18, 55);
    display.print("No internal IMU detected.");
    display.setCursor(18, 67);
    display.print("This demo requires Cardputer-ADV.");
    display.setCursor(18, 79);
    display.printf("Detected: %s", imu_name(M5.Imu.getType()));
    M5Cardputer.Display.startWrite();
    display.pushSprite(&M5Cardputer.Display, 0, 0);
    M5Cardputer.Display.endWrite();
    return;
  }

  const float roll_deg = current_roll_deg();
  const float pitch_deg = current_pitch_deg();
  const float tilt_deg = current_tilt_deg();

  draw_keycap(14, 12, 70, 22, kColorAccent);
  draw_key_label(14, 12, "ADV", "LEVEL", kColorAccent);
  draw_keycap(90, 12, 60, 22, kColorMuted);
  draw_key_label(90, 12, "ZERO", "BtnA", kColorMuted);
  draw_status_pill(14, 40, 54, 18, tilt_deg);

  display.setTextColor(kColorMuted, kColorCase);
  display.setCursor(76, 44);
  display.print("Keyboard-style 2-axis level");

  draw_horizontal_gauge(14, 55, 160, 43, roll_deg);
  draw_vertical_gauge(182, 40, 42, 62, pitch_deg);

  char roll_text[16];
  char pitch_text[16];
  char tilt_text[16];
  snprintf(roll_text, sizeof(roll_text), "%+5.1f deg", roll_deg);
  snprintf(pitch_text, sizeof(pitch_text), "%+5.1f deg", pitch_deg);
  snprintf(tilt_text, sizeof(tilt_text), "%4.1f deg", tilt_deg);

  draw_value_key(14, 106, 68, "ROLL", roll_text, kColorAccent);
  draw_value_key(87, 106, 68, "PITCH", pitch_text, kColorWarn);
  draw_value_key(160, 106, 64, "TILT", tilt_text, status_color(tilt_deg));

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
  Serial.println("Cardputer ADV level demo started");
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
    g_zero_roll_deg = g_roll_deg;
    g_zero_pitch_deg = g_pitch_deg;
    g_last_zero_ms = millis();
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
