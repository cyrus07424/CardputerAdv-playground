# CardputerAdv-playground

Cardputer-ADV 向けの Arduino デモ集です。Arduino IDE で開いてそのまま試せる、最小構成のサンプルを置いています。

## 含まれているデモ

| Project | 内容 | 主な外部ライブラリ |
| --- | --- | --- |
| `CardputerAdvKeyboardInputDemo` | 日本語 IME 風変換入力と SD 保存 / 読込に対応したテキストエディター | `M5Cardputer`, `SD`, `SPI` |
| `CardputerAdvMandelbrotDemo` | マンデルブロ集合の描画とキーボード操作によるパン / ズーム | `M5Cardputer` |
| `CardputerAdvRouletteRecorderDemo` | ルーレットの出目をセッション単位で CSV 記録し、ヒートマップや履歴で可視化するツール | `M5Cardputer`, `SD`, `SPI` |
| `CardputerAdvMinecraftDemo` | `Minecraft` 風デモ | `M5Cardputer` |
| `CardputerAdvOutRunDemo` | `OutRun` 風デモ | `M5Cardputer` |
| `CardputerAdvIkachanDemo` | `Ikachan` 風デモ | `M5Cardputer` |
| `CardputerAdvBlindJumpDemo` | `Blind Jump` 風デモ | `M5Cardputer` |
| `CardputerAdvTRexRunnerDemo` | `T-Rex Runner` 風デモ | `M5Cardputer` |
| `CardputerAdvCharisoDemo` | `チャリ走 2nd Race` 風デモ | `M5Cardputer` |
| `CardputerAdvVoxelburgDemo` | `Attack on Voxelburg` 風デモ | `M5Cardputer` |
| `CardputerAdvImu3dDemo` | 内蔵加速度 / ジャイロを使った 3D 姿勢ビジュアライズ | `M5Cardputer` |
| `CardputerAdvNekoFlightDemo` | `NekoFlight` を移植した 3D フライト / 戦闘デモ | `M5Cardputer` |
| `CardputerAdvNekoFlightAdvancedDemo` | `NekoFlight` を移植した 3D フライト / 戦闘デモ(拡張版) | `M5Cardputer` |
| `CardputerAdvGpsInfoDemo` | Cap LoRa-1262 内蔵 GNSS の受信情報表示 | `M5Cardputer`, `TinyGPSPlus` |
| `CardputerAdvLoraChatDemo` | Cap LoRa-1262 を使ったシンプルな LoRa メッセージ送受信 | `M5Cardputer`, `RadioLib` |
| `CardputerAdvDataHarvester` | Cap LoRa-1262 内蔵 GNSS と LoRa の受信情報収集・表示等 | `M5Cardputer`, `RadioLib`, `TinyGPSPlus` |

## 共通前提

- ボード設定: `m5stack:esp32:m5stack_cardputer`
- ライブラリはできるだけ Arduino IDE のライブラリマネージャからインストール
- 各プロジェクトの詳しい手順は、それぞれの `README.md` を参照
