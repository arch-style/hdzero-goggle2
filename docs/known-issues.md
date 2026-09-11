# feature/fast-switch — 未修正の課題

このブランチのコードレビューで見つかり、**まだ直していない**もの。直したものは
[fork-changes.md](fork-changes.md) に、その根拠となる実測値は
[measurements.md](measurements.md) にある。

行番号は 2026-09-08 のレビュー時点のもので、その後の編集でずれている可能性がある。

---

## 1. 不具合

セクション 1 にあった 5 件は 2026-09-10 にすべて修正した。内容は
[fork-changes.md](fork-changes.md) の 4.8〜4.13 に移した。

新しく見つかった未修正のものはここに追記する。

### 1.1 I2C バスが 5 秒止まる — 原因確定、対策あり（既定値の判断だけ残る）

**2026-09-12 の実測で決着した。** 詳細は [measurements.md 5](measurements.md)。

| | 1.2MHz（純正） | 800kHz |
|---|---|---|
| 初期化 41 回 / 20 回中の詰まり | 4 回（5 件、各 ~500ms） | **0 回** |
| 正常時の初期化 | 768ms | 886ms（+118ms） |
| 平均（詰まり込み、タイムアウト 5 秒の純正換算） | 約 1380ms | 886ms |

V5 の TWI に duty ビットは無い（起動時に確認）。**既定は純正の 1.2M のまま、800k は
選択肢として残す**（2026-09-12 決定）。「全 OFF = 純正」を守る。使う人は
`Switch Speed → Tuner Bus` で 800k を選ぶ。

**追記: 1.2MHz では読み出しが黙って化けることがある**（[measurements.md 5.3](measurements.md)）。
179 回中 1 回、チップ 2 の校正値の指紋が違った。エラー無し。`Short I2C Timeout` では
防げない。**未対応の改善案:** 指紋を前回の値（または `setting.ini` に保存した既知の値）と
照合し、違ったら EFUSE を読み直す。読み直し 1 回は約 640ms だが、化けた校正で飛ぶより
安い。純正には指紋自体が無いので、そもそも気づけない。

---

#### （経緯）以前の記述

**2026-09-10 更新: 犯人が写った。**

```
04:03:31.792 [ERROR] i2c: port 2, addr 0x64, burst took 5006ms
04:03:31.792 [ERROR] i2c: port 2, thread 1102 waited 4915ms, released by thread 941
```

- 遅いのは**転送 1 回**。ポート 2 (メイン)、アドレス `0x64` = FPGA、burst 書き込み
- 待たされたのは thread 1102 = `peripheral` (HDZERO/AV/HDMI 検出の周期スレッド)
- 5 秒握っていた thread 941 = DM5680 の右 UART スレッド。**右ボタンの処理は
  このスレッドで走る** (`select()` のタイムアウト分岐が `rbtn_click()` を呼ぶ)
- **`SPI: burst refused` は出ていない** → この転送は 5 秒かけて**成功**している

5.006 秒で成功、という形はカーネル I2C ドライバの挙動そのもの: 割り込みを取り
こぼす → `adap->timeout` (このアダプタの既定は 5 秒) を待つ → コントローラを
リセット → 再送して成功。その 5 秒の間、バスロックは握られたままなので**ポート 2
全体が止まる**。起動が 13.2 秒かかったときの 4.6 秒 / 5.1 秒の沈黙も同じ形。

**2026-09-10 追記 (3 セッション目): 遅いのは必ず burst。**

転送 1 回ごとの計測を全経路に入れた状態で、報告されたのは**すべて
`addr 0x64, burst`** だった。単発の読み書きが 200ms を超えたことは一度も無い。

| 回 | 場所 | 時間 |
|---|---|---|
| 1 | 使用中 (右ボタン → チューナ初期化) | 5006ms |
| 2 | 起動中 `DM6302_M0()` の途中 (`M0 write 64→128`) | 5009ms |
| 3 | 使用中、バスを 1MHz にした直後 | 5076ms |

さらに `SPI: burst refused (errno 70)` = `ECOMM` も 1 回出た (再送で成功)。
**7 レジスタを 1 トランザクションで送る形そのものが、このバスで時々失敗する。**

推定される深い原因: バス速度の切り替えは `aww 0x05002814` で
**I2C コントローラのクロック分周レジスタを直接叩いている** (純正から)。
ドライバはクロックが変わったことを知らないまま次の転送を組み立てる。
7 メッセージの連続転送はその影響を最も受けやすい。

**対処 2 つ:**

1. **Short I2C Timeout** (`Fixes`、既定 OFF): アダプタのタイムアウトを 5 秒 → 500ms。
   取りこぼし自体は減らないが、被害が 1/10 になる。**まだ実機で ON にして
   試していない**
2. **burst の自動停止** (`f243ee1`、常時): burst が 500ms 以上かかったら数え、
   2 回で**そのセッションの burst を止める**。既存の「20 回拒否されたら止める」
   と同じ仕組みに、「成功したが遅い」を追加したもの。ドライバは成功を返すので、
   時間を測る以外に気づく方法が無い

**次の切り分け:** それでも 5 秒が出るなら `Burst Tuner Writes` を OFF にして
比較する (起動が 0.6〜0.9 秒遅くなる代わりに、この形の転送が消える)。

**2026-09-12 追記: 「1MHz」は 1.2MHz だった。実験の道具を入れた。**

sunxi TWI のクロック式 `SCL = 24MHz / (2^N × (M+1) × 10)` で純正の `0x08` を解くと
**1200kHz**。I2C Fast-mode Plus の上限 1000kHz、Allwinner が保証する 400kHz の
どちらも超えている。純正コメントの `0x58 = 200KHz` が式と一致するのが 24MHz の裏付け。
本家は 2026-04-30 (#609) にこの引き上げ自体を撤去し、issue #598
(アンテナ 1 本が死ぬ) が消えたと 2 名が確認している。

fork に入れたもの (`Switch Speed` ページ、既定は純正と同じ):

| 行 / ログ | 内容 |
|---|---|
| `Tuner Bus` 1.2M / 800k / 200k | 初期化中のバス速度。800k (`0x10`) は規格内の最速で**誰も試していない** |
| `Tuner Bus 40% Duty` | H616 系 TWI にはビット 7 = CLK_DUTY (既定 40%) がある。純正はレジスタ全体に `0x08` / `0x58` を書くので、ビットがあれば duty を 50% に変えたまま放置していたことになる |
| `i2c: TWI2 CCR as the kernel left it 0x.. = ..kHz, duty bit present/absent` | 起動時に 1 回。カーネルの既定値と、ビット 7 の実在 (書いて読み戻す) |
| `twi: tuner bus 800kHz (CCR 0x10)` | 初期化ごとに、実際に書いた値 |

**やること:** Wide/Narrow ボタンは押すたびに `DM6302_init()` を丸ごと走らせるので、
設定ごとに 20 回押して `SPI: burst refused` / `SPI: burst took` / `i2c: … took` の
件数を数えれば、再起動なしで比較できる。順番は 0-1 (既定値を読む) → 800k → 
ビットがあれば 1.2M+40%。

---

### 1.2 起動時の I2C 競合が再発する（相手が不明）

[fork-changes.md 4.2](fork-changes.md) の対処 (`1d62e02`) を入れた後も、
2026-09-10 の起動で `boot step: wait for the tuner bus 10080ms` / boot total
13222ms が出た。メインスレッドは 10 秒間 join で待っていたので **OLED ではない**。
`DM6302_init()` の中で 2 回、ログが沈黙する空白がある (Init14 の前に 4.6 秒、
M0 転送 64→128 の間に 5.1 秒)。

計測は入れた (`6c44c0e`)。`i2c_bus_lock()` の待ちが 100ms を超えると、ポート・
待ったスレッド・渡したスレッドを出す。**次の再発ログを待つ。**

- 出なかった場合: バス待ちではなく、カーネルドライバか FPGA 側の応答が遅い。
  1 転送ごとの計測に降りる必要がある
- 出た場合: スレッド ID を `boot: main thread` /
  `HDZero: async open on thread` と突き合わせれば相手が分かる

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
