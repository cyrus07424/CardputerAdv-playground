struct DateTime;
struct PointF;
enum class Action : unsigned char;

#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>
#include <string.h>
#include <time.h>

namespace app_config {
constexpr int16_t SCREEN_W = 240;
constexpr int16_t SCREEN_H = 135;
constexpr int16_t DIAL_CX = 74;
constexpr int16_t DIAL_CY = 67;
constexpr int16_t DIAL_R = 62;
constexpr int16_t PANEL_X = 145;
constexpr uint32_t FRAME_INTERVAL_MS = 16;
}  // namespace app_config

namespace keyboard_hid {
constexpr uint8_t RIGHT = 0x4F;
constexpr uint8_t LEFT = 0x50;
constexpr uint8_t DOWN = 0x51;
constexpr uint8_t UP = 0x52;
}  // namespace keyboard_hid

namespace face_ratio {
constexpr float LENGTH_CLOCK_S = 0.135f;
constexpr float LENGTH_CLOCK_M = 0.375f;
constexpr float LENGTH_CLOCK_H = 0.25f;
constexpr float LENGTH_CHRONO_100S = 0.11f;
constexpr float LENGTH_CHRONO_10S = 0.09f;
constexpr float LENGTH_CHRONO_M = 0.125f;
constexpr float LENGTH_CHRONO_S = 0.40f;
constexpr float WIDTH_CLOCK_H = (0.06f / 3.0f) * 2.0f;
constexpr float WIDTH_CLOCK_M = (0.06f / 3.0f) * 2.0f;

constexpr float RADIUS_DOT_MAIN = 0.015f;
constexpr float RADIUS_DOT_METER1 = 0.005f;
constexpr float RADIUS_DOT_METER2 = 0.005f;
constexpr float RADIUS_DOT_METER3 = 0.005f;

constexpr float RADIUS_RING_MAIN_0 = 0.495f;
constexpr float RADIUS_RING_MAIN_1 = 0.45f;
constexpr float RADIUS_RING_MAIN_2 = 0.30f;
constexpr float RADIUS_RING_METER1_1 = 0.13f;
constexpr float RADIUS_RING_METER2_1 = 0.15f;
constexpr float RADIUS_RING_METER3_1 = 0.15f;

constexpr float MAIN_LINE_12_WIDTH_1 = 0.95f;
constexpr float MAIN_LINE_12_WIDTH_2 = 0.85f;
constexpr float MAIN_LINE_15_WIDTH_1 = 0.95f;
constexpr float MAIN_LINE_15_WIDTH_2 = 0.875f;
constexpr float MAIN_LINE_45_WIDTH_1 = 0.95f;
constexpr float MAIN_LINE_45_WIDTH_2 = 0.90f;
constexpr float METER1_LINE_10_WIDTH_1 = 0.875f;
constexpr float METER1_LINE_10_WIDTH_2 = 0.80f;
constexpr float METER2_LINE_4_WIDTH_1 = 0.875f;
constexpr float METER2_LINE_4_WIDTH_2 = 0.65f;
constexpr float METER3_LINE_30_WIDTH_1 = 0.875f;
constexpr float METER3_LINE_30_WIDTH_2 = 0.80f;
constexpr float METER3_LINE_6_WIDTH_1 = 0.875f;
constexpr float METER3_LINE_6_WIDTH_2 = 0.70f;
constexpr float RADIUS_CIRCLE_CHRONO_SEC_1 = 0.015f;
constexpr float RADIUS_CIRCLE_CHRONO_SEC_2 = 0.01f;
constexpr float RADIUS_CIRCLE_DIST_CHRONO_SEC_1 = 0.825f;
constexpr float RADIUS_CENTER_POINT = 0.028f;

constexpr float CHRONO_MINUTE_MAX = 450.0f;
constexpr int CIRCLE_LOOP_NUM_SMALL = 15;
constexpr int CIRCLE_LOOP_NUM_LARGE = 60;
}  // namespace face_ratio

constexpr float kTwoPi = 2.0f * PI;

constexpr uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kLightGray = color565(230, 230, 230);
constexpr uint16_t kGray = color565(180, 180, 180);
constexpr uint16_t kDarkGray = color565(50, 50, 50);
constexpr uint16_t kRed = TFT_RED;
constexpr uint16_t kWhite = TFT_WHITE;
constexpr uint16_t kBlack = TFT_BLACK;

struct DateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

struct PointF {
  float x;
  float y;
};

enum class Action : unsigned char {
  StartResume,
  Stop,
  Split,
  Reset,
};

M5Canvas g_canvas(&M5Cardputer.Display);

DateTime g_clock_time = {};
bool g_clock_from_system = false;
uint32_t g_last_clock_ms = 0;
uint32_t g_last_frame_ms = 0;

double g_starttime = 0.0;
double g_stoptime = 0.0;
double g_resettime = 0.0;
int g_sw1000s = 0;
int g_sw100s = 0;
int g_sw10s = 0;
int g_sw1s = 0;
int g_sw1m = 0;
int g_rsw1000s = 0;
int g_rsw100s = 0;
int g_rsw10s = 0;
int g_rsw1s = 0;
int g_rsw1m = 0;
int g_stopwatchflg = 0;
int g_resetflg = 0;

bool g_prev_start_pressed = false;
bool g_prev_stop_pressed = false;
bool g_prev_split_pressed = false;
bool g_prev_reset_pressed = false;
bool g_prev_help_pressed = false;
bool g_show_help = true;

float dial_diameter() {
  return static_cast<float>(app_config::DIAL_R * 2);
}

float center_x() {
  return app_config::DIAL_CX;
}

float center_y() {
  return app_config::DIAL_CY;
}

float meter1_x() {
  return center_x();
}

float meter1_y() {
  return center_y() - dial_diameter() * 0.2f;
}

float meter2_x() {
  return center_x() - dial_diameter() * 0.25f;
}

float meter2_y() {
  return center_y();
}

float meter3_x() {
  return center_x();
}

float meter3_y() {
  return center_y() + dial_diameter() * 0.25f;
}

float meter4_x() {
  return center_x() + dial_diameter() * 0.18f;
}

float meter4_y() {
  return center_y();
}

double current_time_sec() {
  return static_cast<double>(millis()) * 1.0e-3;
}

bool contains_char_key(const Keyboard_Class::KeysState& status, char key_code) {
  for (const auto key : status.word) {
    if (key == key_code) {
      return true;
    }
  }
  return false;
}

bool contains_alpha_key(const Keyboard_Class::KeysState& status, char lower_case) {
  return contains_char_key(status, lower_case) || contains_char_key(status, lower_case - ('a' - 'A'));
}

bool rising_edge(bool pressed, bool& previous) {
  const bool triggered = pressed && !previous;
  previous = pressed;
  return triggered;
}

int month_from_abbrev(const char* month) {
  static constexpr const char* kMonths[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
  };
  for (int i = 0; i < 12; ++i) {
    if (strncmp(month, kMonths[i], 3) == 0) {
      return i + 1;
    }
  }
  return 1;
}

bool is_leap_year(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int days_in_month(int year, int month) {
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  return kDays[month - 1];
}

void advance_one_second(DateTime& dt) {
  ++dt.second;
  if (dt.second < 60) {
    return;
  }
  dt.second = 0;
  ++dt.minute;
  if (dt.minute < 60) {
    return;
  }
  dt.minute = 0;
  ++dt.hour;
  if (dt.hour < 24) {
    return;
  }
  dt.hour = 0;
  ++dt.day;
  if (dt.day <= days_in_month(dt.year, dt.month)) {
    return;
  }
  dt.day = 1;
  ++dt.month;
  if (dt.month <= 12) {
    return;
  }
  dt.month = 1;
  ++dt.year;
}

DateTime build_time_seed() {
  char month[4] = {};
  int day = 1;
  int year = 2000;
  int hour = 0;
  int minute = 0;
  int second = 0;
  sscanf(__DATE__, "%3s %d %d", month, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
  return {year, month_from_abbrev(month), day, hour, minute, second};
}

DateTime system_time_seed(bool* used_system_time) {
  time_t now = time(nullptr);
  if (now >= 1704067200) {
    tm local_tm = {};
    localtime_r(&now, &local_tm);
    *used_system_time = true;
    return {
        local_tm.tm_year + 1900,
        local_tm.tm_mon + 1,
        local_tm.tm_mday,
        local_tm.tm_hour,
        local_tm.tm_min,
        local_tm.tm_sec,
    };
  }

  *used_system_time = false;
  return build_time_seed();
}

void update_clock(uint32_t now_ms) {
  while (now_ms - g_last_clock_ms >= 1000) {
    g_last_clock_ms += 1000;
    advance_one_second(g_clock_time);
  }
}

int16_t round_i(float value) {
  return static_cast<int16_t>(lroundf(value));
}

int scaled_px(float value) {
  return max(1, static_cast<int>(lroundf(value)));
}

void polar_point(float cx, float cy, float angle, float radius, int16_t* x, int16_t* y) {
  *x = round_i(cx + sinf(angle) * radius);
  *y = round_i(cy - cosf(angle) * radius);
}

void fill_convex_polygon(const PointF* points, int count, uint16_t color) {
  if (count < 3) {
    return;
  }
  for (int i = 1; i < count - 1; ++i) {
    g_canvas.fillTriangle(
        round_i(points[0].x), round_i(points[0].y),
        round_i(points[i].x), round_i(points[i].y),
        round_i(points[i + 1].x), round_i(points[i + 1].y),
        color);
  }
}

void draw_thick_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color, int thickness) {
  const int steps = max(abs(x1 - x0), abs(y1 - y0));
  const int radius = max(0, thickness / 2);
  if (steps == 0) {
    if (radius == 0) {
      g_canvas.drawPixel(x0, y0, color);
    } else {
      g_canvas.fillCircle(x0, y0, radius, color);
    }
    return;
  }

  for (int i = 0; i <= steps; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(steps);
    const int16_t x = round_i(x0 + static_cast<float>(x1 - x0) * t);
    const int16_t y = round_i(y0 + static_cast<float>(y1 - y0) * t);
    if (radius == 0) {
      g_canvas.drawPixel(x, y, color);
    } else {
      g_canvas.fillCircle(x, y, radius, color);
    }
  }
}

void draw_tick(float cx, float cy, float angle, float radius0, float radius1, uint16_t color, int thickness) {
  int16_t x0 = 0;
  int16_t y0 = 0;
  int16_t x1 = 0;
  int16_t y1 = 0;
  polar_point(cx, cy, angle, radius0, &x0, &y0);
  polar_point(cx, cy, angle, radius1, &x1, &y1);
  draw_thick_line(x0, y0, x1, y1, color, max(1, thickness));
}

void draw_label(int16_t x, int16_t y, const char* text) {
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);
  g_canvas.setTextColor(kWhite, kBlack);
  g_canvas.setCursor(x, y);
  g_canvas.print(text);
}

void variable_init() {
  g_starttime = 0.0;
  g_stoptime = 0.0;
  g_resettime = 0.0;
  g_sw1000s = 0;
  g_sw100s = 0;
  g_sw10s = 0;
  g_sw1s = 0;
  g_sw1m = 0;
  g_rsw1000s = 0;
  g_rsw100s = 0;
  g_rsw10s = 0;
  g_rsw1s = 0;
  g_rsw1m = 0;
  g_stopwatchflg = 0;
  g_resetflg = 0;
}

void get_stopwatch_data() {
  const double difftimedata = (current_time_sec() - g_starttime) * 1000.0;

  g_sw1000s = static_cast<int>(difftimedata) % 10;
  g_sw100s = static_cast<int>(difftimedata / 10.0) % 10;
  g_sw10s = static_cast<int>(difftimedata / 100.0) % 10;
  g_sw1s = static_cast<int>(difftimedata / 1000.0) % 60;
  g_sw1m = static_cast<int>(difftimedata / 60000.0);
  if (g_sw1m > 5) {
    g_sw1m = 5;
  }
}

void stopwatch_reset_animation() {
  const double difftimedata = (current_time_sec() - g_resettime) * 1000.0;

  if ((difftimedata / 20.0 + g_rsw1000s) < 10.0) {
    g_sw1000s = static_cast<int>((difftimedata / 20.0) + g_rsw1000s);
  } else {
    g_sw1000s = 0;
  }
  if ((difftimedata / 20.0 + g_rsw100s) < 10.0) {
    g_sw100s = static_cast<int>((difftimedata / 20.0) + g_rsw100s);
  } else {
    g_sw100s = 0;
  }
  if ((difftimedata / 20.0 + g_rsw10s) < 10.0) {
    g_sw10s = static_cast<int>((difftimedata / 20.0) + g_rsw10s);
  } else {
    g_sw10s = 0;
  }
  if ((difftimedata / 20.0 + g_rsw1s) < 60.0) {
    g_sw1s = static_cast<int>((difftimedata / 20.0) + g_rsw1s);
  } else {
    g_sw1s = 0;
  }
  if ((difftimedata / 20.0 + g_rsw1m) < 5.0) {
    g_sw1m = g_rsw1m - static_cast<int>(difftimedata / 20.0);
  } else {
    g_sw1m = 0;
  }

  if (g_sw1000s == 0 && g_sw100s == 0 && g_sw10s == 0 && g_sw1s == 0 && g_sw1m == 0) {
    g_resetflg = 0;
    g_stopwatchflg = 0;
  }
}

void stopwatch_split_animation() {
  double difftimedata = (current_time_sec() - g_starttime) * 1000.0;

  int sw1000sbuf = static_cast<int>(difftimedata) % 10;
  int sw100sbuf = static_cast<int>(difftimedata / 10.0) % 10;
  int sw10sbuf = static_cast<int>(difftimedata / 100.0) % 10;
  int sw1sbuf = static_cast<int>(difftimedata / 1000.0) % 60;
  int sw1mbuf = static_cast<int>(difftimedata / 60000.0);
  if (sw1mbuf > 5) {
    sw1mbuf = 5;
  }

  difftimedata = (current_time_sec() - g_resettime) * 1000.0;

  if (static_cast<int>(difftimedata / 50.0 + g_rsw1000s) < sw1000sbuf) {
    g_sw1000s = static_cast<int>((difftimedata / 50.0) + g_rsw1000s);
    if (g_sw1000s >= 10) {
      g_sw1000s = 0;
    }
  } else {
    g_sw1000s = sw1000sbuf;
  }
  if (static_cast<int>(difftimedata / 50.0 + g_rsw100s) < sw100sbuf) {
    g_sw100s = static_cast<int>((difftimedata / 50.0) + g_rsw100s);
    if (g_sw100s >= 10) {
      g_sw100s = 0;
    }
  } else {
    g_sw100s = sw100sbuf;
  }
  if (static_cast<int>(difftimedata / 50.0 + g_rsw10s) < sw10sbuf) {
    g_sw10s = static_cast<int>((difftimedata / 50.0) + g_rsw10s);
    if (g_sw10s >= 10) {
      g_sw10s = 0;
    }
  } else {
    g_sw10s = sw10sbuf;
  }
  if (static_cast<int>(difftimedata / 50.0 + g_rsw1s) < sw1sbuf) {
    g_sw1s = static_cast<int>((difftimedata / 50.0) + g_rsw1s);
    if (g_sw1s >= 60) {
      g_sw1s = 0;
    }
  } else {
    g_sw1s = sw1sbuf;
  }
  if (static_cast<int>(difftimedata / 50.0 + g_rsw1m) < sw1mbuf) {
    g_sw1m = static_cast<int>((difftimedata / 50.0) + g_rsw1m);
    if (g_sw1m >= 10) {
      g_sw1m = 0;
    }
  } else {
    g_sw1m = sw1mbuf;
  }

  if (g_sw1000s == sw1000sbuf && g_sw100s == sw100sbuf && g_sw10s == sw10sbuf &&
      g_sw1s == sw1sbuf && g_sw1m == sw1mbuf) {
    g_resetflg = 0;
    g_stopwatchflg = 1;
  }
}

void update_stopwatch_animation() {
  if (g_resetflg == 1) {
    stopwatch_reset_animation();
  } else if (g_resetflg == 2) {
    stopwatch_split_animation();
  } else if (g_resetflg == 0 && g_stopwatchflg == 1) {
    get_stopwatch_data();
  }
}

void snapshot_reset_digits() {
  g_rsw1000s = g_sw1000s;
  g_rsw100s = g_sw100s;
  g_rsw10s = g_sw10s;
  g_rsw1s = g_sw1s;
  g_rsw1m = (g_sw1m > 5) ? g_sw1m : 5;
}

void perform_action(Action action) {
  const double now = current_time_sec();

  switch (action) {
    case Action::StartResume:
      if (g_stopwatchflg == 0 && g_resetflg == 0) {
        g_starttime = now;
        g_stopwatchflg = 1;
      } else if (g_stopwatchflg == 2) {
        g_starttime = g_starttime + (now - g_stoptime);
        g_stopwatchflg = 1;
      } else if (g_stopwatchflg == 4) {
        g_resettime = now;
        g_resetflg = 2;
        g_stopwatchflg = 1;
        snapshot_reset_digits();
      }
      break;
    case Action::Stop:
      if (g_stopwatchflg == 1) {
        g_stoptime = now;
        g_stopwatchflg = 2;
      }
      break;
    case Action::Split:
      if (g_stopwatchflg == 1) {
        g_stopwatchflg = 4;
      }
      break;
    case Action::Reset:
      if (g_stopwatchflg == 2) {
        g_resettime = now;
        g_resetflg = 1;
        g_stopwatchflg = 3;
        snapshot_reset_digits();
      }
      break;
  }
}

void draw_needle(float theta, float width, float length) {
  const float cx = center_x();
  const float cy = center_y();
  const float diameter = dial_diameter();

  PointF base_points[6] = {};
  float thetabuf = theta + (PI * 0.5f * 0.3f);
  base_points[0] = {cx, cy};
  base_points[1] = {cx + (diameter * width) * sinf(thetabuf), cy - (diameter * width) * cosf(thetabuf)};

  thetabuf = theta + (0.1f * (0.9f - length));
  base_points[2] = {
      cx + (diameter * length * 0.95f) * sinf(thetabuf),
      cy - (diameter * length * 0.95f) * cosf(thetabuf),
  };

  thetabuf = theta;
  base_points[3] = {cx + (diameter * length) * sinf(thetabuf), cy - (diameter * length) * cosf(thetabuf)};

  thetabuf = theta - (0.1f * (0.9f - length));
  base_points[4] = {
      cx + (diameter * length * 0.95f) * sinf(thetabuf),
      cy - (diameter * length * 0.95f) * cosf(thetabuf),
  };

  thetabuf = theta - (PI * 0.5f * 0.3f);
  base_points[5] = {cx + (diameter * width) * sinf(thetabuf), cy - (diameter * width) * cosf(thetabuf)};
  fill_convex_polygon(base_points, 6, kLightGray);

  PointF red_points[6] = {};
  const float base_x = cx + (diameter * length * 0.15f) * sinf(theta);
  const float base_y = cy - (diameter * length * 0.15f) * cosf(theta);
  red_points[0] = {base_x, base_y};

  thetabuf = theta + (PI * 0.5f * 0.55f);
  red_points[1] = {
      base_x + (diameter * width * 0.4f) * sinf(thetabuf),
      base_y - (diameter * width * 0.4f) * cosf(thetabuf),
  };

  thetabuf = theta + (0.1f * (0.8f - length));
  red_points[2] = {
      cx + (diameter * length * 0.95f * 0.9f) * sinf(thetabuf),
      cy - (diameter * length * 0.95f * 0.9f) * cosf(thetabuf),
  };

  thetabuf = theta;
  red_points[3] = {
      cx + (diameter * length * 0.9f) * sinf(thetabuf),
      cy - (diameter * length * 0.9f) * cosf(thetabuf),
  };

  thetabuf = theta - (0.1f * (0.8f - length));
  red_points[4] = {
      cx + (diameter * length * 0.95f * 0.9f) * sinf(thetabuf),
      cy - (diameter * length * 0.95f * 0.9f) * cosf(thetabuf),
  };

  thetabuf = theta - (PI * 0.5f * 0.55f);
  red_points[5] = {
      base_x + (diameter * width * 0.4f) * sinf(thetabuf),
      base_y - (diameter * width * 0.4f) * cosf(thetabuf),
  };
  fill_convex_polygon(red_points, 6, kRed);
}

void draw_surface() {
  const float diameter = dial_diameter();
  const float cx = center_x();
  const float cy = center_y();
  const int main_tick_wide = scaled_px(diameter / 50.0f);
  const int main_tick_fine = scaled_px(diameter / 300.0f);
  const int sub_tick = scaled_px(diameter / 500.0f);
  const int chrono_tick = scaled_px(diameter / 400.0f);

  g_canvas.fillCircle(round_i(cx), round_i(cy), round_i(diameter * face_ratio::RADIUS_RING_MAIN_0), kDarkGray);
  g_canvas.fillCircle(round_i(cx), round_i(cy), round_i(diameter * face_ratio::RADIUS_RING_MAIN_1), kBlack);
  g_canvas.drawCircle(round_i(cx), round_i(cy), round_i(diameter * face_ratio::RADIUS_RING_MAIN_0), kLightGray);

  for (int j = 0; j < 5; ++j) {
    const float radius = (diameter * face_ratio::RADIUS_RING_MAIN_2) - (diameter / 100.0f) * static_cast<float>(j);
    g_canvas.drawCircle(round_i(cx), round_i(cy), round_i(radius), kDarkGray);
  }

  for (int i = 0; i < 15; ++i) {
    const float angle = (kTwoPi * static_cast<float>(i)) / static_cast<float>(face_ratio::CIRCLE_LOOP_NUM_LARGE);
    draw_tick(
        cx,
        cy,
        angle,
        diameter * face_ratio::RADIUS_RING_MAIN_1 * face_ratio::MAIN_LINE_15_WIDTH_1,
        diameter * face_ratio::RADIUS_RING_MAIN_1 * face_ratio::MAIN_LINE_15_WIDTH_2,
        kRed,
        main_tick_wide);
  }
  for (int i = 15; i < face_ratio::CIRCLE_LOOP_NUM_LARGE; ++i) {
    const float angle = (kTwoPi * static_cast<float>(i)) / static_cast<float>(face_ratio::CIRCLE_LOOP_NUM_LARGE);
    draw_tick(
        cx,
        cy,
        angle,
        diameter * face_ratio::RADIUS_RING_MAIN_1 * face_ratio::MAIN_LINE_45_WIDTH_1,
        diameter * face_ratio::RADIUS_RING_MAIN_1 * face_ratio::MAIN_LINE_45_WIDTH_2,
        kGray,
        main_tick_fine);
  }
  for (int i = 0; i < 12; ++i) {
    const float angle = (kTwoPi * static_cast<float>(i)) / 12.0f;
    draw_tick(
        cx,
        cy,
        angle,
        diameter * face_ratio::RADIUS_RING_MAIN_1 * face_ratio::MAIN_LINE_12_WIDTH_1,
        diameter * face_ratio::RADIUS_RING_MAIN_1 * face_ratio::MAIN_LINE_12_WIDTH_2,
        kLightGray,
        main_tick_wide);
  }

  g_canvas.fillCircle(round_i(meter1_x()), round_i(meter1_y()), round_i(diameter * face_ratio::RADIUS_RING_METER1_1), kBlack);
  for (int j = 0; j < 2; ++j) {
    const float radius =
        (diameter * face_ratio::RADIUS_RING_METER1_1) - (diameter / 100.0f) * static_cast<float>(j);
    g_canvas.drawCircle(round_i(meter1_x()), round_i(meter1_y()), round_i(radius), kDarkGray);
  }
  for (int i = 0; i < 10; ++i) {
    const float angle = (kTwoPi * static_cast<float>(i)) / 10.0f;
    draw_tick(
        meter1_x(),
        meter1_y(),
        angle,
        diameter * face_ratio::RADIUS_RING_METER1_1 * face_ratio::METER1_LINE_10_WIDTH_1,
        diameter * face_ratio::RADIUS_RING_METER1_1 * face_ratio::METER1_LINE_10_WIDTH_2,
        kRed,
        sub_tick);
  }

  g_canvas.fillCircle(round_i(meter2_x()), round_i(meter2_y()), round_i(diameter * face_ratio::RADIUS_RING_METER2_1), kBlack);
  for (int j = 0; j < 2; ++j) {
    const float radius =
        (diameter * face_ratio::RADIUS_RING_METER2_1) - (diameter / 100.0f) * static_cast<float>(j);
    g_canvas.drawCircle(round_i(meter2_x()), round_i(meter2_y()), round_i(radius), kDarkGray);
  }
  for (int i = 0; i < 4; ++i) {
    const float angle = (kTwoPi * static_cast<float>(i)) / 4.0f;
    draw_tick(
        meter2_x(),
        meter2_y(),
        angle,
        diameter * face_ratio::RADIUS_RING_METER2_1 * face_ratio::METER2_LINE_4_WIDTH_1,
        diameter * face_ratio::RADIUS_RING_METER2_1 * face_ratio::METER2_LINE_4_WIDTH_2,
        kRed,
        sub_tick);
  }

  g_canvas.fillCircle(round_i(meter3_x()), round_i(meter3_y()), round_i(diameter * face_ratio::RADIUS_RING_METER3_1), kBlack);
  for (int j = 0; j < 2; ++j) {
    const float radius =
        (diameter * face_ratio::RADIUS_RING_METER3_1) - (diameter / 100.0f) * static_cast<float>(j);
    g_canvas.drawCircle(round_i(meter3_x()), round_i(meter3_y()), round_i(radius), kDarkGray);
  }
  for (int i = 0; i < static_cast<int>(face_ratio::CHRONO_MINUTE_MAX / 15.0f); ++i) {
    if (i % 6 == 0) {
      continue;
    }
    const float angle =
        (kTwoPi * static_cast<float>(i)) / (face_ratio::CHRONO_MINUTE_MAX / 10.0f) - (2.0f * PI / 3.0f);
    draw_tick(
        meter3_x(),
        meter3_y(),
        angle,
        diameter * face_ratio::RADIUS_RING_METER3_1 * face_ratio::METER3_LINE_30_WIDTH_1,
        diameter * face_ratio::RADIUS_RING_METER3_1 * face_ratio::METER3_LINE_30_WIDTH_2,
        kGray,
        sub_tick);
  }
  for (int i = 0; i < static_cast<int>(face_ratio::CHRONO_MINUTE_MAX / 90.0f) + 1; ++i) {
    const float angle =
        (kTwoPi * static_cast<float>(i)) / (face_ratio::CHRONO_MINUTE_MAX / 60.0f) - (2.0f * PI / 3.0f);
    draw_tick(
        meter3_x(),
        meter3_y(),
        angle,
        diameter * face_ratio::RADIUS_RING_METER3_1 * face_ratio::METER3_LINE_6_WIDTH_1,
        diameter * face_ratio::RADIUS_RING_METER3_1 * face_ratio::METER3_LINE_6_WIDTH_2,
        kRed,
        chrono_tick);
  }

  g_canvas.fillCircle(round_i(cx), round_i(cy), round_i(diameter * face_ratio::RADIUS_CENTER_POINT), kDarkGray);

  const int16_t meter4_left = round_i(meter4_x()) + 3;
  const int16_t meter4_top = round_i(meter4_y()) - 7;
  g_canvas.drawRect(meter4_left, meter4_top, 20, 15, kDarkGray);
}

void draw_analog_clock() {
  const float diameter = dial_diameter();
  const float cx = center_x();
  const float cy = center_y();
  const float m1x = meter1_x();
  const float m1y = meter1_y();
  const float m2x = meter2_x();
  const float m2y = meter2_y();
  const float m3x = meter3_x();
  const float m3y = meter3_y();

  char mdaystr[3] = {};
  snprintf(mdaystr, sizeof(mdaystr), "%02d", g_clock_time.day);
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);
  g_canvas.setTextColor(kWhite, kBlack);
  g_canvas.setCursor(round_i(meter4_x()) + 8, round_i(meter4_y()) - 3);
  g_canvas.print(mdaystr);

  float theta = (kTwoPi * (g_sw10s + (g_sw100s / 10.0f) + (g_sw1000s / 100.0f))) / 10.0f;
  int16_t x0 = round_i(m1x);
  int16_t y0 = round_i(m1y);
  int16_t x1 = 0;
  int16_t y1 = 0;
  polar_point(m1x, m1y, theta, diameter * face_ratio::LENGTH_CHRONO_10S, &x1, &y1);
  draw_thick_line(x0, y0, x1, y1, kRed, 1);
  polar_point(m1x, m1y, theta + PI, diameter * face_ratio::LENGTH_CHRONO_10S * 0.1f, &x1, &y1);
  draw_thick_line(x0, y0, x1, y1, kRed, 1);

  theta = (kTwoPi * (g_sw100s + (g_sw1000s / 10.0f))) / 10.0f;
  polar_point(m1x, m1y, theta, diameter * face_ratio::LENGTH_CHRONO_100S, &x1, &y1);
  draw_thick_line(x0, y0, x1, y1, kRed, 1);
  polar_point(m1x, m1y, theta + PI, diameter * face_ratio::LENGTH_CHRONO_100S * 0.1f, &x1, &y1);
  draw_thick_line(x0, y0, x1, y1, kRed, 1);
  g_canvas.fillCircle(x0, y0, scaled_px(diameter * face_ratio::RADIUS_DOT_METER1), kRed);

  if (g_resetflg == 0 || g_resetflg == 2) {
    theta = ((kTwoPi * (60.0f * g_sw1m + g_sw1s + (g_sw10s / 10.0f) + (g_sw100s / 100.0f))) /
             face_ratio::CHRONO_MINUTE_MAX) -
            (2.0f * PI / 3.0f);
    const float max_theta =
        ((kTwoPi * (60.0f * 5.0f)) / face_ratio::CHRONO_MINUTE_MAX) - (2.0f * PI / 3.0f);
    if (theta > max_theta) {
      theta = max_theta;
    }
  } else if (g_resetflg == 1 &&
             (60.0f * g_sw1m - (10.0f - g_sw1s) - ((10.0f - g_sw10s) / 10.0f) -
              ((10.0f - g_sw100s) / 100.0f)) > 0.0f) {
    theta = ((kTwoPi * (60.0f * g_sw1m - (10.0f - g_sw1s) - ((10.0f - g_sw10s) / 10.0f) -
                        ((10.0f - g_sw100s) / 100.0f))) /
             face_ratio::CHRONO_MINUTE_MAX) -
            (2.0f * PI / 3.0f);
  } else {
    theta = -2.0f * PI / 3.0f;
  }
  polar_point(m3x, m3y, theta, diameter * face_ratio::LENGTH_CHRONO_M, &x1, &y1);
  draw_thick_line(round_i(m3x), round_i(m3y), x1, y1, kRed, 1);

  if (g_resetflg == 0 || g_resetflg == 2) {
    theta = ((kTwoPi * (60.0f * g_sw1m + g_sw1s + (g_sw10s / 10.0f) + (g_sw100s / 100.0f))) /
             face_ratio::CHRONO_MINUTE_MAX) +
            (PI / 3.0f);
  } else if (g_resetflg == 1 &&
             (60.0f * g_sw1m - (10.0f - g_sw1s) - ((10.0f - g_sw10s) / 10.0f) -
              ((10.0f - g_sw100s) / 100.0f)) >
                 (-2.0f * PI / 3.0f)) {
    theta = ((kTwoPi * (60.0f * g_sw1m - (10.0f - g_sw1s) - ((10.0f - g_sw10s) / 10.0f) -
                        ((10.0f - g_sw100s) / 100.0f))) /
             face_ratio::CHRONO_MINUTE_MAX) +
            (PI / 3.0f);
  } else {
    theta = PI / 3.0f;
  }
  polar_point(m3x, m3y, theta, diameter * face_ratio::LENGTH_CHRONO_M * 0.1f, &x1, &y1);
  draw_thick_line(round_i(m3x), round_i(m3y), x1, y1, kRed, 1);
  g_canvas.fillCircle(round_i(m3x), round_i(m3y), scaled_px(diameter * face_ratio::RADIUS_DOT_METER3), kRed);

  theta = (kTwoPi * static_cast<float>(g_clock_time.second)) / 60.0f;
  polar_point(m2x, m2y, theta, diameter * face_ratio::LENGTH_CLOCK_S, &x1, &y1);
  draw_thick_line(round_i(m2x), round_i(m2y), x1, y1, kRed, 1);
  polar_point(m2x, m2y, theta + PI, diameter * face_ratio::LENGTH_CLOCK_S * 0.1f, &x1, &y1);
  draw_thick_line(round_i(m2x), round_i(m2y), x1, y1, kRed, 1);
  g_canvas.fillCircle(round_i(m2x), round_i(m2y), scaled_px(diameter * face_ratio::RADIUS_DOT_METER2), kRed);

  const float thetah =
      (kTwoPi * (3600.0f * g_clock_time.hour + 60.0f * g_clock_time.minute + g_clock_time.second)) / 43200.0f;
  const float thetam = (kTwoPi * (60.0f * g_clock_time.minute + g_clock_time.second)) / 3600.0f;
  draw_needle(thetah, face_ratio::WIDTH_CLOCK_H, face_ratio::LENGTH_CLOCK_H);
  draw_needle(thetam, face_ratio::WIDTH_CLOCK_M, face_ratio::LENGTH_CLOCK_M);

  theta = (kTwoPi * (g_sw1s + (g_sw10s / 10.0f) + (g_sw100s / 100.0f))) / 60.0f;
  polar_point(cx, cy, theta, diameter * face_ratio::LENGTH_CHRONO_S, &x1, &y1);
  draw_thick_line(round_i(cx), round_i(cy), x1, y1, kRed, scaled_px(diameter / 160.0f));
  polar_point(cx, cy, theta + PI, diameter * face_ratio::LENGTH_CHRONO_S * 0.15f, &x1, &y1);
  draw_thick_line(round_i(cx), round_i(cy), x1, y1, kRed, scaled_px(diameter / 160.0f));
  g_canvas.fillCircle(round_i(cx), round_i(cy), scaled_px(diameter * face_ratio::RADIUS_DOT_MAIN), kRed);

  int16_t indicator_x = 0;
  int16_t indicator_y = 0;
  polar_point(
      cx,
      cy,
      theta,
      diameter * face_ratio::LENGTH_CHRONO_S * face_ratio::RADIUS_CIRCLE_DIST_CHRONO_SEC_1,
      &indicator_x,
      &indicator_y);
  g_canvas.fillCircle(indicator_x, indicator_y, scaled_px(diameter * face_ratio::RADIUS_CIRCLE_CHRONO_SEC_1), kRed);
  g_canvas.fillCircle(indicator_x, indicator_y, scaled_px(diameter * face_ratio::RADIUS_CIRCLE_CHRONO_SEC_2), kWhite);
}

const char* stopwatch_status_label() {
  if (g_stopwatchflg == 4) {
    return "SPLIT";
  }
  if (g_stopwatchflg == 1 || g_resetflg == 2) {
    return "RUN";
  }
  if (g_stopwatchflg == 3 || g_resetflg == 1) {
    return "RESET";
  }
  return (g_sw1m == 0 && g_sw1s == 0 && g_sw10s == 0 && g_sw100s == 0 && g_sw1000s == 0) ? "READY" : "STOP";
}

void draw_info_panel() {
  const uint32_t chrono_minutes = static_cast<uint32_t>(max(0, g_sw1m));
  const uint32_t chrono_seconds = static_cast<uint32_t>(max(0, g_sw1s));
  const uint32_t chrono_hundredths = static_cast<uint32_t>(max(0, g_sw10s * 10 + g_sw100s));

  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);
  g_canvas.setTextColor(TFT_CYAN, kBlack);
  g_canvas.setCursor(app_config::PANEL_X, 8);
  g_canvas.print("Analog Clock");

  g_canvas.setTextColor(kWhite, kBlack);
  g_canvas.setCursor(app_config::PANEL_X, 24);
  g_canvas.printf("%04d-%02d-%02d", g_clock_time.year, g_clock_time.month, g_clock_time.day);
  g_canvas.setCursor(app_config::PANEL_X, 36);
  g_canvas.printf("%02d:%02d:%02d", g_clock_time.hour, g_clock_time.minute, g_clock_time.second);

  g_canvas.setTextColor(TFT_ORANGE, kBlack);
  g_canvas.setCursor(app_config::PANEL_X, 54);
  g_canvas.print("CHRONO");
  g_canvas.setTextColor(kWhite, kBlack);
  g_canvas.setCursor(app_config::PANEL_X, 66);
  g_canvas.printf("%02lu:%02lu.%02lu", chrono_minutes, chrono_seconds, chrono_hundredths);
  g_canvas.setCursor(app_config::PANEL_X, 78);
  g_canvas.printf("STATE %s", stopwatch_status_label());

  g_canvas.setTextColor(TFT_DARKGREY, kBlack);
  g_canvas.setCursor(app_config::PANEL_X, 92);
  g_canvas.printf("time:%s", g_clock_from_system ? "system" : "build");

  if (!g_show_help) {
    g_canvas.setCursor(app_config::PANEL_X, 114);
    g_canvas.print("Enter/BtnA: help");
    return;
  }

  g_canvas.setTextColor(TFT_GREENYELLOW, kBlack);
  g_canvas.setCursor(app_config::PANEL_X, 104);
  g_canvas.print("A start/resume");
  g_canvas.setCursor(app_config::PANEL_X, 114);
  g_canvas.print("S stop  D split");
  g_canvas.setCursor(app_config::PANEL_X, 124);
  g_canvas.print("R reset Enter help");
}

void draw_ui() {
  g_canvas.fillScreen(kBlack);
  draw_surface();
  draw_analog_clock();
  draw_info_panel();
  g_canvas.pushSprite(&M5Cardputer.Display, 0, 0);
}

void handle_input() {
  const auto status = M5Cardputer.Keyboard.keysState();
  const bool start_pressed = contains_alpha_key(status, 'a');
  const bool stop_pressed = contains_alpha_key(status, 's');
  const bool split_pressed = contains_alpha_key(status, 'd');
  const bool reset_pressed = contains_alpha_key(status, 'r');
  const bool help_pressed = status.enter;

  if (rising_edge(start_pressed, g_prev_start_pressed)) {
    perform_action(Action::StartResume);
  }
  if (rising_edge(stop_pressed, g_prev_stop_pressed)) {
    perform_action(Action::Stop);
  }
  if (rising_edge(split_pressed, g_prev_split_pressed)) {
    perform_action(Action::Split);
  }
  if (rising_edge(reset_pressed, g_prev_reset_pressed)) {
    perform_action(Action::Reset);
  }
  if (rising_edge(help_pressed, g_prev_help_pressed) || M5Cardputer.BtnA.wasClicked()) {
    g_show_help = !g_show_help;
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, true);

  setenv("TZ", "JST-9", 1);
  tzset();

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);

  g_canvas.setColorDepth(16);
  g_canvas.createSprite(app_config::SCREEN_W, app_config::SCREEN_H);
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);

  variable_init();
  g_clock_time = system_time_seed(&g_clock_from_system);
  g_last_clock_ms = millis();
  g_last_frame_ms = g_last_clock_ms;
  draw_ui();
}

void loop() {
  M5Cardputer.update();

  const uint32_t now_ms = millis();
  handle_input();
  update_clock(now_ms);
  update_stopwatch_animation();

  if (now_ms - g_last_frame_ms >= app_config::FRAME_INTERVAL_MS) {
    draw_ui();
    g_last_frame_ms = now_ms;
  }
}
