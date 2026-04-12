#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>
#include <Adafruit_TCA8418.h>

enum class KeyType : uint8_t {
  Printable,
  Backspace,
  Enter,
  Shift,
  CapsLock,
  Ignored,
};

namespace board_pins {
constexpr int I2C_SDA = 8;
constexpr int I2C_SCL = 9;
}

struct KeyCell {
  KeyType type;
  char base;
  char shifted;
  const char* label;
};

constexpr KeyCell KEYMAP[4][14] = {
    {
        {KeyType::Printable, '`', '~', "`"}, {KeyType::Printable, '1', '!', "1"},
        {KeyType::Printable, '2', '@', "2"}, {KeyType::Printable, '3', '#', "3"},
        {KeyType::Printable, '4', '$', "4"}, {KeyType::Printable, '5', '%', "5"},
        {KeyType::Printable, '6', '^', "6"}, {KeyType::Printable, '7', '&', "7"},
        {KeyType::Printable, '8', '*', "8"}, {KeyType::Printable, '9', '(', "9"},
        {KeyType::Printable, '0', ')', "0"}, {KeyType::Printable, '-', '_', "-"},
        {KeyType::Printable, '=', '+', "="}, {KeyType::Backspace, 0, 0, "del"},
    },
    {
        {KeyType::Ignored, '\t', '\t', "tab"}, {KeyType::Printable, 'q', 'Q', "q"},
        {KeyType::Printable, 'w', 'W', "w"},   {KeyType::Printable, 'e', 'E', "e"},
        {KeyType::Printable, 'r', 'R', "r"},   {KeyType::Printable, 't', 'T', "t"},
        {KeyType::Printable, 'y', 'Y', "y"},   {KeyType::Printable, 'u', 'U', "u"},
        {KeyType::Printable, 'i', 'I', "i"},   {KeyType::Printable, 'o', 'O', "o"},
        {KeyType::Printable, 'p', 'P', "p"},   {KeyType::Printable, '[', '{', "["},
        {KeyType::Printable, ']', '}', "]"},   {KeyType::Printable, '\\', '|', "\\"},
    },
    {
        {KeyType::Shift, 0, 0, "shift"},         {KeyType::CapsLock, 0, 0, "caps"},
        {KeyType::Printable, 'a', 'A', "a"},     {KeyType::Printable, 's', 'S', "s"},
        {KeyType::Printable, 'd', 'D', "d"},     {KeyType::Printable, 'f', 'F', "f"},
        {KeyType::Printable, 'g', 'G', "g"},     {KeyType::Printable, 'h', 'H', "h"},
        {KeyType::Printable, 'j', 'J', "j"},     {KeyType::Printable, 'k', 'K', "k"},
        {KeyType::Printable, 'l', 'L', "l"},     {KeyType::Printable, ';', ':', ";"},
        {KeyType::Printable, '\'', '\"', "'"},   {KeyType::Enter, 0, 0, "enter"},
    },
    {
        {KeyType::Ignored, 0, 0, "ctrl"},      {KeyType::Ignored, 0, 0, "opt"},
        {KeyType::Ignored, 0, 0, "alt"},       {KeyType::Printable, 'z', 'Z', "z"},
        {KeyType::Printable, 'x', 'X', "x"},   {KeyType::Printable, 'c', 'C', "c"},
        {KeyType::Printable, 'v', 'V', "v"},   {KeyType::Printable, 'b', 'B', "b"},
        {KeyType::Printable, 'n', 'N', "n"},   {KeyType::Printable, 'm', 'M', "m"},
        {KeyType::Printable, ',', '<', ","},   {KeyType::Printable, '.', '>', "."},
        {KeyType::Printable, '/', '?', "/"},   {KeyType::Printable, ' ', ' ', "space"},
    },
};

Adafruit_TCA8418 keyboard;

bool g_keyboard_ready = false;
bool g_shift_pressed = false;
bool g_caps_locked = false;
bool g_needs_redraw = true;
String g_current_line;

constexpr size_t HISTORY_LINES = 6;
String g_history[HISTORY_LINES];

void push_history(const String& line) {
  for (size_t i = 0; i + 1 < HISTORY_LINES; ++i) {
    g_history[i] = g_history[i + 1];
  }
  g_history[HISTORY_LINES - 1] = line;
}

String tail_text(const String& text, size_t max_len) {
  if (text.length() <= max_len) {
    return text;
  }
  return text.substring(text.length() - max_len);
}

bool decode_key_event(uint8_t raw, bool& pressed, uint8_t& mapped_row, uint8_t& mapped_col) {
  if (raw == 0) {
    return false;
  }

  pressed = (raw & 0x80) != 0;
  uint8_t value = raw & 0x7F;
  if (value == 0) {
    return false;
  }

  value--;
  uint8_t row = value / 10;
  uint8_t col = value % 10;

  uint8_t remapped_col = row * 2;
  if (col > 3) {
    remapped_col++;
  }
  uint8_t remapped_row = (col + 4) % 4;

  if (remapped_row >= 4 || remapped_col >= 14) {
    return false;
  }

  mapped_row = remapped_row;
  mapped_col = remapped_col;
  return true;
}

char select_printable_char(const KeyCell& cell) {
  bool shifted = g_shift_pressed;
  if (cell.base >= 'a' && cell.base <= 'z') {
    shifted = g_shift_pressed || g_caps_locked;
  }
  return shifted ? cell.shifted : cell.base;
}

void handle_key_event(uint8_t raw) {
  bool pressed = false;
  uint8_t row = 0;
  uint8_t col = 0;
  if (!decode_key_event(raw, pressed, row, col)) {
    return;
  }

  const KeyCell& key = KEYMAP[row][col];

  if (key.type == KeyType::Shift) {
    g_shift_pressed = pressed;
    g_needs_redraw = true;
    return;
  }

  if (key.type == KeyType::CapsLock) {
    if (pressed) {
      g_caps_locked = !g_caps_locked;
      g_needs_redraw = true;
    }
    return;
  }

  if (!pressed) {
    return;
  }

  switch (key.type) {
    case KeyType::Printable:
      if (g_current_line.length() < 80) {
        g_current_line += select_printable_char(key);
        g_needs_redraw = true;
      }
      break;

    case KeyType::Backspace:
      if (g_current_line.length() > 0) {
        g_current_line.remove(g_current_line.length() - 1);
        g_needs_redraw = true;
      }
      break;

    case KeyType::Enter:
      push_history(g_current_line);
      g_current_line = "";
      g_needs_redraw = true;
      break;

    case KeyType::Ignored:
      if (key.base == '\t' && g_current_line.length() < 79) {
        g_current_line += "  ";
        g_needs_redraw = true;
      }
      break;

    default:
      break;
  }
}

void draw_ui() {
  auto& display = M5.Display;
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
  display.printf("Shift:%s  Caps:%s\n", g_shift_pressed ? "ON " : "OFF", g_caps_locked ? "ON" : "OFF");
  display.println("------------------------------");
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.printf("> %s\n", tail_text(g_current_line, 28).c_str());
  display.println("------------------------------");
  for (const auto& line : g_history) {
    display.println(tail_text(line, 30));
  }
  display.endWrite();
}

bool init_keyboard() {
  Wire.begin(board_pins::I2C_SDA, board_pins::I2C_SCL);
  if (!keyboard.begin(TCA8418_DEFAULT_ADDR, &Wire)) {
    return false;
  }
  keyboard.matrix(7, 8);
  keyboard.flush();
  return true;
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5.begin(cfg);

  M5.Display.setRotation(1);
  g_keyboard_ready = init_keyboard();

  if (!g_keyboard_ready) {
    push_history("Keyboard init failed");
  } else {
    push_history("Keyboard ready");
  }

  draw_ui();
  g_needs_redraw = false;
}

void loop() {
  M5.update();

  if (g_keyboard_ready) {
    while (keyboard.available() > 0) {
      handle_key_event(keyboard.getEvent());
    }
  }

  if (M5.BtnA.wasClicked()) {
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
