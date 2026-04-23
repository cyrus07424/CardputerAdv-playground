#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>

#include <cctype>
#include <vector>

namespace app_config {
inline int display_width() { return M5Cardputer.Display.width(); }
inline int display_height() { return M5Cardputer.Display.height(); }
#define SCREEN_W display_width()
#define SCREEN_H display_height()
constexpr int SD_SPI_SCK_PIN = 40;
constexpr int SD_SPI_MISO_PIN = 39;
constexpr int SD_SPI_MOSI_PIN = 14;
constexpr int SD_SPI_CS_PIN = 12;
constexpr const char* DATA_DIR = "/roulette";
constexpr const char* SESSIONS_FILE = "/roulette/sessions.csv";
constexpr const char* SPINS_FILE = "/roulette/spins.csv";
constexpr int HISTORY_ROWS = 2;
constexpr uint32_t STORAGE_RETRY_MS = 3000;
constexpr uint32_t INITIAL_STORAGE_DELAY_MS = 800;
constexpr uint8_t HID_DOWN = 0x51;
constexpr uint8_t HID_UP = 0x52;
}  // namespace app_config

enum class WheelType : uint8_t {
  American,
  European,
};

enum class ScreenMode : uint8_t {
  SelectWheel,
  RecordSpins,
};

enum class RecordPanel : uint8_t {
  Heatmap,
  History,
};

struct SpinRecord {
  int index = 0;
  int value = 0;
  String text;
  String color;
};

struct PredictionItem {
  int value = 0;
  String text;
  int count = 0;
};

struct SessionMeta {
  int id = 0;
  WheelType wheel = WheelType::American;
};

struct StoredSpin {
  int session_id = 0;
  WheelType wheel = WheelType::American;
  SpinRecord spin;
};

static M5Canvas g_canvas(&M5Cardputer.Display);
static ScreenMode g_screen_mode = ScreenMode::SelectWheel;
static WheelType g_selected_wheel = WheelType::American;
static int g_current_session_id = -1;
static std::vector<SpinRecord> g_session_spins;
static std::vector<PredictionItem> g_predictions;
static std::vector<SessionMeta> g_all_sessions;
static std::vector<StoredSpin> g_all_spins;
static String g_prediction_basis = "データ不足";
static String g_input_buffer;
static String g_status_message = "初期化中";
static String g_prev_pressed_chars;
static bool g_prev_backspace = false;
static bool g_prev_enter = false;
static bool g_prev_tab = false;
static bool g_needs_redraw = true;
static bool g_sd_ready = false;
static uint32_t g_last_storage_try_ms = 0;
static int g_history_scroll_offset = 0;
static int g_red_count = 0;
static int g_black_count = 0;
static int g_green_count = 0;
static int g_odd_count = 0;
static int g_even_count = 0;
static int g_dozen_frequency[3] = {};
static int g_column_frequency[3] = {};
static int g_even_money_frequency[6] = {};
static int g_spin_frequency[38] = {};
static int g_max_spin_frequency = 0;
static int g_max_group_frequency = 0;
static RecordPanel g_record_panel = RecordPanel::Heatmap;

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t COLOR_BG = rgb565(6, 12, 18);
constexpr uint16_t COLOR_PANEL = rgb565(12, 24, 34);
constexpr uint16_t COLOR_PANEL_ALT = rgb565(10, 18, 26);
constexpr uint16_t COLOR_BORDER = rgb565(40, 92, 108);
constexpr uint16_t COLOR_ACCENT = rgb565(32, 228, 194);
constexpr uint16_t COLOR_TEXT_DIM = rgb565(138, 166, 180);
constexpr uint16_t COLOR_GOLD = rgb565(255, 196, 64);
constexpr uint16_t COLOR_RED_DARK = rgb565(76, 10, 18);
constexpr uint16_t COLOR_RED_BRIGHT = rgb565(232, 42, 68);
constexpr uint16_t COLOR_BLACK_DARK = rgb565(22, 30, 42);
constexpr uint16_t COLOR_BLACK_BRIGHT = rgb565(122, 144, 164);
constexpr uint16_t COLOR_GREEN_DARK = rgb565(8, 60, 36);
constexpr uint16_t COLOR_GREEN_BRIGHT = rgb565(22, 220, 120);

const int kEuropeanWheelOrder[] = {0, 32, 15, 19, 4, 21, 2, 25, 17, 34, 6, 27, 13, 36, 11, 30, 8, 23, 10,
                                   5, 24, 16, 33, 1, 20, 14, 31, 9, 22, 18, 29, 7, 28, 12, 35, 3, 26};
const int kAmericanWheelOrder[] = {0, 28, 9, 26, 30, 11, 7, 20, 32, 17, 5, 22, 34, 15, 3, 24, 36, 13, 1,
                                   37, 27, 10, 25, 29, 12, 8, 19, 31, 18, 6, 21, 33, 16, 4, 23, 35, 14, 2};

void set_status(const String& message) {
  g_status_message = message;
  g_needs_redraw = true;
}

const char* wheel_name(WheelType wheel) {
  return wheel == WheelType::American ? "American" : "European";
}

bool is_special_key(char key) {
  return key == KEY_FN || key == KEY_LEFT_SHIFT || key == KEY_LEFT_CTRL || key == KEY_LEFT_ALT ||
         key == KEY_OPT || key == KEY_ENTER || key == KEY_BACKSPACE || key == KEY_TAB;
}

String collect_pressed_chars(const Keyboard_Class::KeysState& status) {
  String result;
  const bool shifted = status.fn || status.shift;

  for (const auto& key_pos : M5Cardputer.Keyboard.keyList()) {
    const auto key_value = M5Cardputer.Keyboard.getKeyValue(key_pos);
    const char key = shifted ? key_value.value_second : key_value.value_first;
    if (is_special_key(key)) {
      continue;
    }
    result += key;
  }
  return result;
}

bool contains_hid_key(const Keyboard_Class::KeysState& status, uint8_t hid_key) {
  for (const auto raw_hid_key : status.hid_keys) {
    if (raw_hid_key == hid_key) {
      return true;
    }
  }
  return false;
}

bool is_red_number(int value) {
  switch (value) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 9:
    case 12:
    case 14:
    case 16:
    case 18:
    case 19:
    case 21:
    case 23:
    case 25:
    case 27:
    case 30:
    case 32:
    case 34:
    case 36:
      return true;
    default:
      return false;
  }
}

String roulette_color_text(int value) {
  if (value == 0 || value == 37) {
    return "green";
  }
  return is_red_number(value) ? "red" : "black";
}

uint16_t roulette_color_ui(const String& color) {
  if (color == "red") return TFT_RED;
  if (color == "black") return TFT_WHITE;
  return TFT_GREEN;
}

uint16_t roulette_color_fill(int value, float intensity) {
  const bool is_green = value == 0 || value == 37;
  const bool is_red = !is_green && is_red_number(value);
  if (intensity < 0.01f) {
    return is_green ? COLOR_GREEN_DARK : (is_red ? COLOR_RED_DARK : COLOR_BLACK_DARK);
  }
  if (is_green) {
    return intensity > 0.80f ? COLOR_GREEN_BRIGHT : intensity > 0.45f ? rgb565(18, 164, 92) : rgb565(12, 104, 58);
  }
  if (is_red) {
    return intensity > 0.80f ? COLOR_RED_BRIGHT : intensity > 0.45f ? rgb565(180, 28, 52) : rgb565(114, 18, 34);
  }
  return intensity > 0.80f ? rgb565(160, 182, 210) : intensity > 0.45f ? rgb565(94, 112, 136) : rgb565(46, 58, 74);
}

uint16_t accent_heat_fill(uint16_t low, uint16_t mid, uint16_t high, float intensity) {
  if (intensity > 0.80f) return high;
  if (intensity > 0.45f) return mid;
  if (intensity > 0.01f) return low;
  return COLOR_PANEL_ALT;
}

String number_to_text(int value) {
  if (value == 37) return "00";
  return String(value);
}

int spin_value_to_frequency_index(int value) {
  return value >= 0 && value <= 37 ? value : 0;
}

bool parse_spin_token(const String& token, WheelType wheel, int& out_value, String& out_text) {
  if (token.length() == 0) {
    return false;
  }

  if (wheel == WheelType::American && token == "00") {
    out_value = 37;
    out_text = "00";
    return true;
  }

  for (size_t i = 0; i < token.length(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(token[i]))) {
      return false;
    }
  }

  const int number = token.toInt();
  if (number < 0 || number > 36) {
    return false;
  }

  out_value = number;
  out_text = String(number);
  return true;
}

String read_csv_field(const String& line, int field_index) {
  int start = 0;
  int current_field = 0;
  while (start <= line.length()) {
    int end = line.indexOf(',', start);
    if (end < 0) {
      end = line.length();
    }
    if (current_field == field_index) {
      return line.substring(start, end);
    }
    start = end + 1;
    ++current_field;
  }
  return "";
}

bool append_line(const char* path, const String& line) {
  if (!g_sd_ready) {
    return true;
  }
  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    return false;
  }
  file.println(line);
  file.close();
  return true;
}

bool ensure_storage_files() {
  if (!SD.exists(app_config::DATA_DIR) && !SD.mkdir(app_config::DATA_DIR)) {
    set_status("保存ディレクトリ作成失敗");
    return false;
  }

  if (!SD.exists(app_config::SESSIONS_FILE)) {
    File file = SD.open(app_config::SESSIONS_FILE, FILE_WRITE);
    if (!file) {
      set_status("sessions.csv作成失敗");
      return false;
    }
    file.close();
  }

  if (!SD.exists(app_config::SPINS_FILE)) {
    File file = SD.open(app_config::SPINS_FILE, FILE_WRITE);
    if (!file) {
      set_status("spins.csv作成失敗");
      return false;
    }
    file.close();
  }

  return true;
}

void clear_loaded_history() {
  g_all_sessions.clear();
  g_all_spins.clear();
}

void load_history_from_csv() {
  clear_loaded_history();
  if (!g_sd_ready) {
    return;
  }

  File sessions_file = SD.open(app_config::SESSIONS_FILE, FILE_READ);
  if (sessions_file) {
    while (sessions_file.available()) {
      String line = sessions_file.readStringUntil('\n');
      line.replace("\r", "");
      if (line.length() == 0) {
        continue;
      }
      SessionMeta session;
      session.id = read_csv_field(line, 0).toInt();
      const String wheel = read_csv_field(line, 1);
      session.wheel = wheel == "European" ? WheelType::European : WheelType::American;
      g_all_sessions.push_back(session);
    }
    sessions_file.close();
  }

  File spins_file = SD.open(app_config::SPINS_FILE, FILE_READ);
  if (spins_file) {
    while (spins_file.available()) {
      String line = spins_file.readStringUntil('\n');
      line.replace("\r", "");
      if (line.length() == 0) {
        continue;
      }

      StoredSpin stored;
      stored.session_id = read_csv_field(line, 0).toInt();
      stored.spin.index = read_csv_field(line, 1).toInt();
      stored.spin.value = read_csv_field(line, 2).toInt();
      stored.spin.text = read_csv_field(line, 3);
      stored.spin.color = read_csv_field(line, 4);

      WheelType wheel = WheelType::American;
      for (const auto& session : g_all_sessions) {
        if (session.id == stored.session_id) {
          wheel = session.wheel;
          break;
        }
      }
      stored.wheel = wheel;
      g_all_spins.push_back(stored);
    }
    spins_file.close();
  }
}

void try_init_storage() {
  static constexpr uint32_t kSdSpeeds[] = {25000000, 10000000, 4000000, 1000000};

  g_last_storage_try_ms = millis();
  SPI.begin(app_config::SD_SPI_SCK_PIN, app_config::SD_SPI_MISO_PIN, app_config::SD_SPI_MOSI_PIN, app_config::SD_SPI_CS_PIN);

  g_sd_ready = false;
  for (const auto speed : kSdSpeeds) {
    SD.end();
    if (SD.begin(app_config::SD_SPI_CS_PIN, SPI, speed)) {
      g_sd_ready = true;
      break;
    }
  }

  if (!g_sd_ready || SD.cardType() == CARD_NONE) {
    g_sd_ready = false;
    set_status("SDなし: RAMのみで継続");
    return;
  }

  if (!ensure_storage_files()) {
    return;
  }

  load_history_from_csv();
  set_status("CSV保存モード");
}

void ensure_storage_ready() {
  if (g_sd_ready) {
    return;
  }

  const uint32_t now = millis();
  const uint32_t retry_ms =
      g_last_storage_try_ms == 0 ? app_config::INITIAL_STORAGE_DELAY_MS : app_config::STORAGE_RETRY_MS;
  if (now - g_last_storage_try_ms < retry_ms) {
    return;
  }

  try_init_storage();
}

int next_session_id() {
  int max_id = 0;
  for (const auto& session : g_all_sessions) {
    if (session.id > max_id) {
      max_id = session.id;
    }
  }
  return max_id + 1;
}

void count_prediction_item(std::vector<PredictionItem>& items, int value, const String& text) {
  for (auto& item : items) {
    if (item.value == value) {
      item.count++;
      return;
    }
  }
  PredictionItem item;
  item.value = value;
  item.text = text;
  item.count = 1;
  items.push_back(item);
}

void sort_prediction_items(std::vector<PredictionItem>& items) {
  for (size_t i = 0; i < items.size(); ++i) {
    for (size_t j = i + 1; j < items.size(); ++j) {
      if (items[j].count > items[i].count || (items[j].count == items[i].count && items[j].value < items[i].value)) {
        PredictionItem temp = items[i];
        items[i] = items[j];
        items[j] = temp;
      }
    }
  }
  if (items.size() > 3) {
    items.resize(3);
  }
}

void refresh_predictions() {
  g_predictions.clear();
  g_prediction_basis = "データ不足";
  g_red_count = 0;
  g_black_count = 0;
  g_green_count = 0;
  g_odd_count = 0;
  g_even_count = 0;
  g_max_spin_frequency = 0;
  g_max_group_frequency = 0;
  for (int& count : g_dozen_frequency) {
    count = 0;
  }
  for (int& count : g_column_frequency) {
    count = 0;
  }
  for (int& count : g_even_money_frequency) {
    count = 0;
  }
  for (int& count : g_spin_frequency) {
    count = 0;
  }

  std::vector<PredictionItem> transition_items;
  std::vector<PredictionItem> session_items;
  std::vector<PredictionItem> overall_items;

  for (size_t i = 0; i < g_all_spins.size(); ++i) {
    const auto& stored = g_all_spins[i];
    if (stored.wheel != g_selected_wheel) {
      continue;
    }

    if (stored.spin.color == "red") {
      ++g_red_count;
    } else if (stored.spin.color == "black") {
      ++g_black_count;
    } else {
      ++g_green_count;
    }
    const int frequency_index = spin_value_to_frequency_index(stored.spin.value);
    ++g_spin_frequency[frequency_index];
    if (g_spin_frequency[frequency_index] > g_max_spin_frequency) {
      g_max_spin_frequency = g_spin_frequency[frequency_index];
    }
    if (stored.spin.value != 0 && stored.spin.value != 37) {
      const int dozen_index = (stored.spin.value - 1) / 12;
      ++g_dozen_frequency[dozen_index];
      if (g_dozen_frequency[dozen_index] > g_max_group_frequency) {
        g_max_group_frequency = g_dozen_frequency[dozen_index];
      }

      const int column_index = (stored.spin.value - 1) % 3;
      ++g_column_frequency[column_index];
      if (g_column_frequency[column_index] > g_max_group_frequency) {
        g_max_group_frequency = g_column_frequency[column_index];
      }

      if (stored.spin.value <= 18) {
        ++g_even_money_frequency[0];
      } else {
        ++g_even_money_frequency[5];
      }
      ++g_even_money_frequency[is_red_number(stored.spin.value) ? 2 : 3];
      if ((stored.spin.value & 1) == 0) {
        ++g_even_count;
        ++g_even_money_frequency[1];
      } else {
        ++g_odd_count;
        ++g_even_money_frequency[4];
      }

      for (const int count : g_even_money_frequency) {
        if (count > g_max_group_frequency) {
          g_max_group_frequency = count;
        }
      }
    }

    if (stored.session_id == g_current_session_id) {
      count_prediction_item(session_items, stored.spin.value, stored.spin.text);
    }
    count_prediction_item(overall_items, stored.spin.value, stored.spin.text);

    if (!g_session_spins.empty() && stored.spin.value == g_session_spins.back().value && i + 1 < g_all_spins.size()) {
      const auto& next = g_all_spins[i + 1];
      if (next.session_id == stored.session_id && next.wheel == g_selected_wheel) {
        count_prediction_item(transition_items, next.spin.value, next.spin.text);
      }
    }
  }

  sort_prediction_items(transition_items);
  sort_prediction_items(session_items);
  sort_prediction_items(overall_items);

  if (!transition_items.empty() && !g_session_spins.empty()) {
    g_predictions = transition_items;
    g_prediction_basis = String("直前 ") + number_to_text(g_session_spins.back().value) + " の次";
  } else if (!session_items.empty()) {
    g_predictions = session_items;
    g_prediction_basis = "現セッション頻度";
  } else if (!overall_items.empty()) {
    g_predictions = overall_items;
    g_prediction_basis = "全履歴頻度";
  }
  g_needs_redraw = true;
}

bool start_new_session(WheelType wheel) {
  g_current_session_id = next_session_id();

  SessionMeta session;
  session.id = g_current_session_id;
  session.wheel = wheel;
  g_all_sessions.push_back(session);

  if (!append_line(app_config::SESSIONS_FILE,
                   String(g_current_session_id) + "," + wheel_name(wheel) + "," + String(static_cast<unsigned long>(millis())))) {
    set_status("保存失敗: sessions.csv");
  }

  g_selected_wheel = wheel;
  g_screen_mode = ScreenMode::RecordSpins;
  g_input_buffer = "";
  g_history_scroll_offset = 0;
  g_record_panel = RecordPanel::Heatmap;
  g_session_spins.clear();
  refresh_predictions();
  set_status(String("Session ") + g_current_session_id + (g_sd_ready ? " 開始" : " 開始(RAM)"));
  return true;
}

bool record_spin_token(const String& token) {
  if (g_current_session_id < 0) {
    set_status("セッション未開始");
    return false;
  }

  int value = 0;
  String text;
  if (!parse_spin_token(token, g_selected_wheel, value, text)) {
    set_status("無効な出目");
    return false;
  }

  SpinRecord spin;
  spin.index = static_cast<int>(g_session_spins.size()) + 1;
  spin.value = value;
  spin.text = text;
  spin.color = roulette_color_text(value);

  g_session_spins.push_back(spin);

  StoredSpin stored;
  stored.session_id = g_current_session_id;
  stored.wheel = g_selected_wheel;
  stored.spin = spin;
  g_all_spins.push_back(stored);

  if (!append_line(app_config::SPINS_FILE,
                   String(g_current_session_id) + "," + String(spin.index) + "," + String(spin.value) + "," + spin.text +
                       "," + spin.color + "," + String(static_cast<unsigned long>(millis())))) {
    set_status("保存失敗: spins.csv");
  } else {
    set_status(String("記録: #") + spin.index + " = " + text + (g_sd_ready ? "" : " (RAM)"));
  }

  g_input_buffer = "";
  refresh_predictions();
  return true;
}

String tail_utf8(const String& text, size_t max_chars) {
  if (max_chars == 0 || text.length() == 0) {
    return "";
  }
  int index = text.length();
  size_t count = 0;
  while (index > 0 && count < max_chars) {
    --index;
    while (index > 0 && (static_cast<uint8_t>(text[index]) & 0xC0) == 0x80) {
      --index;
    }
    ++count;
  }
  return text.substring(index);
}

void draw_panel(int x, int y, int w, int h, uint16_t fill, uint16_t border) {
  g_canvas.fillRoundRect(x, y, w, h, 6, fill);
  g_canvas.drawRoundRect(x, y, w, h, 6, border);
}

void draw_badge(int x, int y, int w, int h, const String& title, const String& value, uint16_t accent) {
  draw_panel(x, y, w, h, COLOR_PANEL, accent);
  if (h <= 18) {
    g_canvas.setTextColor(COLOR_TEXT_DIM, COLOR_PANEL);
    g_canvas.drawString(title, x + 4, y + 3);
    g_canvas.setTextColor(TFT_WHITE, COLOR_PANEL);
    g_canvas.drawRightString(value, x + w - 5, y + 3, 1);
  } else {
    g_canvas.setTextColor(COLOR_TEXT_DIM, COLOR_PANEL);
    g_canvas.drawString(title, x + 6, y + 3);
    g_canvas.setTextColor(TFT_WHITE, COLOR_PANEL);
    g_canvas.drawString(value, x + 6, y + 15);
  }
}

void draw_select_screen() {
  g_canvas.fillRect(0, 0, app_config::SCREEN_W, 32, rgb565(8, 28, 24));
  g_canvas.fillRect(0, 32, app_config::SCREEN_W, 3, COLOR_ACCENT);
  g_canvas.setTextColor(COLOR_GOLD, rgb565(8, 28, 24));
  g_canvas.drawString("CASINO RECORDER", 8, 6);
  g_canvas.setTextColor(TFT_WHITE, rgb565(8, 28, 24));
  g_canvas.drawString("Tab: wheel  Enter: start", 8, 18);

  const bool american_selected = g_selected_wheel == WheelType::American;
  draw_panel(10, 46, 220, 28, american_selected ? rgb565(78, 58, 10) : COLOR_PANEL, american_selected ? COLOR_GOLD : COLOR_BORDER);
  g_canvas.setTextColor(american_selected ? COLOR_GOLD : TFT_WHITE, american_selected ? rgb565(78, 58, 10) : COLOR_PANEL);
  g_canvas.drawString("American", 18, 50);
  g_canvas.setTextColor(COLOR_TEXT_DIM, american_selected ? rgb565(78, 58, 10) : COLOR_PANEL);
  g_canvas.drawString("0 / 00 / 1-36", 112, 50);

  const bool european_selected = g_selected_wheel == WheelType::European;
  draw_panel(10, 82, 220, 28, european_selected ? rgb565(8, 72, 46) : COLOR_PANEL, european_selected ? COLOR_ACCENT : COLOR_BORDER);
  g_canvas.setTextColor(european_selected ? COLOR_ACCENT : TFT_WHITE, european_selected ? rgb565(8, 72, 46) : COLOR_PANEL);
  g_canvas.drawString("European", 18, 86);
  g_canvas.setTextColor(COLOR_TEXT_DIM, european_selected ? rgb565(8, 72, 46) : COLOR_PANEL);
  g_canvas.drawString("0 / 1-36", 128, 86);

  g_canvas.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
  g_canvas.drawString("Stylish heatmap + history analyzer", 12, 118);
}

void draw_prediction_row(int y) {
  draw_panel(8, y, 224, 26, COLOR_PANEL_ALT, COLOR_BORDER);
  g_canvas.setTextColor(COLOR_ACCENT, COLOR_PANEL_ALT);
  g_canvas.drawString(tail_utf8(String("PRED ") + g_prediction_basis, 20), 12, y + 4);

  int x = 116;
  for (size_t i = 0; i < g_predictions.size(); ++i) {
    const auto& item = g_predictions[i];
    const String label = item.text + ":" + item.count;
    const uint16_t bg = roulette_color_fill(item.value, 1.0f);
    draw_panel(x, y + 3, 34, 18, bg, COLOR_BORDER);
    g_canvas.setTextColor(item.value == 0 || item.value == 37 ? TFT_BLACK : TFT_WHITE, bg);
    g_canvas.drawCentreString(label, x + 17, y + 7, 1);
    x += 38;
  }
}

String prediction_summary() {
  String text;
  for (size_t i = 0; i < g_predictions.size(); ++i) {
    if (i > 0) {
      text += ' ';
    }
    text += g_predictions[i].text;
  }
  return text.length() > 0 ? text : "-";
}

void draw_heat_cell(int x, int y, int w, int h, const String& label, uint16_t fill, int count, bool dark_text = false) {
  draw_panel(x, y, w, h, fill, count > 0 ? COLOR_BORDER : rgb565(24, 40, 52));
  g_canvas.setTextColor(dark_text ? TFT_BLACK : TFT_WHITE, fill);
  g_canvas.drawCentreString(label, x + w / 2, y + (h >= 16 ? 3 : 1), 1);
  if (h >= 16) {
    g_canvas.setTextColor(dark_text ? TFT_BLACK : COLOR_TEXT_DIM, fill);
    g_canvas.drawCentreString(String(count), x + w / 2, y + h - 11, 1);
  }
}

void draw_number_table_heatmap(int origin_x, int origin_y) {
  constexpr int zero_w = 16;
  constexpr int cell_w = 15;
  constexpr int cell_h = 14;
  constexpr int right_w = 16;

  if (g_selected_wheel == WheelType::American) {
    const int zero_h = cell_h * 3 / 2;
    const int double_zero_h = cell_h * 3 - zero_h;
    const float zero_intensity = g_max_spin_frequency > 0 ? static_cast<float>(g_spin_frequency[0]) / g_max_spin_frequency : 0.0f;
    const float double_zero_intensity = g_max_spin_frequency > 0 ? static_cast<float>(g_spin_frequency[37]) / g_max_spin_frequency : 0.0f;
    draw_heat_cell(origin_x, origin_y, zero_w, zero_h, "0", roulette_color_fill(0, zero_intensity), g_spin_frequency[0]);
    draw_heat_cell(origin_x, origin_y + zero_h, zero_w, double_zero_h, "00", roulette_color_fill(37, double_zero_intensity),
                   g_spin_frequency[37]);
  } else {
    const float zero_intensity = g_max_spin_frequency > 0 ? static_cast<float>(g_spin_frequency[0]) / g_max_spin_frequency : 0.0f;
    draw_heat_cell(origin_x, origin_y + cell_h / 2, zero_w, cell_h * 2, "0", roulette_color_fill(0, zero_intensity), g_spin_frequency[0]);
  }

  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 12; ++col) {
      const int value = col * 3 + (3 - row);
      const int count = g_spin_frequency[spin_value_to_frequency_index(value)];
      const float intensity = g_max_spin_frequency > 0 ? static_cast<float>(count) / g_max_spin_frequency : 0.0f;
      const int x = origin_x + zero_w + 2 + col * cell_w;
      const int y = origin_y + row * cell_h;
      const uint16_t fill = roulette_color_fill(value, intensity);
      draw_heat_cell(x, y, cell_w, cell_h, number_to_text(value), fill, count, intensity > 0.45f && !is_red_number(value));
    }
  }

  for (int row = 0; row < 3; ++row) {
    const int count = g_column_frequency[2 - row];
    const float intensity = g_max_group_frequency > 0 ? static_cast<float>(count) / g_max_group_frequency : 0.0f;
    const uint16_t fill = accent_heat_fill(rgb565(36, 76, 82), rgb565(48, 122, 136), rgb565(76, 186, 198), intensity);
    draw_heat_cell(origin_x + zero_w + 2 + cell_w * 12 + 2, origin_y + row * cell_h, right_w, cell_h, "2:1", fill, count,
                   intensity > 0.45f);
  }

  for (int dozen = 0; dozen < 3; ++dozen) {
    const int count = g_dozen_frequency[dozen];
    const float intensity = g_max_group_frequency > 0 ? static_cast<float>(count) / g_max_group_frequency : 0.0f;
    const uint16_t fill = accent_heat_fill(rgb565(64, 46, 12), rgb565(126, 88, 18), rgb565(214, 154, 26), intensity);
    draw_heat_cell(origin_x + zero_w + 2 + dozen * cell_w * 4, origin_y + cell_h * 3 + 2, cell_w * 4, 12,
                   dozen == 0 ? "1st12" : dozen == 1 ? "2nd12" : "3rd12", fill, count, intensity > 0.45f);
  }

  static const char* even_money_labels[6] = {"1-18", "EVEN", "RED", "BLACK", "ODD", "19-36"};
  for (int i = 0; i < 6; ++i) {
    const int count = g_even_money_frequency[i];
    const float intensity = g_max_group_frequency > 0 ? static_cast<float>(count) / g_max_group_frequency : 0.0f;
    uint16_t fill = accent_heat_fill(rgb565(28, 54, 68), rgb565(40, 96, 122), rgb565(64, 154, 194), intensity);
    bool dark_text = intensity > 0.55f;
    if (i == 2) {
      fill = accent_heat_fill(COLOR_RED_DARK, rgb565(180, 28, 52), COLOR_RED_BRIGHT, intensity);
      dark_text = false;
    } else if (i == 3) {
      fill = accent_heat_fill(COLOR_BLACK_DARK, rgb565(94, 112, 136), COLOR_BLACK_BRIGHT, intensity);
      dark_text = intensity > 0.45f;
    }
    draw_heat_cell(origin_x + zero_w + 2 + i * cell_w * 2, origin_y + cell_h * 3 + 16, cell_w * 2, 12, even_money_labels[i], fill,
                   count, dark_text);
  }
}

void draw_history_rows(int start_y) {
  std::vector<const StoredSpin*> visible_history;
  for (const auto& stored : g_all_spins) {
    if (stored.wheel == g_selected_wheel) {
      visible_history.push_back(&stored);
    }
  }

  const int history_count = static_cast<int>(visible_history.size());
  int start_index = history_count - app_config::HISTORY_ROWS - g_history_scroll_offset;
  if (start_index < 0) {
    start_index = 0;
  }

  for (int row = 0; row < app_config::HISTORY_ROWS; ++row) {
    const int spin_index = start_index + row;
    if (spin_index >= history_count) {
      break;
    }

    const auto& entry = *visible_history[spin_index];
    const auto& spin = entry.spin;
    const int y = start_y + row * 16;
    draw_panel(8, y, 224, 14, COLOR_PANEL_ALT, COLOR_BORDER);
    g_canvas.setTextColor(COLOR_TEXT_DIM, COLOR_PANEL_ALT);
    g_canvas.drawString(String("S") + entry.session_id + "#" + spin.index, 14, y + 3);
    const uint16_t fill = roulette_color_fill(spin.value, 1.0f);
    draw_panel(176, y + 1, 46, 12, fill, COLOR_BORDER);
    g_canvas.setTextColor((spin.value == 0 || spin.value == 37) ? TFT_BLACK : TFT_WHITE, fill);
    g_canvas.drawCentreString(spin.text, 199, y + 3, 1);
  }
}

void draw_record_screen() {
  if (g_record_panel == RecordPanel::Heatmap) {
    g_canvas.fillRect(0, 0, app_config::SCREEN_W, 22, rgb565(8, 28, 24));
    g_canvas.fillRect(0, 22, app_config::SCREEN_W, 2, COLOR_ACCENT);
    g_canvas.setTextColor(COLOR_GOLD, rgb565(8, 28, 24));
    g_canvas.drawString(String(wheel_name(g_selected_wheel)) + " HEATMAP", 6, 5);
    g_canvas.setTextColor(TFT_WHITE, rgb565(8, 28, 24));
    g_canvas.drawRightString(String("S") + g_current_session_id + " H:Hist", 234, 5, 1);
    g_canvas.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
    g_canvas.drawString(tail_utf8(String("Pred ") + g_prediction_basis + " " + prediction_summary(), 31), 6, 27);

    draw_number_table_heatmap(2, 40);

    draw_badge(8, 118, 78, 14, "INPUT", g_input_buffer.length() > 0 ? g_input_buffer + "_" : "_", COLOR_BORDER);
    draw_badge(90, 118, 62, 14, "LAST", g_session_spins.empty() ? "-" : g_session_spins.back().text, COLOR_BORDER);
    draw_badge(156, 118, 76, 14, "MODE", g_sd_ready ? "CSV H:Hist" : "RAM H:Hist", COLOR_BORDER);
    return;
  }

  g_canvas.fillRect(0, 0, app_config::SCREEN_W, 24, rgb565(8, 28, 24));
  g_canvas.fillRect(0, 24, app_config::SCREEN_W, 2, COLOR_ACCENT);
  g_canvas.setTextColor(COLOR_GOLD, rgb565(8, 28, 24));
  g_canvas.drawString("ROULETTE HUD", 8, 5);
  g_canvas.setTextColor(TFT_WHITE, rgb565(8, 28, 24));
  g_canvas.drawString(String(wheel_name(g_selected_wheel)) + " S" + g_current_session_id, 112, 5);

  draw_badge(8, 30, 52, 26, "INPUT", g_input_buffer.length() > 0 ? g_input_buffer + "_" : "_", COLOR_ACCENT);
  draw_badge(66, 30, 48, 26, "LAST", g_session_spins.empty() ? "-" : g_session_spins.back().text, COLOR_GOLD);
  draw_badge(120, 30, 52, 26, "SPINS", String(g_session_spins.size()), COLOR_BORDER);
  draw_badge(178, 30, 54, 26, "MODE", g_sd_ready ? "CSV" : "RAM", g_sd_ready ? COLOR_ACCENT : COLOR_GOLD);

  draw_prediction_row(60);

  draw_badge(8, 88, 104, 16, "HISTORY", String("offset ") + g_history_scroll_offset, COLOR_BORDER);
  draw_badge(118, 88, 114, 16, "PANEL", "HISTORY ;/. UP/DOWN", COLOR_BORDER);
  draw_history_rows(106);

  g_canvas.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
  g_canvas.drawRightString(tail_utf8(g_status_message, 24), 236, 0, 1);
}

void draw_ui() {
  g_canvas.fillScreen(COLOR_BG);
  g_canvas.setFont(&fonts::efontJA_12);
  g_canvas.setTextSize(1);
  g_canvas.setTextWrap(false);

  if (g_screen_mode == ScreenMode::SelectWheel) {
    draw_select_screen();
  } else {
    draw_record_screen();
  }

  g_canvas.pushSprite(0, 0);
}

void handle_select_mode_keys(const Keyboard_Class::KeysState& status, const String& pressed_chars) {
  for (size_t i = 0; i < pressed_chars.length(); ++i) {
    const char c = pressed_chars[i];
    if (g_prev_pressed_chars.indexOf(c) >= 0) {
      continue;
    }

    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower == 'a') {
      g_selected_wheel = WheelType::American;
      set_status("American を選択");
    } else if (lower == 'e') {
      g_selected_wheel = WheelType::European;
      set_status("European を選択");
    }
  }

  if (status.tab && !g_prev_tab) {
    g_selected_wheel = g_selected_wheel == WheelType::American ? WheelType::European : WheelType::American;
    set_status(String("Wheel: ") + wheel_name(g_selected_wheel));
  }

  if (status.enter && !g_prev_enter) {
    start_new_session(g_selected_wheel);
  }
}

void handle_record_mode_keys(const Keyboard_Class::KeysState& status, const String& pressed_chars) {
  for (size_t i = 0; i < pressed_chars.length(); ++i) {
    const char c = pressed_chars[i];
    if (g_prev_pressed_chars.indexOf(c) >= 0) {
      continue;
    }

    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (status.ctrl && lower == 'n') {
      start_new_session(g_selected_wheel);
      continue;
    }

    if (lower == 'h') {
      g_record_panel = g_record_panel == RecordPanel::Heatmap ? RecordPanel::History : RecordPanel::Heatmap;
      g_needs_redraw = true;
    } else if (c == ';') {
      ++g_history_scroll_offset;
      g_needs_redraw = true;
    } else if (c == '.' && g_history_scroll_offset > 0) {
      --g_history_scroll_offset;
      g_needs_redraw = true;
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
      if (g_input_buffer.length() < 2) {
        g_input_buffer += c;
        g_needs_redraw = true;
      }
    } else if (lower == 'z' && g_selected_wheel == WheelType::American) {
      g_input_buffer = "00";
      g_needs_redraw = true;
    }
  }

  if (contains_hid_key(status, app_config::HID_UP)) {
    ++g_history_scroll_offset;
    g_needs_redraw = true;
  } else if (contains_hid_key(status, app_config::HID_DOWN) && g_history_scroll_offset > 0) {
    --g_history_scroll_offset;
    g_needs_redraw = true;
  }

  if (status.tab && !g_prev_tab) {
    g_screen_mode = ScreenMode::SelectWheel;
    g_input_buffer = "";
    set_status("ホイール選択へ");
  }

  if (status.del && !g_prev_backspace && g_input_buffer.length() > 0) {
    g_input_buffer.remove(g_input_buffer.length() - 1);
    g_needs_redraw = true;
  }

  if (status.enter && !g_prev_enter) {
    record_spin_token(g_input_buffer);
  }
}

void update_input_state(const Keyboard_Class::KeysState& status, const String& pressed_chars) {
  if (g_screen_mode == ScreenMode::SelectWheel) {
    handle_select_mode_keys(status, pressed_chars);
  } else {
    handle_record_mode_keys(status, pressed_chars);
  }

  g_prev_pressed_chars = pressed_chars;
  g_prev_backspace = status.del;
  g_prev_enter = status.enter;
  g_prev_tab = status.tab;
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);

  g_canvas.setColorDepth(16);
  g_canvas.createSprite(app_config::SCREEN_W, app_config::SCREEN_H);

  try_init_storage();
  g_needs_redraw = true;
  draw_ui();
  g_needs_redraw = false;
}

void loop() {
  M5Cardputer.update();
  ensure_storage_ready();

  if (M5Cardputer.BtnA.wasClicked()) {
    start_new_session(g_selected_wheel);
  }

  if (M5Cardputer.Keyboard.isChange()) {
    const auto status = M5Cardputer.Keyboard.keysState();
    const String pressed_chars = collect_pressed_chars(status);
    update_input_state(status, pressed_chars);
  }

  if (g_needs_redraw) {
    draw_ui();
    g_needs_redraw = false;
  }
}
