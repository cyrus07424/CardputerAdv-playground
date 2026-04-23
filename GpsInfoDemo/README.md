# GpsInfoDemo

Cardputer-ADV と Cap LoRa-1262 を使って、内蔵 GNSS の情報を画面に表示する Arduino デモです。

## 必要なライブラリ

- M5Cardputer
- TinyGPSPlus

## Arduino IDE 設定

- 開くファイル: `GpsInfoDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 概要

- Cap LoRa-1262 の GNSS を UART (`RX=15`, `TX=13`, `115200bps`) で受信
- 緯度・経度・高度・衛星数・HDOP・速度・方位・UTC 時刻を表示
- **Text / Satellites / Compass / Speed** の表示モードを切り替え可能
- 右上に Cardputer-ADV のバッテリー残量を常時表示
- GPS fix が取れていて microSD が挿さっていれば、標準的な **NMEA ログ (`.nmea`)** を自動保存
- LoRa 無線機能は使わず、GPS 表示に絞ったシンプルなサンプル

## 使い方

1. Arduino IDE で `GpsInfoDemo.ino` を開く
2. ボードを `m5stack:esp32:m5stack_cardputer` に設定する
3. ライブラリマネージャで `M5Cardputer` と `TinyGPSPlus` をインストールする
4. 書き込んで、GPS を受信しやすい場所で起動する
5. BtnA を押して表示モードを切り替える
6. microSD が挿さっていて GPS fix が有効なら、ログが自動で保存される

## 配線 / 前提

- Cardputer-ADV に Cap LoRa-1262 を装着
- このデモは Cap 内蔵 GNSS を使用し、LoRa 無線機能は使いません
- GNSS UART は `RX=15`, `TX=13`, `115200bps`
- microSD は FAT32 で使用してください

## メモ

- Compass モードは GPS の進行方位を使って描画します
- Speed モードは km/h のメーター表示です
- Cardputer / Cardputer-ADV はハードウェア制約により実際の充電状態を読めないため、充電状態表示は `N/A` になります
- ログファイルは `/gps_YYYYMMDD_HHMMSS.nmea` 形式で保存し、内容は生の NMEA 文です
- GPS fix が有効で、microSD が使用可能な間だけ NMEA 文を追記します
- `TinyGPSPlus` は Arduino IDE のライブラリマネージャからインストールしてください
- Cap LoRa-1262 の GNSS は NMEA を UART で出力します
