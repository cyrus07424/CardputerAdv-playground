# CardputerAdvKeyboardInputDemo

Cardputer-ADV 本体キーボードで日本語テキストを入力できる、SD カード対応の簡易テキストエディターです。

## 必要なライブラリ

- M5Cardputer
- SD
- SPI

## Arduino IDE 設定

- 開くファイル: `CardputerAdvKeyboardInputDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## SD カードに置くファイル

- `/editor.txt`
  - 保存先 / 読込元のテキストファイルです。初回は無くても構いません。
- `/SKK-JISYO.M`
  - UTF-8 の SKK 辞書です。漢字変換候補の表示に使います。
- `/kanadic.txt` (任意)
  - `M5JapaneseInput` 方式の `roma:kana` マップです。置かない場合はスケッチ内の標準ローマ字かな変換を使います。

## 使い方

1. 起動後、そのままローマ字入力すると日本語変換用の入力行にたまります
2. `Space` で SKK 辞書から候補を引き、再度 `Space` で候補を巡回します
3. `Enter` で候補またはかなを確定し、入力中でなければ改行します
4. `Backspace` は候補解除 → ローマ字入力の削除 → 文書本体の削除の順で動きます
5. `Tab` で日本語入力 / ASCII 直接入力を切り替えます
6. `Ctrl+S` または `BtnA` クリックで `/editor.txt` に保存します
7. `Ctrl+L` または `BtnA` 長押しで `/editor.txt` を読み込みます
8. `Ctrl+N` で新規文書に戻します

## 補足

- 日本語表示には M5GFX 内蔵の日本語フォントを使っています
- 漢字変換は SD カード上の SKK 辞書を逐次検索する方式です
- `M5JapaneseInput` の運用を参考に、`/kanadic.txt` と `/SKK-JISYO.M` をそのまま使える構成にしています
