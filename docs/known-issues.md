# feature/fast-switch — 未修正の課題

このブランチのコードレビューで見つかり、**まだ直していない**もの。直したものは
[fork-changes.md](fork-changes.md) に、その根拠となる実測値は
[measurements.md](measurements.md) にある。

行番号は 2026-09-08 のレビュー時点のもので、その後の編集でずれている可能性がある。

---

## 1. 不具合

### 1.1 アンチエイリアスが映像に戻っても復帰しない

`ui_main_menu.c` の `main_menu_apply_antialiasing()` は
`menu_antialias_off && menu_scaled && main_menu_is_shown()` で判定する。復帰は
`main_menu_show(false)` の分岐にあるが、**`main_menu_show(false)` はどこからも
呼ばれない** — メニューは OSD 画面に覆われるだけで HIDDEN にならない。

- **条件:** Menu Antialias OFF + Menu Over Video + 720p 映像
- **症状:** 映像に戻っても `disp_drv.antialiasing` が 0 のままで、OSD と全ウィジェット
  がギザギザ。角丸や影にも効く
- **修正案:** `app_exit_menu()` で `menu_scaled` / 表示中を無視して復帰させるか、
  実際にメニューを HIDDEN にする
- **注意:** `417950c` で `main_menu_refit_display()` が
  `main_menu_apply_antialiasing()` を呼ぶようになったので、Playback を経由した場合は
  部分的に直っている可能性がある。**着手前に現状を実機で確認すること**

### 1.2 ステータスバーの差分判定が省略記号付きラベルで常に失敗

`ui_statusbar.c` の `sb_label()` は `lv_label_get_text()` と比較する。ところが
LVGL 8.3 の `LV_LABEL_LONG_DOT` は**ラベル内部の文字列を「…」で上書きする**
(`lv_label.c` の 1140 行付近)。

`STS_SDCARD` を除く全ラベルが `LONG_DOT` で、幅 267px を超えると毎回不一致になる。
結果、Skip Idle Redraws を ON にしても差分判定がすり抜け、20Hz で全画面再描画が
走る。縮小表示中のメニューなら 5.6MB の ARGB レイヤーを毎回作り直すことになる。

- **修正案:** バー側で最後に設定した文字列を保持して比較する

### 1.3 オーバーレイ中にカメラ解像度が変わると OSD がメニューの上に出る

`osd.c` の `fhd_change()` は `osd_hdzero_update()` から無条件に呼ばれる。
Menu Over Video では `Display_UI()` を呼ばないので `source_mode` が HDZERO のままで、
`HDZERO_detect()` が動き続ける。720p↔1080p の変化で `fhd_req` が立ち、
`fhd_change()` が `osd_show(true)` するのでメニューが OSD の下に隠れる。ダイヤルは
メニューを操作し続け、拡大率も古いまま。

- **修正案:** `g_app_state == APP_STATE_VIDEO` のときだけ `fhd_change()` を走らせる

### 1.4 お気に入りの帯域外スロットが「L1」等と表示される

`channel2str()` は範囲外チャンネルで先頭要素を返す。Raceband で登録した F 帯を
Lowband 表示中に見ると、本物の L1 と区別できない。`slot_usable()` はそれを飛ばすので、
**見えているのに選べない**状態になる。

- **修正案:** `slot_label_update()` で帯域外を明示表示する

### 1.5 IMU 初期化失敗時もゲートを開ける

`main.c` の `ht_set_imu_ready()` は無条件。`enable_bmi270()` は `void` で失敗をログ
するだけ。失敗時は 100Hz の `SIGEV_THREAD` タイマーが毎回 `get_bmi270()` の無限ループ
スレッドを生み、`i2c_mutex` を占有する。純正と同じ挙動だが、ゲートがあるので直せる位置。

- **修正案:** `enable_bmi270()` に戻り値を持たせ、成功時のみ `ht_set_imu_ready()`
- **補足:** `ht.c` のコメント「FPGA とバスを共有」は誤り。BMI270 はポート 1、
  FPGA はポート 2。共有しているのは `i2c_mutex` だけ

---

## 2. リスク（状況次第で問題になる）

### 2.1 HDMI 経路が dispw の記録を迂回

`hardware.c` の `HDMI_in_detect()` は **7 箇所**で直接 `system_exec("dispw ...")` を
呼び、`vdpo_tmg` を直接代入する。`vdpo_pending` / `vdpo_applied_once` を触らない。

- HDMI オーバーレイ中に HDZero へ切り替えると、非同期 dispw と HDMI 側 dispw が
  同時実行され得る。`vdpo_collect()` は後勝ちを無視して pending 値で上書きする
- HDMI は `vdpo_applied_once` を立てないので、Skip Display Setup + HDMI 起動 +
  ソース切替で 1.1 秒の冗長な dispw が走る
- **修正案:** 7 箇所を `vdpo_set_timing()` 経由にする

### 2.2 Keep Tuner Alive のスタンバイ受信機が HDMI / OLED 保護中も通電

- `app_switch_to_hdmi_in()` は HDZero を閉じない（純正はメニューで reset していた）
- OLED 保護は `source_mode == HDZERO` のときだけ閉じるので、通常メニュー中（UI）の
  スタンバイ受信機は 1 分の保護中も通電し続ける
- **修正案:** `app_switch_to_hdmi_in()` と `OLED_MD_PRE_OFF` で `hdz_standby` なら
  `HDZero_Close()`

### 2.3 オーバーレイ中の OLED 保護 → 復帰時にメニューが約 2 秒固まる

`ht.c` は `source_mode == HDZERO` のまま 1 分無操作で受信機を閉じ、次の動きで
`DM6302_init` + `SetChannel` を**メインループ（`lvgl_mutex` 保持）で**実行する。

- **修正案:** `ht.c` の判定を `source_mode` ではなく `g_app_state` にし、
  メニュー表示中を UI 扱いにする

### 2.4 `thread_version` が `lvgl_mutex` なしで `lv_timer_handler()` を呼ぶ

`page_version.c` → `ui_main_menu.c` の `progress_bar_update()`。**LVGL をロック外で
触る唯一の経路**。純正から持ち越し。

### 2.5 Timed Long Press はリピートイベントが来たときしか判定しない

`input_device.c`。時間で判定するようにはなったが、判定に入るきっかけは今もカーネルの
キーリピート。100ms / 50ms の設定はリピート周期（推定 250ms 遅延 + 33ms）に丸められる。
`EV_REP` のないデバイスでは長押しがクリックに退化する。

### 2.6 Preload OSD Fonts が SD マウント前にフォントを読む可能性

`osd.c`。純正より 1〜2 秒早く SD を読む。マウントが遅いとカスタムフォントが黙って
内蔵にフォールバックする。ログの "failed!" で判別できる。

### 2.7 Skip Audio Setup のキャッシュは record デーモンがミキサーを触らない前提

`dvr.c` の前提はアプリ内では真だが、`mkapp/app/app/record` のバイナリ
（`audio-setup.txt` に `amixer cset` の記述がある）は**検査できない**。

### 2.8 見出しスクロールがコメント通りに動いていない

見出しパネルは HIDDEN でレイアウトされず、座標が親原点のまま。最初の呼び出しで
リスト先頭へ飛ぶ。現在は `page_speed_common.c` の `speed_scroll_to()` に引き継がれて
いる。**見た目だけの問題**。

### 2.9 確定前の編集がメモリだけに残る

`page_favorites.c` / `page_input_feel.c`。フォーカス中に長押しで抜けると INI に
保存されず、次回起動で戻る。純正の fans ページも同じ。

---

## 3. 改善案のバックログ

効果 ÷ リスク順。上位 5 件は実装済み。

| # | 案 | 見込み | リスク | 状態 |
|---|---|---|---|---|
| 1 | 受信機初期化を UI 構築と同時に開始 | 約 0.9〜1.0 秒 | 中 | **実装済 (Async Tuner Init)** |
| 2 | SPI_Write の 7 転送を 1 回の I2C_RDWR に | 0.6〜0.9 秒 | 中 | **実装済 (Burst Tuner Writes)** |
| 3 | 起動時 `wlan_stop.sh` をスキップ | 映像後 1.08 秒 | 低 | **実装済 (Skip WiFi Stop)** |
| 4 | EFUSE1 を 2 チップ同時読み | 0.33 秒 | 中 | **実装済 (Dual-Chip EFUSE Read)**、実測 −215ms |
| 5 | dispw を起動直後に開始 | 約 0.36 秒 | 中 | **実装済 (Early Video Timing)** |
| 6 | `audio_sel.sh` をスレッド化 / `amixer` 1 プロセス化 / `libasound` 直叩き | 切り替え 0.48 秒 | 低 | 未着手 |
| 7 | `aww` のフォークを `/sys/class/sunxi_dump/write` への書き込みに置換 | 0.13 秒+ | 低 | 未着手 |
| 8 | IT66021/IT66121/TP2825 初期化をスレッド化 | 0.15 秒 | 低 | 未着手 |
| 9 | 起動時の OLED on/off 往復をやめる | 60ms | 低〜中 | 未着手 |
| 10 | 1080p OSD セットを遅延生成 | 70〜80ms | 中 | 未着手 |
| 11 | `OLED_Startup` 温度テストの 2 回目読み出しを書き込み時のみに | 25〜50ms | 低 | 未着手 |
| 12 | `language_init` を使用言語のみ + LOGD 削除 | 45ms | なし | 未着手 |
| 13 | `pclk_phase` の cp/rm を `access()` で回避 | 26ms | なし | 未着手 |

**ただし起動側は打ち止め。** [measurements.md](measurements.md) の「3 本のトラックが
均衡している」を参照。映像までの 1440ms のうち dispw が 1117ms を占め、他は全部その
裏に隠れているので、6〜13 をやっても映像には出ない。効くのは切り替え側と常時負荷。

### 常時負荷（未着手、効果は大きい）

- **映像モード:** `osd.c` がアンテナアイコンを毎パス無条件で `lv_img_set_src()`。
  これは即 invalidate + BMP ヘッダ再読み込みで、`full_refresh=1` なので 100Hz 上限で
  全画面描画 + 3.75MB memcpy。差分判定で映像中のアイドル CPU をほぼゼロにできる
- **メニューモード:** SD カードラベルが `LONG_SCROLL_CIRCULAR`。267px を超えると
  スクロールアニメが毎ティック invalidate し、縮小メニュー全体（5.6MB ARGB レイヤー）
  を最大 100Hz で再生成する。`LONG_DOT` にするだけで解消（ただし 1.2 の問題と絡む）
- `ui_porting.c` の flush が `area->x1` を無視するため `full_refresh` が必須。
  部分描画に対応すれば RSSI アイコン更新が 64×64 のブリットで済む（中リスク）

---

## 4. 細かい点（記録のみ）

- `settings.h` の `ease_use_t` 内に `fast_menu` の説明コメントだけが取り残されている
- `hardware.c` で同じ文が 2 回
- `dm6302.c` のコメントが存在しないチェックを説明しており、初期化ごとに 10 転送の
  読み出しを追加している
- `app_state.c` / `settings.h` は `DM6302_init` を 2.3 秒と書くが実測は 1.8 秒
- `ui_throttle` で低電圧ビープの間隔が約 0.1 秒 → 1 秒に変わる（未記載の副作用）
- `msp_displayport.c` の `osd_traffic_report` は行ごとに memcmp して 5 秒ごとに LOGI
  するが、その結果を誰も使っていない
- `Display_UI()` の 4 つの switch mark は `system_exec` の計測と重複している
- 一部ファイルのモードが 100755 → 100644 に変わっている

---

## 5. 検証済みで問題なしと判断したもの

再検査の手間を省くための記録。

- INI キーの load/save 一致（favorites / speed / bugfix / input）
- 新ページの行数とパネル数、Back 行の選択可能性、ロック下でのコールバック実行
- **ロック順序** lvgl → hardware → i2c に逆転なし。`vdpo_start_timing_async()` は
  自分で `hardware_mutex` を取り、保持中に呼ばれる経路はない。非同期 dispw は同じ
  `app_switch_to_hdzero()` 内で必ず回収される（初期化失敗・default ケースを含む）
- Skip Display Setup は `rc.sh` が先に 1080p50 を設定しているので安全
- standby / open / close / 帯域変更 / sleep / scan の相互作用（帯域変更で再初期化、
  `go_sleep` と `Source_AV` は close して `hdz_standby` をクリア、
  `Display_VO_SWITCH(1)` が M0 を再オープン）
- `time.c` の CLOCK_MONOTONIC 化: 全呼び出し元は差分のみ使用。LVGL tick の 32bit
  オーバーフローも解消
- Async Motion Sensor: BMI270 はポート 1 で、同時に初期化される機器とは別ポート。
  ゲートは join 後に立つ
- Split UI Lock: 段間で共有する状態は handler が触らない（ただし効果は未計測）
