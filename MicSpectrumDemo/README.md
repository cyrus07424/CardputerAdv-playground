# MicSpectrumDemo

Cardputer-ADV の内蔵マイクで拾った音を周波数解析し、リアルタイムのスペクトルバーとして表示するデモです。

## 必要なライブラリ

- M5Cardputer

## Arduino IDE 設定

- 開くファイル: `MicSpectrumDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 概要

- Cardputer-ADV の内蔵マイクを 17kHz / 128 サンプルで取得
- ハニング窓を掛けた後に周波数成分を算出し、80Hz〜3.6kHz を 24 本のバーで表示
- もっとも強い周波数を `PEAK` として上部に表示
- 入力レベルメーターとピークホールド表示を下部に表示

## 使い方

1. Arduino IDE で `MicSpectrumDemo.ino` を開く
2. ボードを `m5stack:esp32:m5stack_cardputer` に設定する
3. ライブラリマネージャで `M5Cardputer` をインストールする
4. 書き込んで本体に向かって音を入れる
5. スペクトルバーと `PEAK` 周波数の変化を見る

## 操作

- `BtnA` または `Enter`: ライブ更新 / ホールドを切り替え
- `Backspace`: ピークホールド表示をリセット

## 配線 / 前提

- Cardputer-ADV 単体で動作します
- 内蔵マイクが有効な `M5Cardputer` ライブラリ環境を前提にしています

## トラブルシュート

- リセット直後は何も操作しなくても自動で録音が始まる想定です
- 画面右上の数値は `ピーク周波数 / 生サンプルの最大振幅` です。後ろの値がほぼ増えない場合は、マイク入力自体が取得できていません
- Cardputer-ADV の ES8311 マイク系は、環境によっては Arduino 側の下層 `ESP-IDF` / `M5Unified` 系の既知不具合で無音になる報告があります。今回の確認では **M5Stack board package 3.2.x** にするとマイクが動作しました
- スケッチが正しくても反応しない場合は、`M5Cardputer` / `M5Unified` / ESP32 ボードパッケージの組み合わせを見直し、まず **M5Stack board package 3.2.x** を試してください
