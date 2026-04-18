# CardputerAdvMandelbrotDemo

Cardputer-ADV 向けに、マンデルブロ集合を段階描画しながらキーボードで移動・ズーム・表示調整できる Arduino デモです。

## 必要なライブラリ

- M5Cardputer

## Arduino IDE 設定

- 開くファイル: `CardputerAdvMandelbrotDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 使い方

1. Arduino IDE で `CardputerAdvMandelbrotDemo.ino` を開く
2. ボードを `m5stack:esp32:m5stack_cardputer` に設定する
3. ライブラリマネージャで `M5Cardputer` をインストールする
4. 書き込むとマンデルブロ集合が段階的に描画される
5. キーボードで表示を動かし、必要に応じてパレットや反復回数を調整する

## 操作

- 矢印キー または `, / ; .`: パン
- `A`: 拡大
- `Z`: 縮小
- `W`: 反復回数を増やす
- `Q`: 反復回数を減らす
- `P`: カラーパレット切替
- `Enter`: ヘルプ表示の切替
- `BtnA`: 初期ビューへリセット

## メモ

- 描画は行単位で更新するため、ズームや移動を繰り返しても固まりにくい構成です
- パン / ズーム時は直前フレームを新しい表示範囲へ再投影してから正しい画を上書きするため、黒画面からの描き直しよりちらつきが少なくなっています
- 深いズームでは `W` で反復回数を増やすと輪郭が見やすくなります
