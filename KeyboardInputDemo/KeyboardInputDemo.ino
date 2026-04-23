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
constexpr int LINE_HEIGHT = 14;
constexpr int DOC_TOP = 30;
constexpr int DOC_ROWS = 5;
constexpr const char* TEXT_PATH = "/editor.txt";
constexpr const char* KANA_MAP_PATH = "/kanadic.txt";
constexpr const char* DICT_PATH = "/SKK-JISYO.M";
}  // namespace app_config

struct RomaKanaEntry {
  const char* roma;
  const char* kana;
};

struct CompositionResult {
  String converted;
  String pending;
};

struct RomaKanaRule {
  String roma;
  String kana;
};

static M5Canvas g_canvas(&M5Cardputer.Display);
static std::vector<String> g_lines = {String("")};
static String g_raw_input;
static std::vector<String> g_candidates;
static int g_candidate_index = -1;
static String g_prev_pressed_chars;
static bool g_prev_backspace = false;
static bool g_prev_enter = false;
static bool g_prev_space = false;
static bool g_prev_tab = false;
static bool g_ime_enabled = true;
static bool g_sd_ready = false;
static bool g_kana_map_ready = false;
static bool g_dict_ready = false;
static bool g_needs_redraw = true;
static String g_status_message = "起動中";
static std::vector<RomaKanaRule> g_roma_kana_rules;

static constexpr RomaKanaEntry kRomaKanaTable[] = {
    {"kya", "きゃ"}, {"kyu", "きゅ"}, {"kyo", "きょ"}, {"gya", "ぎゃ"}, {"gyu", "ぎゅ"}, {"gyo", "ぎょ"},
    {"sha", "しゃ"}, {"shu", "しゅ"}, {"sho", "しょ"}, {"sya", "しゃ"}, {"syu", "しゅ"}, {"syo", "しょ"},
    {"ja", "じゃ"},   {"ju", "じゅ"},   {"jo", "じょ"},   {"jya", "じゃ"}, {"jyu", "じゅ"}, {"jyo", "じょ"},
    {"cha", "ちゃ"}, {"chu", "ちゅ"}, {"cho", "ちょ"}, {"cya", "ちゃ"}, {"cyu", "ちゅ"}, {"cyo", "ちょ"},
    {"nya", "にゃ"}, {"nyu", "にゅ"}, {"nyo", "にょ"}, {"hya", "ひゃ"}, {"hyu", "ひゅ"}, {"hyo", "ひょ"},
    {"bya", "びゃ"}, {"byu", "びゅ"}, {"byo", "びょ"}, {"pya", "ぴゃ"}, {"pyu", "ぴゅ"}, {"pyo", "ぴょ"},
    {"mya", "みゃ"}, {"myu", "みゅ"}, {"myo", "みょ"}, {"rya", "りゃ"}, {"ryu", "りゅ"}, {"ryo", "りょ"},
    {"fa", "ふぁ"},  {"fi", "ふぃ"},  {"fe", "ふぇ"},  {"fo", "ふぉ"},  {"la", "ぁ"},    {"li", "ぃ"},
    {"lu", "ぅ"},    {"le", "ぇ"},    {"lo", "ぉ"},    {"xa", "ぁ"},    {"xi", "ぃ"},    {"xu", "ぅ"},
    {"xe", "ぇ"},    {"xo", "ぉ"},    {"ltu", "っ"},   {"xtu", "っ"},   {"lya", "ゃ"},   {"lyu", "ゅ"},
    {"lyo", "ょ"},   {"xya", "ゃ"},   {"xyu", "ゅ"},   {"xyo", "ょ"},   {"shi", "し"},   {"chi", "ち"},
    {"tsu", "つ"},   {"dhi", "でぃ"}, {"dhu", "でゅ"}, {"thi", "てぃ"}, {"thu", "てゅ"}, {"dya", "ぢゃ"},
    {"dyu", "ぢゅ"}, {"dyo", "ぢょ"}, {"tya", "ちゃ"}, {"tyu", "ちゅ"}, {"tyo", "ちょ"}, {"wi", "うぃ"},
    {"we", "うぇ"},  {"wo", "を"},    {"va", "ゔぁ"},  {"vi", "ゔぃ"},  {"vu", "ゔ"},    {"ve", "ゔぇ"},
    {"vo", "ゔぉ"},  {"ka", "か"},    {"ki", "き"},    {"ku", "く"},    {"ke", "け"},    {"ko", "こ"},
    {"ga", "が"},    {"gi", "ぎ"},    {"gu", "ぐ"},    {"ge", "げ"},    {"go", "ご"},    {"sa", "さ"},
    {"si", "し"},    {"su", "す"},    {"se", "せ"},    {"so", "そ"},    {"za", "ざ"},    {"zi", "じ"},
    {"ji", "じ"},    {"zu", "ず"},    {"ze", "ぜ"},    {"zo", "ぞ"},    {"ta", "た"},    {"ti", "ち"},
    {"tu", "つ"},    {"te", "て"},    {"to", "と"},    {"da", "だ"},    {"di", "ぢ"},    {"du", "づ"},
    {"de", "で"},    {"do", "ど"},    {"na", "な"},    {"ni", "に"},    {"nu", "ぬ"},    {"ne", "ね"},
    {"no", "の"},    {"ha", "は"},    {"hi", "ひ"},    {"hu", "ふ"},    {"fu", "ふ"},    {"he", "へ"},
    {"ho", "ほ"},    {"ba", "ば"},    {"bi", "び"},    {"bu", "ぶ"},    {"be", "べ"},    {"bo", "ぼ"},
    {"pa", "ぱ"},    {"pi", "ぴ"},    {"pu", "ぷ"},    {"pe", "ぺ"},    {"po", "ぽ"},    {"ma", "ま"},
    {"mi", "み"},    {"mu", "む"},    {"me", "め"},    {"mo", "も"},    {"ya", "や"},    {"yu", "ゆ"},
    {"yo", "よ"},    {"ra", "ら"},    {"ri", "り"},    {"ru", "る"},    {"re", "れ"},    {"ro", "ろ"},
    {"wa", "わ"},    {"nn", "ん"},    {"a", "あ"},     {"i", "い"},     {"u", "う"},     {"e", "え"},
    {"o", "お"},     {"-", "ー"},
};

void set_status(const String& message) {
  g_status_message = message;
  g_needs_redraw = true;
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

bool is_vowel(char c) {
  switch (c) {
    case 'a':
    case 'i':
    case 'u':
    case 'e':
    case 'o':
      return true;
    default:
      return false;
  }
}

bool is_consonant(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) && !is_vowel(c);
}

bool is_mapping_prefix(const String& token) {
  if (!g_roma_kana_rules.empty()) {
    for (const auto& entry : g_roma_kana_rules) {
      if (entry.roma.startsWith(token)) {
        return true;
      }
    }
    return false;
  }

  for (const auto& entry : kRomaKanaTable) {
    const String roma = entry.roma;
    if (roma.startsWith(token)) {
      return true;
    }
  }
  return false;
}

String match_roma(const String& text, int index, int* consumed) {
  if (!g_roma_kana_rules.empty()) {
    for (const auto& entry : g_roma_kana_rules) {
      if (text.startsWith(entry.roma, index)) {
        *consumed = entry.roma.length();
        return entry.kana;
      }
    }
  }

  for (const auto& entry : kRomaKanaTable) {
    const String roma = entry.roma;
    if (text.startsWith(roma, index)) {
      *consumed = roma.length();
      return entry.kana;
    }
  }
  *consumed = 0;
  return "";
}

CompositionResult build_composition(const String& raw_input) {
  CompositionResult result;
  int index = 0;

  while (index < raw_input.length()) {
    const char current = raw_input[index];

    if (index + 1 < raw_input.length() && raw_input[index] == raw_input[index + 1] &&
        is_consonant(current) && current != 'n') {
      result.converted += "っ";
      ++index;
      continue;
    }

    if (current == 'n') {
      if (index + 1 >= raw_input.length()) {
        break;
      }

      const char next = raw_input[index + 1];
      if (next == 'n') {
        if (index + 2 < raw_input.length() && (is_vowel(raw_input[index + 2]) || raw_input[index + 2] == 'y')) {
          result.converted += "ん";
          ++index;
          continue;
        }
        result.converted += "ん";
        index += 2;
        continue;
      }

      if (!is_vowel(next) && next != 'y') {
        result.converted += "ん";
        ++index;
        continue;
      }
    }

    int consumed = 0;
    String kana = match_roma(raw_input, index, &consumed);
    if (consumed > 0) {
      result.converted += kana;
      index += consumed;
      continue;
    }

    const String rest = raw_input.substring(index);
    const int check_len = rest.length() > 3 ? 3 : rest.length();
    bool should_wait = false;
    for (int len = 1; len <= check_len; ++len) {
      if (is_mapping_prefix(rest.substring(0, len))) {
        should_wait = true;
      }
    }

    if (should_wait) {
      break;
    }

    result.converted += raw_input[index];
    ++index;
  }

  result.pending = raw_input.substring(index);
  return result;
}

String current_document_line() {
  if (g_lines.empty()) {
    g_lines.push_back("");
  }
  return g_lines.back();
}

String& editable_line() {
  if (g_lines.empty()) {
    g_lines.push_back("");
  }
  return g_lines.back();
}

void clear_candidates() {
  g_candidates.clear();
  g_candidate_index = -1;
}

String current_composition_text() {
  if (g_candidate_index >= 0 && g_candidate_index < static_cast<int>(g_candidates.size())) {
    return g_candidates[g_candidate_index];
  }
  const auto composition = build_composition(g_raw_input);
  return composition.converted + composition.pending;
}

String current_conversion_key() {
  const auto composition = build_composition(g_raw_input);
  if (composition.pending.length() > 0) {
    return "";
  }
  return composition.converted;
}

void append_text(const String& text) {
  editable_line() += text;
  g_needs_redraw = true;
}

void new_line() {
  g_lines.push_back("");
  g_needs_redraw = true;
}

int utf8_char_size(uint8_t lead) {
  if ((lead & 0x80) == 0) return 1;
  if ((lead & 0xE0) == 0xC0) return 2;
  if ((lead & 0xF0) == 0xE0) return 3;
  if ((lead & 0xF8) == 0xF0) return 4;
  return 1;
}

void remove_last_utf8(String& text) {
  if (text.length() == 0) {
    return;
  }
  int index = text.length() - 1;
  while (index > 0 && (static_cast<uint8_t>(text[index]) & 0xC0) == 0x80) {
    --index;
  }
  text.remove(index);
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

void backspace_document() {
  if (editable_line().length() > 0) {
    remove_last_utf8(editable_line());
  } else if (g_lines.size() > 1) {
    g_lines.pop_back();
  }
  g_needs_redraw = true;
}

void clear_document() {
  g_lines.clear();
  g_lines.push_back("");
  g_raw_input = "";
  clear_candidates();
  set_status("新規ドキュメント");
}

bool save_document() {
  if (!g_sd_ready) {
    set_status("SDカード未接続");
    return false;
  }

  if (SD.exists(app_config::TEXT_PATH)) {
    SD.remove(app_config::TEXT_PATH);
  }
  File file = SD.open(app_config::TEXT_PATH, FILE_WRITE);
  if (!file) {
    set_status("保存失敗");
    return false;
  }
  for (size_t i = 0; i < g_lines.size(); ++i) {
    file.print(g_lines[i]);
    if (i + 1 < g_lines.size()) {
      file.print('\n');
    }
  }
  file.close();
  set_status(String("保存: ") + app_config::TEXT_PATH);
  return true;
}

bool load_document() {
  if (!g_sd_ready) {
    set_status("SDカード未接続");
    return false;
  }

  File file = SD.open(app_config::TEXT_PATH, FILE_READ);
  if (!file) {
    set_status("読込失敗");
    return false;
  }

  g_lines.clear();
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.replace("\r", "");
    g_lines.push_back(line);
  }
  file.close();

  if (g_lines.empty()) {
    g_lines.push_back("");
  }

  g_raw_input = "";
  clear_candidates();
  set_status(String("読込: ") + app_config::TEXT_PATH);
  g_needs_redraw = true;
  return true;
}

String extract_dictionary_key(const String& line) {
  for (int i = 0; i < line.length(); ++i) {
    if (line[i] == ' ' || line[i] == '\t') {
      return line.substring(0, i);
    }
  }
  return "";
}

std::vector<String> parse_candidates(const String& line) {
  std::vector<String> result;
  int start = line.indexOf('/');
  if (start < 0) {
    return result;
  }

  start += 1;
  while (start < line.length()) {
    const int end = line.indexOf('/', start);
    if (end < 0) {
      break;
    }
    String item = line.substring(start, end);
    const int note_index = item.indexOf(';');
    if (note_index >= 0) {
      item = item.substring(0, note_index);
    }
    item.trim();
    if (item.length() > 0) {
      result.push_back(item);
    }
    start = end + 1;
  }
  return result;
}

bool lookup_candidates(const String& key, std::vector<String>& out_candidates) {
  out_candidates.clear();
  if (!g_sd_ready || !g_dict_ready || key.length() == 0) {
    return false;
  }

  File file = SD.open(app_config::DICT_PATH, FILE_READ);
  if (!file) {
    g_dict_ready = false;
    set_status("辞書を開けません");
    return false;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.replace("\r", "");
    if (line.length() == 0 || line[0] == ';') {
      continue;
    }

    const String dict_key = extract_dictionary_key(line);
    if (dict_key == key) {
      out_candidates = parse_candidates(line);
      file.close();
      return !out_candidates.empty();
    }
  }

  file.close();
  return false;
}

bool load_roma_kana_map() {
  g_roma_kana_rules.clear();
  if (!g_sd_ready) {
    return false;
  }

  File file = SD.open(app_config::KANA_MAP_PATH, FILE_READ);
  if (!file) {
    return false;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.replace("\r", "");
    line.trim();
    if (line.length() == 0 || line[0] == ';' || line[0] == '#') {
      continue;
    }
    const int separator = line.indexOf(':');
    if (separator <= 0) {
      continue;
    }

    RomaKanaRule rule;
    rule.roma = line.substring(0, separator);
    rule.kana = line.substring(separator + 1);
    rule.roma.trim();
    rule.kana.trim();
    if (rule.roma.length() > 0 && rule.kana.length() > 0) {
      g_roma_kana_rules.push_back(rule);
    }
  }
  file.close();
  return !g_roma_kana_rules.empty();
}

void commit_composition() {
  const String text = current_composition_text();
  if (text.length() == 0) {
    clear_candidates();
    g_raw_input = "";
    return;
  }
  append_text(text);
  g_raw_input = "";
  clear_candidates();
  g_needs_redraw = true;
}

bool start_candidate_lookup() {
  clear_candidates();
  const String key = current_conversion_key();
  if (key.length() == 0) {
    return false;
  }
  if (!lookup_candidates(key, g_candidates)) {
    set_status("候補なし");
    return false;
  }
  g_candidate_index = 0;
  set_status(String("候補 ") + String(g_candidate_index + 1) + "/" + String(g_candidates.size()));
  return true;
}

void cycle_candidates() {
  if (g_candidates.empty()) {
    return;
  }
  g_candidate_index = (g_candidate_index + 1) % g_candidates.size();
  set_status(String("候補 ") + String(g_candidate_index + 1) + "/" + String(g_candidates.size()));
}

void toggle_ime() {
  if (g_raw_input.length() > 0) {
    commit_composition();
  }
  g_ime_enabled = !g_ime_enabled;
  set_status(g_ime_enabled ? "日本語入力ON" : "ASCII入力ON");
}

void handle_ascii_input(char c) {
  String text;
  text += c;
  append_text(text);
}

void handle_japanese_input(char c) {
  if (std::isalpha(static_cast<unsigned char>(c)) || c == '\'') {
    g_raw_input += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    clear_candidates();
    g_needs_redraw = true;
    return;
  }

  if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
    if (g_raw_input.length() > 0) {
      commit_composition();
    }
    handle_ascii_input(c);
    return;
  }

  if (g_raw_input.length() > 0) {
    commit_composition();
  }
  handle_ascii_input(c);
}

void draw_ui() {
  g_canvas.fillScreen(TFT_BLACK);
  g_canvas.setTextWrap(false);
  g_canvas.setTextSize(1);
  g_canvas.setFont(&fonts::efontJA_12);

  g_canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  g_canvas.drawString(String("JP Editor  IME:") + (g_ime_enabled ? "ON" : "ASCII"), 0, 0);

  g_canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  g_canvas.drawString(String("Ctrl+S保存 Ctrl+L読込 Tab切替 SD:") + (g_sd_ready ? "OK" : "NG"), 0, 14);

  const int start_line =
      g_lines.size() > static_cast<size_t>(app_config::DOC_ROWS) ? static_cast<int>(g_lines.size()) - app_config::DOC_ROWS : 0;

  for (int row = 0; row < app_config::DOC_ROWS; ++row) {
    const int line_index = start_line + row;
    if (line_index >= static_cast<int>(g_lines.size())) {
      break;
    }

    String line = g_lines[line_index];
    if (line_index == static_cast<int>(g_lines.size()) - 1) {
      line += current_composition_text();
      line += "|";
      g_canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    } else {
      g_canvas.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    }

    g_canvas.drawString(tail_utf8(line, 18), 0, app_config::DOC_TOP + row * app_config::LINE_HEIGHT);
  }

  const int compose_y = app_config::DOC_TOP + app_config::DOC_ROWS * app_config::LINE_HEIGHT + 2;
  g_canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  const String composition_line =
      g_raw_input.length() > 0 ? String("変換: ") + tail_utf8(current_composition_text(), 14) : String("変換: (なし)");
  g_canvas.drawString(composition_line, 0, compose_y);

  g_canvas.setTextColor(TFT_MAGENTA, TFT_BLACK);
  String candidate_line = "候補: (なし)";
  if (g_candidate_index >= 0 && g_candidate_index < static_cast<int>(g_candidates.size())) {
    candidate_line = String("候補 ") + String(g_candidate_index + 1) + "/" + String(g_candidates.size()) + ": " +
                     tail_utf8(g_candidates[g_candidate_index], 10);
  }
  g_canvas.drawString(candidate_line, 0, compose_y + app_config::LINE_HEIGHT);

  g_canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  g_canvas.drawString(tail_utf8(g_status_message, 18), 0, compose_y + app_config::LINE_HEIGHT * 2);

  g_canvas.pushSprite(0, 0);
}

void handle_special_keys(const Keyboard_Class::KeysState& status, const String& pressed_chars) {
  for (size_t i = 0; i < pressed_chars.length(); ++i) {
    const char c = pressed_chars[i];
    if (g_prev_pressed_chars.indexOf(c) >= 0) {
      continue;
    }

    const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (status.ctrl) {
      if (lower == 's') {
        save_document();
      } else if (lower == 'l') {
        load_document();
      } else if (lower == 'n') {
        clear_document();
      }
      continue;
    }

    if (g_ime_enabled) {
      handle_japanese_input(c);
    } else {
      handle_ascii_input(c);
    }
  }

  if (status.tab && !g_prev_tab) {
    toggle_ime();
  }

  if (status.space && !g_prev_space) {
    if (g_candidate_index >= 0) {
      cycle_candidates();
    } else if (g_ime_enabled && g_raw_input.length() > 0) {
      if (!start_candidate_lookup()) {
        commit_composition();
      }
    } else {
      append_text(" ");
    }
  }

  if (status.del && !g_prev_backspace) {
    if (g_candidate_index >= 0) {
      clear_candidates();
      set_status("候補選択終了");
    } else if (g_raw_input.length() > 0) {
      g_raw_input.remove(g_raw_input.length() - 1);
      g_needs_redraw = true;
    } else {
      backspace_document();
    }
  }

  if (status.enter && !g_prev_enter) {
    if (g_candidate_index >= 0 || g_raw_input.length() > 0) {
      commit_composition();
    } else {
      new_line();
    }
  }

  g_prev_pressed_chars = pressed_chars;
  g_prev_backspace = status.del;
  g_prev_enter = status.enter;
  g_prev_space = status.space;
  g_prev_tab = status.tab;
}

void init_sd() {
  SPI.begin(app_config::SD_SPI_SCK_PIN, app_config::SD_SPI_MISO_PIN, app_config::SD_SPI_MOSI_PIN, app_config::SD_SPI_CS_PIN);
  g_sd_ready = SD.begin(app_config::SD_SPI_CS_PIN, SPI, 25000000);
  if (!g_sd_ready) {
    set_status("SDカード初期化失敗");
    g_kana_map_ready = false;
    g_dict_ready = false;
    return;
  }

  g_kana_map_ready = load_roma_kana_map();
  File dict = SD.open(app_config::DICT_PATH, FILE_READ);
  g_dict_ready = static_cast<bool>(dict);
  if (dict) {
    dict.close();
  }

  if (g_kana_map_ready && g_dict_ready) {
    set_status("kanadic/SKK 準備OK");
  } else if (g_dict_ready) {
    set_status("SKKのみOK /kanadic.txtは任意");
  } else {
    set_status("辞書なし: /SKK-JISYO.M");
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);

  g_canvas.setColorDepth(16);
  g_canvas.createSprite(app_config::SCREEN_W, app_config::SCREEN_H);

  init_sd();
  g_needs_redraw = true;
  draw_ui();
  g_needs_redraw = false;
}

void loop() {
  M5Cardputer.update();

  if (M5Cardputer.BtnA.wasClicked()) {
    save_document();
  }
  if (M5Cardputer.BtnA.wasHold()) {
    load_document();
  }

  if (M5Cardputer.Keyboard.isChange()) {
    const auto status = M5Cardputer.Keyboard.keysState();
    const String pressed_chars = collect_pressed_chars(status);
    handle_special_keys(status, pressed_chars);
  }

  if (g_needs_redraw) {
    draw_ui();
    g_needs_redraw = false;
  }
}
