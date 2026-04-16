#include <Arduino.h>
#include <M5Cardputer.h>
#include <TinyGPS++.h>
#include <RadioLib.h>
#include <SPI.h>
#include <SD.h>
#include <USB.h>
#include <USBMSC.h>
#include <math.h>

namespace board_pins {
constexpr int GPS_RX = 15;
constexpr int GPS_TX = 13;
constexpr int LORA_NSS = 5;
constexpr int LORA_RST = 3;
constexpr int LORA_BUSY = 6;
constexpr int LORA_DIO1 = 4;
constexpr int SD_SCK = 40;
constexpr int SD_MISO = 39;
constexpr int SD_MOSI = 14;
constexpr int SD_CS = 12;
}

namespace gps_config {
constexpr uint32_t UART_BAUD = 115200;
constexpr uint32_t UI_REFRESH_MS = 1000;
constexpr uint32_t SD_RETRY_MS = 3000;
constexpr float SPEED_MAX_KMH = 120.0f;
constexpr size_t NMEA_SENTENCE_MAX_LEN = 120;
}

namespace lora_config {
constexpr float FREQUENCY_MHZ = 868.0f;
constexpr float BANDWIDTH_KHZ = 500.0f;
constexpr uint8_t SPREADING_FACTOR = 7;
constexpr uint8_t CODING_RATE = 5;
constexpr uint8_t SYNC_WORD = 0x34;
constexpr int8_t OUTPUT_POWER_DBM = 10;
constexpr uint16_t PREAMBLE_LENGTH = 10;
constexpr float TCXO_VOLTAGE = 3.0f;
constexpr uint16_t BEEP_FREQUENCY_HZ = 4000;
constexpr uint16_t BEEP_DURATION_MS = 80;
}

namespace explorer_config {
constexpr size_t MAX_ENTRIES = 64;
constexpr size_t VISIBLE_ENTRIES = 7;
constexpr size_t VISIBLE_LINES = 6;
constexpr size_t PATH_LABEL_LEN = 28;
constexpr size_t ENTRY_LABEL_LEN = 29;
constexpr size_t LINE_LABEL_LEN = 30;
}

namespace menu_config {
constexpr size_t ITEM_COUNT = 3;
}

namespace keyboard_hid {
constexpr uint8_t ESC = 0x29;
constexpr uint8_t RIGHT = 0x4F;
constexpr uint8_t LEFT = 0x50;
constexpr uint8_t DOWN = 0x51;
constexpr uint8_t UP = 0x52;
}

enum class DisplayMode : uint8_t {
  Text = 0,
  Satellites,
  Compass,
  Speed,
  Count
};

enum class AppMode : uint8_t {
  Menu = 0,
  Harvest,
  Explorer,
  UsbStorage
};

HardwareSerial GPSSerial(1);
TinyGPSPlus gps;
SX1262 radio = new Module(
    board_pins::LORA_NSS,
    board_pins::LORA_DIO1,
    board_pins::LORA_RST,
    board_pins::LORA_BUSY,
    SPI);
USBMSC g_usb_msc;
File g_gps_log_file;
File g_lora_log_file;

volatile bool g_radio_rx_flag = false;
bool g_needs_redraw = true;
bool g_sd_ready = false;
bool g_radio_ready = false;
bool g_beep_enabled = true;
bool g_prev_fn_pressed = false;
bool g_usb_started = false;
bool g_usb_storage_active = false;
String g_status = "Booting...";
String g_gps_log_path;
String g_lora_log_path;
String g_nmea_sentence;
String g_lora_status = "LoRa init...";
String g_usb_status = "Select from menu";
uint32_t g_last_draw_ms = 0;
uint32_t g_last_data_ms = 0;
uint32_t g_last_sd_check_ms = 0;
uint32_t g_logged_sentence_count = 0;
uint32_t g_logged_lora_count = 0;
DisplayMode g_display_mode = DisplayMode::Text;
AppMode g_app_mode = AppMode::Menu;
size_t g_menu_selected_index = 0;
String g_explorer_path = "/";
String g_explorer_entry_names[explorer_config::MAX_ENTRIES];
String g_explorer_entry_paths[explorer_config::MAX_ENTRIES];
bool g_explorer_entry_dirs[explorer_config::MAX_ENTRIES];
size_t g_explorer_entry_sizes[explorer_config::MAX_ENTRIES];
size_t g_explorer_entry_count = 0;
size_t g_explorer_selected_index = 0;
size_t g_explorer_scroll_offset = 0;
bool g_explorer_loaded = false;
bool g_explorer_viewing_file = false;
String g_explorer_file_path;
uint32_t g_explorer_top_line = 0;
String g_explorer_file_lines[explorer_config::VISIBLE_LINES];
size_t g_explorer_file_line_count = 0;
bool g_explorer_file_has_more = false;

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

String format_log_timestamp() {
  const String utc = format_date_time();
  if (utc != "-") {
    return utc;
  }
  return "millis=" + String(millis());
}

String fit_text(const String& text, size_t max_len) {
  if (text.length() <= max_len) {
    return text;
  }
  if (max_len <= 3) {
    return text.substring(0, max_len);
  }
  return text.substring(0, max_len - 3) + "...";
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

const char* app_mode_name(AppMode mode) {
  switch (mode) {
    case AppMode::Menu:
      return "Menu";
    case AppMode::Harvest:
      return "Harvester";
    case AppMode::Explorer:
      return "Explorer";
    case AppMode::UsbStorage:
      return "USB Storage";
    default:
      return "?";
  }
}

const char* menu_item_name(size_t index) {
  switch (index) {
    case 0:
      return "Data Harvester";
    case 1:
      return "File Explorer";
    case 2:
      return "USB Storage";
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

bool has_gps_fix() {
  return gps.location.isValid() && satellite_count() > 0;
}

String log_status_text() {
  if (!g_sd_ready) {
    return "No SD";
  }
  if (g_gps_log_file || g_lora_log_file) {
    return "REC";
  }
  return "Ready";
}

String log_path_label(const String& path) {
  if (path.length() == 0) {
    return "-";
  }
  const int slash = path.lastIndexOf('/');
  return slash >= 0 ? path.substring(slash + 1) : path;
}

bool is_root_path(const String& path) {
  return path.length() == 0 || path == "/";
}

String parent_path(const String& path) {
  if (is_root_path(path)) {
    return "/";
  }
  const int slash = path.lastIndexOf('/');
  if (slash <= 0) {
    return "/";
  }
  return path.substring(0, slash);
}

bool explorer_entry_less(bool lhs_is_dir,
                         const String& lhs_name,
                         bool rhs_is_dir,
                         const String& rhs_name) {
  if (lhs_is_dir != rhs_is_dir) {
    return lhs_is_dir && !rhs_is_dir;
  }
  String lhs = lhs_name;
  String rhs = rhs_name;
  lhs.toLowerCase();
  rhs.toLowerCase();
  return lhs.compareTo(rhs) < 0;
}

String format_file_size(size_t size_bytes) {
  if (size_bytes < 1024) {
    return String(size_bytes) + "B";
  }
  if (size_bytes < 1024UL * 1024UL) {
    return String((size_bytes + 1023UL) / 1024UL) + "K";
  }
  return String((size_bytes + (1024UL * 1024UL - 1)) / (1024UL * 1024UL)) + "M";
}

const char* heading_label(double degrees) {
  static const char* labels[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  const int index = static_cast<int>((degrees + 22.5) / 45.0) % 8;
  return labels[index];
}

void IRAM_ATTR on_radio_rx() {
  g_radio_rx_flag = true;
}

void close_file(File& file) {
  if (file) {
    file.flush();
    file.close();
  }
}

void close_log_files() {
  close_file(g_gps_log_file);
  close_file(g_lora_log_file);
}

void clear_explorer_entries() {
  for (size_t i = 0; i < explorer_config::MAX_ENTRIES; ++i) {
    g_explorer_entry_names[i] = "";
    g_explorer_entry_paths[i] = "";
    g_explorer_entry_dirs[i] = false;
    g_explorer_entry_sizes[i] = 0;
  }
  g_explorer_entry_count = 0;
  g_explorer_selected_index = 0;
  g_explorer_scroll_offset = 0;
}

void insert_explorer_entry(const String& name, const String& path, bool is_dir, size_t size_bytes) {
  if (g_explorer_entry_count >= explorer_config::MAX_ENTRIES) {
    return;
  }

  size_t insert_at = g_explorer_entry_count;
  const size_t sort_start = is_root_path(g_explorer_path) ? 0 : 1;
  while (insert_at > sort_start &&
         explorer_entry_less(
             is_dir,
             name,
             g_explorer_entry_dirs[insert_at - 1],
             g_explorer_entry_names[insert_at - 1])) {
    g_explorer_entry_names[insert_at] = g_explorer_entry_names[insert_at - 1];
    g_explorer_entry_paths[insert_at] = g_explorer_entry_paths[insert_at - 1];
    g_explorer_entry_dirs[insert_at] = g_explorer_entry_dirs[insert_at - 1];
    g_explorer_entry_sizes[insert_at] = g_explorer_entry_sizes[insert_at - 1];
    --insert_at;
  }

  g_explorer_entry_names[insert_at] = name;
  g_explorer_entry_paths[insert_at] = path;
  g_explorer_entry_dirs[insert_at] = is_dir;
  g_explorer_entry_sizes[insert_at] = size_bytes;
  ++g_explorer_entry_count;
}

void load_explorer_file_page() {
  while (true) {
    for (size_t i = 0; i < explorer_config::VISIBLE_LINES; ++i) {
      g_explorer_file_lines[i] = "";
    }
    g_explorer_file_line_count = 0;
    g_explorer_file_has_more = false;

    if (!g_sd_ready || g_explorer_file_path.length() == 0) {
      return;
    }

    File file = SD.open(g_explorer_file_path.c_str(), FILE_READ);
    if (!file || file.isDirectory()) {
      close_file(file);
      return;
    }

    uint32_t visual_line_index = 0;
    bool needs_retry = false;
    String current_visual_line;

    auto emit_visual_line = [&](const String& line) -> bool {
      if (visual_line_index++ < g_explorer_top_line) {
        return true;
      }
      if (g_explorer_file_line_count < explorer_config::VISIBLE_LINES) {
        g_explorer_file_lines[g_explorer_file_line_count++] = line;
        return true;
      }
      g_explorer_file_has_more = true;
      return false;
    };

    while (file.available()) {
      const char c = static_cast<char>(file.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        if (!emit_visual_line(current_visual_line)) {
          close_file(file);
          return;
        }
        current_visual_line = "";
        continue;
      }

      if (static_cast<unsigned char>(c) < 32 || c == 127) {
        current_visual_line += '.';
      } else {
        current_visual_line += c;
      }

      if (current_visual_line.length() >= explorer_config::LINE_LABEL_LEN) {
        if (!emit_visual_line(current_visual_line)) {
          close_file(file);
          return;
        }
        current_visual_line = "";
      }
    }

    if (current_visual_line.length() > 0 || visual_line_index == 0) {
      if (!emit_visual_line(current_visual_line)) {
        close_file(file);
        return;
      }
    }

    close_file(file);

    if (g_explorer_file_line_count == 0 && g_explorer_top_line > 0) {
      --g_explorer_top_line;
      needs_retry = true;
    }

    if (!needs_retry) {
      return;
    }
  }
}

void refresh_explorer_directory() {
  clear_explorer_entries();
  g_explorer_viewing_file = false;

  if (!g_sd_ready) {
    g_explorer_loaded = true;
    return;
  }

  File dir = SD.open(g_explorer_path.c_str(), FILE_READ);
  if (!dir || !dir.isDirectory()) {
    close_file(dir);
    g_explorer_path = "/";
    g_explorer_loaded = true;
    return;
  }

  if (!is_root_path(g_explorer_path)) {
    g_explorer_entry_names[0] = "..";
    g_explorer_entry_paths[0] = parent_path(g_explorer_path);
    g_explorer_entry_dirs[0] = true;
    g_explorer_entry_sizes[0] = 0;
    g_explorer_entry_count = 1;
  }

  File entry = dir.openNextFile();
  while (entry && g_explorer_entry_count < explorer_config::MAX_ENTRIES) {
    const String path = String(entry.name());
    insert_explorer_entry(log_path_label(path), path, entry.isDirectory(), entry.size());
    close_file(entry);
    entry = dir.openNextFile();
  }

  close_file(dir);
  g_explorer_loaded = true;
}

void ensure_explorer_selection_visible() {
  if (g_explorer_selected_index < g_explorer_scroll_offset) {
    g_explorer_scroll_offset = g_explorer_selected_index;
  } else if (g_explorer_selected_index >= g_explorer_scroll_offset + explorer_config::VISIBLE_ENTRIES) {
    g_explorer_scroll_offset = g_explorer_selected_index - explorer_config::VISIBLE_ENTRIES + 1;
  }
}

void open_selected_explorer_entry() {
  if (g_explorer_entry_count == 0 || g_explorer_selected_index >= g_explorer_entry_count) {
    return;
  }

  const String& path = g_explorer_entry_paths[g_explorer_selected_index];
  if (g_explorer_entry_dirs[g_explorer_selected_index]) {
    g_explorer_path = path.length() == 0 ? "/" : path;
    refresh_explorer_directory();
    return;
  }

  g_explorer_viewing_file = true;
  g_explorer_file_path = path;
  g_explorer_top_line = 0;
  load_explorer_file_page();
}

bool contains_hid_key(const Keyboard_Class::KeysState& status, uint8_t key_code) {
  for (const auto key : status.hid_keys) {
    if (key == key_code) {
      return true;
    }
  }
  return false;
}

bool contains_char_key(const Keyboard_Class::KeysState& status, char key_code) {
  for (const auto key : status.word) {
    if (key == key_code) {
      return true;
    }
  }
  return false;
}

int32_t on_usb_msc_write(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  const uint32_t sector_size = SD.sectorSize();
  if (!sector_size || offset != 0) {
    return false;
  }
  for (uint32_t block = 0; block < bufsize / sector_size; ++block) {
    if (!SD.writeRAW(buffer + (block * sector_size), lba + block)) {
      return false;
    }
  }
  return bufsize;
}

int32_t on_usb_msc_read(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  const uint32_t sector_size = SD.sectorSize();
  if (!sector_size || offset != 0) {
    return false;
  }
  for (uint32_t block = 0; block < bufsize / sector_size; ++block) {
    if (!SD.readRAW(static_cast<uint8_t*>(buffer) + (block * sector_size), lba + block)) {
      return false;
    }
  }
  return bufsize;
}

bool on_usb_msc_start_stop(uint8_t power_condition, bool start, bool load_eject) {
  (void)power_condition;
  (void)start;
  (void)load_eject;
  return true;
}

void mark_sd_unavailable() {
  g_sd_ready = false;
  close_log_files();
  g_gps_log_path = "";
  g_lora_log_path = "";
  clear_explorer_entries();
  g_explorer_loaded = false;
  g_explorer_viewing_file = false;
  g_explorer_file_path = "";
  g_needs_redraw = true;
}

void stop_usb_storage_mode() {
  if (!g_usb_storage_active) {
    return;
  }
  g_usb_msc.end();
  g_usb_storage_active = false;
  g_usb_status = "USB storage stopped";
  SD.end();
  init_sd_card();
}

bool start_usb_storage_mode() {
  close_log_files();
  if (!g_sd_ready) {
    init_sd_card();
  }
  if (!g_sd_ready || SD.cardType() == CARD_NONE) {
    g_usb_status = "No SD card";
    return false;
  }

  g_usb_msc.vendorID("M5Stack");
  g_usb_msc.productID("CardAdv SD");
  g_usb_msc.productRevision("1.0");
  g_usb_msc.onRead(on_usb_msc_read);
  g_usb_msc.onWrite(on_usb_msc_write);
  g_usb_msc.onStartStop(on_usb_msc_start_stop);
  g_usb_msc.mediaPresent(true);
  g_usb_msc.isWritable(true);
  if (!g_usb_msc.begin(SD.numSectors(), SD.sectorSize())) {
    g_usb_status = "MSC start failed";
    return false;
  }

  if (!g_usb_started) {
    g_usb_started = USB.begin();
    if (!g_usb_started) {
      g_usb_msc.end();
      g_usb_status = "USB start failed";
      return false;
    }
  }
  g_usb_storage_active = true;
  g_usb_status = "Connect USB to PC";
  return true;
}

void enter_menu_mode() {
  if (g_app_mode == AppMode::UsbStorage) {
    stop_usb_storage_mode();
  }
  close_log_files();
  g_app_mode = AppMode::Menu;
  g_needs_redraw = true;
}

void enter_harvest_mode() {
  if (g_app_mode == AppMode::UsbStorage) {
    stop_usb_storage_mode();
  }
  g_app_mode = AppMode::Harvest;
  update_status();
  g_needs_redraw = true;
}

void enter_explorer_mode() {
  if (g_app_mode == AppMode::UsbStorage) {
    stop_usb_storage_mode();
  }
  close_log_files();
  g_app_mode = AppMode::Explorer;
  g_explorer_path = "/";
  g_explorer_loaded = false;
  g_explorer_viewing_file = false;
  g_explorer_file_path = "";
  g_explorer_top_line = 0;
  g_needs_redraw = true;
}

void enter_usb_storage_app_mode() {
  close_log_files();
  g_app_mode = AppMode::UsbStorage;
  start_usb_storage_mode();
  g_needs_redraw = true;
}

String build_log_path(const char* prefix, const char* extension) {
  char buffer[48];
  if (gps.date.isValid() && gps.time.isValid()) {
    snprintf(
        buffer,
        sizeof(buffer),
        "/%s_%04d%02d%02d_%02d%02d%02d%s",
        prefix,
        gps.date.year(),
        gps.date.month(),
        gps.date.day(),
        gps.time.hour(),
        gps.time.minute(),
        gps.time.second(),
        extension);
  } else {
    snprintf(buffer, sizeof(buffer), "/%s_%lu%s", prefix, millis() / 1000UL, extension);
  }
  return String(buffer);
}

String build_gps_log_path() {
  return build_log_path("gps", ".nmea");
}

String build_lora_log_path() {
  return build_log_path("lora", ".log");
}

void init_spi_bus() {
  SPI.begin(board_pins::SD_SCK, board_pins::SD_MISO, board_pins::SD_MOSI, board_pins::SD_CS);
}

bool init_sd_card() {
  if (!SD.begin(board_pins::SD_CS, SPI, 25000000)) {
    mark_sd_unavailable();
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    mark_sd_unavailable();
    return false;
  }

  g_sd_ready = true;
  g_needs_redraw = true;
  return true;
}

void ensure_sd_card() {
  if (g_sd_ready) {
    return;
  }

  const uint32_t now = millis();
  if (now - g_last_sd_check_ms < gps_config::SD_RETRY_MS) {
    return;
  }
  g_last_sd_check_ms = now;
  init_sd_card();
}

bool open_gps_log_file_if_needed() {
  if (!g_sd_ready) {
    return false;
  }
  if (g_gps_log_file) {
    return true;
  }

  g_gps_log_path = build_gps_log_path();
  g_gps_log_file = SD.open(g_gps_log_path.c_str(), FILE_APPEND);
  if (!g_gps_log_file) {
    mark_sd_unavailable();
    return false;
  }

  g_needs_redraw = true;
  return true;
}

bool open_lora_log_file_if_needed() {
  if (!g_sd_ready) {
    return false;
  }
  if (g_lora_log_file) {
    return true;
  }

  g_lora_log_path = build_lora_log_path();
  g_lora_log_file = SD.open(g_lora_log_path.c_str(), FILE_APPEND);
  if (!g_lora_log_file) {
    mark_sd_unavailable();
    return false;
  }

  g_needs_redraw = true;
  return true;
}

void append_nmea_log(const String& sentence) {
  if (!g_sd_ready || sentence.length() == 0 || sentence[0] != '$') {
    return;
  }
  if (!open_gps_log_file_if_needed()) {
    return;
  }

  const size_t expected_bytes = sentence.length() + 2;
  size_t written_bytes = g_gps_log_file.print(sentence);
  written_bytes += g_gps_log_file.print("\r\n");
  g_gps_log_file.flush();

  if (written_bytes < expected_bytes) {
    mark_sd_unavailable();
    return;
  }

  ++g_logged_sentence_count;
  g_needs_redraw = true;
}

String sanitize_lora_message(const String& message) {
  String sanitized = message;
  sanitized.replace("\r", " ");
  sanitized.replace("\n", " ");
  return sanitized;
}

void append_lora_log(const String& message) {
  if (!g_sd_ready || message.length() == 0) {
    return;
  }
  if (!open_lora_log_file_if_needed()) {
    return;
  }

  const String timestamp = format_log_timestamp();
  const String sanitized = sanitize_lora_message(message);
  const String line = timestamp + " RX> " + sanitized;
  const size_t expected_bytes = line.length() + 2;
  size_t written_bytes = g_lora_log_file.print(line);
  written_bytes += g_lora_log_file.print("\r\n");
  g_lora_log_file.flush();

  if (written_bytes < expected_bytes) {
    mark_sd_unavailable();
    return;
  }

  ++g_logged_lora_count;
  g_needs_redraw = true;
}

void handle_completed_nmea_sentence() {
  append_nmea_log(g_nmea_sentence);
  g_nmea_sentence = "";
}

void play_lora_receive_beep() {
  if (!g_beep_enabled) {
    return;
  }
  M5Cardputer.Speaker.tone(lora_config::BEEP_FREQUENCY_HZ, lora_config::BEEP_DURATION_MS);
}

void start_receive() {
  radio.setPacketReceivedAction(on_radio_rx);
  const int16_t state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    g_lora_status = "Listening";
  } else {
    g_lora_status = "RX error " + String(state);
  }
  g_needs_redraw = true;
}

bool init_radio() {
  const int16_t state = radio.begin(
      lora_config::FREQUENCY_MHZ,
      lora_config::BANDWIDTH_KHZ,
      lora_config::SPREADING_FACTOR,
      lora_config::CODING_RATE,
      lora_config::SYNC_WORD,
      lora_config::OUTPUT_POWER_DBM,
      lora_config::PREAMBLE_LENGTH,
      lora_config::TCXO_VOLTAGE,
      false);
  if (state != RADIOLIB_ERR_NONE) {
    g_lora_status = "Init error " + String(state);
    return false;
  }

  radio.setDio2AsRfSwitch(true);
  radio.setCurrentLimit(140);
  g_lora_status = "Listening";
  start_receive();
  return true;
}

void handle_radio_events() {
  if (!g_radio_ready || !g_radio_rx_flag) {
    return;
  }

  g_radio_rx_flag = false;
  String incoming;
  const int16_t state = radio.readData(incoming);
  if (state == RADIOLIB_ERR_NONE) {
    play_lora_receive_beep();
    append_lora_log(incoming);
    g_lora_status = "RSSI " + String(radio.getRSSI(), 1) + " / SNR " + String(radio.getSNR(), 1);
  } else {
    g_lora_status = "Read error " + String(state);
  }
  start_receive();
}

void handle_keyboard_input() {
  if (!M5Cardputer.Keyboard.isChange()) {
    return;
  }

  const auto status = M5Cardputer.Keyboard.keysState();
  if (status.fn && !g_prev_fn_pressed) {
    g_beep_enabled = !g_beep_enabled;
    g_needs_redraw = true;
  }
  g_prev_fn_pressed = status.fn;

  const bool move_up = contains_hid_key(status, keyboard_hid::UP) || contains_char_key(status, ';');
  const bool move_down = contains_hid_key(status, keyboard_hid::DOWN) || contains_char_key(status, '.');
  const bool move_left = contains_hid_key(status, keyboard_hid::LEFT) || contains_char_key(status, ',');
  const bool move_right = contains_hid_key(status, keyboard_hid::RIGHT) || contains_char_key(status, '/');
  const bool accept = status.enter || move_right;
  const bool escape = status.del || contains_hid_key(status, keyboard_hid::ESC);

  if (g_app_mode == AppMode::Menu) {
    if (move_up && g_menu_selected_index > 0) {
      --g_menu_selected_index;
      g_needs_redraw = true;
    } else if (move_down && g_menu_selected_index + 1 < menu_config::ITEM_COUNT) {
      ++g_menu_selected_index;
      g_needs_redraw = true;
    } else if (accept) {
      switch (g_menu_selected_index) {
        case 0:
          enter_harvest_mode();
          break;
        case 1:
          enter_explorer_mode();
          break;
        case 2:
          enter_usb_storage_app_mode();
          break;
      }
    }
    return;
  }

  if (escape) {
    enter_menu_mode();
    return;
  }

  if (g_app_mode == AppMode::UsbStorage) {
    return;
  }

  if (g_app_mode != AppMode::Explorer) {
    return;
  }

  if (!g_explorer_loaded) {
    refresh_explorer_directory();
  }

  if (g_explorer_viewing_file) {
    if (move_up && g_explorer_top_line > 0) {
      --g_explorer_top_line;
      load_explorer_file_page();
      g_needs_redraw = true;
    } else if (move_down && g_explorer_file_has_more) {
      ++g_explorer_top_line;
      load_explorer_file_page();
      g_needs_redraw = true;
    } else if (move_left && g_explorer_top_line >= explorer_config::VISIBLE_LINES) {
      g_explorer_top_line -= explorer_config::VISIBLE_LINES;
      load_explorer_file_page();
      g_needs_redraw = true;
    } else if (accept && g_explorer_file_has_more) {
      g_explorer_top_line += explorer_config::VISIBLE_LINES;
      load_explorer_file_page();
      g_needs_redraw = true;
    } else if (status.enter) {
      g_explorer_viewing_file = false;
      g_needs_redraw = true;
    }
    return;
  }

  if (move_up && g_explorer_selected_index > 0) {
    --g_explorer_selected_index;
    ensure_explorer_selection_visible();
    g_needs_redraw = true;
  } else if (move_down && g_explorer_selected_index + 1 < g_explorer_entry_count) {
    ++g_explorer_selected_index;
    ensure_explorer_selection_visible();
    g_needs_redraw = true;
  } else if (accept) {
    open_selected_explorer_entry();
    g_needs_redraw = true;
  } else if (move_left && !is_root_path(g_explorer_path)) {
    g_explorer_path = parent_path(g_explorer_path);
    refresh_explorer_directory();
    g_needs_redraw = true;
  }
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
  } else if (has_gps_fix()) {
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
  if (g_app_mode == AppMode::Explorer) {
    display.print("Cur mv Ent ok Esc/BS bk");
  } else {
    display.printf("BtnA:%s Esc:Menu", mode_name(g_display_mode));
  }
}

void draw_menu_mode() {
  auto& display = M5Cardputer.Display;
  const int width = display.width();
  const int height = display.height();
  display.fillScreen(TFT_BLACK);
  display.setTextFont(1);
  display.setTextSize(1);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setCursor(0, 0);
  display.print("Cardputer ADV Menu");
  draw_battery_status(width);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.setCursor(0, 10);
  display.printf("SD:%s USB:%s\n", g_sd_ready ? "OK" : "NG", g_usb_storage_active ? "MSC" : "Idle");
  display.drawFastHLine(0, 18, width, TFT_DARKGREY);
  display.setCursor(0, 22);
  for (size_t i = 0; i < menu_config::ITEM_COUNT; ++i) {
    const bool selected = i == g_menu_selected_index;
    display.setTextColor(selected ? TFT_BLACK : TFT_WHITE, selected ? TFT_GREENYELLOW : TFT_BLACK);
    display.printf("%c %s\n", selected ? '>' : ' ', menu_item_name(i));
  }
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(0, height - 10);
  display.print("Cur:Select Enter:Start");
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
  display.printf("GPS : %s %lu\n", log_status_text().c_str(), static_cast<unsigned long>(g_logged_sentence_count));
  display.printf("LoRa: %s\n", g_radio_ready ? g_lora_status.c_str() : "OFF");
  display.printf("Cnt : G%lu L%lu B:%s\n",
                  static_cast<unsigned long>(g_logged_sentence_count),
                  static_cast<unsigned long>(g_logged_lora_count),
                  g_beep_enabled ? "ON" : "OFF");
  display.printf("GFil: %s\n", log_path_label(g_gps_log_path).c_str());
  display.printf("LFil: %s\n", log_path_label(g_lora_log_path).c_str());
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
  display.printf("Fix : %s", has_gps_fix() ? "YES" : "NO");
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

void draw_usb_storage_mode() {
  auto& display = M5Cardputer.Display;
  const int width = display.width();
  const int height = display.height();
  display.fillScreen(TFT_BLACK);
  display.setTextFont(1);
  display.setTextSize(1);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setCursor(0, 0);
  display.print("USB Storage");
  draw_battery_status(width);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.setCursor(0, 10);
  display.printf("%s\n", g_usb_status.c_str());
  display.drawFastHLine(0, 18, width, TFT_DARKGREY);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(0, 24);
  display.printf("SD : %s\n", g_sd_ready ? "Mounted" : "Not ready");
  display.printf("USB: %s\n", g_usb_storage_active ? "MSC active" : "Inactive");
  display.printf("App: %s\n", app_mode_name(g_app_mode));
  display.println("Connect USB to PC");
  display.println("Logging is stopped");
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(0, height - 10);
  display.print("Esc/BS: Back to menu");
}

void draw_explorer_mode() {
  auto& display = M5Cardputer.Display;
  draw_common_frame("Explorer");
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.setCursor(0, 22);

  if (!g_sd_ready) {
    display.print("SD card not ready");
    return;
  }
  if (!g_explorer_loaded) {
    refresh_explorer_directory();
  }

  if (g_explorer_viewing_file) {
    display.printf("%s\n", fit_text(g_explorer_file_path, explorer_config::PATH_LABEL_LEN).c_str());
    display.printf("Ln %lu%s\n",
                   static_cast<unsigned long>(g_explorer_top_line + 1),
                   g_explorer_file_has_more ? "+" : "");
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    if (g_explorer_file_line_count == 0) {
      display.print("(empty file)");
      return;
    }
    for (size_t i = 0; i < g_explorer_file_line_count; ++i) {
      display.println(g_explorer_file_lines[i]);
    }
    return;
  }

  display.printf("%s\n", fit_text(g_explorer_path, explorer_config::PATH_LABEL_LEN).c_str());
  if (g_explorer_entry_count == 0) {
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.print("(no entries)");
    return;
  }

  for (size_t row = 0; row < explorer_config::VISIBLE_ENTRIES; ++row) {
    const size_t index = g_explorer_scroll_offset + row;
    if (index >= g_explorer_entry_count) {
      break;
    }

    const bool selected = index == g_explorer_selected_index;
    display.setTextColor(selected ? TFT_BLACK : TFT_WHITE, selected ? TFT_GREENYELLOW : TFT_BLACK);
    const char prefix = g_explorer_entry_dirs[index] ? 'D' : 'F';
    const String size_label = g_explorer_entry_dirs[index] ? "<DIR>" : format_file_size(g_explorer_entry_sizes[index]);
    const size_t name_width = explorer_config::ENTRY_LABEL_LEN - size_label.length() - 1;
    display.printf("%c %s %s\n",
                   prefix,
                   fit_text(g_explorer_entry_names[index], name_width).c_str(),
                   size_label.c_str());
  }
}

void draw_ui() {
  auto& display = M5Cardputer.Display;
  display.startWrite();

  switch (g_app_mode) {
    case AppMode::Menu:
      draw_menu_mode();
      break;
    case AppMode::Harvest:
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
      break;
    case AppMode::Explorer:
      draw_explorer_mode();
      break;
    case AppMode::UsbStorage:
      draw_usb_storage_mode();
      break;
  }

  display.endWrite();
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, true);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);

  init_spi_bus();
  GPSSerial.begin(gps_config::UART_BAUD, SERIAL_8N1, board_pins::GPS_RX, board_pins::GPS_TX);
  init_sd_card();
  g_radio_ready = init_radio();

  Serial.println("Cardputer ADV data harvester started");
  Serial.println("GNSS UART: RX=15 TX=13 @115200");
  Serial.println(g_radio_ready ? "LoRa RX ready" : "LoRa init failed");

  enter_menu_mode();
  update_status();
  draw_ui();
  g_needs_redraw = false;
  g_last_draw_ms = millis();
}

void loop() {
  M5Cardputer.update();
  handle_keyboard_input();

  if (g_app_mode != AppMode::UsbStorage) {
    ensure_sd_card();
  }

  if (g_app_mode == AppMode::Harvest) {
    handle_radio_events();

    bool received_data = false;
    while (GPSSerial.available() > 0) {
      const char c = static_cast<char>(GPSSerial.read());
      gps.encode(c);
      received_data = true;

      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        handle_completed_nmea_sentence();
        continue;
      }
      if (g_nmea_sentence.length() < gps_config::NMEA_SENTENCE_MAX_LEN) {
        g_nmea_sentence += c;
      } else {
        g_nmea_sentence = "";
      }
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
