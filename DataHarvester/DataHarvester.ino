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
#ifndef BUILD_ENABLE_WIGLE_LOG
#define BUILD_ENABLE_WIGLE_LOG 1
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
// 4. BUILD_ENABLE_WIGLE_LOG
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
constexpr const char* WIGLE = "/wigle";
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
constexpr size_t VISIBLE_NETWORKS = 6;
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
  uint16_t manufacturer_id = 0;
  bool has_manufacturer_id = false;
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
File g_wigle_log_file;

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
String g_wigle_log_path;
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
uint32_t g_last_wifi_scan_ms = 0;
uint32_t g_last_ble_scan_ms = 0;
uint32_t g_logged_sentence_count = 0;
uint32_t g_logged_lora_count = 0;
uint32_t g_logged_wigle_count = 0;
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
size_t g_explorer_file_total_lines = 0;
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

size_t text_mode_line_count() {
  size_t line_count = 0;
  line_count += 1;  // Lat
  line_count += 1;  // Lng
  line_count += 1;  // Alt
  line_count += 1;  // Sat/HDOP
  line_count += 1;  // Speed
  line_count += 1;  // Course
  line_count += 1;  // UTC
  line_count += 1;  // GPS log status
  line_count += 1;  // LoRa status
  line_count += 1;  // Counters
  line_count += 1;  // WiGLE count
  line_count += 1;  // GNSS mode
  line_count += 1;  // GNSS version
  line_count += 1;  // WiFi / BT
  line_count += 1;  // GPS file
  line_count += 1;  // LoRa file
  line_count += 1;  // WiGLE file
  return line_count;
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
  if (g_gps_log_file || g_lora_log_file || g_wigle_log_file) {
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

void upsert_bluetooth_scan_result(const String& address,
                                  const String& name,
                                  int32_t rssi,
                                  uint16_t manufacturer_id,
                                  bool has_manufacturer_id) {
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
  result.manufacturer_id = manufacturer_id;
  result.has_manufacturer_id = has_manufacturer_id;
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
  g_bluetooth_scan_count = 0;
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
        bool has_manufacturer_id = false;
        upsert_bluetooth_scan_result(
            bluetooth_address_string(param->scan_rst.bda),
            bluetooth_adv_name(param->scan_rst.ble_adv, adv_len),
            param->scan_rst.rssi,
            bluetooth_manufacturer_id(param->scan_rst.ble_adv, adv_len, &has_manufacturer_id),
            has_manufacturer_id);
        g_needs_redraw = true;
      } else if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
        g_ble_scan_active = false;
        g_last_ble_scan_ms = millis();
        append_bluetooth_scan_results_to_wigle();
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
  close_file(g_wigle_log_file);
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
