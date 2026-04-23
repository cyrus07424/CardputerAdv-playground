# BlindJumpDemo

`Blind Jump` の手触りを Cardputer-ADV 向けに再構成した clean-room ポートです。

upstream の engine やアートをそのまま持ち込むのではなく、Cardputer で成立する範囲に絞って、**部屋探索 / 別方向射撃 / 回避 / 鍵と転送ゲート** のループを 1 本の Arduino スケッチに落とし込んでいます。

## このデモでできること

- 3x3 フロア内の部屋探索
- `WASD` 移動と `IJKL` の別方向射撃
- `U` / `BtnA` の短い回避ダッシュ
- ドア閉鎖付きの戦闘部屋
- 鍵部屋クリアでキー取得、出口部屋で転送して次フロアへ進行
- 簡易ミニマップ、HP、スコア、フロア表示

## 省略しているもの

- upstream のアート / 音楽 / スクリプト / 会話 / ショップ / マルチプレイ
- 複数バイオームや大規模なボス戦
- 永続セーブ

## 必要なライブラリ

- M5Cardputer

## Arduino IDE 設定

- 開くファイル: `BlindJumpDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 操作

1. `WASD` で移動
2. `IJKL` で上下左右へ射撃
3. `U` または `BtnA` で回避 / 転送ゲート起動
4. `Enter` または `BtnA` で開始 / リスタート

## 補足

- Blind Jump の **移動と別方向射撃** の感触を Cardputer キーボードで成立させるため、左手 `WASD`、右手 `IJKL` の配置にしています。
- upstream のアセットは直接同梱せず、描画はすべてこのスケッチ内で描く clean-room 構成です。
- まずは遊べる first playable port を優先し、Cardputer 上での反応速度と見通しの良い 2D ループに寄せています。
