#include <Arduino.h>
#include <M5Cardputer.h>
#include <TinyGPS++.h>

namespace board_pins {
constexpr int GPS_RX = 15;
constexpr int GPS_TX = 13;
}

namespace gps_config {
constexpr uint32_t UART_BAUD = 115200;
constexpr uint32_t UI_REFRESH_MS = 1000;
}

HardwareSerial GPSSerial(1);
TinyGPSPlus gps;

bool g_needs_redraw = true;
String g_status = "Booting...";
uint32_t g_last_draw_ms = 0;
uint32_t g_last_data_ms = 0;

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

void draw_ui() {
  auto& display = M5Cardputer.Display;
  display.startWrite();
  display.fillScreen(TFT_BLACK);
  display.setTextFont(1);
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.println("Cardputer ADV GPS Info");
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.println("Cap LoRa-1262 GNSS");
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.printf("Status: %s\n", g_status.c_str());
  display.println("------------------------------");

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.printf("Lat : %s\n", format_float_value(gps.location.isValid(), gps.location.lat(), 6).c_str());
  display.printf("Lng : %s\n", format_float_value(gps.location.isValid(), gps.location.lng(), 6).c_str());
  display.printf("Alt : %s m\n", format_float_value(gps.altitude.isValid(), gps.altitude.meters(), 1).c_str());
  display.printf("Sat : %s\n", format_uint_value(gps.satellites.isValid(), gps.satellites.value()).c_str());
  display.printf("HDOP: %s\n", format_float_value(gps.hdop.isValid(), gps.hdop.hdop(), 1).c_str());
  display.printf("Spd : %s km/h\n", format_float_value(gps.speed.isValid(), gps.speed.kmph(), 1).c_str());
  display.printf("Crs : %s deg\n", format_float_value(gps.course.isValid(), gps.course.deg(), 1).c_str());
  display.printf("UTC : %s\n", format_date_time().c_str());
  display.printf("Age : %s\n", format_fix_age().c_str());
  display.printf("Chars: %lu Err: %lu\n", gps.charsProcessed(), gps.failedChecksum());

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
