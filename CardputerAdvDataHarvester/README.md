# CardputerAdvDataHarvester

Cardputer-ADV と Cap LoRa-1262 を使って、GNSS と LoRa 受信内容を常時 microSD に収集する Arduino デモです。

## 必要なライブラリ

- M5Cardputer
- TinyGPSPlus
- RadioLib

## Arduino IDE 設定

- 開くファイル: `CardputerAdvDataHarvester.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 概要

- Cap LoRa-1262 の GNSS を UART (`RX=15`, `TX=13`, `115200bps`) で受信
- 緯度・経度・高度・衛星数・HDOP・速度・方位・UTC 時刻を表示
- **Text / Satellites / Compass / Speed** の表示モードを切り替え可能
- 右上に Cardputer-ADV のバッテリー残量を常時表示
- GNSS から受信した NMEA 文を、fix の有無に関係なく microSD に継続保存
- LoRa で受信したメッセージを、受信時刻付きで microSD に継続保存

## 使い方

1. Arduino IDE で `CardputerAdvDataHarvester.ino` を開く
2. ボードを `m5stack:esp32:m5stack_cardputer` に設定する
3. ライブラリマネージャで `M5Cardputer`、`TinyGPSPlus`、`RadioLib` をインストールする
4. microSD を挿した状態で書き込み、GPS を受信しやすい場所で起動する
5. BtnA を押して表示モードを切り替える
6. 起動後は GNSS の NMEA 文と LoRa 受信内容が自動で保存される

## 配線 / 前提

- Cardputer-ADV に Cap LoRa-1262 を装着
- このデモは Cap 内蔵 GNSS と LoRa 無線の両方を使用します
- GNSS UART は `RX=15`, `TX=13`, `115200bps`
- microSD は FAT32 で使用してください

## メモ

- Compass モードは GPS の進行方位を使って描画します
- Speed モードは km/h のメーター表示です
- Cardputer / Cardputer-ADV はハードウェア制約により実際の充電状態を読めないため、充電状態表示は `N/A` になります
- GPS ログは `/gps_YYYYMMDD_HHMMSS.nmea` 形式で保存し、内容は生の NMEA 文です
- LoRa ログは `/lora_YYYYMMDD_HHMMSS.log` 形式で保存し、`YYYY-MM-DD HH:MM:SS RX> message` 形式で追記します
- GPS 時刻がまだ有効でない間の LoRa ログ時刻は `millis=...` で記録します
- `TinyGPSPlus` と `RadioLib` は Arduino IDE のライブラリマネージャからインストールしてください
- Cap LoRa-1262 の GNSS は NMEA を UART で出力します
