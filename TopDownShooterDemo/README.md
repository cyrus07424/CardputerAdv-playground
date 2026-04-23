# TopDownShooterDemo

見下ろし型シューティングデモです。

## 必要なライブラリ

- M5Cardputer

## Arduino IDE 設定

- 開くファイル: `TopDownShooterDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 操作

1. `, / ; .` または 矢印キー / `WASD`: 自機移動
2. `Enter` または `BtnA`: タイトル / ゲームオーバー画面から開始・再開

## 内容

- 背景 2 レイヤー + 床グリッドの視差付きサイバー空間描画
- 画面内の近い敵を自動ロックし、自機から自動で弾を発射
- 接近型 / 射撃型 / ボス型を含むウェーブ進行
- HP・スコア・ウェーブ・ロック状態を表示する簡易 HUD

