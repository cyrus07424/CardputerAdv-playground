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

size_t count_explorer_file_visual_lines() {
  if (!g_sd_ready || g_explorer_file_path.length() == 0) {
    return 0;
  }

  File file = SD.open(g_explorer_file_path.c_str(), FILE_READ);
  if (!file || file.isDirectory()) {
    close_file(file);
    return 0;
  }

  size_t visual_line_count = 0;
  size_t current_visual_len = 0;

  while (file.available()) {
    const char c = static_cast<char>(file.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      ++visual_line_count;
      current_visual_len = 0;
      continue;
    }

    ++current_visual_len;
    if (current_visual_len >= explorer_config::LINE_LABEL_LEN) {
      ++visual_line_count;
      current_visual_len = 0;
    }
  }

  close_file(file);

  if (current_visual_len > 0 || visual_line_count == 0) {
    ++visual_line_count;
  }
  return visual_line_count;
}

void load_explorer_file_page() {
  const size_t visible_lines = visible_explorer_file_lines();
  for (size_t i = 0; i < explorer_config::MAX_VISIBLE_LINES; ++i) {
    g_explorer_file_lines[i] = "";
  }
  g_explorer_file_line_count = 0;
  g_explorer_file_has_more = false;

  if (!g_sd_ready || g_explorer_file_path.length() == 0) {
    return;
  }

  if (g_explorer_file_total_lines == 0) {
    g_explorer_file_total_lines = count_explorer_file_visual_lines();
  }

  const size_t max_top_line = g_explorer_file_total_lines > visible_lines ? g_explorer_file_total_lines - visible_lines : 0;
  if (g_explorer_top_line > max_top_line) {
    g_explorer_top_line = max_top_line;
  }

  File file = SD.open(g_explorer_file_path.c_str(), FILE_READ);
  if (!file || file.isDirectory()) {
    close_file(file);
    return;
  }

  uint32_t visual_line_index = 0;
  String current_visual_line;

  auto emit_visual_line = [&](const String& line) -> bool {
    if (visual_line_index++ < g_explorer_top_line) {
      return true;
    }
    if (g_explorer_file_line_count < visible_lines) {
      g_explorer_file_lines[g_explorer_file_line_count++] = line;
      return true;
    }
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
        g_explorer_file_has_more = g_explorer_top_line + g_explorer_file_line_count < g_explorer_file_total_lines;
        return;
      }
      current_visual_line = "";
      continue;
    }

    current_visual_line += (static_cast<unsigned char>(c) < 32 || c == 127) ? '.' : c;
    if (current_visual_line.length() >= explorer_config::LINE_LABEL_LEN) {
      if (!emit_visual_line(current_visual_line)) {
        close_file(file);
        g_explorer_file_has_more = g_explorer_top_line + g_explorer_file_line_count < g_explorer_file_total_lines;
        return;
      }
      current_visual_line = "";
    }
  }

  if (current_visual_line.length() > 0 || visual_line_index == 0) {
    emit_visual_line(current_visual_line);
  }

  close_file(file);
  g_explorer_file_has_more = g_explorer_top_line + g_explorer_file_line_count < g_explorer_file_total_lines;
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
  g_explorer_file_total_lines = count_explorer_file_visual_lines();
  load_explorer_file_page();
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
    display.printf("Ln %lu/%lu\n",
                   static_cast<unsigned long>(g_explorer_top_line + 1),
                   static_cast<unsigned long>(g_explorer_file_total_lines));
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    if (g_explorer_file_line_count == 0) {
      display.print("(empty file)");
      return;
    }
    for (size_t i = 0; i < g_explorer_file_line_count; ++i) {
      display.println(g_explorer_file_lines[i]);
    }
    draw_content_scrollbar(22, display.height() - 24, g_explorer_file_total_lines, visible_explorer_file_lines(), g_explorer_top_line);
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