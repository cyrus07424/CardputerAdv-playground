#include <Arduino.h>
#include <M5Cardputer.h>
#include <TinyGPS++.h>
#include <RadioLib.h>
#include <SPI.h>
#include <SD.h>
#include <USB.h>
#include <USBMSC.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <math.h>

#ifndef BUILD_ENABLE_USB_STORAGE
#define BUILD_ENABLE_USB_STORAGE 1
#endif
#ifndef BUILD_ENABLE_WIFI_SCAN
#define BUILD_ENABLE_WIFI_SCAN 1
#endif
#ifndef BUILD_ENABLE_BLE_SCAN
#define BUILD_ENABLE_BLE_SCAN 0
#endif
#ifndef BUILD_ENABLE_JSONL_LOG
#define BUILD_ENABLE_JSONL_LOG 1
#endif

#if BUILD_ENABLE_BLE_SCAN
#include <BLEDevice.h>
#include <esp32-hal-bt.h>

extern "C" {
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
}
#endif

// Build-time feature switches. Set 0/1 here before compiling.
// Recommended first trims when flash gets tight:
// 1. BUILD_ENABLE_BLE_SCAN
// 2. BUILD_ENABLE_USB_STORAGE
// 3. BUILD_ENABLE_WIFI_SCAN
// 4. BUILD_ENABLE_JSONL_LOG
// Note: BUILD_ENABLE_BLE_SCAN remains experimental on the current Arduino core.

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
constexpr uint32_t INFO_REFRESH_MS = 30000;
constexpr uint32_t QUERY_TIMEOUT_MS = 400;
constexpr uint32_t SATELLITE_TIMEOUT_MS = 15000;
constexpr float SPEED_MAX_KMH = 120.0f;
constexpr size_t NMEA_SENTENCE_MAX_LEN = 120;
constexpr size_t QUERY_LINE_MAX_LEN = 127;
}

namespace harvest_log_config {
constexpr uint32_t SNAPSHOT_INTERVAL_MS = 5000;
constexpr uint32_t WIFI_SCAN_INTERVAL_MS = 30000;
constexpr uint32_t BLE_SCAN_INTERVAL_MS = 30000;
constexpr uint32_t BLE_SCAN_DURATION_S = 4;
constexpr uint16_t WIFI_SCAN_MAX_MS_PER_CHANNEL = 40;
constexpr size_t MAX_WIFI_RESULTS = 12;
constexpr size_t MAX_BLE_RESULTS = 8;
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
constexpr size_t MAX_VISIBLE_ENTRIES = 9;
constexpr size_t MAX_VISIBLE_LINES = 8;
constexpr size_t PATH_LABEL_LEN = 28;
constexpr size_t ENTRY_LABEL_LEN = 29;
constexpr size_t LINE_LABEL_LEN = 30;
constexpr int LIST_BODY_Y = 32;
constexpr int FILE_BODY_Y = 42;
}

namespace menu_config {
constexpr size_t ITEM_COUNT = 3;
}

namespace log_dir_config {
constexpr const char* GPS = "/gps";
constexpr const char* LORA = "/lora";
constexpr const char* JSONL = "/jsonl";
}

namespace text_mode_config {
constexpr size_t MAX_LINES = 20;
constexpr int TOP_Y = 22;
constexpr int LINE_HEIGHT = 10;
}

namespace wireless_mode_config {
constexpr int INFO_TOP_Y = 22;
constexpr int LIST_TOP_Y = 44;
constexpr int ROW_HEIGHT = 11;
constexpr int ROW_GAP = 2;
constexpr size_t VISIBLE_NETWORKS = 5;
constexpr int BLUETOOTH_LIST_TOP_Y = 34;
constexpr size_t VISIBLE_BLUETOOTH = 7;
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
  SignalBars,
  Wireless,
  Bluetooth,
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

struct TrackedSatellite {
  uint16_t id = 0;
  int elevation = -1;
  int azimuth = -1;
  int snr = -1;
  char system = '?';
  uint32_t last_seen_ms = 0;
};

struct WifiScanResult {
  String ssid;
  String bssid;
  int32_t rssi = 0;
  int32_t channel = 0;
  wifi_auth_mode_t encryption = WIFI_AUTH_OPEN;
};

struct BluetoothScanResult {
  String name;
  String address;
  int32_t rssi = 0;
  uint32_t last_seen_ms = 0;
};

HardwareSerial GPSSerial(1);
TinyGPSPlus gps;
SX1262 radio = new Module(
    board_pins::LORA_NSS,
    board_pins::LORA_DIO1,
    board_pins::LORA_RST,
    board_pins::LORA_BUSY,
    SPI);
M5Canvas g_canvas(&M5Cardputer.Display);
#if BUILD_ENABLE_USB_STORAGE
USBMSC g_usb_msc;
#endif
File g_gps_log_file;
File g_lora_log_file;
File g_jsonl_log_file;

volatile bool g_radio_rx_flag = false;
bool g_needs_redraw = true;
bool g_sd_ready = false;
bool g_radio_ready = false;
bool g_beep_enabled = true;
bool g_prev_fn_pressed = false;
bool g_usb_started = false;
bool g_usb_storage_active = false;
bool g_ble_ready = false;
bool g_ble_scan_configured = false;
bool g_ble_scan_active = false;
String g_status = "Booting...";
String g_gps_log_path;
String g_lora_log_path;
String g_jsonl_log_path;
String g_nmea_sentence;
String g_gnss_version = "-";
String g_gnss_mode = "-";
String g_lora_status = "LoRa init...";
String g_last_lora_message;
String g_last_lora_timestamp = "-";
String g_usb_status = "Select from menu";
uint32_t g_last_draw_ms = 0;
uint32_t g_last_data_ms = 0;
uint32_t g_last_gnss_info_ms = 0;
uint32_t g_last_sd_check_ms = 0;
uint32_t g_last_json_snapshot_ms = 0;
uint32_t g_last_wifi_scan_ms = 0;
uint32_t g_last_ble_scan_ms = 0;
uint32_t g_logged_sentence_count = 0;
uint32_t g_logged_lora_count = 0;
uint32_t g_logged_json_count = 0;
size_t g_text_scroll_offset = 0;
size_t g_signal_scroll_offset = 0;
size_t g_wireless_scroll_offset = 0;
size_t g_bluetooth_scroll_offset = 0;
TrackedSatellite g_tracked_satellites[48];
size_t g_tracked_satellite_count = 0;
WifiScanResult g_wifi_scan_results[harvest_log_config::MAX_WIFI_RESULTS];
size_t g_wifi_scan_count = 0;
BluetoothScanResult g_bluetooth_scan_results[harvest_log_config::MAX_BLE_RESULTS];
size_t g_bluetooth_scan_count = 0;
float g_last_lora_rssi = 0.0f;
float g_last_lora_snr = 0.0f;
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
String g_explorer_file_lines[explorer_config::MAX_VISIBLE_LINES];
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

String json_escape(const String& text) {
  String escaped;
  escaped.reserve(text.length() + 8);
  for (size_t i = 0; i < text.length(); ++i) {
    const char c = text[i];
    switch (c) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 32) {
          char buffer[7];
          snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned char>(c));
          escaped += buffer;
        } else {
          escaped += c;
        }
        break;
    }
  }
  return escaped;
}

String json_string(const String& text) {
  return "\"" + json_escape(text) + "\"";
}

const char* wifi_auth_name(wifi_auth_mode_t encryption) {
  switch (encryption) {
    case WIFI_AUTH_OPEN:
      return "open";
    case WIFI_AUTH_WEP:
      return "wep";
    case WIFI_AUTH_WPA_PSK:
      return "wpa-psk";
    case WIFI_AUTH_WPA2_PSK:
      return "wpa2-psk";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "wpa-wpa2-psk";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "wpa2-enterprise";
    case WIFI_AUTH_WPA3_PSK:
      return "wpa3-psk";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "wpa2-wpa3-psk";
    case WIFI_AUTH_WAPI_PSK:
      return "wapi-psk";
    default:
      return "unknown";
  }
}

String bluetooth_mac_address() {
  uint8_t mac[6];
  if (esp_read_mac(mac, ESP_MAC_BT) != ESP_OK) {
    return "";
  }

  char buffer[18];
  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buffer);
}

String bluetooth_address_string(const uint8_t* address) {
  char buffer[18];
  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X", address[0], address[1], address[2], address[3], address[4], address[5]);
  return String(buffer);
}

String bluetooth_adv_name(const uint8_t* adv_data, uint8_t adv_len) {
#if BUILD_ENABLE_BLE_SCAN
  if (!adv_data || adv_len == 0) {
    return "";
  }

  uint8_t name_len = 0;
  uint8_t* name_data = esp_ble_resolve_adv_data_by_type(const_cast<uint8_t*>(adv_data), adv_len, ESP_BLE_AD_TYPE_NAME_CMPL, &name_len);
  if (!name_data || name_len == 0) {
    name_data = esp_ble_resolve_adv_data_by_type(const_cast<uint8_t*>(adv_data), adv_len, ESP_BLE_AD_TYPE_NAME_SHORT, &name_len);
  }
  if (!name_data || name_len == 0) {
    return "";
  }

  String name;
  for (uint8_t i = 0; i < name_len; ++i) {
    const char c = static_cast<char>(name_data[i]);
    name += (static_cast<unsigned char>(c) < 32 || c == 127) ? '.' : c;
  }
  return name;
#else
  (void)adv_data;
  (void)adv_len;
  return "";
#endif
}

String gnss_mode_name(const String& mode_code) {
  if (mode_code == "G") {
    return "GPS";
  }
  if (mode_code == "B") {
    return "BDS";
  }
  if (mode_code == "R") {
    return "GLONASS";
  }
  if (mode_code == "E") {
    return "GALILEO";
  }
  if (mode_code == "Q") {
    return "QZSS";
  }
  if (mode_code.length() == 0) {
    return "-";
  }
  return mode_code;
}

size_t visible_text_line_count() {
  const int content_height = M5Cardputer.Display.height() - text_mode_config::TOP_Y - 12;
  const int visible = content_height / text_mode_config::LINE_HEIGHT;
  return visible > 0 ? static_cast<size_t>(visible) : 1;
}

size_t visible_explorer_file_lines() {
  const int content_height = M5Cardputer.Display.height() - explorer_config::FILE_BODY_Y - 12;
  const int visible = content_height / text_mode_config::LINE_HEIGHT;
  if (visible <= 0) {
    return 1;
  }
  const size_t visible_lines = static_cast<size_t>(visible);
  return visible_lines < explorer_config::MAX_VISIBLE_LINES ? visible_lines : explorer_config::MAX_VISIBLE_LINES;
}

size_t visible_explorer_entries() {
  const int content_height = M5Cardputer.Display.height() - explorer_config::LIST_BODY_Y - 12;
  const int visible = content_height / text_mode_config::LINE_HEIGHT;
  if (visible <= 0) {
    return 1;
  }
  const size_t visible_lines = static_cast<size_t>(visible);
  return visible_lines < explorer_config::MAX_VISIBLE_ENTRIES ? visible_lines : explorer_config::MAX_VISIBLE_ENTRIES;
}

size_t max_text_scroll_offset(size_t line_count) {
  const size_t visible_lines = visible_text_line_count();
  return line_count > visible_lines ? line_count - visible_lines : 0;
}

void draw_vertical_scrollbar(int x, int top_y, int height, size_t total_items, size_t visible_items, size_t offset) {
  if (total_items <= visible_items || visible_items == 0 || height <= 0) {
    return;
  }

  auto& display = g_canvas;
  display.drawRect(x, top_y, 4, height, TFT_DARKGREY);

  int thumb_h = static_cast<int>((static_cast<float>(visible_items) / static_cast<float>(total_items)) * (height - 2));
  if (thumb_h < 8) {
    thumb_h = 8;
  }
  if (thumb_h > height - 2) {
    thumb_h = height - 2;
  }

  const size_t max_offset = total_items - visible_items;
  int thumb_y = top_y + 1;
  if (max_offset > 0) {
    thumb_y += static_cast<int>((static_cast<float>(offset) / static_cast<float>(max_offset)) * (height - thumb_h - 2));
  }
  display.fillRect(x + 1, thumb_y, 2, thumb_h, TFT_CYAN);
}

void draw_content_scrollbar(int top_y, int bottom_y, size_t total_items, size_t visible_items, size_t offset) {
  if (bottom_y < top_y) {
    return;
  }
  draw_vertical_scrollbar(g_canvas.width() - 5, top_y, bottom_y - top_y + 1, total_items, visible_items, offset);
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
    case DisplayMode::SignalBars:
      return "Signal";
    case DisplayMode::Wireless:
      return "WiFi";
    case DisplayMode::Bluetooth:
      return "BT";
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
#if BUILD_ENABLE_USB_STORAGE
    case 2:
      return "USB Storage";
#endif
    default:
      return "?";
  }
}

size_t menu_item_count() {
  return 2 + (BUILD_ENABLE_USB_STORAGE ? 1 : 0);
}

bool mode_enabled(DisplayMode mode) {
  switch (mode) {
    case DisplayMode::Text:
    case DisplayMode::Satellites:
    case DisplayMode::SignalBars:
    case DisplayMode::Compass:
    case DisplayMode::Speed:
      return true;
    case DisplayMode::Wireless:
      return BUILD_ENABLE_WIFI_SCAN;
    case DisplayMode::Bluetooth:
      return BUILD_ENABLE_BLE_SCAN;
    default:
      return false;
  }
}

DisplayMode next_mode(DisplayMode mode) {
  const uint8_t count = static_cast<uint8_t>(DisplayMode::Count);
  uint8_t next = static_cast<uint8_t>(mode);
  for (uint8_t i = 0; i < count; ++i) {
    next = (next + 1) % count;
    const DisplayMode candidate = static_cast<DisplayMode>(next);
    if (mode_enabled(candidate)) {
      return candidate;
    }
  }
  return DisplayMode::Text;
}

DisplayMode previous_mode(DisplayMode mode) {
  const uint8_t count = static_cast<uint8_t>(DisplayMode::Count);
  uint8_t previous = static_cast<uint8_t>(mode);
  for (uint8_t i = 0; i < count; ++i) {
    previous = (previous + count - 1) % count;
    const DisplayMode candidate = static_cast<DisplayMode>(previous);
    if (mode_enabled(candidate)) {
      return candidate;
    }
  }
  return DisplayMode::Text;
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

String explorer_child_path(const String& entry_name) {
  if (entry_name.length() == 0) {
    return g_explorer_path;
  }
  if (entry_name[0] == '/') {
    return entry_name;
  }
  if (is_root_path(g_explorer_path)) {
    return "/" + entry_name;
  }
  return g_explorer_path + "/" + entry_name;
}

char satellite_system_from_talker(const String& talker) {
  if (talker == "GP") {
    return 'P';
  }
  if (talker == "GL") {
    return 'L';
  }
  if (talker == "GA") {
    return 'A';
  }
  if (talker == "GB" || talker == "BD") {
    return 'B';
  }
  if (talker == "GQ") {
    return 'Q';
  }
  return '?';
}

const char* satellite_system_label(char system) {
  switch (system) {
    case 'P':
      return "GPS";
    case 'L':
      return "GLN";
    case 'A':
      return "GAL";
    case 'B':
      return "BDS";
    case 'Q':
      return "QZS";
    default:
      return "UNK";
  }
}

uint16_t satellite_system_color(char system) {
  switch (system) {
    case 'P':
      return TFT_YELLOW;
    case 'L':
      return TFT_CYAN;
    case 'A':
      return 0x54BF;
    case 'B':
      return TFT_ORANGE;
    case 'Q':
      return TFT_MAGENTA;
    default:
      return TFT_LIGHTGREY;
  }
}

uint16_t wifi_signal_color(int32_t rssi) {
  if (rssi >= -55) {
    return TFT_GREEN;
  }
  if (rssi >= -70) {
    return TFT_YELLOW;
  }
  if (rssi >= -82) {
    return TFT_ORANGE;
  }
  return TFT_RED;
}

void upsert_bluetooth_scan_result(const String& address, const String& name, int32_t rssi) {
  if (address.length() == 0) {
    return;
  }

  const uint32_t now = millis();
  size_t existing_index = harvest_log_config::MAX_BLE_RESULTS;
  for (size_t i = 0; i < g_bluetooth_scan_count; ++i) {
    if (g_bluetooth_scan_results[i].address == address) {
      existing_index = i;
      break;
    }
  }

  BluetoothScanResult result;
  result.address = address;
  result.name = name;
  result.rssi = rssi;
  result.last_seen_ms = now;

  if (existing_index < g_bluetooth_scan_count) {
    g_bluetooth_scan_results[existing_index] = result;
  } else if (g_bluetooth_scan_count < harvest_log_config::MAX_BLE_RESULTS) {
    g_bluetooth_scan_results[g_bluetooth_scan_count++] = result;
    existing_index = g_bluetooth_scan_count - 1;
  } else if (rssi > g_bluetooth_scan_results[g_bluetooth_scan_count - 1].rssi) {
    g_bluetooth_scan_results[g_bluetooth_scan_count - 1] = result;
    existing_index = g_bluetooth_scan_count - 1;
  } else {
    return;
  }

  while (existing_index > 0 && g_bluetooth_scan_results[existing_index - 1].rssi < g_bluetooth_scan_results[existing_index].rssi) {
    const BluetoothScanResult swap = g_bluetooth_scan_results[existing_index - 1];
    g_bluetooth_scan_results[existing_index - 1] = g_bluetooth_scan_results[existing_index];
    g_bluetooth_scan_results[existing_index] = swap;
    --existing_index;
  }
}

#if BUILD_ENABLE_BLE_SCAN
void start_bluetooth_scan() {
  if (!g_ble_ready || !g_ble_scan_configured || g_ble_scan_active) {
    return;
  }
  if (esp_ble_gap_start_scanning(harvest_log_config::BLE_SCAN_DURATION_S) != ESP_OK) {
    return;
  }
  g_ble_scan_active = true;
  g_last_ble_scan_ms = millis();
  g_needs_redraw = true;
}

void bluetooth_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
  if (!param) {
    return;
  }

  switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
      g_ble_scan_configured = param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS;
      if (g_ble_scan_configured) {
        start_bluetooth_scan();
      }
      break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        g_ble_scan_active = false;
      }
      g_needs_redraw = true;
      break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
      if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
        const uint8_t adv_len = param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len;
        upsert_bluetooth_scan_result(
            bluetooth_address_string(param->scan_rst.bda),
            bluetooth_adv_name(param->scan_rst.ble_adv, adv_len),
            param->scan_rst.rssi);
        g_needs_redraw = true;
      } else if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
        g_ble_scan_active = false;
        g_last_ble_scan_ms = millis();
        g_needs_redraw = true;
      }
      break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
      g_ble_scan_active = false;
      g_last_ble_scan_ms = millis();
      g_needs_redraw = true;
      break;
    default:
      break;
  }
}

bool init_bluetooth_scanner() {
  if (!btStarted() && !btStartMode(BT_MODE_BLE)) {
    return false;
  }

  esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
  if (bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED && esp_bluedroid_init() != ESP_OK) {
    return false;
  }
  bt_state = esp_bluedroid_get_status();
  if (bt_state != ESP_BLUEDROID_STATUS_ENABLED && esp_bluedroid_enable() != ESP_OK) {
    return false;
  }

  if (esp_ble_gap_register_callback(bluetooth_gap_callback) != ESP_OK) {
    return false;
  }

  static esp_ble_scan_params_t scan_params = {
      BLE_SCAN_TYPE_ACTIVE,
      BLE_ADDR_TYPE_PUBLIC,
      BLE_SCAN_FILTER_ALLOW_ALL,
      0x50,
      0x30,
      BLE_SCAN_DUPLICATE_ENABLE};
  if (esp_ble_gap_set_scan_params(&scan_params) != ESP_OK) {
    return false;
  }
  return true;
}
#else
bool init_bluetooth_scanner() {
  return false;
}
#endif

int visible_tracked_satellite_count() {
  int visible = 0;
  const uint32_t now = millis();
  for (size_t i = 0; i < g_tracked_satellite_count; ++i) {
    if (now - g_tracked_satellites[i].last_seen_ms <= gps_config::SATELLITE_TIMEOUT_MS) {
      ++visible;
    }
  }
  return visible;
}

size_t collect_signal_satellite_indices(int* indices, size_t capacity) {
  const uint32_t now = millis();
  size_t count = 0;

  for (size_t i = 0; i < g_tracked_satellite_count && count < capacity; ++i) {
    const TrackedSatellite& sat = g_tracked_satellites[i];
    if (now - sat.last_seen_ms > gps_config::SATELLITE_TIMEOUT_MS || sat.snr < 0) {
      continue;
    }

    size_t insert_at = count;
    while (insert_at > 0) {
      const int prev_index = indices[insert_at - 1];
      if (prev_index < 0 || g_tracked_satellites[prev_index].snr >= sat.snr) {
        break;
      }
      indices[insert_at] = indices[insert_at - 1];
      --insert_at;
    }
    indices[insert_at] = static_cast<int>(i);
    ++count;
  }

  return count;
}

TrackedSatellite* find_tracked_satellite(char system, uint16_t id) {
  for (size_t i = 0; i < g_tracked_satellite_count; ++i) {
    if (g_tracked_satellites[i].system == system && g_tracked_satellites[i].id == id) {
      return &g_tracked_satellites[i];
    }
  }

  if (g_tracked_satellite_count < (sizeof(g_tracked_satellites) / sizeof(g_tracked_satellites[0]))) {
    TrackedSatellite& sat = g_tracked_satellites[g_tracked_satellite_count++];
    sat.id = id;
    sat.system = system;
    sat.elevation = -1;
    sat.azimuth = -1;
    sat.snr = -1;
    sat.last_seen_ms = 0;
    return &sat;
  }

  size_t oldest_index = 0;
  for (size_t i = 1; i < g_tracked_satellite_count; ++i) {
    if (g_tracked_satellites[i].last_seen_ms < g_tracked_satellites[oldest_index].last_seen_ms) {
      oldest_index = i;
    }
  }
  TrackedSatellite& sat = g_tracked_satellites[oldest_index];
  sat.id = id;
  sat.system = system;
  sat.elevation = -1;
  sat.azimuth = -1;
  sat.snr = -1;
  sat.last_seen_ms = 0;
  return &sat;
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
  close_file(g_jsonl_log_file);
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
    const size_t visible_lines = visible_explorer_file_lines();
    for (size_t i = 0; i < explorer_config::MAX_VISIBLE_LINES; ++i) {
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
      if (g_explorer_file_line_count < visible_lines) {
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
    const String path = explorer_child_path(String(entry.name()));
    insert_explorer_entry(log_path_label(path), path, entry.isDirectory(), entry.size());
    close_file(entry);
    entry = dir.openNextFile();
  }

  close_file(dir);
  g_explorer_loaded = true;
}

void ensure_explorer_selection_visible() {
  const size_t visible_entries = visible_explorer_entries();
  if (g_explorer_selected_index < g_explorer_scroll_offset) {
    g_explorer_scroll_offset = g_explorer_selected_index;
  } else if (g_explorer_selected_index >= g_explorer_scroll_offset + visible_entries) {
    g_explorer_scroll_offset = g_explorer_selected_index - visible_entries + 1;
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
  g_jsonl_log_path = "";
  clear_explorer_entries();
  g_explorer_loaded = false;
  g_explorer_viewing_file = false;
  g_explorer_file_path = "";
  g_needs_redraw = true;
}

#if BUILD_ENABLE_USB_STORAGE
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
#else
void stop_usb_storage_mode() {
  g_usb_storage_active = false;
}

bool start_usb_storage_mode() {
  g_usb_status = "USB storage disabled";
  return false;
}
#endif

void enter_menu_mode() {
  if (g_app_mode == AppMode::UsbStorage) {
    stop_usb_storage_mode();
  }
  close_log_files();
  g_app_mode = AppMode::Menu;
  if (g_menu_selected_index >= menu_item_count()) {
    g_menu_selected_index = menu_item_count() > 0 ? menu_item_count() - 1 : 0;
  }
  g_needs_redraw = true;
}

void enter_harvest_mode() {
  if (g_app_mode == AppMode::UsbStorage) {
    stop_usb_storage_mode();
  }
  g_app_mode = AppMode::Harvest;
  g_last_gnss_info_ms = 0;
  g_last_json_snapshot_ms = 0;
  g_last_wifi_scan_ms = 0;
  g_last_ble_scan_ms = 0;
  g_text_scroll_offset = 0;
  g_signal_scroll_offset = 0;
  g_wireless_scroll_offset = 0;
  g_bluetooth_scroll_offset = 0;
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
#if BUILD_ENABLE_USB_STORAGE
  close_log_files();
  g_app_mode = AppMode::UsbStorage;
  start_usb_storage_mode();
#else
  g_usb_status = "USB storage disabled";
  g_app_mode = AppMode::Menu;
#endif
  g_needs_redraw = true;
}

bool ensure_log_directory(const char* directory) {
  if (!g_sd_ready) {
    return false;
  }
  if (SD.exists(directory)) {
    return true;
  }
  return SD.mkdir(directory);
}

String build_log_path(const char* directory, const char* prefix, const char* extension) {
  char buffer[64];
  if (gps.date.isValid() && gps.time.isValid()) {
    snprintf(
        buffer,
        sizeof(buffer),
        "%s/%s_%04d%02d%02d_%02d%02d%02d%s",
        directory,
        prefix,
        gps.date.year(),
        gps.date.month(),
        gps.date.day(),
        gps.time.hour(),
        gps.time.minute(),
        gps.time.second(),
        extension);
  } else {
    snprintf(buffer, sizeof(buffer), "%s/%s_%lu%s", directory, prefix, millis() / 1000UL, extension);
  }
  return String(buffer);
}

String build_gps_log_path() {
  return build_log_path(log_dir_config::GPS, "gps", ".nmea");
}

String build_lora_log_path() {
  return build_log_path(log_dir_config::LORA, "lora", ".log");
}

String build_jsonl_log_path() {
  return build_log_path(log_dir_config::JSONL, "harvest", ".jsonl");
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
  if (!ensure_log_directory(log_dir_config::GPS)) {
    mark_sd_unavailable();
    return false;
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
  if (!ensure_log_directory(log_dir_config::LORA)) {
    mark_sd_unavailable();
    return false;
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

bool open_jsonl_log_file_if_needed() {
#if BUILD_ENABLE_JSONL_LOG
  if (!g_sd_ready) {
    return false;
  }
  if (g_jsonl_log_file) {
    return true;
  }
  if (!ensure_log_directory(log_dir_config::JSONL)) {
    mark_sd_unavailable();
    return false;
  }

  g_jsonl_log_path = build_jsonl_log_path();
  g_jsonl_log_file = SD.open(g_jsonl_log_path.c_str(), FILE_APPEND);
  if (!g_jsonl_log_file) {
    mark_sd_unavailable();
    return false;
  }

  g_needs_redraw = true;
  return true;
#else
  return false;
#endif
}

void scan_wifi_networks() {
#if BUILD_ENABLE_WIFI_SCAN
  if (WiFi.getMode() != WIFI_MODE_STA) {
    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect(false, false);
  }

  g_wifi_scan_count = 0;
  const int found = WiFi.scanNetworks(false, true, false, harvest_log_config::WIFI_SCAN_MAX_MS_PER_CHANNEL);
  if (found <= 0) {
    WiFi.scanDelete();
    return;
  }

  const int limit = found < static_cast<int>(harvest_log_config::MAX_WIFI_RESULTS)
                        ? found
                        : static_cast<int>(harvest_log_config::MAX_WIFI_RESULTS);
  for (int i = 0; i < limit; ++i) {
    g_wifi_scan_results[g_wifi_scan_count].ssid = WiFi.SSID(i);
    g_wifi_scan_results[g_wifi_scan_count].bssid = WiFi.BSSIDstr(i);
    g_wifi_scan_results[g_wifi_scan_count].rssi = WiFi.RSSI(i);
    g_wifi_scan_results[g_wifi_scan_count].channel = WiFi.channel(i);
    g_wifi_scan_results[g_wifi_scan_count].encryption = WiFi.encryptionType(i);
    ++g_wifi_scan_count;
  }
  WiFi.scanDelete();
#else
  g_wifi_scan_count = 0;
#endif
}

void refresh_wireless_scans() {
  const uint32_t now = millis();
#if BUILD_ENABLE_WIFI_SCAN
  if (now - g_last_wifi_scan_ms >= harvest_log_config::WIFI_SCAN_INTERVAL_MS) {
    scan_wifi_networks();
    g_last_wifi_scan_ms = now;
  }
#endif
#if BUILD_ENABLE_BLE_SCAN
  if (g_ble_ready && g_ble_scan_configured && !g_ble_scan_active &&
      now - g_last_ble_scan_ms >= harvest_log_config::BLE_SCAN_INTERVAL_MS) {
    start_bluetooth_scan();
  }
#endif
}

String build_jsonl_snapshot() {
  String line = "{";
  line += "\"time\":" + json_string(format_log_timestamp());
  line += ",\"millis\":" + String(millis());
  line += ",\"app_mode\":" + json_string(app_mode_name(g_app_mode));
  line += ",\"display_mode\":" + json_string(mode_name(g_display_mode));
  line += ",\"battery\":{\"percent\":" + String(M5.Power.getBatteryLevel()) + "}";
  line += ",\"device\":{\"heap_free\":" + String(ESP.getFreeHeap()) + ",\"psram_free\":" + String(ESP.getFreePsram()) + "}";
  line += ",\"gps\":{";
  line += "\"fix\":" + String(has_gps_fix() ? "true" : "false");
  line += ",\"mode\":" + json_string(g_gnss_mode);
  line += ",\"version\":" + json_string(g_gnss_version);
  line += ",\"lat\":" + (gps.location.isValid() ? String(gps.location.lat(), 6) : "null");
  line += ",\"lon\":" + (gps.location.isValid() ? String(gps.location.lng(), 6) : "null");
  line += ",\"alt_m\":" + (gps.altitude.isValid() ? String(gps.altitude.meters(), 1) : "null");
  line += ",\"speed_kmh\":" + (gps.speed.isValid() ? String(gps.speed.kmph(), 1) : "null");
  line += ",\"course_deg\":" + (gps.course.isValid() ? String(gps.course.deg(), 1) : "null");
  line += ",\"satellites_used\":" + (gps.satellites.isValid() ? String(gps.satellites.value()) : "null");
  line += ",\"satellites_visible\":" + String(visible_tracked_satellite_count());
  line += ",\"hdop\":" + (gps.hdop.isValid() ? String(gps.hdop.hdop(), 1) : "null");
  line += ",\"utc\":" + json_string(format_date_time());
  line += "}";
  line += ",\"lora\":{";
  line += "\"status\":" + json_string(g_radio_ready ? g_lora_status : "OFF");
  line += ",\"count\":" + String(g_logged_lora_count);
  line += ",\"last_message\":" + json_string(g_last_lora_message);
  line += ",\"last_time\":" + json_string(g_last_lora_timestamp);
  line += ",\"last_rssi\":" + String(g_last_lora_rssi, 1);
  line += ",\"last_snr\":" + String(g_last_lora_snr, 1);
  line += "}";
#if BUILD_ENABLE_WIFI_SCAN
  line += ",\"wifi\":[";
  for (size_t i = 0; i < g_wifi_scan_count; ++i) {
    if (i > 0) {
      line += ",";
    }
    line += "{";
    line += "\"ssid\":" + json_string(g_wifi_scan_results[i].ssid);
    line += ",\"bssid\":" + json_string(g_wifi_scan_results[i].bssid);
    line += ",\"rssi\":" + String(g_wifi_scan_results[i].rssi);
    line += ",\"channel\":" + String(g_wifi_scan_results[i].channel);
    line += ",\"auth\":" + json_string(wifi_auth_name(g_wifi_scan_results[i].encryption));
    line += "}";
  }
  line += "]";
#endif
  line += ",\"bluetooth\":{";
  line += "\"mac\":" + json_string(bluetooth_mac_address());
  line += ",\"supported\":" + String(BUILD_ENABLE_BLE_SCAN ? "true" : "false");
  line += "}";
  line += "}";
  return line;
}

void append_jsonl_snapshot_if_needed() {
#if BUILD_ENABLE_JSONL_LOG
  const uint32_t now = millis();
  if (now - g_last_json_snapshot_ms < harvest_log_config::SNAPSHOT_INTERVAL_MS) {
    return;
  }
  if (!open_jsonl_log_file_if_needed()) {
    return;
  }

  const String line = build_jsonl_snapshot();
  const size_t expected_bytes = line.length() + 2;
  size_t written_bytes = g_jsonl_log_file.print(line);
  written_bytes += g_jsonl_log_file.print("\r\n");
  g_jsonl_log_file.flush();

  if (written_bytes < expected_bytes) {
    mark_sd_unavailable();
    return;
  }

  g_last_json_snapshot_ms = now;
  ++g_logged_json_count;
  g_needs_redraw = true;
#endif
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

String nmea_field(const String& sentence, size_t field_index) {
  const int checksum = sentence.indexOf('*');
  const int end = checksum >= 0 ? checksum : sentence.length();
  int start = 0;
  size_t current_index = 0;

  for (int i = 0; i <= end; ++i) {
    if (i == end || sentence[i] == ',') {
      if (current_index == field_index) {
        return sentence.substring(start, i);
      }
      start = i + 1;
      ++current_index;
    }
  }
  return "";
}

void parse_gsv_sentence(const String& sentence) {
  if (sentence.length() < 6 || sentence[0] != '$') {
    return;
  }

  const char system = satellite_system_from_talker(sentence.substring(1, 3));
  const uint32_t now = millis();
  for (size_t group = 0; group < 4; ++group) {
    const size_t base = 4 + group * 4;
    const String id_text = nmea_field(sentence, base);
    if (id_text.length() == 0) {
      continue;
    }

    TrackedSatellite* sat = find_tracked_satellite(system, static_cast<uint16_t>(id_text.toInt()));
    if (!sat) {
      continue;
    }

    sat->elevation = nmea_field(sentence, base + 1).toInt();
    sat->azimuth = nmea_field(sentence, base + 2).toInt();
    const String snr_text = nmea_field(sentence, base + 3);
    sat->snr = snr_text.length() == 0 ? -1 : snr_text.toInt();
    sat->last_seen_ms = now;
  }
}

void handle_completed_nmea_sentence() {
  parse_gsv_sentence(g_nmea_sentence);
  append_nmea_log(g_nmea_sentence);
  g_nmea_sentence = "";
}

void handle_gnss_serial_char(char c) {
  gps.encode(c);

  if (c == '\r') {
    return;
  }
  if (c == '\n') {
    handle_completed_nmea_sentence();
    return;
  }
  if (g_nmea_sentence.length() < gps_config::NMEA_SENTENCE_MAX_LEN) {
    g_nmea_sentence += c;
  } else {
    g_nmea_sentence = "";
  }
}

String extract_gnss_field(const String& line, const char* key) {
  const int start = line.indexOf(key);
  if (start < 0) {
    return "";
  }

  const int value_start = start + static_cast<int>(strlen(key));
  int value_end = line.indexOf('*', value_start);
  if (value_end < 0) {
    value_end = line.length();
  }
  return line.substring(value_start, value_end);
}

String query_gnss_value(const char* command, const char* key) {
  GPSSerial.print(command);

  String line;
  const uint32_t started_ms = millis();
  while (millis() - started_ms < gps_config::QUERY_TIMEOUT_MS) {
    while (GPSSerial.available() > 0) {
      const char c = static_cast<char>(GPSSerial.read());
      handle_gnss_serial_char(c);

      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        const String value = extract_gnss_field(line, key);
        if (value.length() > 0) {
          return value;
        }
        line = "";
        continue;
      }

      if (line.length() < gps_config::QUERY_LINE_MAX_LEN) {
        line += c;
      }
    }
  }
  return "";
}

void refresh_gnss_info(bool force = false) {
  const uint32_t now = millis();
  if (!force && now - g_last_gnss_info_ms < gps_config::INFO_REFRESH_MS) {
    return;
  }
  g_last_gnss_info_ms = now;

  const String version = query_gnss_value("$PCAS06,0*1B\r\n", "SW=");
  if (version.length() > 0 && version != g_gnss_version) {
    g_gnss_version = version;
    g_needs_redraw = true;
  }

  const String mode = gnss_mode_name(query_gnss_value("$PCAS06,2*19\r\n", "MO="));
  if (mode.length() > 0 && mode != g_gnss_mode) {
    g_gnss_mode = mode;
    g_needs_redraw = true;
  }
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
    g_last_lora_message = sanitize_lora_message(incoming);
    g_last_lora_timestamp = format_log_timestamp();
    g_last_lora_rssi = radio.getRSSI();
    g_last_lora_snr = radio.getSNR();
    g_lora_status = "RSSI " + String(g_last_lora_rssi, 1) + " / SNR " + String(g_last_lora_snr, 1);
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
    } else if (move_down && g_menu_selected_index + 1 < menu_item_count()) {
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
#if BUILD_ENABLE_USB_STORAGE
        case 2:
          enter_usb_storage_app_mode();
          break;
#endif
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

  if (g_app_mode == AppMode::Harvest) {
    if (g_display_mode == DisplayMode::Text) {
      const size_t text_line_count = 12;
      const size_t max_scroll = max_text_scroll_offset(text_line_count);
      if (move_up && g_text_scroll_offset > 0) {
        --g_text_scroll_offset;
        g_needs_redraw = true;
        return;
      }
      if (move_down && g_text_scroll_offset < max_scroll) {
        ++g_text_scroll_offset;
        g_needs_redraw = true;
        return;
      }
    }
    if (g_display_mode == DisplayMode::SignalBars) {
      int indices[48];
      const size_t total_bars = collect_signal_satellite_indices(indices, sizeof(indices) / sizeof(indices[0]));
      const size_t visible_bars = 7;
      const size_t max_scroll = total_bars > visible_bars ? total_bars - visible_bars : 0;
      if (move_up && g_signal_scroll_offset > 0) {
        --g_signal_scroll_offset;
        g_needs_redraw = true;
        return;
      }
      if (move_down && g_signal_scroll_offset < max_scroll) {
        ++g_signal_scroll_offset;
        g_needs_redraw = true;
        return;
      }
    }
    if (g_display_mode == DisplayMode::Wireless) {
      const size_t visible_networks = wireless_mode_config::VISIBLE_NETWORKS;
      const size_t max_scroll = g_wifi_scan_count > visible_networks ? g_wifi_scan_count - visible_networks : 0;
      if (move_up && g_wireless_scroll_offset > 0) {
        --g_wireless_scroll_offset;
        g_needs_redraw = true;
        return;
      }
      if (move_down && g_wireless_scroll_offset < max_scroll) {
        ++g_wireless_scroll_offset;
        g_needs_redraw = true;
        return;
      }
    }
    if (g_display_mode == DisplayMode::Bluetooth) {
      const size_t visible_devices = wireless_mode_config::VISIBLE_BLUETOOTH;
      const size_t max_scroll = g_bluetooth_scan_count > visible_devices ? g_bluetooth_scan_count - visible_devices : 0;
      if (move_up && g_bluetooth_scroll_offset > 0) {
        --g_bluetooth_scroll_offset;
        g_needs_redraw = true;
        return;
      }
      if (move_down && g_bluetooth_scroll_offset < max_scroll) {
        ++g_bluetooth_scroll_offset;
        g_needs_redraw = true;
        return;
      }
    }

    if (move_left) {
      g_display_mode = previous_mode(g_display_mode);
      g_text_scroll_offset = 0;
      g_signal_scroll_offset = 0;
      g_wireless_scroll_offset = 0;
      g_bluetooth_scroll_offset = 0;
      g_needs_redraw = true;
    } else if (move_right) {
      g_display_mode = next_mode(g_display_mode);
      g_text_scroll_offset = 0;
      g_signal_scroll_offset = 0;
      g_wireless_scroll_offset = 0;
      g_bluetooth_scroll_offset = 0;
      g_needs_redraw = true;
    }
    return;
  }

  if (g_app_mode != AppMode::Explorer) {
    return;
  }

  if (!g_explorer_loaded) {
    refresh_explorer_directory();
  }

  if (g_explorer_viewing_file) {
    const size_t visible_lines = visible_explorer_file_lines();
    if (move_up && g_explorer_top_line > 0) {
      --g_explorer_top_line;
      load_explorer_file_page();
      g_needs_redraw = true;
    } else if (move_down && g_explorer_file_has_more) {
      ++g_explorer_top_line;
      load_explorer_file_page();
      g_needs_redraw = true;
    } else if (move_left && g_explorer_top_line >= visible_lines) {
      g_explorer_top_line -= visible_lines;
      load_explorer_file_page();
      g_needs_redraw = true;
    } else if (accept && g_explorer_file_has_more) {
      g_explorer_top_line += visible_lines;
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
  auto& display = g_canvas;
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
  auto& display = g_canvas;
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
  } else if (g_app_mode == AppMode::Harvest &&
             (g_display_mode == DisplayMode::Text || g_display_mode == DisplayMode::SignalBars ||
              g_display_mode == DisplayMode::Wireless || g_display_mode == DisplayMode::Bluetooth)) {
    display.print("U/D:Scr L/R:Mode Esc:Menu");
  } else {
    display.printf("BtnA/Cur:%s Esc:Menu", mode_name(g_display_mode));
  }
}

void draw_menu_mode() {
  auto& display = g_canvas;
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
  for (size_t i = 0; i < menu_item_count(); ++i) {
    const bool selected = i == g_menu_selected_index;
    display.setTextColor(selected ? TFT_BLACK : TFT_WHITE, selected ? TFT_GREENYELLOW : TFT_BLACK);
    display.printf("%c %s\n", selected ? '>' : ' ', menu_item_name(i));
  }
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(0, height - 10);
  display.print("Cur:Select Enter:Start");
}

void draw_text_mode() {
  auto& display = g_canvas;
  draw_common_frame("Text");
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  String lines[text_mode_config::MAX_LINES];
  size_t line_count = 0;
  lines[line_count++] = "Lat : " + format_float_value(gps.location.isValid(), gps.location.lat(), 6);
  lines[line_count++] = "Lng : " + format_float_value(gps.location.isValid(), gps.location.lng(), 6);
  lines[line_count++] = "Alt : " + format_float_value(gps.altitude.isValid(), gps.altitude.meters(), 1) + " m";
  lines[line_count++] = "Sat : " + format_uint_value(gps.satellites.isValid(), gps.satellites.value()) +
                        "  HDOP:" + format_float_value(gps.hdop.isValid(), gps.hdop.hdop(), 1);
  lines[line_count++] = "Spd : " + format_float_value(gps.speed.isValid(), gps.speed.kmph(), 1) + " km/h";
  lines[line_count++] = "Crs : " + format_float_value(gps.course.isValid(), gps.course.deg(), 1) + " deg";
  lines[line_count++] = "UTC : " + format_date_time();
  lines[line_count++] = "GPS : " + log_status_text() + " " + String(static_cast<unsigned long>(g_logged_sentence_count));
  lines[line_count++] = "LoRa: " + String(g_radio_ready ? g_lora_status.c_str() : "OFF");
  lines[line_count++] = "Cnt : G" + String(static_cast<unsigned long>(g_logged_sentence_count)) + " L" +
                        String(static_cast<unsigned long>(g_logged_lora_count)) + " B:" +
                        String(g_beep_enabled ? "ON" : "OFF");
  lines[line_count++] = "JCnt: " + String(static_cast<unsigned long>(g_logged_json_count));
  lines[line_count++] = "Mode: " + g_gnss_mode;
  lines[line_count++] = "Ver : " + fit_text(g_gnss_version, 24);
  lines[line_count++] = "WiFi: " + String(g_wifi_scan_count) + " BT:OK";
  lines[line_count++] = "GFil: " + log_path_label(g_gps_log_path);
  lines[line_count++] = "LFil: " + log_path_label(g_lora_log_path);
  lines[line_count++] = "JFil: " + log_path_label(g_jsonl_log_path);

  const size_t max_scroll = max_text_scroll_offset(line_count);
  if (g_text_scroll_offset > max_scroll) {
    g_text_scroll_offset = max_scroll;
  }

  const size_t visible_lines = visible_text_line_count();
  for (size_t i = 0; i < visible_lines; ++i) {
    const size_t line_index = g_text_scroll_offset + i;
    if (line_index >= line_count) {
      break;
    }
    display.setCursor(0, text_mode_config::TOP_Y + static_cast<int>(i) * text_mode_config::LINE_HEIGHT);
    display.print(lines[line_index]);
  }

  const int scrollbar_bottom = text_mode_config::TOP_Y + static_cast<int>(visible_lines) * text_mode_config::LINE_HEIGHT - 1;
  draw_content_scrollbar(text_mode_config::TOP_Y, scrollbar_bottom, line_count, visible_lines, g_text_scroll_offset);
}

void draw_satellite_mode() {
  auto& display = g_canvas;
  draw_common_frame("Satellites");
  const int plot_center_x = 182;
  const int plot_center_y = 70;
  const int plot_radius = 42;
  const uint32_t now = millis();
  int vis_gps = 0;
  int vis_gln = 0;
  int vis_gal = 0;
  int vis_bds = 0;
  int vis_qzs = 0;

  display.drawCircle(plot_center_x, plot_center_y, plot_radius, TFT_DARKGREY);
  display.drawCircle(plot_center_x, plot_center_y, plot_radius * 2 / 3, TFT_DARKGREY);
  display.drawCircle(plot_center_x, plot_center_y, plot_radius / 3, TFT_DARKGREY);
  display.drawLine(plot_center_x - plot_radius, plot_center_y, plot_center_x + plot_radius, plot_center_y, TFT_DARKGREY);
  display.drawLine(plot_center_x, plot_center_y - plot_radius, plot_center_x, plot_center_y + plot_radius, TFT_DARKGREY);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(plot_center_x - 3, plot_center_y - plot_radius - 10);
  display.print("N");
  display.setCursor(plot_center_x - 3, plot_center_y + plot_radius + 2);
  display.print("S");
  display.setCursor(plot_center_x - plot_radius - 10, plot_center_y - 3);
  display.print("W");
  display.setCursor(plot_center_x + plot_radius + 4, plot_center_y - 3);
  display.print("E");

  for (size_t i = 0; i < g_tracked_satellite_count; ++i) {
    const TrackedSatellite& sat = g_tracked_satellites[i];
    if (now - sat.last_seen_ms > gps_config::SATELLITE_TIMEOUT_MS || sat.elevation < 0 || sat.azimuth < 0) {
      continue;
    }

    switch (sat.system) {
      case 'P':
        ++vis_gps;
        break;
      case 'L':
        ++vis_gln;
        break;
      case 'A':
        ++vis_gal;
        break;
      case 'B':
        ++vis_bds;
        break;
      case 'Q':
        ++vis_qzs;
        break;
    }

    const float angle = (sat.azimuth - 90.0f) * DEG_TO_RAD;
    const float radius = (90.0f - sat.elevation) * plot_radius / 90.0f;
    const int x = plot_center_x + static_cast<int>(cosf(angle) * radius);
    const int y = plot_center_y + static_cast<int>(sinf(angle) * radius);
    const uint16_t color = satellite_system_color(sat.system);
    display.fillCircle(x, y, 4, color);
    display.drawCircle(x, y, 5, TFT_BLACK);
    if (sat.id > 0) {
      display.setTextColor(color, TFT_BLACK);
      display.setCursor(x + 6, y - 3);
      display.print(sat.id);
    }
  }

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(0, 22);
  display.printf("Fix : %s\n", has_gps_fix() ? "YES" : "NO");
  display.printf("Used: %s\n", format_uint_value(gps.satellites.isValid(), gps.satellites.value()).c_str());
  display.printf("Vis : %d\n", visible_tracked_satellite_count());
  display.printf("Mode: %s\n", g_gnss_mode.c_str());
  display.printf("HDOP: %s\n", format_float_value(gps.hdop.isValid(), gps.hdop.hdop(), 1).c_str());
  display.printf("Spd : %s\n", format_float_value(gps.speed.isValid(), gps.speed.kmph(), 1).c_str());
  display.printf("Alt : %s\n", format_float_value(gps.altitude.isValid(), gps.altitude.meters(), 0).c_str());
  display.printf("UTC : %s\n", format_date_time().c_str());

  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.setCursor(138, 100);
  display.printf("P%02d", vis_gps);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(170, 100);
  display.printf("L%02d", vis_gln);
  display.setTextColor(0x54BF, TFT_BLACK);
  display.setCursor(202, 100);
  display.printf("A%02d", vis_gal);
  display.setTextColor(TFT_ORANGE, TFT_BLACK);
  display.setCursor(138, 110);
  display.printf("B%02d", vis_bds);
  display.setTextColor(TFT_MAGENTA, TFT_BLACK);
  display.setCursor(170, 110);
  display.printf("Q%02d", vis_qzs);
}

void draw_signal_bars_mode() {
  auto& display = g_canvas;
  draw_common_frame("Signal");

  const int chart_x = 36;
  const int chart_y = 28;
  const int chart_w = 194;
  const int bar_h = 11;
  const int bar_gap = 2;
  const int max_bars = 7;
  int selected_indices[48];
  const size_t total_count = collect_signal_satellite_indices(selected_indices, sizeof(selected_indices) / sizeof(selected_indices[0]));
  const size_t visible_count = total_count > static_cast<size_t>(max_bars) ? static_cast<size_t>(max_bars) : total_count;
  const size_t max_scroll = total_count > visible_count ? total_count - visible_count : 0;
  if (g_signal_scroll_offset > max_scroll) {
    g_signal_scroll_offset = max_scroll;
  }

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(0, 22);
  display.printf("Vis:%d Used:%s",
                 visible_tracked_satellite_count(),
                 format_uint_value(gps.satellites.isValid(), gps.satellites.value()).c_str());

  if (total_count == 0) {
    display.setCursor(48, 60);
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.print("No SNR data yet");
    return;
  }

  for (size_t row = 0; row < visible_count; ++row) {
    const TrackedSatellite& sat = g_tracked_satellites[selected_indices[g_signal_scroll_offset + row]];
    const int y = chart_y + row * (bar_h + bar_gap);
    const int value_w = sat.snr > 0 ? (chart_w * sat.snr) / 60 : 0;
    const uint16_t color = satellite_system_color(sat.system);

    display.setTextColor(color, TFT_BLACK);
    display.setCursor(0, y + 2);
    display.printf("%s%02u", satellite_system_label(sat.system), static_cast<unsigned>(sat.id));
    display.drawRect(chart_x, y, chart_w, bar_h, TFT_DARKGREY);
    if (value_w > 0) {
      display.fillRect(chart_x + 1, y + 1, value_w - 2 > 0 ? value_w - 2 : 1, bar_h - 2, color);
    }
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(chart_x + chart_w - 24, y + 2);
    display.printf("%2d", sat.snr);
  }

  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setCursor(chart_x, 118);
  display.print("0");
  display.setCursor(chart_x + chart_w / 2 - 6, 118);
  display.print("30");
  display.setCursor(chart_x + chart_w - 12, 118);
  display.print("60");
  draw_vertical_scrollbar(235, chart_y, max_bars * (bar_h + bar_gap) - bar_gap, total_count, visible_count, g_signal_scroll_offset);
}

void draw_wireless_mode() {
  auto& display = g_canvas;
  draw_common_frame("Wireless");

  const int chart_x = 86;
  const int chart_w = 126;
  const int row_h = wireless_mode_config::ROW_HEIGHT;
  const int row_gap = wireless_mode_config::ROW_GAP;
  const size_t visible_count = wireless_mode_config::VISIBLE_NETWORKS;
  const size_t max_scroll = g_wifi_scan_count > visible_count ? g_wifi_scan_count - visible_count : 0;
  if (g_wireless_scroll_offset > max_scroll) {
    g_wireless_scroll_offset = max_scroll;
  }

  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(0, wireless_mode_config::INFO_TOP_Y);
  display.printf("WiFi:%u AP  BT:%s",
                 static_cast<unsigned>(g_wifi_scan_count),
                 bluetooth_mac_address().length() > 0 ? "MAC" : "N/A");
  display.setCursor(0, wireless_mode_config::INFO_TOP_Y + text_mode_config::LINE_HEIGHT);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.print(fit_text(bluetooth_mac_address().length() > 0 ? bluetooth_mac_address() : "BT scan unavailable", 30));

  if (g_wifi_scan_count == 0) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.setCursor(34, 68);
    display.print("No WiFi scan yet");
    return;
  }

  for (size_t row = 0; row < visible_count; ++row) {
    const size_t index = g_wireless_scroll_offset + row;
    if (index >= g_wifi_scan_count) {
      break;
    }

    const WifiScanResult& ap = g_wifi_scan_results[index];
    const int y = wireless_mode_config::LIST_TOP_Y + static_cast<int>(row) * (row_h + row_gap);
    const int clamped_rssi = ap.rssi < -100 ? -100 : (ap.rssi > -30 ? -30 : ap.rssi);
    const int value_w = ((clamped_rssi + 100) * chart_w) / 70;
    const uint16_t color = wifi_signal_color(ap.rssi);
    const String ssid = ap.ssid.length() > 0 ? ap.ssid : "(hidden)";

    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(0, y + 2);
    display.print(fit_text(ssid, 11));
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.setCursor(66, y + 2);
    display.printf("C%02ld", static_cast<long>(ap.channel));
    display.drawRect(chart_x, y, chart_w, row_h, TFT_DARKGREY);
    if (value_w > 0) {
      display.fillRect(chart_x + 1, y + 1, value_w > chart_w - 2 ? chart_w - 2 : value_w, row_h - 2, color);
    }
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(chart_x + chart_w - 30, y + 2);
    display.printf("%4ld", static_cast<long>(ap.rssi));
  }

  draw_vertical_scrollbar(235,
                          wireless_mode_config::LIST_TOP_Y,
                          static_cast<int>(visible_count) * (row_h + row_gap) - row_gap,
                          g_wifi_scan_count,
                          visible_count,
                          g_wireless_scroll_offset);
}

void draw_bluetooth_mode() {
  auto& display = g_canvas;
  draw_common_frame("Bluetooth");

  const size_t visible_count = wireless_mode_config::VISIBLE_BLUETOOTH;
  const size_t max_scroll = g_bluetooth_scan_count > visible_count ? g_bluetooth_scan_count - visible_count : 0;
  if (g_bluetooth_scroll_offset > max_scroll) {
    g_bluetooth_scroll_offset = max_scroll;
  }

  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(0, wireless_mode_config::INFO_TOP_Y);
  display.printf("BLE:%u %s",
                 static_cast<unsigned>(g_bluetooth_scan_count),
                 g_ble_scan_active ? "Scanning" : "Idle");
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setCursor(0, wireless_mode_config::INFO_TOP_Y + text_mode_config::LINE_HEIGHT);
  display.print(fit_text(bluetooth_mac_address(), 30));

  if (g_bluetooth_scan_count == 0) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.setCursor(26, 66);
    display.print(g_ble_ready ? "No BLE devices yet" : "BLE unavailable");
    return;
  }

  for (size_t row = 0; row < visible_count; ++row) {
    const size_t index = g_bluetooth_scroll_offset + row;
    if (index >= g_bluetooth_scan_count) {
      break;
    }

    const BluetoothScanResult& device = g_bluetooth_scan_results[index];
    const int y = wireless_mode_config::BLUETOOTH_LIST_TOP_Y + static_cast<int>(row) *
                                                              (wireless_mode_config::ROW_HEIGHT + wireless_mode_config::ROW_GAP);
    const uint16_t color = wifi_signal_color(device.rssi);
    const String label = device.name.length() > 0 ? device.name : device.address;
    const String suffix = device.address.length() > 8 ? device.address.substring(device.address.length() - 8) : device.address;

    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(0, y + 2);
    display.print(fit_text(label, 12));
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.setCursor(98, y + 2);
    display.print(suffix);
    display.setTextColor(color, TFT_BLACK);
    display.setCursor(206, y + 2);
    display.printf("%4ld", static_cast<long>(device.rssi));
  }

  draw_vertical_scrollbar(235,
                          wireless_mode_config::BLUETOOTH_LIST_TOP_Y,
                          static_cast<int>(visible_count) * (wireless_mode_config::ROW_HEIGHT + wireless_mode_config::ROW_GAP) -
                              wireless_mode_config::ROW_GAP,
                          g_bluetooth_scan_count,
                          visible_count,
                          g_bluetooth_scroll_offset);
}

void draw_compass_mode() {
  auto& display = g_canvas;
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
  auto& display = g_canvas;
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
  auto& display = g_canvas;
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
  auto& display = g_canvas;
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
    const size_t total_lines = g_explorer_top_line + g_explorer_file_line_count + (g_explorer_file_has_more ? 1 : 0);
    draw_content_scrollbar(22, display.height() - 24, total_lines, g_explorer_file_line_count, g_explorer_top_line);
    return;
  }

  display.printf("%s\n", fit_text(g_explorer_path, explorer_config::PATH_LABEL_LEN).c_str());
  if (g_explorer_entry_count == 0) {
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.print("(no entries)");
    return;
  }

  const size_t visible_entries = visible_explorer_entries();
  for (size_t row = 0; row < visible_entries; ++row) {
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

  draw_content_scrollbar(22, display.height() - 24, g_explorer_entry_count, visible_entries, g_explorer_scroll_offset);
}

void draw_ui() {
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
        case DisplayMode::SignalBars:
          draw_signal_bars_mode();
          break;
        case DisplayMode::Wireless:
          draw_wireless_mode();
          break;
        case DisplayMode::Bluetooth:
          draw_bluetooth_mode();
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
  M5Cardputer.Display.startWrite();
  g_canvas.pushSprite(0, 0);
  M5Cardputer.Display.endWrite();
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  M5Cardputer.begin(cfg, true);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextFont(1);
  M5Cardputer.Display.setTextSize(1);
  g_canvas.setColorDepth(16);
  g_canvas.createSprite(M5Cardputer.Display.width(), M5Cardputer.Display.height());
  g_canvas.setTextFont(1);
  g_canvas.setTextSize(1);
#if BUILD_ENABLE_WIFI_SCAN
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect(false, false);
  WiFi.setSleep(false);
#endif
#if BUILD_ENABLE_BLE_SCAN
  g_ble_ready = init_bluetooth_scanner();
#else
  g_ble_ready = false;
#endif

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
    refresh_gnss_info();
    handle_radio_events();

    bool received_data = false;
    while (GPSSerial.available() > 0) {
      const char c = static_cast<char>(GPSSerial.read());
      handle_gnss_serial_char(c);
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

     refresh_wireless_scans();
     append_jsonl_snapshot_if_needed();

     if (M5Cardputer.BtnA.wasClicked()) {
       g_display_mode = next_mode(g_display_mode);
       g_text_scroll_offset = 0;
       g_signal_scroll_offset = 0;
       g_wireless_scroll_offset = 0;
       g_bluetooth_scroll_offset = 0;
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
