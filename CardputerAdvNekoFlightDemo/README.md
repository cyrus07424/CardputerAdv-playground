# CardputerAdvNekoFlightDemo

`NekoFlight` を Cardputer-ADV 向けに Arduino/C++ へ移植した 3D フライトデモです。元の `Plane` / `Wing` / `Bullet` / `Missile` の挙動をベースに、Cardputer-ADV の画面とキーボードで遊べるようにしています。

## 必要なライブラリ

- M5Cardputer

## Arduino IDE 設定

- 開くファイル: `CardputerAdvNekoFlightDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 操作

1. `A` で射撃 / ミサイル
2. カーソルキー表示のあるキー（`;` `,` `.` `/`）または HID 矢印キーで操縦
3. `S` でブースト
4. `Enter` で AUTO / MANUAL を切り替え

## 概要

- Java/Swing 版の簡易飛行物理、敵 AI、機銃、ミサイルを Cardputer-ADV 上へ移植
- 地表グリッド、敵機ワイヤーフレーム、弾道、ミサイル煙を 240x135 画面へ描画
- 画面下に速度・高度・ターゲット距離・機銃温度を HUD 表示
- 起動直後は自動操縦で飛行し、手動介入や AUTO 切り替えが可能

## メモ

- 地形は元コード同様に平地です
- サウンドは未実装です
- 物理定数は元の `DT=0.1`, `PMAX=4`, `GSCALE=16` を維持しています
