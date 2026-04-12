#include <Arduino.h>
#include <M5Cardputer.h>

String g_current_line;
String g_history[6];
String g_prev_pressed_chars;
bool g_prev_backspace = false;
bool g_prev_enter = false;
bool g_shift_active = false;
bool g_needs_redraw = true;

void push_history(const String& line) {
  for (size_t i = 0; i + 1 < 6; ++i) {
    g_history[i] = g_history[i + 1];
  }
  g_history[5] = line;
}

String tail_text(const String& text, size_t max_len) {
  if (text.length() <= max_len) {
    return text;
  }
  return text.substring(text.length() - max_len);
}

bool is_special_key(char key) {
  return key == KEY_FN || key == KEY_LEFT_SHIFT || key == KEY_LEFT_CTRL ||
         key == KEY_LEFT_ALT || key == KEY_OPT || key == KEY_ENTER ||
         key == KEY_BACKSPACE || key == KEY_TAB;
}

String collect_pressed_chars(const Keyboard_Class::KeysState& status) {
  String result;
  bool shifted = status.fn || status.shift;

  for (const auto& key_pos : M5Cardputer.Keyboard.keyList()) {
    const auto key_value = M5Cardputer.Keyboard.getKeyValue(key_pos);
    if (is_special_key(key_value.value_first)) {
      continue;
    }

    result += shifted ? key_value.value_second : key_value.value_first;
  }
  return result;
}

void draw_ui() {
  auto& display = M5Cardputer.Display;
  display.startWrite();
  display.fillScreen(TFT_BLACK);
  display.setTextFont(1);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.println("Cardputer ADV Keyboard Demo");
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.println("Type on built-in keyboard");
  display.println("Enter: commit / BtnA: clear");
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.printf("Shift:%s\n", g_shift_active ? "ON" : "OFF");
  display.println("------------------------------");
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.printf("> %s\n", tail_text(g_current_line, 28).c_str());
  display.println("------------------------------");
  for (const auto& line : g_history) {
    display.println(tail_text(line, 30));
  }
  display.endWrite();
}

void handle_keyboard() {
  if (!M5Cardputer.Keyboard.isChange()) {
    return;
  }

  const auto status = M5Cardputer.Keyboard.keysState();
  g_shift_active = status.fn || status.shift;

  const String pressed_chars = collect_pressed_chars(status);
  for (size_t i = 0; i < pressed_chars.length(); ++i) {
    const char c = pressed_chars[i];
    if (g_prev_pressed_chars.indexOf(c) < 0 && g_current_line.length() < 80) {
      g_current_line += c;
      g_needs_redraw = true;
    }
  }

  if (status.del && !g_prev_backspace && g_current_line.length() > 0) {
    g_current_line.remove(g_current_line.length() - 1);
    g_needs_redraw = true;
  }

  if (status.enter && !g_prev_enter) {
    push_history(g_current_line);
    g_current_line = "";
    g_needs_redraw = true;
  }

  g_prev_pressed_chars = pressed_chars;
  g_prev_backspace = status.del;
  g_prev_enter = status.enter;
  g_needs_redraw = true;
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, true);

  M5Cardputer.Display.setRotation(1);
  push_history("Keyboard ready");
  draw_ui();
  g_needs_redraw = false;
}

void loop() {
  M5Cardputer.update();

  handle_keyboard();

  if (M5Cardputer.BtnA.wasClicked()) {
    g_current_line = "";
    for (auto& line : g_history) {
      line = "";
    }
    push_history("Cleared");
    g_needs_redraw = true;
  }

  if (g_needs_redraw) {
    draw_ui();
    g_needs_redraw = false;
  }
}
