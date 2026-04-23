# CardputerAdv-playground

Cardputer-ADV 向けの Arduino デモ集です。Arduino IDE で開いてそのまま試せる、最小構成のサンプルを置いています。

## 含まれているデモ

| Project | 内容 | 主な外部ライブラリ |
| --- | --- | --- |
| `KeyboardInputDemo` | 日本語 IME 風変換入力と SD 保存 / 読込に対応したテキストエディター | `M5Cardputer`, `SD`, `SPI` |
| `MandelbrotDemo` | マンデルブロ集合の描画とキーボード操作によるパン / ズーム | `M5Cardputer` |
| `RouletteRecorderDemo` | ルーレットの出目をセッション単位で CSV 記録し、ヒートマップや履歴で可視化するツール | `M5Cardputer`, `SD`, `SPI` |
| `MinecraftDemo` | `Minecraft` 風デモ | `M5Cardputer` |
| `OutRunDemo` | `OutRun` 風デモ | `M5Cardputer` |
| `IkachanDemo` | `Ikachan` 風デモ | `M5Cardputer` |
| `BlindJumpDemo` | `Blind Jump` 風デモ | `M5Cardputer` |
| `TRexRunnerDemo` | `T-Rex Runner` 風デモ | `M5Cardputer` |
| `CharisoDemo` | `チャリ走 2nd Race` 風デモ | `M5Cardputer` |
| `VoxelburgDemo` | `Attack on Voxelburg` 風デモ | `M5Cardputer` |
| `AnalogClockDemo` | クロノグラフアナログ時計 | `M5Cardputer` |
| `GhostmanDemo` | ゴースト側を操作する追跡アクション | `M5Cardputer` |
| `TopDownShooterDemo` | 見下ろし型シューティングデモ | `M5Cardputer` |
| `DanmakuDemo` | 弾幕シューティングデモ | `M5Cardputer` |
| `Imu3dDemo` | 内蔵加速度 / ジャイロを使った 3D 姿勢ビジュアライズ | `M5Cardputer` |
| `LevelDemo` | 黒系キーボード風 UI の 2 軸水平器 | `M5Cardputer` |
| `PxtonePlayerDemo` | SD カード上の `*.ptcop` / `*.pttune` を選んで再生するpxtone プレイヤー | `M5Cardputer`, `SD`, `SPI` |
| `NekoFlightDemo` | `NekoFlight` を移植した 3D フライト / 戦闘デモ | `M5Cardputer` |
| `NekoFlightAdvancedDemo` | `NekoFlight` を移植した 3D フライト / 戦闘デモ(拡張版) | `M5Cardputer` |
| `GpsInfoDemo` | Cap LoRa-1262 内蔵 GNSS の受信情報表示 | `M5Cardputer`, `TinyGPSPlus` |
| `LoraChatDemo` | Cap LoRa-1262 を使ったシンプルな LoRa メッセージ送受信 | `M5Cardputer`, `RadioLib` |
| `DataHarvester` | Cap LoRa-1262 内蔵 GNSS と LoRa の受信情報収集・表示等 | `M5Cardputer`, `RadioLib`, `TinyGPSPlus` |

## 共通前提

- ボード設定: `m5stack:esp32:m5stack_cardputer`
- ライブラリはできるだけ Arduino IDE のライブラリマネージャからインストール
- 各プロジェクトの詳しい手順は、それぞれの `README.md` を参照
