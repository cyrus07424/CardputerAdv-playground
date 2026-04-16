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
- 起動時に **Menu / Data Harvester / File Explorer / USB Storage** のモード選択画面を表示
- Data Harvester では **Text / Satellites / Compass / Speed** の表示モードを切り替え可能
- 右上に Cardputer-ADV のバッテリー残量を常時表示
- GNSS から受信した NMEA 文を、fix の有無に関係なく microSD に継続保存
- LoRa で受信したメッセージを、受信時刻付きで microSD に継続保存
- LoRa 受信時にビープ音を鳴らし、Fn キーでビープの ON/OFF を切り替え可能
- USB Storage では microSD を PC からリムーバブルディスクとして扱える

## 使い方

1. Arduino IDE で `CardputerAdvDataHarvester.ino` を開く
2. ボードを `m5stack:esp32:m5stack_cardputer` に設定する
3. ライブラリマネージャで `M5Cardputer`、`TinyGPSPlus`、`RadioLib` をインストールする
4. microSD を挿した状態で書き込み、GPS を受信しやすい場所で起動する
5. 起動後のメニューで **Data Harvester / File Explorer / USB Storage** を選ぶ
6. Data Harvester では BtnA で表示モードを切り替え、Fn キーで LoRa 受信ビープ音の ON/OFF を切り替える
7. File Explorer では SD カード内を閲覧できる
8. Explorer と Menu の操作は Cardputer-ADV 本体の **カーソルキー表示のあるキー**（`;` `,` `.` `/` の位置）と `Enter` を使う
9. 各モードから `Esc` / `Backspace` でメニュー画面に戻れる
10. USB Storage を選ぶとロギングを停止し、microSD を USB ストレージとして PC に公開する
11. Data Harvester 起動中のみ GNSS の NMEA 文と LoRa 受信内容が自動で保存される

## 配線 / 前提

- Cardputer-ADV に Cap LoRa-1262 を装着
- このデモは Cap 内蔵 GNSS と LoRa 無線の両方を使用します
- GNSS UART は `RX=15`, `TX=13`, `115200bps`
- microSD は FAT32 で使用してください

## メモ

- Compass モードは GPS の進行方位を使って描画します
- Speed モードは km/h のメーター表示です
- Explorer モードではディレクトリ一覧とテキストファイル内容を画面上で閲覧できます
- USB Storage モード中はファイル破損を避けるため、本体側のロギングを停止します
- Cardputer / Cardputer-ADV はハードウェア制約により実際の充電状態を読めないため、充電状態表示は `N/A` になります
- GPS ログは `/gps_YYYYMMDD_HHMMSS.nmea` 形式で保存し、内容は生の NMEA 文です
- LoRa ログは `/lora_YYYYMMDD_HHMMSS.log` 形式で保存し、`YYYY-MM-DD HH:MM:SS RX> message` 形式で追記します
- GPS 時刻がまだ有効でない間の LoRa ログ時刻は `millis=...` で記録します
- `TinyGPSPlus` と `RadioLib` は Arduino IDE のライブラリマネージャからインストールしてください
- Cap LoRa-1262 の GNSS は NMEA を UART で出力します
