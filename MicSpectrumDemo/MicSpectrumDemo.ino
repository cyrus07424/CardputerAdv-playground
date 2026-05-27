#include <Arduino.h>
#include <M5Cardputer.h>
#include <math.h>

namespace app_config {
constexpr uint32_t SAMPLE_RATE = 17000;
constexpr size_t RECORD_COUNT = 240;
constexpr size_t FFT_SAMPLE_COUNT = 128;
constexpr size_t FFT_BIN_COUNT = FFT_SAMPLE_COUNT / 2;
constexpr int BAR_COUNT = 24;
constexpr uint32_t FRAME_INTERVAL_MS = 45;
constexpr uint32_t MIC_RETRY_INTERVAL_MS = 1000;
constexpr int SPECTRUM_X = 6;
constexpr int SPECTRUM_Y = 30;
constexpr int SPECTRUM_W = 228;
constexpr int SPECTRUM_H = 58;
constexpr int WAVEFORM_X = 6;
constexpr int WAVEFORM_Y = 93;
constexpr int WAVEFORM_W = 146;
constexpr int WAVEFORM_H = 20;
constexpr float MIN_FREQ_HZ = 80.0f;
constexpr float MAX_FREQ_HZ = 3600.0f;
}  // namespace app_config

constexpr float kTwoPi = 6.28318530717958647692f;

constexpr uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kColorBg = color565(5, 8, 14);
constexpr uint16_t kColorPanel = color565(12, 18, 28);
constexpr uint16_t kColorGrid = color565(28, 46, 66);
constexpr uint16_t kColorText = color565(230, 238, 244);
constexpr uint16_t kColorMuted = color565(112, 138, 160);
constexpr uint16_t kColorAccent = color565(48, 224, 200);
constexpr uint16_t kColorAccentDim = color565(18, 94, 88);
constexpr uint16_t kColorPeak = color565(255, 197, 92);
constexpr uint16_t kColorWarn = color565(255, 110, 86);
constexpr uint16_t kColorHold = color565(255, 158, 54);
constexpr uint16_t kColorLevel = color565(90, 240, 120);

M5Canvas g_canvas(&M5Cardputer.Display);

int16_t g_mic_samples[app_config::RECORD_COUNT] = {};
float g_window[app_config::FFT_SAMPLE_COUNT] = {};
float g_windowed_samples[app_config::FFT_SAMPLE_COUNT] = {};
float g_spectrum[app_config::FFT_BIN_COUNT] = {};
float g_bar_levels[app_config::BAR_COUNT] = {};
float g_peak_levels[app_config::BAR_COUNT] = {};
int g_bar_bin_start[app_config::BAR_COUNT] = {};
int g_bar_bin_end[app_config::BAR_COUNT] = {};

bool g_mic_ready = false;
bool g_hold = false;
bool g_needs_redraw = true;
bool g_prev_enter = false;
bool g_prev_backspace = false;

uint32_t g_last_frame_ms = 0;
uint32_t g_last_mic_retry_ms = 0;
float g_input_level = 0.0f;
float g_gain_reference = 1.0f;
float g_dominant_frequency_hz = 0.0f;
float g_last_peak_abs = 0.0f;
float g_waveform_scale = 1.0f;
String g_status_message = "READY";

float clamp_value(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

void reset_peak_levels() {
  for (int i = 0; i < app_config::BAR_COUNT; ++i) {
    g_peak_levels[i] = g_bar_levels[i];
  }
}

void build_window() {
  for (size_t i = 0; i < app_config::FFT_SAMPLE_COUNT; ++i) {
    g_window[i] = 0.5f - 0.5f * cosf(kTwoPi * static_cast<float>(i) / static_cast<float>(app_config::FFT_SAMPLE_COUNT - 1));
  }
}

void build_bar_mapping() {
  const float bin_hz = static_cast<float>(app_config::SAMPLE_RATE) / static_cast<float>(app_config::FFT_SAMPLE_COUNT);
  const int max_bin = static_cast<int>(app_config::FFT_BIN_COUNT) - 1;
  const float ratio = app_config::MAX_FREQ_HZ / app_config::MIN_FREQ_HZ;

  for (int i = 0; i < app_config::BAR_COUNT; ++i) {
    const float start_t = static_cast<float>(i) / static_cast<float>(app_config::BAR_COUNT);
    const float end_t = static_cast<float>(i + 1) / static_cast<float>(app_config::BAR_COUNT);
    const float start_hz = app_config::MIN_FREQ_HZ * powf(ratio, start_t);
    const float end_hz = app_config::MIN_FREQ_HZ * powf(ratio, end_t);
    int bin_start = static_cast<int>(floorf(start_hz / bin_hz));
    int bin_end = static_cast<int>(ceilf(end_hz / bin_hz));
    if (bin_start < 1) {
      bin_start = 1;
    }
    if (bin_end < bin_start) {
      bin_end = bin_start;
    }
    if (bin_end > max_bin) {
      bin_end = max_bin;
    }
    g_bar_bin_start[i] = bin_start;
    g_bar_bin_end[i] = bin_end;
  }
}

void update_controls() {
  if (M5Cardputer.BtnA.wasClicked()) {
    g_hold = !g_hold;
    g_status_message = g_hold ? "HOLD" : "LIVE";
    g_needs_redraw = true;
  }

  if (!M5Cardputer.Keyboard.isChange()) {
    return;
  }

  const auto status = M5Cardputer.Keyboard.keysState();
  if (status.enter && !g_prev_enter) {
    g_hold = !g_hold;
    g_status_message = g_hold ? "HOLD" : "LIVE";
    g_needs_redraw = true;
  }
  if (status.del && !g_prev_backspace) {
    reset_peak_levels();
    g_status_message = "PEAK RESET";
    g_needs_redraw = true;
  }

  g_prev_enter = status.enter;
  g_prev_backspace = status.del;
}

bool capture_audio() {
  if (!g_mic_ready || g_hold) {
    return false;
  }

  if (!M5Cardputer.Mic.isEnabled()) {
    g_mic_ready = false;
    g_status_message = "MIC DISABLED";
    g_needs_redraw = true;
    return false;
  }

  if (!M5Cardputer.Mic.record(g_mic_samples, app_config::RECORD_COUNT, app_config::SAMPLE_RATE, false)) {
    g_status_message = "MIC TIMEOUT";
    g_needs_redraw = true;
    return false;
  }

  return true;
}

void analyze_audio() {
  float mean = 0.0f;
  for (size_t i = 0; i < app_config::FFT_SAMPLE_COUNT; ++i) {
    mean += static_cast<float>(g_mic_samples[i]);
  }
  mean /= static_cast<float>(app_config::FFT_SAMPLE_COUNT);

  float level_accum = 0.0f;
  float max_abs = 1.0f;
  for (size_t i = 0; i < app_config::FFT_SAMPLE_COUNT; ++i) {
    const float centered = static_cast<float>(g_mic_samples[i]) - mean;
    g_windowed_samples[i] = centered * g_window[i];
    const float abs_centered = fabsf(centered);
    level_accum += abs_centered;
    if (abs_centered > max_abs) {
      max_abs = abs_centered;
    }
  }

  for (size_t i = 0; i < app_config::FFT_BIN_COUNT; ++i) {
    g_spectrum[i] = 0.0f;
  }

  g_waveform_scale = max_abs;
  g_last_peak_abs = max_abs;
  const float normalized_level = clamp_value(max_abs / 2048.0f, 0.0f, 1.0f);
  g_input_level = 0.75f * g_input_level + 0.25f * clamp_value(normalized_level, 0.0f, 1.0f);

  float frame_max = 1.0f;
  int dominant_bin = 1;

  for (size_t bin = 1; bin < app_config::FFT_BIN_COUNT; ++bin) {
    const float step = kTwoPi * static_cast<float>(bin) / static_cast<float>(app_config::FFT_SAMPLE_COUNT);
    const float cos_step = cosf(step);
    const float sin_step = sinf(step);
    float cos_value = 1.0f;
    float sin_value = 0.0f;
    float real = 0.0f;
    float imag = 0.0f;

    for (size_t sample = 0; sample < app_config::FFT_SAMPLE_COUNT; ++sample) {
      const float value = g_windowed_samples[sample];
      real += value * cos_value;
      imag -= value * sin_value;

      const float next_cos = cos_value * cos_step - sin_value * sin_step;
      sin_value = sin_value * cos_step + cos_value * sin_step;
      cos_value = next_cos;
    }

    const float magnitude = sqrtf(real * real + imag * imag);
    g_spectrum[bin] = magnitude;
    if (magnitude > frame_max) {
      frame_max = magnitude;
      dominant_bin = static_cast<int>(bin);
    }
  }

  g_gain_reference = frame_max > g_gain_reference ? frame_max : (g_gain_reference * 0.92f + frame_max * 0.08f);
  const float bin_hz = static_cast<float>(app_config::SAMPLE_RATE) / static_cast<float>(app_config::FFT_SAMPLE_COUNT);
  g_dominant_frequency_hz = static_cast<float>(dominant_bin) * bin_hz;

  for (int i = 0; i < app_config::BAR_COUNT; ++i) {
    float band_peak = 0.0f;
    for (int bin = g_bar_bin_start[i]; bin <= g_bar_bin_end[i]; ++bin) {
      if (g_spectrum[bin] > band_peak) {
        band_peak = g_spectrum[bin];
      }
    }

    const float normalized = clamp_value(sqrtf(band_peak / g_gain_reference), 0.0f, 1.0f);
    g_bar_levels[i] = normalized > g_bar_levels[i] ? normalized : (g_bar_levels[i] * 0.72f + normalized * 0.28f);
    g_peak_levels[i] = normalized > g_peak_levels[i] ? normalized : clamp_value(g_peak_levels[i] - 0.018f, 0.0f, 1.0f);
  }

  g_status_message = max_abs > 96.0f ? "REC" : "LISTENING";
}

void draw_error_screen() {
  auto& canvas = g_canvas;
  canvas.fillSprite(kColorBg);
  canvas.fillRoundRect(12, 18, 216, 98, 10, kColorPanel);
  canvas.drawRoundRect(12, 18, 216, 98, 10, kColorWarn);

  canvas.setTextColor(kColorWarn, kColorPanel);
  canvas.setCursor(24, 32);
  canvas.print("MIC INITIALIZATION FAILED");

  canvas.setTextColor(kColorText, kColorPanel);
  canvas.setCursor(24, 54);
  canvas.print("Check M5Cardputer library");
  canvas.setCursor(24, 66);
  canvas.print("and Cardputer-ADV hardware.");

  canvas.setTextColor(kColorMuted, kColorPanel);
  canvas.setCursor(24, 92);
  canvas.print("BtnA / Enter: retry after reset");

  canvas.pushSprite(0, 0);
}

void draw_header() {
  auto& canvas = g_canvas;
  canvas.setTextColor(kColorText, kColorBg);
  canvas.setCursor(8, 8);
  canvas.print("MIC SPECTRUM");

  const uint16_t mode_color = g_hold ? kColorHold : kColorAccent;
  canvas.fillRoundRect(168, 5, 64, 16, 7, g_hold ? color565(56, 28, 8) : color565(10, 42, 36));
  canvas.drawRoundRect(168, 5, 64, 16, 7, mode_color);
  canvas.setTextColor(mode_color, g_hold ? color565(56, 28, 8) : color565(10, 42, 36));
  canvas.setCursor(186, 10);
  canvas.print(g_hold ? "HOLD" : "LIVE");

  canvas.setTextColor(kColorMuted, kColorBg);
  canvas.setCursor(8, 20);
  canvas.printf("%lu Hz  %u rec", static_cast<unsigned long>(app_config::SAMPLE_RATE),
                static_cast<unsigned int>(app_config::RECORD_COUNT));
  canvas.setCursor(128, 20);
  canvas.printf("%4.0fHz %4.0f", g_dominant_frequency_hz, g_last_peak_abs);
}

void draw_spectrum() {
  auto& canvas = g_canvas;
  canvas.fillRoundRect(app_config::SPECTRUM_X, app_config::SPECTRUM_Y, app_config::SPECTRUM_W, app_config::SPECTRUM_H, 8,
                       kColorPanel);
  canvas.drawRoundRect(app_config::SPECTRUM_X, app_config::SPECTRUM_Y, app_config::SPECTRUM_W, app_config::SPECTRUM_H, 8,
                       kColorGrid);

  for (int i = 1; i <= 3; ++i) {
    const int y = app_config::SPECTRUM_Y + (app_config::SPECTRUM_H - 10) * i / 4;
    canvas.drawFastHLine(app_config::SPECTRUM_X + 5, y, app_config::SPECTRUM_W - 10, kColorGrid);
  }

  const int inner_x = app_config::SPECTRUM_X + 6;
  const int inner_y = app_config::SPECTRUM_Y + 6;
  const int inner_w = app_config::SPECTRUM_W - 12;
  const int inner_h = app_config::SPECTRUM_H - 12;
  const int bar_w = inner_w / app_config::BAR_COUNT;

  for (int i = 0; i < app_config::BAR_COUNT; ++i) {
    const int x = inner_x + i * bar_w;
    const int width = bar_w - 1;
    const int height = static_cast<int>(g_bar_levels[i] * static_cast<float>(inner_h));
    const int peak_y = inner_y + inner_h - static_cast<int>(g_peak_levels[i] * static_cast<float>(inner_h));

    if (height > 0) {
      const int y = inner_y + inner_h - height;
      const uint16_t fill = g_hold ? kColorHold : kColorAccent;
      canvas.fillRect(x, y, width, height, fill);
      canvas.drawFastVLine(x, y, height, g_hold ? color565(255, 214, 128) : kColorAccentDim);
    }

    canvas.drawFastHLine(x, peak_y, width, kColorPeak);
  }

  canvas.setTextColor(kColorMuted, kColorPanel);
  canvas.setCursor(app_config::SPECTRUM_X + 8, app_config::SPECTRUM_Y + app_config::SPECTRUM_H - 10);
  canvas.print("80Hz");
  canvas.setCursor(app_config::SPECTRUM_X + app_config::SPECTRUM_W - 38, app_config::SPECTRUM_Y + app_config::SPECTRUM_H - 10);
  canvas.print("3.6k");
}

void draw_waveform() {
  auto& canvas = g_canvas;
  canvas.fillRoundRect(app_config::WAVEFORM_X, app_config::WAVEFORM_Y, app_config::WAVEFORM_W, app_config::WAVEFORM_H, 6,
                       kColorPanel);
  canvas.drawRoundRect(app_config::WAVEFORM_X, app_config::WAVEFORM_Y, app_config::WAVEFORM_W, app_config::WAVEFORM_H, 6,
                       kColorGrid);

  const int inner_x = app_config::WAVEFORM_X + 4;
  const int inner_y = app_config::WAVEFORM_Y + 3;
  const int inner_w = app_config::WAVEFORM_W - 8;
  const int inner_h = app_config::WAVEFORM_H - 6;
  const int center_y = inner_y + inner_h / 2;
  canvas.drawFastHLine(inner_x, center_y, inner_w, kColorGrid);

  float scale = g_waveform_scale;
  if (scale < 256.0f) {
    scale = 256.0f;
  }

  int prev_x = inner_x;
  int prev_y = center_y;
  const int sample_count = static_cast<int>(app_config::RECORD_COUNT);
  for (int x = 0; x < inner_w; ++x) {
    const int sample_index = x * sample_count / inner_w;
    const float normalized = clamp_value(static_cast<float>(g_mic_samples[sample_index]) / scale, -1.0f, 1.0f);
    const int y = center_y - static_cast<int>(normalized * static_cast<float>(inner_h / 2 - 1));
    const int draw_x = inner_x + x;
    canvas.drawLine(prev_x, prev_y, draw_x, y, kColorAccent);
    prev_x = draw_x;
    prev_y = y;
  }
}

void draw_footer() {
  auto& canvas = g_canvas;
  const int meter_x = 158;
  const int meter_y = 98;
  const int meter_w = 72;
  const int meter_h = 10;
  const int fill_w = static_cast<int>(clamp_value(g_input_level, 0.0f, 1.0f) * static_cast<float>(meter_w - 2));

  canvas.setTextColor(kColorMuted, kColorBg);
  canvas.setCursor(meter_x, meter_y - 10);
  canvas.print("LEVEL");
  canvas.drawRoundRect(meter_x, meter_y, meter_w, meter_h, 5, kColorGrid);
  canvas.fillRoundRect(meter_x + 1, meter_y + 1, meter_w - 2, meter_h - 2, 4, color565(16, 24, 34));
  if (fill_w > 0) {
    canvas.fillRoundRect(meter_x + 1, meter_y + 1, fill_w, meter_h - 2, 4, kColorLevel);
  }

  canvas.setTextColor(kColorText, kColorBg);
  canvas.setCursor(158, 112);
  canvas.print("BtnA/Enter");
  canvas.setTextColor(kColorMuted, kColorBg);
  canvas.setCursor(158, 122);
  canvas.print("toggle hold");
  canvas.setCursor(158, 132);
  canvas.print("BS reset peak");

  canvas.setTextColor(g_hold ? kColorHold : kColorMuted, kColorBg);
  canvas.setCursor(8, 116);
  canvas.print(g_status_message);
}

void draw_ui() {
  if (!g_mic_ready) {
    draw_error_screen();
    return;
  }

  auto& canvas = g_canvas;
  canvas.fillSprite(kColorBg);
  draw_header();
  draw_spectrum();
  draw_waveform();
  draw_footer();
  canvas.pushSprite(0, 0);
}

void ensure_mic_ready() {
  if (g_mic_ready) {
    return;
  }

  const uint32_t now = millis();
  if (now - g_last_mic_retry_ms < app_config::MIC_RETRY_INTERVAL_MS) {
    return;
  }
  g_last_mic_retry_ms = now;

  if (M5Cardputer.Speaker.isEnabled()) {
    M5Cardputer.Speaker.end();
  }
  g_mic_ready = M5Cardputer.Mic.begin() && M5Cardputer.Mic.isEnabled();
  delay(200);
  g_status_message = g_mic_ready ? "REC" : "RETRYING MIC";
  g_needs_redraw = true;
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  cfg.output_power = true;
  M5Cardputer.begin(cfg, true);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);

  g_canvas.setColorDepth(16);
  g_canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);

  build_window();
  build_bar_mapping();

  if (M5Cardputer.Speaker.isEnabled()) {
    M5Cardputer.Speaker.end();
  }
  g_mic_ready = M5Cardputer.Mic.begin() && M5Cardputer.Mic.isEnabled();
  delay(200);

  g_status_message = g_mic_ready ? "REC" : "MIC INIT FAILED";
  draw_ui();
  g_needs_redraw = false;
}

void loop() {
  M5Cardputer.update();
  ensure_mic_ready();
  update_controls();

  const uint32_t now = millis();
  if (g_mic_ready && !g_hold && now - g_last_frame_ms >= app_config::FRAME_INTERVAL_MS) {
    g_last_frame_ms = now;
    if (capture_audio()) {
      analyze_audio();
      g_needs_redraw = true;
    }
  }

  if (g_needs_redraw) {
    draw_ui();
    g_needs_redraw = false;
  }
}
