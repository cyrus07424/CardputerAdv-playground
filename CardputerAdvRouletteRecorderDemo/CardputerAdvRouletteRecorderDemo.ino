#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <sqlite3.h>

#include <cctype>
#include <vector>

namespace app_config {
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;
constexpr int SD_SPI_SCK_PIN = 40;
constexpr int SD_SPI_MISO_PIN = 39;
constexpr int SD_SPI_MOSI_PIN = 14;
constexpr int SD_SPI_CS_PIN = 12;
constexpr const char* DB_DIR_FS = "/roulette";
constexpr const char* DB_FILE_FS = "/roulette/roulette.db";
constexpr const char* DB_PATH = "/sd/roulette/roulette.db";
constexpr int HISTORY_ROWS = 5;
constexpr uint32_t STORAGE_RETRY_MS = 3000;
constexpr uint32_t INITIAL_STORAGE_DELAY_MS = 800;
}  // namespace app_config

enum class WheelType : uint8_t {
  American,
  European,
};

enum class ScreenMode : uint8_t {
  SelectWheel,
  RecordSpins,
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

static M5Canvas g_canvas(&M5Cardputer.Display);
static sqlite3* g_db = nullptr;
static ScreenMode g_screen_mode = ScreenMode::SelectWheel;
static WheelType g_selected_wheel = WheelType::American;
static int g_current_session_id = -1;
static std::vector<SpinRecord> g_session_spins;
static std::vector<PredictionItem> g_predictions;
static String g_prediction_basis = "データ不足";
static String g_input_buffer;
static String g_status_message = "初期化中";
static String g_prev_pressed_chars;
static bool g_prev_backspace = false;
static bool g_prev_enter = false;
static bool g_prev_tab = false;
static bool g_needs_redraw = true;
static bool g_sd_ready = false;
static bool g_db_ready = false;
static uint32_t g_last_storage_try_ms = 0;

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

String number_to_text(int value) {
  if (value == 37) {
    return "00";
  }
  return String(value);
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

int sqlite_exec(const char* sql, sqlite3_callback callback = nullptr, void* context = nullptr) {
  char* error_message = nullptr;
  const int rc = sqlite3_exec(g_db, sql, callback, context, &error_message);
  if (rc != SQLITE_OK) {
    if (error_message != nullptr) {
      set_status(String("SQL error: ") + error_message);
      sqlite3_free(error_message);
    } else {
      set_status("SQL error");
    }
  }
  return rc;
}

bool open_database() {
  if (!g_sd_ready) {
    return false;
  }

  sqlite3_initialize();
  const int rc = sqlite3_open(app_config::DB_PATH, &g_db);
  if (rc != SQLITE_OK) {
    set_status(String("DB open失敗: ") + sqlite3_errmsg(g_db));
    return false;
  }

  const char* schema_sql =
      "CREATE TABLE IF NOT EXISTS sessions ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "wheel_type TEXT NOT NULL,"
      "started_at INTEGER NOT NULL"
      ");"
      "CREATE TABLE IF NOT EXISTS spins ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "session_id INTEGER NOT NULL,"
      "spin_index INTEGER NOT NULL,"
      "number_value INTEGER NOT NULL,"
      "number_text TEXT NOT NULL,"
      "color TEXT NOT NULL,"
      "recorded_at INTEGER NOT NULL,"
      "FOREIGN KEY(session_id) REFERENCES sessions(id)"
      ");"
      "CREATE INDEX IF NOT EXISTS idx_spins_session_spin ON spins(session_id, spin_index);"
      "CREATE INDEX IF NOT EXISTS idx_spins_value ON spins(number_value);";

  if (sqlite_exec(schema_sql) != SQLITE_OK) {
    return false;
  }

  g_db_ready = true;
  set_status("DB準備OK");
  return true;
}

bool ensure_database_file() {
  if (!SD.exists(app_config::DB_DIR_FS) && !SD.mkdir(app_config::DB_DIR_FS)) {
    set_status("DBディレクトリ作成失敗");
    return false;
  }

  if (!SD.exists(app_config::DB_FILE_FS)) {
    File file = SD.open(app_config::DB_FILE_FS, FILE_WRITE);
    if (!file) {
      set_status("DBファイル作成失敗");
      return false;
    }
    file.close();
  }

  return true;
}

void init_storage() {
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

  if (!g_sd_ready) {
    set_status("SDカード初期化失敗(速度リトライ後)");
    return;
  }

  if (SD.cardType() == CARD_NONE) {
    g_sd_ready = false;
    set_status("SDカード未検出");
    return;
  }

  if (!ensure_database_file()) {
    return;
  }
  open_database();
}

void ensure_storage_ready() {
  if (g_sd_ready && g_db_ready) {
    return;
  }

  const uint32_t now = millis();
  const uint32_t retry_ms = g_last_storage_try_ms == 0 ? app_config::INITIAL_STORAGE_DELAY_MS : app_config::STORAGE_RETRY_MS;
  if (now - g_last_storage_try_ms < retry_ms) {
    return;
  }

  init_storage();
}

struct PredictionCallbackData {
  std::vector<PredictionItem>* items = nullptr;
};

int prediction_callback(void* context, int argc, char** argv, char** col_names) {
  auto* data = static_cast<PredictionCallbackData*>(context);
  if (data == nullptr || data->items == nullptr || argc < 3) {
    return 0;
  }

  PredictionItem item;
  item.value = argv[0] ? atoi(argv[0]) : 0;
  item.text = argv[1] ? argv[1] : number_to_text(item.value);
  item.count = argv[2] ? atoi(argv[2]) : 0;
  data->items->push_back(item);
  return 0;
}

std::vector<PredictionItem> query_prediction_items(const char* sql) {
  std::vector<PredictionItem> items;
  PredictionCallbackData callback_data = {&items};
  sqlite_exec(sql, prediction_callback, &callback_data);
  return items;
}

void refresh_predictions() {
  g_predictions.clear();
  g_prediction_basis = "データ不足";

  if (!g_db_ready) {
    return;
  }

  char sql[768];
  const char* wheel = wheel_name(g_selected_wheel);

  if (!g_session_spins.empty()) {
    const int last_value = g_session_spins.back().value;
    snprintf(sql, sizeof(sql),
             "SELECT next.number_value, next.number_text, COUNT(*) AS cnt "
             "FROM spins prev "
             "JOIN spins next ON next.session_id = prev.session_id AND next.spin_index = prev.spin_index + 1 "
             "JOIN sessions s ON s.id = prev.session_id "
             "WHERE s.wheel_type = '%s' AND prev.number_value = %d "
             "GROUP BY next.number_value, next.number_text "
             "ORDER BY cnt DESC, next.number_value ASC "
             "LIMIT 3;",
             wheel, last_value);
    g_predictions = query_prediction_items(sql);
    if (!g_predictions.empty()) {
      g_prediction_basis = String("直前 ") + number_to_text(last_value) + " の次";
      g_needs_redraw = true;
      return;
    }
  }

  if (g_current_session_id >= 0) {
    snprintf(sql, sizeof(sql),
             "SELECT number_value, number_text, COUNT(*) AS cnt "
             "FROM spins "
             "WHERE session_id = %d "
             "GROUP BY number_value, number_text "
             "ORDER BY cnt DESC, number_value ASC "
             "LIMIT 3;",
             g_current_session_id);
    g_predictions = query_prediction_items(sql);
    if (!g_predictions.empty()) {
      g_prediction_basis = "現セッション頻度";
      g_needs_redraw = true;
      return;
    }
  }

  snprintf(sql, sizeof(sql),
           "SELECT spins.number_value, spins.number_text, COUNT(*) AS cnt "
           "FROM spins "
           "JOIN sessions ON sessions.id = spins.session_id "
           "WHERE sessions.wheel_type = '%s' "
           "GROUP BY spins.number_value, spins.number_text "
           "ORDER BY cnt DESC, spins.number_value ASC "
           "LIMIT 3;",
           wheel);
  g_predictions = query_prediction_items(sql);
  if (!g_predictions.empty()) {
    g_prediction_basis = "全履歴頻度";
  }
  g_needs_redraw = true;
}

bool start_new_session(WheelType wheel) {
  if (!g_db_ready) {
    set_status("DB未接続");
    return false;
  }

  char sql[256];
  snprintf(sql, sizeof(sql), "INSERT INTO sessions (wheel_type, started_at) VALUES ('%s', %lu);", wheel_name(wheel),
           static_cast<unsigned long>(millis()));
  if (sqlite_exec(sql) != SQLITE_OK) {
    return false;
  }

  g_current_session_id = static_cast<int>(sqlite3_last_insert_rowid(g_db));
  g_selected_wheel = wheel;
  g_screen_mode = ScreenMode::RecordSpins;
  g_input_buffer = "";
  g_session_spins.clear();
  refresh_predictions();
  set_status(String("Session ") + g_current_session_id + " 開始");
  return true;
}

bool record_spin_token(const String& token) {
  if (!g_db_ready || g_current_session_id < 0) {
    set_status("セッション未開始");
    return false;
  }

  int value = 0;
  String text;
  if (!parse_spin_token(token, g_selected_wheel, value, text)) {
    set_status("無効な出目");
    return false;
  }

  const int spin_index = static_cast<int>(g_session_spins.size()) + 1;
  const String color = roulette_color_text(value);

  char sql[512];
  snprintf(sql, sizeof(sql),
           "INSERT INTO spins (session_id, spin_index, number_value, number_text, color, recorded_at) "
           "VALUES (%d, %d, %d, '%s', '%s', %lu);",
           g_current_session_id, spin_index, value, text.c_str(), color.c_str(), static_cast<unsigned long>(millis()));
  if (sqlite_exec(sql) != SQLITE_OK) {
    return false;
  }

  SpinRecord spin;
  spin.index = spin_index;
  spin.value = value;
  spin.text = text;
  spin.color = color;
  g_session_spins.push_back(spin);
  g_input_buffer = "";
  refresh_predictions();
  set_status(String("記録: #") + spin_index + " = " + text);
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

void draw_select_screen() {
  g_canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  g_canvas.drawString("Roulette Recorder", 0, 0);
  g_canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  g_canvas.drawString("Tab: wheel  Enter: start", 0, 14);

  const bool american_selected = g_selected_wheel == WheelType::American;
  g_canvas.setTextColor(american_selected ? TFT_BLACK : TFT_WHITE, american_selected ? TFT_YELLOW : TFT_BLACK);
  g_canvas.fillRect(8, 42, 224, 24, american_selected ? TFT_YELLOW : TFT_DARKGRAY);
  g_canvas.drawString("American (0 / 00 / 1-36)", 12, 46);

  const bool european_selected = g_selected_wheel == WheelType::European;
  g_canvas.setTextColor(european_selected ? TFT_BLACK : TFT_WHITE, european_selected ? TFT_YELLOW : TFT_BLACK);
  g_canvas.fillRect(8, 76, 224, 24, european_selected ? TFT_YELLOW : TFT_DARKGRAY);
  g_canvas.drawString("European (0 / 1-36)", 12, 80);

  g_canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  g_canvas.drawString(tail_utf8(g_status_message, 20), 0, 112);
}

void draw_prediction_row(int y) {
  g_canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  g_canvas.drawString(tail_utf8(String("予想: ") + g_prediction_basis, 18), 0, y);

  int x = 0;
  for (size_t i = 0; i < g_predictions.size(); ++i) {
    const auto& item = g_predictions[i];
    const String label = item.text + "(" + item.count + ")";
    g_canvas.setTextColor(roulette_color_ui(roulette_color_text(item.value)), TFT_BLACK);
    g_canvas.drawString(label, x, y + 14);
    x += 76;
  }
}

void draw_history_rows(int start_y) {
  const int history_count = static_cast<int>(g_session_spins.size());
  const int start_index = history_count > app_config::HISTORY_ROWS ? history_count - app_config::HISTORY_ROWS : 0;

  for (int row = 0; row < app_config::HISTORY_ROWS; ++row) {
    const int spin_index = start_index + row;
    if (spin_index >= history_count) {
      break;
    }

    const auto& spin = g_session_spins[spin_index];
    g_canvas.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    g_canvas.drawString(String("#") + spin.index, 0, start_y + row * 14);
    g_canvas.setTextColor(roulette_color_ui(spin.color), TFT_BLACK);
    g_canvas.drawString(spin.text, 42, start_y + row * 14);
  }
}

void draw_record_screen() {
  g_canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  g_canvas.drawString(String("Wheel: ") + wheel_name(g_selected_wheel), 0, 0);
  g_canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  g_canvas.drawString(String("Session: ") + g_current_session_id + "  Spins: " + g_session_spins.size(), 0, 14);

  g_canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  g_canvas.drawString(String("Input: ") + g_input_buffer + "_", 0, 30);

  draw_prediction_row(46);
  draw_history_rows(78);

  g_canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  g_canvas.drawString(tail_utf8(g_status_message, 20), 0, 120);
}

void draw_ui() {
  g_canvas.fillScreen(TFT_BLACK);
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
    g_selected_wheel =
        g_selected_wheel == WheelType::American ? WheelType::European : WheelType::American;
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

    if (std::isdigit(static_cast<unsigned char>(c))) {
      if (g_input_buffer.length() < 2) {
        g_input_buffer += c;
        g_needs_redraw = true;
      }
    } else if (lower == 'z' && g_selected_wheel == WheelType::American) {
      g_input_buffer = "00";
      g_needs_redraw = true;
    }
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

  init_storage();
  g_needs_redraw = true;
  draw_ui();
  g_needs_redraw = false;
}

void loop() {
  M5Cardputer.update();
  ensure_storage_ready();

  if (M5Cardputer.BtnA.wasClicked()) {
    if (g_screen_mode == ScreenMode::SelectWheel) {
      start_new_session(g_selected_wheel);
    } else {
      start_new_session(g_selected_wheel);
    }
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
