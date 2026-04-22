# CardputerAdvPxtonePlayerDemo

`libpxtone` を同梱し、Cardputer-ADV で `pxtone collage` 形式の楽曲を SD カードから選んで再生する Arduino デモです。

## 対応

- `*.ptcop`
- `*.pttune`
- SD カード内のディレクトリ移動と全ファイル表示
- 再生中のシークバー表示

## 制約

- 再生は **mono / 8000Hz** です
- 内部の音色展開もこの出力設定に合わせて行います
- 読み込み後は一時的な RAW キャッシュ `__pxtone_cache.raw` を SD に生成し、以降は SD から再生して常駐メモリを抑えます
- `libpxtone` の OGG Vorbis 系音色はこのデモでは有効化していません

## 必要なライブラリ

- M5Cardputer
- SD
- SPI

`libpxtone` 本体のソースはこのデモに同梱しています。

## Arduino IDE 設定

- 開くファイル: `CardputerAdvPxtonePlayerDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 使い方

1. microSD に `*.ptcop` または `*.pttune` を入れて Cardputer-ADV に挿す
2. Arduino IDE で書き込んで起動する
3. ブラウザ画面でカーソルキーまたは `; , . /` 配列の方向キーで移動する
4. ブラウザ画面では全ファイルを表示し、非対応ファイルは `*` 付きで表示する
5. `Enter` または `BtnA` でディレクトリを開く / 対応楽曲を再生する
6. 再生画面では `Enter` / `BtnA` で再生・一時停止、`Backspace` / `Esc` でブラウザへ戻る

## 同梱ライブラリ

- `libpxtone`: https://github.com/Wohlstand/libpxtone
- ライセンス: `libpxtone-LICENSE.txt`
