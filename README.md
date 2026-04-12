# CardputerAdv-playground

Cardputer-ADV 向けの小さな Arduino デモ集です。Arduino IDE で開いてそのまま試せる、最小構成のサンプルを置いています。

## 含まれているデモ

| Project | 内容 | 主な外部ライブラリ |
| --- | --- | --- |
| `CardputerAdvLoraChatDemo` | Cap LoRa-1262 を使ったシンプルな LoRa メッセージ送受信 | `M5Cardputer`, `RadioLib` |
| `CardputerAdvKeyboardInputDemo` | Cardputer-ADV 本体キーボードでの文字入力表示 | `M5Cardputer` |
| `CardputerAdvGpsInfoDemo` | Cap LoRa-1262 内蔵 GNSS の受信情報表示 | `M5Cardputer`, `TinyGPSPlus` |

## 共通前提

- ボード設定: `m5stack:esp32:m5stack_cardputer`
- ライブラリはできるだけ Arduino IDE のライブラリマネージャからインストール
- 各プロジェクトの詳しい手順は、それぞれの `README.md` を参照
