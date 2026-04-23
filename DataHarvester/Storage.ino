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
  g_explorer_file_total_lines = 0;
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