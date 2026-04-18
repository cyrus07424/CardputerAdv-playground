# CardputerAdvRouletteRecorderDemo

Cardputer-ADV 向けの、ルーレットの出目を記録・分析するツールです。

American / European を最初に選び、キーボードから出目を順に記録できます。記録は SD カード上の SQLite データベースに保存され、履歴から次の出目候補を表示します。

## 必要なライブラリ

- M5Cardputer
- SD
- SPI
- Sqlite3Esp32

## Arduino IDE 設定

- 開くファイル: `CardputerAdvRouletteRecorderDemo.ino`
- ボード: `m5stack:esp32:m5stack_cardputer`

## 保存先

- SQLite DB: `/sd/roulette/roulette.db`
- SD API 上の実ファイル位置: `/roulette/roulette.db`

通常の `SD.open()` / `SD.exists()` では `/roulette/roulette.db` を使い、`Sqlite3Esp32` の `sqlite3_open()` に渡す時だけ `/sd/roulette/roulette.db` を使います。起動時に `/roulette` ディレクトリや DB ファイルが無ければ自動作成します。

## 使い方

### 1. ホイール選択

- 起動直後はホイール選択画面です
- `Tab` で `American` / `European` を切り替えます
- `A` で American、`E` で European を直接選べます
- `Enter` または `BtnA` で新しいセッションを開始します

### 2. 出目の記録

- `0`〜`36` を入力して `Enter` で記録します
- American のみ `00` も記録できます
  - `0` `0` と入力するか、`Z` キーで一発入力できます
- `Backspace` で入力中の数字を消します

### 3. セッション操作

- `Ctrl+N` または `BtnA` で同じホイール種別の新しいセッションを開始します
- `Tab` でホイール選択画面へ戻ります

## 予想ロジック

次の出目予想は、以下の順で求めています。

1. 直前の出目の次に、同じホイール種別の過去履歴でよく続いた数字
2. 現在のセッションでよく出ている数字
3. 同じホイール種別の全履歴でよく出ている数字

つまり、まず「直前の出目からの遷移」を優先し、履歴が足りない場合は頻度ベースにフォールバックします。

## データ構造

SQLite には以下の 2 テーブルを作成します。

- `sessions`
  - セッション単位の記録
- `spins`
  - 各セッション内のスピン順序と出目

これにより、連続した試行ごとに履歴を分けて保存できます。
