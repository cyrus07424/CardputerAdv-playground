# CardputerAdvMinecraftDemo

Minecraftの雰囲気を Cardputer-ADV 向けに落とし込んだ、Arduino IDE 用の軽量 Minecraft 風デモです。

Cardputer-ADV で成立する範囲に絞って再構成しています。

## このデモでできること

- 一人称視点で小さなボクセル世界を移動
- 草原 / 砂地 / 水面を含む簡易な地形生成
- 木 / サボテンのような簡易構造物生成
- 狙っているブロックの破壊 / 設置
- Cardputer-ADV 本体キーボードだけで操作

## 省略しているもの

- テクスチャ付きメッシュ描画
- 無限チャンク生成
- セーブ / ロード
- 音 / GUI メニュー / 昼夜サイクル

## 必要なライブラリ

- M5Cardputer

## Arduino IDE 設定

- 開くファイル: `CardputerAdvMinecraftDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 操作

1. `WASD` で移動
2. `J` / `L` で左右旋回、`I` / `K` で視点上下
3. `G` でジャンプ
4. `F` で飛行モード切替、飛行中は `Q` / `E` で下降 / 上昇
5. `Z` でブロック破壊、`X` で設置、`C` で設置ブロック切替
6. `R` でワールド再生成
7. `BtnA` でヘルプ表示切替

## 補足

- デモ重視のため、描画は低解像度のボクセルレイキャストを拡大表示しています。
- 元実装のブロック系ゲームプレイを Cardputer-ADV 上で触れることを優先し、機能はかなり絞っています。
