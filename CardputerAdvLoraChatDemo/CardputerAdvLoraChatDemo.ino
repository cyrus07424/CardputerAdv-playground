#include <Arduino.h>
#include <M5Unified.h>
#include <SPI.h>
#include <RadioLib.h>

namespace board_pins {
constexpr int KEYBOARD_INT = 11;
constexpr int SPI_MISO = 39;
constexpr int SPI_MOSI = 14;
constexpr int SPI_SCLK = 40;
constexpr int LORA_NSS = 5;
constexpr int LORA_RST = 3;
constexpr int LORA_BUSY = 6;
constexpr int LORA_DIO1 = 4;
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
}

SX1262 radio = new Module(
    board_pins::LORA_NSS,
    board_pins::LORA_DIO1,
    board_pins::LORA_RST,
    board_pins::LORA_BUSY,
    SPI);

volatile bool g_rx_flag = false;
volatile bool g_tx_flag = false;

bool g_radio_ready = false;
bool g_needs_redraw = true;
String g_input_line;
String g_last_status = "Booting...";
String g_last_metrics = "-";
uint32_t g_ping_counter = 0;

constexpr size_t LOG_LINE_COUNT = 6;
String g_log_lines[LOG_LINE_COUNT];

void IRAM_ATTR on_radio_rx(void) {
  g_rx_flag = true;
}

void IRAM_ATTR on_radio_tx(void) {
  g_tx_flag = true;
}

void push_log(const String& line) {
  for (size_t i = 0; i + 1 < LOG_LINE_COUNT; ++i) {
    g_log_lines[i] = g_log_lines[i + 1];
  }
  g_log_lines[LOG_LINE_COUNT - 1] = line;
  g_needs_redraw = true;
  Serial.println(line);
}

void draw_ui() {
  auto& display = M5.Display;
  display.startWrite();
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Cardputer ADV LoRa Chat");
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.println("USB Serial line -> send");
  display.println("BtnA -> send ping");
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.printf("Status: %s\n", g_last_status.c_str());
  display.printf("Radio : %s\n", g_last_metrics.c_str());
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.printf("Input : %s\n", g_input_line.c_str());
  display.println("------------------------------");
  for (const auto& line : g_log_lines) {
    display.println(line);
  }
  display.endWrite();
}

void start_receive() {
  radio.setPacketReceivedAction(on_radio_rx);
  int16_t state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    g_last_status = "Listening";
    g_needs_redraw = true;
  } else {
    g_last_status = "RX error " + String(state);
    push_log("RX start failed: " + String(state));
  }
}

bool init_radio() {
  SPI.begin(
      board_pins::SPI_SCLK,
      board_pins::SPI_MISO,
      board_pins::SPI_MOSI,
      board_pins::LORA_NSS);

  int16_t state = radio.begin(
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
    g_last_status = "Init error " + String(state);
    push_log("LoRa init failed: " + String(state));
    return false;
  }

  radio.setDio2AsRfSwitch(true);
  radio.setCurrentLimit(140);
  g_last_metrics = "868MHz BW500 SF7";
  push_log("LoRa ready");
  start_receive();
  return true;
}

void send_message(const String& message) {
  if (!g_radio_ready || message.length() == 0) {
    return;
  }

  String tx_message = message;
  radio.setPacketSentAction(on_radio_tx);
  int16_t state = radio.startTransmit(tx_message);
  if (state == RADIOLIB_ERR_NONE) {
    g_last_status = "Sending";
    g_needs_redraw = true;
    push_log("TX> " + message);
  } else {
    g_last_status = "TX error " + String(state);
    push_log("TX failed: " + String(state));
    start_receive();
  }
}

void handle_radio_events() {
  if (g_rx_flag) {
    g_rx_flag = false;

    String incoming;
    int16_t state = radio.readData(incoming);
    if (state == RADIOLIB_ERR_NONE) {
      g_last_status = "Message received";
      g_last_metrics = "RSSI " + String(radio.getRSSI(), 1) + " dBm / SNR " + String(radio.getSNR(), 1) + " dB";
      g_needs_redraw = true;
      push_log("RX> " + incoming);
    } else {
      g_last_status = "RX read error " + String(state);
      push_log("RX read failed: " + String(state));
    }
    start_receive();
  }

  if (g_tx_flag) {
    g_tx_flag = false;
    g_last_status = "Sent";
    g_needs_redraw = true;
    start_receive();
  }
}

void handle_serial_input() {
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      String message = g_input_line;
      g_input_line = "";
      message.trim();
      send_message(message);
      continue;
    }
    if ((c == '\b' || c == 127) && g_input_line.length() > 0) {
      g_input_line.remove(g_input_line.length() - 1);
      g_needs_redraw = true;
      continue;
    }
    if (c >= 32 && c <= 126 && g_input_line.length() < 48) {
      g_input_line += c;
      g_needs_redraw = true;
    }
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.clear_display = true;
  cfg.output_power = true;
  M5.begin(cfg);

  M5.Display.setRotation(1);
  M5.Display.setTextSize(1);
  M5.Display.setTextFont(1);

  push_log("Booting");
  push_log("Open Serial @115200");

  g_radio_ready = init_radio();
  if (!g_radio_ready) {
    push_log("Check LoRa Cap wiring");
  }

  draw_ui();
  g_needs_redraw = false;
}

void loop() {
  M5.update();

  handle_serial_input();
  handle_radio_events();

  if (M5.BtnA.wasClicked()) {
    ++g_ping_counter;
    send_message("ping " + String(g_ping_counter));
  }

  if (g_needs_redraw) {
    draw_ui();
    g_needs_redraw = false;
  }
}
