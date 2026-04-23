# Imu3dDemo

Cardputer-ADV の内蔵加速度センサ / ジャイロセンサを読み取り、姿勢を 3D ワイヤーフレームで可視化する Arduino デモです。

## 必要なライブラリ

- M5Cardputer

## Arduino IDE 設定

- 開くファイル: `Imu3dDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 概要

- Cardputer-ADV の内蔵 IMU を `M5.Imu` 経由で読み取り
- 加速度とジャイロを補完フィルタで合成して、画面左に 3D キューブ姿勢を表示
- 赤いベクトルで加速度方向、右側バーでジャイロ角速度を可視化
- 右側に roll / pitch / yaw と加速度ノルムを数値表示
- 右上に Cardputer-ADV のバッテリー残量と充電状態を常時表示

## 使い方

1. Arduino IDE で `Imu3dDemo.ino` を開く
2. ボードを `m5stack:esp32:m5stack_cardputer` に設定する
3. ライブラリマネージャで `M5Cardputer` をインストールする
4. 書き込んで本体を傾けたり回したりして表示の変化を見る
5. BtnA を押すと yaw 表示を 0 度にリセットする

## 配線 / 前提

- Cardputer-ADV 単体で動作します
- Cardputer-ADV の内蔵 IMU が有効な環境を前提にしています

## メモ

- 加速度は重力方向を含むため、静止時は約 `1.00g` を示します
- yaw はジャイロ積分ベースのため、長時間では少しずつドリフトします
- 保存済み IMU オフセットがあれば起動時に `NVS` から読み込みます
