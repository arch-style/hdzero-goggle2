# feature/fast-switch — 変更の記録

HDZero Goggle 2 の純正アプリ (`hd-zero/hdzero-goggle2`) に対する fork の記録。
第三者に説明できるよう、**何が問題で、なぜそうなり、どう直し、どう確認したか**を
残す。コミットメッセージ (英語) に技術的な詳細があるので、コミットハッシュを併記する。

関連文書:

| ファイル | 内容 |
|---|---|
| このファイル | 何をする fork か、各設定の意味、直した不具合、ビルドと確認の手順 |
| [known-issues.md](known-issues.md) | **まだ直していない**不具合・リスク・改善案のバックログ |
| [measurements.md](measurements.md) | ここに書いた数字の根拠となる実測データ |

---

## 1. 大原則

**この fork が変えたことは、すべてゴーグルのメニューから元に戻せる。**

- 高速化は 1 つ 1 つが独立した On/Off 行で、**既定はすべて OFF**
- 全部 OFF = 純正と同じ挙動
- 設定は `setting.ini` の `[speed]` セクションに保存される
- SD カードのルートに `HDZGOGGLE` と `develop.sh` を置くと、本体を書き換えずに
  そのバイナリが動く。**カードを抜けば純正に戻る**

この方針は、対象が実際に飛ばす機材だから。悪い変更をメニューから切れること、
カードを抜けば元に戻ることが、安全に試すための前提になっている。

例外は 3 つだけで、いずれも「戻す対象の体験が無い」もの:

| 例外 | 理由 |
|---|---|
| ログの時刻計測を単調時計に (`eb6f3f6`) | ログの数値が変わるだけ |
| 範囲外チャンネルのクランプ (`45dff4d`) | 壊れた設定ファイルを弾くだけ |
| 選択パネルを行数分だけ作る (`c949f31`) | UI は完全に同一、菜単構築が軽くなるだけ |

いずれも単独コミットなので `git revert` 一発で戻せる。

---

## 2. メニュー構成

サイドバーは 2 ページある。1 ページ目は純正の 19 項目でそのまま。両端で回すと
2 ページ目に移り、そこに fork の 5 ページがある。

### Boot Speed — 次の起動から効く

焼いて再起動し、ログの起動マーカーで確認する種類。

| 行 | 設定キー | 内容 |
|---|---|---|
| Skip Display Setup | `boot_display` | 起動時にメニュー用の表示設定をしない。純正は UI 用に 1080p50 にした直後に映像用へ再設定し、1.1 秒の `dispw` を 2 回払う |
| Preload OSD Fonts | `boot_fonts` | OSD フォント (約 850ms の SD 読み) をワーカーで先読み |
| Skip Boot Menu | `skip_boot_menu` | 起動中メニューを表示しない |
| Defer Menu Build | `defer_menu` | メニューページの構築を映像の後に回す |
| Async Motion Sensor | `async_imu` | IMU 初期化 (686ms の I2C) をワーカーへ |
| Async Display Setup | `async_display` | 表示タイミング変更をチューナ初期化と並行に |
| Early Video Timing | `boot_display_early` | `dispw` を起動直後に開始。**上の 3 つが全部 ON でないと何もしない** |
| Async Tuner Init | `async_tuner` | チューナ初期化 (1.8 秒の I2C) をワーカーへ |
| Skip WiFi Stop | `skip_wifi_stop` | ドライバ未ロード時に `wlan_stop.sh` を省く |

### Switch Speed — 次のボタン操作から効く

| 行 | 設定キー | 内容 |
|---|---|---|
| Menu Over Video | `keep_display` | メニューを映像の上に描く。表示を切り替えないので `dispw` が往復とも消える。**下の 3 行の効き方を決める** |
| Keep Tuner Alive | `fast_menu` | メニュー中もチューナを standby に留め、復帰時の `DM6302_init()` (2.3 秒) を省く |
| Async Menu Display | `menu_async_display` | メニューへの切り替えで 1080p50 を先に投げ、録画停止と音声を `dispw` の裏で走らせる |
| Skip Audio Setup | `skip_audio` | ミキサーが既に望みの状態なら `audio_sel.sh` を再実行しない |
| Poll DVR Stop | `dvr_stop_wait` | 録画停止の固定 2 秒待ちを、`/tmp/record.dat` の監視に置き換え |
| Poll DVR Start | `dvr_start_wait` | 同じことを開始側に |
| Defer DVR Stop | `dvr_defer_stop` | 停止を待たずに先へ進み、待ちを次の録画開始へ移す |
| Burst Tuner Writes | `spi_burst` | チューナ 1 回の SPI 書き込み (FPGA レジスタ 7 回) を 1 回の I2C 転送に |
| Dual-Chip EFUSE Read | `fast_efuse` | 2 チップ分の校正値を 1 回の走査で読む |
| Menu Antialias OFF | `menu_antialias_off` | 縮小表示中のメニューのアンチエイリアスを切る |
| Skip Idle Redraws | `ui_throttle` + `label_diff` | ステータスバーの更新を 200Hz→20Hz にし、内容が変わらないときは描き直さない |
| Split UI Lock | `split_lock` | メインループの `lvgl_mutex` を段階ごとに取り直し、入力の割り込む余地を増やす |

### Input Feel — 操作感

長押しの時間 (500/400/300/200/100/50ms)、ダイヤルとボタンのビープ。
速度とは関係ないので速度ページから分離した。

### Input — ボタンに割り当てる動作 (純正ページへの追加)

`Input` ページは純正のもので、ダイヤル / 左ボタン (短押し・長押し) /
右ボタン (短押し・長押し・ダブル) にそれぞれ動作を割り当てる。ここに 2 つ足した。

| 選択肢 | 内容 |
|---|---|
| Next channel | 次のチャンネルへ即座に選局する。ダイヤルを 1 つ回して押す操作と同じことを 1 押しで行う |
| HDZero Wide/Narrow | HDZero の帯域幅 Wide / Narrow を切り替える (`Source` ページの HDZero BW と同じ設定) |

補足:

- **アナログ / デジタルの切り替えは純正に既にある** — `Toggle source` が
  HDZero ⇔ アナログを往復する (HDMI In / AV In からは HDZero に戻る)
- `Next channel` は選局処理そのものを呼ぶので、Favorites CH の登録があれば
  その 8 つを巡回し、band の上限で 1 に戻り、チャンネル OSD も出る。
  SD カードの `no_dial.txt` で映像中の選局を止めているときは、このボタンも効かない
- `HDZero Wide/Narrow` は HDZero 視聴中なら受信機を開き直すので映像が一瞬切れる。
  他のソースを見ているときは設定を保存するだけで、次に HDZero にしたときから効く

### Favorites CH

ダイヤルで巡回するチャンネルを最大 8 つ登録する。HDZero 用とアナログ用の 2 本を
持ち、ページ先頭の `Source` 行でどちらを編集するか選ぶ。**編集対象を切り替えるだけで、
実際にどちらで選局するかは視聴中の映像ソースが決める** — 設定ページを開いただけで
ダイヤルの挙動が変わらないようにするため。

### Fixes

速度ではなく「正しさ」の修正。現在は Retry Tuner Init のみ。

---

## 3. 「効かない行」の表示

いくつかの行は、他の行の状態によって**何もしなくなる**。そのままだと測定値が
横に出ているのに実際には効いていない、という嘘になるので、灰色にして
`(no effect)` を出し、下の説明行に理由を書く (`cc454d7`)。

| 行 | 効かなくなる条件 | 理由 |
|---|---|---|
| Keep Tuner Alive | Menu Over Video が ON | オーバーレイ時はチューナを止めないので standby に到達しない |
| Async Menu Display | Menu Over Video が ON | 表示を変えないので前倒しする対象が無い |
| Poll DVR Stop | Defer DVR Stop が ON | そもそも待たない |
| Early Video Timing | 依存 3 つが揃わない | コードが警告を出して何もしない |
| Long Press Time | Timed Long Press が OFF | 値が読まれない |

依存関係のある行は同じページの隣に置いてある。

---

## 4. 直した不具合

### 4.1 映像視聴中にメニューへ戻ると 2 秒待つ (`fc26715`, `a6311ce`)

**症状:** 映像を受信しているときにメニューへ戻すと、ビープの後 2 秒ほど待たされる。
映像を受信していないときは待ちがない。チャンネル変更でも同じ。

**原因:** 信号を受けると自動録画が始まる。メニューへの切り替えとチャンネル変更は
どちらも `dvr_cmd(DVR_STOP)` を通り、そこに固定の `sleep(2)` があった。
`app_switch_to_menu()` は `lvgl_mutex` を保持したまま呼ばれるので、この 2 秒は
画面が一切描き変わらない 2 秒になる。

さらに実測で分かったこと: **録画プロセスは開始から約 3 秒経つまでファイルを確定しない。**

| 録画継続 | 停止待ち |
|---|---|
| 6.09s | 101ms |
| 3.35s | 121ms |
| 1.49s | 1451ms |
| 0.25s | 2011ms (上限) |

**対処:**
- Poll DVR Stop / Start — 固定待ちを `/tmp/record.dat` の監視に。録画プロセスは
  `ffpack_close()` の後に状態を書くので、それがクローズ完了の本人申告になる
- Defer DVR Stop — 停止を投げっぱなしにし、待ちを次の録画開始へ移す。間に走るのは
  チャンネル変更と表示タイミングで、どちらも録画プロセスのものではない

**実測 (メニューが出るまで):**

| | 修正前 | 修正後 |
|---|---|---|
| 短い録画の後 | 1467 / 2029 / 2030ms | **6 / 7 / 18 / 18 / 19 / 26 / 33 / 186ms** |

待ちは次の録画開始へ移り、そこでの実測は最大 727ms、しかもボタン操作の裏 (周辺スレッド)。

### 4.2 起動が 12.9 秒かかることがある (`1d62e02`)

**症状:** 通常 2.4 秒の起動が、ときどき 12.9 秒かかる。

**原因:** チューナ初期化 (`DM6302_init()`) は実行中ずっと**メイン I2C バスを 1MHz に
上げる**。OLED は同じバス上の FPGA 経由で読み書きするが、**FPGA は 1MHz で応答できない。**
`Async Tuner Init` が ON だと 2 つが並行に走り、普段はチューナが僅差で先に終わるが、
順番が入れ替わると衝突する。

本来 30µs の 1 転送が約 39ms かかっていた。クロックストレッチかリトライで、待ち行列
ではない。`i2c.c` のターンストルは両者を公平に分け合わせるが、それは飢餓を防ぐための
もので、応答しない速度の通信を速くはできない。律儀に交互に譲った結果、両方が等しく
100 倍遅くなった。

| | 通常 | 衝突時 |
|---|---|---|
| M0 イメージ転送 | 92ms | 10147ms |
| OLED 初期化 | 94ms | 9955ms |
| boot total | 2400ms | 12957ms |

**対処:** `OLED_Startup()` の前でワーカーの完了を待つ。終わればバスは 200kHz に
戻るので、そこから OLED は全速で動く。

**実測 (2 回の起動):**

| | 待ち | OLED | M0 | boot total |
|---|---|---|---|---|
| 衝突するはずだった起動 | **316ms** | 53ms | 92ms | **2549ms** |
| 通常の起動 | 6ms | 53ms | 93ms | 2345ms |

**払った代償 316ms、避けた損失 約 10 秒。**

**この修正の限界:** 待つのは `OLED_Startup()` の前だけ。`Display_UI_init()` も
`pclk_phase_set()` 経由で同じ FPGA に触るが、`OLED_Startup()` が先に走って回収済み
だから安全、という順序依存になっている。この段の順序を変えるときは要再検討。

### 4.3 Menu Over Video で Playback の映像が斜めにずれる (`4ea6d5e`)

**原因:** プレイヤーは 1920x1080 のレイヤーを作り、デコードしたフレームを SoC の VO に
渡す。Menu Over Video だと `app_switch_to_menu()` が `Display_UI()` を呼ばないので、
FPGA はライブ映像源を選んだまま、パネルは 720p60 のまま。そこへ 1080p が来る。

**対処:** `app_menu_end_overlay()` を追加し、`page_playback_enter()` から呼ぶ。
オーバーレイ中だったときだけ、スキップされた切り替えをそこで実行する。

### 4.4 Playback の一覧画面が左上に縮小される (`417950c`)

**4.3 の修正が持ち込んだ欠陥。** メニューの縮小率は `main_menu_show()` で一度だけ
計算され、その後は誰も再計算しない。4.3 の修正はメニューが表示されている最中に
解像度を 720p→1080p に変えるのに、そのことをメニューに伝えていなかった。

```
menu: zoom 170/256 for 728px display   ← メニューに入る (720p、2/3 に縮小)
switch mark: ending the overlay
menu: zoom 256/256 for 1088px display  ← Playback で 1080p に。等倍へ戻す (修正後)
```

**対処:** `main_menu_refit_display()` を追加して解像度変更の直後に呼ぶ。あわせて
`main_menu_apply_antialiasing()` も呼び直す (`menu_scaled` が変わるため)。

### 4.5 メニュー分割で最後の行が消える (`3cc95bd`)

**原因:** グリッドはテーマの `pad_row` (約 11px) を行間に入れる。`style_context` は
これを上書きしていないので、`行数 x 行高` でコンテナを作ると行数 −1 個分の隙間だけ
足りない。Boot Speed は 699px の中身を 600px の箱に入れており、Back は完全に箱の外だった。

**対処:** 3 ページとも同じジオメトリにし、全ページにスクロールを付けた。高さの
見積もりを外しても、最後の行が消えるのではなくスクロールする。

### 4.6 選択パネルの NULL 参照 (`2199d90`)

`c949f31` で各ページが必要な数だけ選択パネルを作るようにしたとき、`set_select_item()`
が `MAX_PANELS` (32) 個を無条件に走査するのを見落とした。LVGL は `LV_USE_ASSERT_OBJ 0`
でビルドされているので `lv_obj_add_flag(NULL, ...)` はそのまま NULL 参照になる。
**メニューを開いた最初の一手で落ちる**バグだった。`panel_arr_t` に実際に作った数を
持たせ、走査をそこで止めるようにした。

### 4.7 純正から持ち越していた不具合

| 項目 | 内容 | コミット |
|---|---|---|
| 2.6 | `system_exec()` の時間計測が壁時計。`rtc_init()` が時計を 1970→2026 に飛ばすので、それをまたぐと数千万 ms と表示される | `eb6f3f6` |
| 3.10 | `channel2str()` のアナログ分岐に範囲チェックが無く、範囲外チャンネルで配列外参照。さらに `settings.c` のアナログ範囲チェックが**別のフィールド (`scan.channel`) に代入していた** | `45dff4d` |
| 3.7 | `Skip Boot Menu` が塗った黒背景を誰も戻さず、起動用の設定がセッション中ずっとメニューの見た目を変えていた | `8fcdb08` |

---

## 5. やってみて駄目だったこと

### 720p Playback (`7a1c887` → `622faf1` で撤去)

Playback を 720p のまま表示できれば `dispw` の 1.1 秒 x 2 が消える、という試み。
理屈は通っていた (`OLED_SetTMG` のコメントが `0=1080P; 1=720P` と言っており、
パネルのタイミングは解像度で決まる) が、**実機で表示が壊れた。**

ラインごとの横ずれ、縦は上 1/4 程度、紫が強い、まばらなランダムピクセル。
そして**長押ししてもメニューに戻れなかった。**

特定できたバグ: `lv_disp_get_hor_res()` は**パネルの解像度ではない**。
`DRAW_HOR_RES_HD = 1280 + DISP_OVERSCAN(8) = 1288`。LVGL の描画バッファはオーバー
スキャン込み。同じ誤りは 1080p でも起きており (1928x1088)、トグル OFF でも矩形が
8px ずれていた。

**撤去した理由は「戻れなかった」こと。** 飛ばす機材で、ON にすると抜けられなく
なりうるスイッチは、既定 OFF でもメニューに置くべきではない。紫や縦 1/4 は矩形の
ずれだけでは説明がつかず、FPGA の他の設定も食い違っている。

**再挑戦するなら:** まず 1080p のまま `stDispRect` を 1920x1080 に直す小さく確実な
修正から。それが正しく映ることを確認してから 720p を試す。

---

## 6. 確認のしかた

### ビルド

arm64 Mac では `setup.sh` が動かないので、amd64 コンテナで増分ビルドする。

```sh
docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src debian:bookworm-slim bash -c '
  apt-get update -qq && apt-get install -y -qq cmake make
  cmake . -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=toolchain/share/buildroot/toolchainfile.cmake -Bbuild
  cd build && make all -j$(nproc)'
```

`CMakeLists.txt` は `src/ui/*.c` を configure 時にしか glob しないので、
**ファイルを追加・削除したら `cmake` を再実行する**こと。Release は `-Werror`。

### 純正との差分を見る

純正のクローンを別に置く必要はない。upstream をリモートに足せば済む:

```sh
git remote add upstream https://github.com/hd-zero/hdzero-goggle2.git
git fetch upstream
git diff upstream/master -- src/
```

**純正の起動時間を測り直したい場合**は `baseline-instrumentation.patch` を使う。
純正のソースには CLOCK_MONOTONIC のアンカーが無く、`rtc_init()` が起動途中で
時計を飛ばすので、素のままでは映像が出るまでの時間を測れない。当てる先は
upstream の作業コピーで、このリポジトリではない(fork 側には `g_boot_start_ms` と
`boot: picture at Nms` が既にある)。

### 実機へ

`out/HDZGOGGLE` を SD カードのルートへコピーし、`cmp` で照合してからアンマウント。
`develop.sh` があればゴーグルは起動時にカード上のバイナリを優先実行する。

### ログの読み方

SD カード直下の `HDZGOGGLE.log` (最新) と `HDZGOGGLE.prev.log` (1 つ前)。

**時計に注意。** RTC に電池が無いので毎回同じ保存値に戻り、日付は常に同じに見える。
`04:01:55.0` 付近が全起動共通のアンカーになる。起動間の比較は
`boot total`(CLOCK_MONOTONIC) を使うこと。

主なマーカー:

| マーカー | 意味 |
|---|---|
| `boot total: app start to switch done Nms` | 起動の総時間。単調時計なので信頼できる |
| `boot step: wait for the tuner bus Nms` | 0ms=通常、数百ms=衝突を回避した |
| `boot step: oled startup Nms` | 通常 53〜94ms。数千ms ならバス衝突 |
| `M0 write N/237 at Nms` | 通常 92ms で完走 |
| `switch mark: ...` | メニュー/映像切り替えの各段階 |
| `dvr: record stop/start took Nms` | 録画プロセスの待ち |
| `dvr: record stop left to finish in the background` | Defer DVR Stop が効いている |
| `menu: zoom N/256 for Npx display` | メニューの縮小率 |
| `speed: ...=on/off` | 起動時の設定一覧 |
