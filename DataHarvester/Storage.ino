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
  g_wigle_log_path = "";
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

String build_wigle_log_path() {
  return build_log_path(log_dir_config::WIGLE, "harvest", ".csv");
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

String wigle_csv_escape(const String& text) {
  bool needs_quotes = false;
  String escaped;
  escaped.reserve(text.length() + 4);

  for (size_t i = 0; i < text.length(); ++i) {
    const char c = text[i];
    if (c == '"' || c == ',' || c == '\n' || c == '\r') {
      needs_quotes = true;
    }
    if (c == '"') {
      escaped += "\"\"";
    } else if (c != '\r') {
      escaped += c;
    }
  }

  if (!needs_quotes) {
    return escaped;
  }
  return "\"" + escaped + "\"";
}

String wigle_timestamp() {
  if (!gps.date.isValid() || !gps.time.isValid()) {
    return "";
  }

  char buffer[24];
  snprintf(buffer,
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

bool wigle_has_observation_fix() {
  return gps.location.isValid() && gps.date.isValid() && gps.time.isValid();
}

String wigle_latitude() {
  return gps.location.isValid() ? String(gps.location.lat(), 8) : "";
}

String wigle_longitude() {
  return gps.location.isValid() ? String(gps.location.lng(), 8) : "";
}

String wigle_altitude_meters() {
  return gps.altitude.isValid() ? String(static_cast<long>(lround(gps.altitude.meters()))) : "";
}

String wigle_accuracy_meters() {
  if (!gps.hdop.isValid()) {
    return "";
  }
  return String(gps.hdop.hdop() * 5.0, 1);
}

int32_t wifi_frequency_mhz(int32_t channel) {
  if (channel == 14) {
    return 2484;
  }
  if (channel >= 1 && channel <= 13) {
    return 2407 + channel * 5;
  }
  if (channel >= 32) {
    return 5000 + channel * 5;
  }
  return 0;
}

String wigle_wifi_capabilities(wifi_auth_mode_t encryption) {
  switch (encryption) {
    case WIFI_AUTH_OPEN:
      return "[ESS]";
    case WIFI_AUTH_WEP:
      return "[WEP][ESS]";
    case WIFI_AUTH_WPA_PSK:
      return "[WPA-PSK][ESS]";
    case WIFI_AUTH_WPA2_PSK:
      return "[WPA2-PSK][ESS]";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "[WPA-PSK][WPA2-PSK][ESS]";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "[WPA2-EAP][ESS]";
    case WIFI_AUTH_WPA3_PSK:
      return "[WPA3-PSK][ESS]";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "[WPA2-PSK][WPA3-PSK][ESS]";
    case WIFI_AUTH_WAPI_PSK:
      return "[WAPI-PSK][ESS]";
    default:
      return "[ESS]";
  }
}

String wigle_preheader_line() {
  return "WigleWifi-1.6,appRelease=DataHarvester,model=" + String(ESP.getChipModel()) +
         ",release=" + String(ESP.getSdkVersion()) +
         ",device=CardputerADV,display=240x135,board=m5stack_cardputer,brand=M5Stack,star=Sol,body=3,subBody=0";
}

String wigle_header_line() {
  return "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type";
}

bool append_wigle_line(const String& line) {
  const size_t expected_bytes = line.length() + 2;
  size_t written_bytes = g_wigle_log_file.print(line);
  written_bytes += g_wigle_log_file.print("\r\n");
  g_wigle_log_file.flush();
  if (written_bytes < expected_bytes) {
    mark_sd_unavailable();
    return false;
  }
  return true;
}

bool open_wigle_log_file_if_needed() {
#if BUILD_ENABLE_WIGLE_LOG
  if (!g_sd_ready) {
    return false;
  }
  if (g_wigle_log_file) {
    return true;
  }
  if (!ensure_log_directory(log_dir_config::WIGLE)) {
    mark_sd_unavailable();
    return false;
  }

  g_wigle_log_path = build_wigle_log_path();
  g_wigle_log_file = SD.open(g_wigle_log_path.c_str(), FILE_APPEND);
  if (!g_wigle_log_file) {
    mark_sd_unavailable();
    return false;
  }

  if (g_wigle_log_file.size() == 0) {
    if (!append_wigle_line(wigle_preheader_line()) || !append_wigle_line(wigle_header_line())) {
      return false;
    }
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

#if BUILD_ENABLE_WIGLE_LOG
  if (g_wifi_scan_count > 0 && wigle_has_observation_fix() && open_wigle_log_file_if_needed()) {
    const String timestamp = wigle_timestamp();
    const String latitude = wigle_latitude();
    const String longitude = wigle_longitude();
    const String altitude = wigle_altitude_meters();
    const String accuracy = wigle_accuracy_meters();

    for (size_t i = 0; i < g_wifi_scan_count; ++i) {
      const WifiScanResult& ap = g_wifi_scan_results[i];
      String line = wigle_csv_escape(ap.bssid);
      line += "," + wigle_csv_escape(ap.ssid);
      line += "," + wigle_csv_escape(wigle_wifi_capabilities(ap.encryption));
      line += "," + wigle_csv_escape(timestamp);
      line += "," + String(static_cast<long>(ap.channel));
      const int32_t frequency = wifi_frequency_mhz(ap.channel);
      line += "," + String(static_cast<long>(frequency));
      line += "," + String(static_cast<long>(ap.rssi));
      line += "," + latitude;
      line += "," + longitude;
      line += "," + altitude;
      line += "," + accuracy;
      line += ",,";
      line += ",WIFI";
      if (!append_wigle_line(line)) {
        return;
      }
      ++g_logged_wigle_count;
    }
    g_needs_redraw = true;
  }
#endif
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

#if BUILD_ENABLE_BLE_SCAN
uint16_t bluetooth_manufacturer_id(const uint8_t* adv_data, uint8_t adv_len, bool* valid) {
  if (valid) {
    *valid = false;
  }
  if (!adv_data || adv_len == 0) {
    return 0;
  }

  size_t offset = 0;
  while (offset + 1 < adv_len) {
    const uint8_t field_len = adv_data[offset];
    if (field_len == 0 || offset + 1 + field_len > adv_len) {
      break;
    }
    const uint8_t field_type = adv_data[offset + 1];
    if (field_type == ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE && field_len >= 3) {
      if (valid) {
        *valid = true;
      }
      return static_cast<uint16_t>(adv_data[offset + 2]) |
             (static_cast<uint16_t>(adv_data[offset + 3]) << 8);
    }
    offset += field_len + 1;
  }
  return 0;
}

void append_bluetooth_scan_results_to_wigle() {
#if BUILD_ENABLE_WIGLE_LOG
  if (g_bluetooth_scan_count == 0 || !wigle_has_observation_fix() || !open_wigle_log_file_if_needed()) {
    return;
  }

  const String timestamp = wigle_timestamp();
  const String latitude = wigle_latitude();
  const String longitude = wigle_longitude();
  const String altitude = wigle_altitude_meters();
  const String accuracy = wigle_accuracy_meters();

  for (size_t i = 0; i < g_bluetooth_scan_count; ++i) {
    const BluetoothScanResult& device = g_bluetooth_scan_results[i];
    String line = wigle_csv_escape(device.address);
    line += "," + wigle_csv_escape(device.name);
    line += "," + wigle_csv_escape("Misc [LE]");
    line += "," + wigle_csv_escape(timestamp);
    line += ",0,";
    line += "," + String(static_cast<long>(device.rssi));
    line += "," + latitude;
    line += "," + longitude;
    line += "," + altitude;
    line += "," + accuracy;
    line += ",";
    if (device.has_manufacturer_id) {
      line += "," + String(static_cast<unsigned long>(device.manufacturer_id));
    } else {
      line += ",";
    }
    line += ",BLE";
    if (!append_wigle_line(line)) {
      return;
    }
    ++g_logged_wigle_count;
  }

  g_needs_redraw = true;
#endif
}
#endif
