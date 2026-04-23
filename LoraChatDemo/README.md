# LoraChatDemo

Cardputer-ADV と Cap LoRa-1262 を使った、シンプルな LoRa メッセージ送受信デモです。

## 必要なライブラリ

- M5Cardputer
- RadioLib

## Arduino IDE 設定

- 開くファイル: `LoraChatDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 使い方

1. 2 台とも同じ設定で書き込む
2. USB Serial (115200bps) で 1 行入力して Enter を押す
3. 本体の BtnA を押すと `ping N` を送信する
4. 受信メッセージは画面とシリアルに表示される

## LoRa 設定

- 868.0 MHz
- BW 500 kHz
- SF7
- CR 4/5
- SyncWord 0x34
