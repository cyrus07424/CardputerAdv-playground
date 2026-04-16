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
      const size_t text_line_count = text_mode_line_count();
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
    const size_t max_scroll = g_explorer_file_total_lines > visible_lines ? g_explorer_file_total_lines - visible_lines : 0;
    if (move_up && g_explorer_top_line > 0) {
      --g_explorer_top_line;
      load_explorer_file_page();
      g_needs_redraw = true;
    } else if (move_down && g_explorer_top_line < max_scroll) {
      ++g_explorer_top_line;
      load_explorer_file_page();
      g_needs_redraw = true;
    } else if (move_left && g_explorer_top_line > 0) {
      g_explorer_top_line = g_explorer_top_line > visible_lines ? g_explorer_top_line - visible_lines : 0;
      load_explorer_file_page();
      g_needs_redraw = true;
    } else if (accept && g_explorer_top_line < max_scroll) {
      g_explorer_top_line += visible_lines;
      if (g_explorer_top_line > max_scroll) {
        g_explorer_top_line = max_scroll;
      }
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