#include <Arduino.h>
#include <M5Cardputer.h>
#include <TinyGPS++.h>
#include <math.h>

namespace board_pins {
constexpr int GPS_RX = 15;
constexpr int GPS_TX = 13;
}

namespace gps_config {
constexpr uint32_t UART_BAUD = 115200;
constexpr uint32_t UI_REFRESH_MS = 1000;
constexpr float SPEED_MAX_KMH = 120.0f;
}

enum class DisplayMode : uint8_t {
  Text = 0,
  Satellites,
  Compass,
  Speed,
  Count
};

HardwareSerial GPSSerial(1);
TinyGPSPlus gps;

bool g_needs_redraw = true;
String g_status = "Booting...";
uint32_t g_last_draw_ms = 0;
uint32_t g_last_data_ms = 0;
DisplayMode g_display_mode = DisplayMode::Text;

String format_float_value(bool valid, double value, uint8_t decimals) {
  if (!valid) {
    return "-";
  }

  char buffer[32];
  dtostrf(value, 0, decimals, buffer);
  return String(buffer);
}

String format_uint_value(bool valid, uint32_t value) {
  if (!valid) {
    return "-";
  }
  return String(value);
}

String format_date_time() {
  if (!gps.date.isValid() || !gps.time.isValid()) {
    return "-";
  }

  char buffer[32];
  snprintf(
      buffer,
      sizeof(buffer),
      "%04d-%02d-%02d %02d:%02d:%02d",
      gps.date.year(),
      gps.date.month(),
      gps.date.day(),
      gps.time.hour(),
      gps.time.minute(),
      gps.time.second());
  return String(buffer);
}

String format_fix_age() {
  if (!gps.location.isValid()) {
    return "-";
  }
  return String(gps.location.age()) + " ms";
}

const char* mode_name(DisplayMode mode) {
  switch (mode) {
    case DisplayMode::Text:
      return "Text";
    case DisplayMode::Satellites:
      return "Sat";
    case DisplayMode::Compass:
      return "Compass";
    case DisplayMode::Speed:
      return "Speed";
    default:
      return "?";
  }
}

DisplayMode next_mode(DisplayMode mode) {
  const uint8_t next = (static_cast<uint8_t>(mode) + 1) % static_cast<uint8_t>(DisplayMode::Count);
  return static_cast<DisplayMode>(next);
}

double normalized_course() {
  if (!gps.course.isValid()) {
    return 0.0;
  }

  double degrees = gps.course.deg();
  while (degrees < 0.0) {
    degrees += 360.0;
  }
  while (degrees >= 360.0) {
    degrees -= 360.0;
  }
  return degrees;
}

uint32_t satellite_count() {
  return gps.satellites.isValid() ? gps.satellites.value() : 0;
}

float speed_kmph() {
  return gps.speed.isValid() ? gps.speed.kmph() : 0.0f;
}

const char* heading_label(double degrees) {
  static const char* labels[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  const int index = static_cast<int>((degrees + 22.5) / 45.0) % 8;
  return labels[index];
}

void draw_battery_status(int width) {
  auto& display = M5Cardputer.Display;
  const int battery_level = M5.Power.getBatteryLevel();
  const int icon_x = width - 24;
  const int icon_y = 3;
  const int icon_w = 16;
  const int icon_h = 8;
  const int capped_level = battery_level < 0 ? 0 : (battery_level > 100 ? 100 : battery_level);
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

  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setCursor(width - 58, 9);
  display.print("N/A");
}

void update_status() {
  String next_status;
  const uint32_t now = millis();

  if (g_last_data_ms == 0 || now - g_last_data_ms > 3000) {
    next_status = "No GPS data";
  } else if (gps.location.isValid()) {
    next_status = "GPS fix";
  } else if (gps.charsProcessed() > 0) {
    next_status = "Searching satellites";
  } else {
    next_status = "Waiting for NMEA";
  }

  if (next_status != g_status) {
    g_status = next_status;
    g_needs_redraw = true;
  }
}

void draw_common_frame(const char* title) {
  auto& display = M5Cardputer.Display;
  const int width = display.width();
  const int height = display.height();

  display.fillScreen(TFT_BLACK);
  display.setTextFont(1);
  display.setTextSize(1);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setCursor(0, 0);
  display.printf("GPS Info / %s", title);
  draw_battery_status(width);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.setCursor(0, 10);
  display.printf("%s\n", g_status.c_str());
  display.drawFastHLine(0, 18, width, TFT_DARKGREY);
  display.drawFastHLine(0, height - 12, width, TFT_DARKGREY);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(0, height - 10);
  display.printf("BtnA: Next (%s)", mode_name(g_display_mode));
}

void draw_text_mode() {
  auto& display = M5Cardputer.Display;
  draw_common_frame("Text");
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(0, 22);
  display.printf("Lat : %s\n", format_float_value(gps.location.isValid(), gps.location.lat(), 6).c_str());
  display.printf("Lng : %s\n", format_float_value(gps.location.isValid(), gps.location.lng(), 6).c_str());
  display.printf("Alt : %s m\n", format_float_value(gps.altitude.isValid(), gps.altitude.meters(), 1).c_str());
  display.printf("Sat : %s  HDOP:%s\n",
                 format_uint_value(gps.satellites.isValid(), gps.satellites.value()).c_str(),
                 format_float_value(gps.hdop.isValid(), gps.hdop.hdop(), 1).c_str());
  display.printf("Spd : %s km/h\n", format_float_value(gps.speed.isValid(), gps.speed.kmph(), 1).c_str());
  display.printf("Crs : %s deg\n", format_float_value(gps.course.isValid(), gps.course.deg(), 1).c_str());
  display.printf("UTC : %s\n", format_date_time().c_str());
  display.printf("Age : %s\n", format_fix_age().c_str());
}

void draw_satellite_mode() {
  auto& display = M5Cardputer.Display;
  const int width = display.width();
  const int center_x = width / 2;
  const int center_y = 68;
  const int radius = 34;
  const uint32_t satellites = satellite_count();
  const uint32_t dots = satellites > 16 ? 16 : satellites;

  draw_common_frame("Satellites");
  display.drawCircle(center_x, center_y, radius, TFT_DARKGREY);
  display.drawCircle(center_x, center_y, radius - 10, TFT_DARKGREY);

  for (uint32_t i = 0; i < dots; ++i) {
    const float angle = ((360.0f / dots) * i - 90.0f) * DEG_TO_RAD;
    const int x = center_x + static_cast<int>(cosf(angle) * radius);
    const int y = center_y + static_cast<int>(sinf(angle) * radius);
    display.fillCircle(x, y, 4, TFT_GREENYELLOW);
  }

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(4);
  display.setCursor(center_x - 20, center_y - 16);
  display.print(satellites);
  display.setTextSize(1);
  display.setCursor(center_x - 12, center_y + 18);
  display.print("SAT");
  display.setCursor(16, 96);
  display.printf("HDOP: %s", format_float_value(gps.hdop.isValid(), gps.hdop.hdop(), 1).c_str());
  display.setCursor(16, 108);
  display.printf("Fix : %s", gps.location.isValid() ? "YES" : "NO");
}

void draw_compass_mode() {
  auto& display = M5Cardputer.Display;
  const int width = display.width();
  const int center_x = width / 2;
  const int center_y = 72;
  const int radius = 34;

  draw_common_frame("Compass");
  display.drawCircle(center_x, center_y, radius, TFT_WHITE);
  display.drawCircle(center_x, center_y, radius - 1, TFT_DARKGREY);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(center_x - 3, center_y - radius - 10);
  display.print("N");
  display.setCursor(center_x - 3, center_y + radius + 2);
  display.print("S");
  display.setCursor(center_x - radius - 10, center_y - 3);
  display.print("W");
  display.setCursor(center_x + radius + 4, center_y - 3);
  display.print("E");

  if (gps.course.isValid()) {
    const double heading = normalized_course();
    const float angle = (heading - 90.0f) * DEG_TO_RAD;
    const int tip_x = center_x + static_cast<int>(cosf(angle) * (radius - 4));
    const int tip_y = center_y + static_cast<int>(sinf(angle) * (radius - 4));
    const int tail_x = center_x - static_cast<int>(cosf(angle) * 12);
    const int tail_y = center_y - static_cast<int>(sinf(angle) * 12);
    display.drawLine(tail_x, tail_y, tip_x, tip_y, TFT_RED);
    display.fillCircle(center_x, center_y, 3, TFT_RED);
    display.fillCircle(tip_x, tip_y, 4, TFT_RED);

    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(64, 104);
    display.printf("%03.0f deg  %s", heading, heading_label(heading));
  } else {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.setCursor(52, 104);
    display.print("Need valid course");
  }
}

void draw_speed_mode() {
  auto& display = M5Cardputer.Display;
  const int width = display.width();
  const int height = display.height();
  const int center_x = width / 2;
  const int center_y = height + 12;
  const int outer_radius = 82;
  const int inner_radius = 68;
  const float speed = speed_kmph();

  draw_common_frame("Speed");

  for (int i = 0; i <= 12; ++i) {
    const float ratio = static_cast<float>(i) / 12.0f;
    const float angle_deg = 210.0f - (240.0f * ratio);
    const float angle = angle_deg * DEG_TO_RAD;
    const int x1 = center_x + static_cast<int>(cosf(angle) * inner_radius);
    const int y1 = center_y + static_cast<int>(sinf(angle) * inner_radius);
    const int x2 = center_x + static_cast<int>(cosf(angle) * outer_radius);
    const int y2 = center_y + static_cast<int>(sinf(angle) * outer_radius);
    display.drawLine(x1, y1, x2, y2, TFT_DARKGREY);
  }

  if (gps.speed.isValid()) {
    const float clamped_speed = speed > gps_config::SPEED_MAX_KMH ? gps_config::SPEED_MAX_KMH : speed;
    const float ratio = clamped_speed / gps_config::SPEED_MAX_KMH;
    const float angle_deg = 210.0f - (240.0f * ratio);
    const float angle = angle_deg * DEG_TO_RAD;
    const int tip_x = center_x + static_cast<int>(cosf(angle) * (inner_radius - 8));
    const int tip_y = center_y + static_cast<int>(sinf(angle) * (inner_radius - 8));
    display.drawLine(center_x, center_y, tip_x, tip_y, TFT_RED);
    display.fillCircle(center_x, center_y, 4, TFT_RED);
  }

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextSize(3);
  display.setCursor(62, 48);
  display.printf("%3.0f", speed);
  display.setTextSize(1);
  display.setCursor(98, 80);
  display.print("km/h");
}

void draw_ui() {
  auto& display = M5Cardputer.Display;
  display.startWrite();

  switch (g_display_mode) {
    case DisplayMode::Text:
      draw_text_mode();
      break;
    case DisplayMode::Satellites:
      draw_satellite_mode();
      break;
    case DisplayMode::Compass:
      draw_compass_mode();
      break;
    case DisplayMode::Speed:
      draw_speed_mode();
      break;
    default:
      draw_text_mode();
      break;
  }

  display.endWrite();
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, false);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);

  GPSSerial.begin(gps_config::UART_BAUD, SERIAL_8N1, board_pins::GPS_RX, board_pins::GPS_TX);

  Serial.println("Cardputer ADV GPS demo started");
  Serial.println("GNSS UART: RX=15 TX=13 @115200");

  update_status();
  draw_ui();
  g_needs_redraw = false;
  g_last_draw_ms = millis();
}

void loop() {
  M5Cardputer.update();

  bool received_data = false;
  while (GPSSerial.available() > 0) {
    const char c = static_cast<char>(GPSSerial.read());
    gps.encode(c);
    received_data = true;
  }

  if (received_data) {
    g_last_data_ms = millis();
    if (gps.location.isUpdated() || gps.altitude.isUpdated() || gps.satellites.isUpdated() ||
        gps.hdop.isUpdated() || gps.speed.isUpdated() || gps.course.isUpdated() ||
        gps.date.isUpdated() || gps.time.isUpdated()) {
      g_needs_redraw = true;
    }
  }

  update_status();

  if (M5Cardputer.BtnA.wasClicked()) {
    g_display_mode = next_mode(g_display_mode);
    g_needs_redraw = true;
  }

  const uint32_t now = millis();
  if (now - g_last_draw_ms >= gps_config::UI_REFRESH_MS) {
    g_needs_redraw = true;
  }

  if (g_needs_redraw) {
    draw_ui();
    g_needs_redraw = false;
    g_last_draw_ms = now;
  }
}
