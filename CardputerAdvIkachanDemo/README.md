# CardputerAdvIkachanDemo

`Ikachan` 風デモ です。  

## 必要なライブラリ

- M5Cardputer

## Arduino IDE 設定

- 開くファイル: `CardputerAdvIkachanDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 現在の内容

- 水中を泳ぐプレイヤーのプロトタイプ
- `Ikachan` を参考にした、噴射インパルス主体の慣性移動
- 背景バブルと簡易 HUD
- 今後のクリーンルーム実装に向けた入力・更新・描画ループ

## 操作

1. `,` で左向き
2. `/` で右向き
3. `A` で水を吹き出して加速

## 今後の候補

- タイルマップとカメラ
- 敵 / 当たり判定
- 攻撃やイベント制御
- オリジナル互換のゲームループ整理
