# CardputerAdvLevelDemo

Cardputer-ADV の内蔵 IMU を使った 2 軸水平器です。黒系 TKL キーボードをイメージしたパネル上に、左右傾きと前後傾きを泡表示で描きます。

## 必要なライブラリ

- M5Cardputer

## Arduino IDE 設定

- 開くファイル: `CardputerAdvLevelDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 概要

- Cardputer-ADV の内蔵 IMU を `M5.Imu` 経由で読み取り
- 加速度とジャイロを補完フィルタで合成し、左右 (`ROLL`) / 前後 (`PITCH`) の傾きを表示
- 黒系キーボード風の UI で、水平に近いほど泡色とステータスが落ち着いた表示に変化
- 右上にバッテリー残量と充電状態を常時表示

## 使い方

1. Arduino IDE で `CardputerAdvLevelDemo.ino` を開く
2. ボードを `m5stack:esp32:m5stack_cardputer` に設定する
3. ライブラリマネージャで `M5Cardputer` をインストールする
4. 書き込んで本体を平らな場所に置く
5. `BtnA` を押して現在の姿勢をゼロ点に合わせる
6. 本体を傾けて `ROLL` / `PITCH` / `TILT` の変化を見る

## 配線 / 前提

- Cardputer-ADV 単体で動作します
- Cardputer-ADV の内蔵 IMU が有効な環境を前提にしています
