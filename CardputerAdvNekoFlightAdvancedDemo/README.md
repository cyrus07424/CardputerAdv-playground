# CardputerAdvNekoFlightAdvancedDemo

`NekoFlight` を Cardputer-ADV 向けに Arduino/C++ へ移植した 3D フライトデモです。元の `Plane` / `Wing` / `Bullet` / `Missile` の挙動をベースに、Cardputer-ADV の画面とキーボードで遊べるようにしています。

## 必要なライブラリ

- M5Cardputer

## Arduino IDE 設定

- 開くファイル: `CardputerAdvNekoFlightAdvancedDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 操作

1. `A` で射撃 / ミサイル
2. カーソルキー表示のあるキー（`;` `,` `.` `/`）でピッチ / ロール操縦
3. `S` でブースト
4. `L` と `"` キーでラダー操作
5. `R` でステージを即リセット
6. `Enter` で AUTO / MANUAL を切り替え
7. 左上の `` ` / ~ `` キーでヘッダー / フッター UI を表示 / 非表示

## 概要

- Java/Swing 版の簡易飛行物理、敵 AI、機銃、ミサイルを Cardputer-ADV 上へ移植
- 地表グリッド、敵機ワイヤーフレーム、弾道、ミサイル煙を 240x135 画面へ描画
- 速度・高度・操縦モードを画面中央寄りの戦闘機風 HUD として重ね描きし、ピッチラダーで機体姿勢を表示
- 射線方向へ追従する可動レティクルと、敵方向を示すオレンジ矢印を HUD 上に表示
- 右側にはターゲット距離 `TGT` をオレンジ系の HUD パネルで表示
- 起動直後は自動操縦で飛行し、手動介入や AUTO 切り替えが可能

## メモ

- 地形は元コード同様に平地です
- サウンドは未実装です
- 物理定数は元の `DT=0.1`, `PMAX=4`, `GSCALE=16` を維持しています
