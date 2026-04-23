# GhostmanDemo

**全体マップ常時表示型**の小型アクションデモです。

## 必要なライブラリ

- M5Cardputer

## Arduino IDE 設定

- 開くファイル: `GhostmanDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 操作

1. 矢印キー または `, / ; .` / `WASD`: ゴースト移動
2. `Enter` または `BtnA`: タイトル / ゲームオーバー画面から開始・再開

## 内容

- ゴースト 1 体 vs パックマン 4 体
- パワーエサ取得中だけ立場が逆転するルール
- ステージ進行、残り時間、スコア、ドット残数の HUD
