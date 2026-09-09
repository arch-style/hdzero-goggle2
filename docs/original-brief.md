# 最初の企画書 (2026-09-08)

**この fork がどういう意図で始まったかの記録。** 作業開始前に書かれたもので、
以降は更新していない。**現在の状態を知りたいなら
[fork-changes.md](fork-changes.md)**、未着手の課題は
[known-issues.md](known-issues.md) を見ること。

ここに書かれた見立てのうち、実測で覆ったものがいくつかある(下の「その後」を参照)。
それも含めて、当初の狙いと実際に分かったことの差を残すために原文のまま置いてある。

---

## リポジトリ
- 場所: `~/LocalCodes/github/hdzero-goggle2`(このリポジトリのルートで作業してください)
- 元: https://github.com/hd-zero/hdzero-goggle2 (Goggle 2 本体アプリのソース。MITライセンス)
- ビルド: `./setup.sh` でBootlinのarmv7-eabihf muslクロスコンパイラを取得し、`cmake . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=toolchain/share/buildroot/toolchainfile.cmake -Bbuild` → `cd build && make clean all -j$(nproc)`
- 成果物: `HDZERO_GOGGLE2-x.x.x.bin`(x.x.x = OTA_VER.RX_VER.VA_VER)
- エミュレータビルドも可能(SDL2使用): `cmake .. -DEMULATOR_BUILD=ON -DCMAKE_BUILD_TYPE=Debug` → UIロジックだけ実機なしで確認できる

## 実機への反映方法(重要: 安全な順序で)
1. まず SDカード経由の一時実行で試す。SDカードのルートに、ビルドで生成した `HDZGOGGLE` 実行バイナリと、以下の `develop.sh` を置く。
   ```sh
   #!/bin/sh
   if [ -e /mnt/extsd/HDZGOGGLE ]; then
       /mnt/extsd/HDZGOGGLE &
   else
       /mnt/app/app/HDZGOGGLE &
   fi
   ```
   ゴーグルは起動時に `develop.sh` の有無を自動チェックし、あればSDカード上の `HDZGOGGLE` を優先実行する。本体の正規ファームウェアは上書きされないため、SDカードを抜けば即座に元に戻せる。
2. 動作確認が取れてから、ゴーグルのメニューからの正式フラッシュ(`.bin`書き込み)に進む。こちらは本体を実際に上書きするためブリックのリスクがある。

## 実装したい機能1: チャンネルのお気に入り登録
### 要件
- 例: R2, F1, R4, R5 のような4チャンネルを登録し、有効時はダイヤル操作でその4つの間だけを循環できるようにする。

### 関連コード(すでに特定済み)
- チャンネル切り替えの本体ロジック: `src/core/input_device.c` の `tune_channel()`。現状は `channel++`/`channel--` を `1〜HDZERO_CHANNEL_NUM`(`ui/page_scannow.h`で `RACE_BAND` なら12、それ以外は8と定義)の範囲で単純にループしているだけ。
- チャンネル名テーブル: `src/ui/page_scannow.c` の `race_band_channel_str[] = {"R1","R2","R3","R4","R5","R6","R7","R8","E1","F1","F2","F4"}` と `low_band_channel_str[] = {"L1"..."L8"}`。
- 既存の類似トグルの前例: `g_setting.ease.no_dial`(映像視聴中のダイヤル操作を無効化する設定。`NO_DIAL_FILE` というファイルの有無で管理、`core/settings.c` 参照)。
- 設定の永続化パターン: `core/settings.c` の `ini_getl`/`ini_putl`/`ini_gets`(minIniライブラリ)を使い、`SETTING_INI` ファイルに書き込む。`setting_scan_t scan` 構造体が `core/settings.h` にある。
- チャンネル決定・チューニング確定時の呼び出し: `DM6302_SetChannel(band, ch)`(`driver/dm6302.c`)。バンドとチャンネルを別々に受け取れるので、バンドをまたぐお気に入りにも対応可能。

### 実装方針(会話内で合意した内容)
1. `setting_scan_t` (または新しい `setting_favorites_t`)に、お気に入り配列(バンド+チャンネル番号のペアを最大4つ)と、有効/無効フラグを追加。`ini_getl`/`ini_putl` で永続化。
2. お気に入り登録・編集用の設定画面(UIページ)を追加、または既存のスキャン画面に「お気に入り登録」操作を追加。
3. `tune_channel()` のUP/DOWN処理を、お気に入りモード有効時は `1〜channel_num` ではなく「お気に入り配列のインデックス」でループするよう分岐。バンドが混在する場合は、切り替え時に `g_setting.source.hdzero_band` も一緒に更新し `DM6302_SetChannel()` に正しいバンドを渡す。

## 実装したい機能2: メニュー⇔映像切り替えの高速化
### 現状分かっていること
- HDZeroデジタル受信のメニュー⇔映像切り替え(`core/app_state.c` の `app_switch_to_menu()` / `app_switch_to_hdzero()`)には明示的な `sleep()` は無い。
- 一方、`util/system.c` の `system_exec()` は標準Cの `system()`(`/bin/sh` をfork+exec)を呼んでいるだけで、切り替えのたびに複数回呼ばれている(`Display_720P60_50_t()` 等の中の `system_exec("aww ...")`、`app_switch_to_menu()`内の`system_script(REC_STOP_LIVE)`など)。`system_script()` はさらに実行結果を `/tmp/*.log` に書き出して読み直すディスクI/Oも伴う。
- アナログ/AV入力/HDMI入力への切り替え(`app_switch_to_analog()`, `app_switch_to_av_in()`, `app_switch_to_hdmi_in()`)には `sleep(1)`/`sleep(2)` が明示的にあり、直上に `// usleep(300*1000);` というコメントアウトが残っている(300msでは不十分で1〜2秒に延長した形跡と思われる)。この経路の待機時間短縮は映像/音声の乱れが再発するリスクが高いため要注意。

### 実装方針(会話内で合意した内容)
- HDZeroデジタル受信の切り替え経路(`app_switch_to_hdzero()` 周辺)にある `system_exec("aww ...")` のようなレジスタ書き込み用外部コマンド呼び出しを、`/dev/mem` を直接mmapしてプロセス内でレジスタ書き込みする関数に置き換え、fork/exec自体をなくす。
- `system_script()` の実行結果ログ出力(`/tmp/*.log`への書き出し・読み直し)を、通常動作時は省略またはデバッグフラグ配下にする。
- ユーザーは「消費電力が増えてもいいので高速化したい」との意向。ただしCPU周波数(cpufreq)制御などSoC電源管理はこのリポジトリのソースに含まれておらず(カーネル/BSPは別リポジトリの可能性が高く未確認)、アプリ層の改造だけでは対応範囲外である点は把握済み。

### 補足: SDカード上の `log` フォルダについて
- `/mnt/extsd/` 直下の `log` フォルダに大量の小さなログファイル(`1970-01-01.log.100` 等、1000個以上)が溜まっていた。`src/record/log.c`(録画プロセス独自のロガー、`LOG_defPATH "/mnt/app/log"`、`LOG_defPERIOD 30`)が関係している可能性が高いが、番号付きファイル名の生成ロジック自体はソースから完全には特定できていない。FAT系ファイルシステムでは1ディレクトリに大量のファイルがあると探索が遅くなる傾向があるため、メニュー遅延の副次的要因の可能性がある(未確定)。ユーザーは一度このフォルダを空にして体感速度が変わるか試す予定。

## その他、会話で確認済みの背景情報(必要なら参照)
- ゴーグルのハードウェア: Allwinner V5系SoC上のLinuxアプリ(`-DAWCHIP=AW_V5`)。LVGLでUI描画。映像デコード/表示は `lib/softwinner` 配下のAllwinner製バイナリSDK(`libvdecoder.so`, `libmpp_vo.so` 等、ソース非公開)経由。
- ライブ視聴(HDZeroデジタル受信)は `DM5680`/`DM6302` という専用チップが電波を復調・独自コーデックをデコードし、ほぼSoCを介さずFPGA経由でOLEDパネルに表示している。これが低遅延な理由。
- SDカード録画は、この専用チップが一度展開した生映像を、SoC汎用のVIPPブロックで取り込み、H.264/H.265にエンコードして保存している(DM5680/DM6302はUART制御のみで、映像データそのものは流れない)。
- 現在の本体ファームウェアバージョンは 9.5.1 (rx:76, va:176)。GitHub上の最新タグも v9.5.1 (Rev 20250715) が最新の正式リリースだが、masterブランチはそこから17コミット進んでおり(直近: FPGA DDRキャリブレーション修正)、未リリースの修正が既に存在する。

## お願いしたいこと
上記を踏まえて、まず「機能1: チャンネルのお気に入り登録」から実装してください。関連ファイルを実際に読んで現状の実装と整合性を取り、`setup.sh`でのビルドが通ることを確認してから、SDカード経由(`develop.sh`)での動作確認手順を案内してください。


---

## その後 (2026-09-10 追記)

この企画書の見立てのうち、実測で変わったもの。

- **「実装したい機能 2」の方針** — `system_exec("aww ...")` を `/dev/mem` 直叩きに
  置き換える案は着手していない。実測すると起動のクリティカルパスは `aww` ではなく
  `DM6302_init`(1795ms) と `dispw`(1117ms) だった。`aww` は起動経路で計 130ms 程度。
  [measurements.md](measurements.md) 参照
- **`system_script()` のログ出力を省く案** — 未着手。同じ理由で効果が小さい
- **「明示的な `sleep()` は無い」** — HDZero 経路については誤り。
  `dvr_cmd()` に `sleep(2)` が 2 箇所あり、これが「映像視聴中にメニューへ戻ると
  2 秒待つ」の原因だった。アナログ/AV/HDMI 経路の `sleep(1)`/`sleep(2)` は指摘の
  とおり存在し、そちらは今も触っていない
- **SD カードの `log` フォルダ** — メニュー遅延との関係は確認していない
- **お気に入り機能** — 実装済み。HDZero とアナログで別々のリストを持ち、
  設定ページで編集対象を切り替えられる
