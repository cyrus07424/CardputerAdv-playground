# CardputerAdvGpsInfoDemo

Cardputer-ADV と Cap LoRa-1262 を使って、内蔵 GNSS の情報を画面に表示する Arduino デモです。

## 必要なライブラリ

- M5Cardputer
- TinyGPSPlus

## Arduino IDE 設定

- 開くファイル: `CardputerAdvGpsInfoDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 概要

- Cap LoRa-1262 の GNSS を UART (`RX=15`, `TX=13`, `115200bps`) で受信
- 緯度・経度・高度・衛星数・HDOP・速度・方位・UTC 時刻を表示
- LoRa 無線機能は使わず、GPS 表示に絞ったシンプルなサンプル

## 使い方

1. Arduino IDE で `CardputerAdvGpsInfoDemo.ino` を開く
2. ボードを `m5stack:esp32:m5stack_cardputer` に設定する
3. ライブラリマネージャで `M5Cardputer` と `TinyGPSPlus` をインストールする
4. 書き込んで、GPS を受信しやすい場所で起動する

## 配線 / 前提

- Cardputer-ADV に Cap LoRa-1262 を装着
- このデモは Cap 内蔵 GNSS を使用し、LoRa 無線機能は使いません
- GNSS UART は `RX=15`, `TX=13`, `115200bps`

## メモ

- BtnA を押すと再描画します
- `TinyGPSPlus` は Arduino IDE のライブラリマネージャからインストールしてください
- Cap LoRa-1262 の GNSS は NMEA を UART で出力します
