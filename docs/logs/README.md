# 引用されているログ

このディレクトリにあるのは、ほかの文書が数字の根拠として名指ししているゴーグルの
ログだけ。`gzip` のまま置いてあるので `zless` / `gzcat` で読む。

| ファイル | 何のログか | 引用元 |
|---|---|---|
| `slow-boot-i2c-13222ms.log.gz` | 起動に 13.2 秒かかった回。`wait for the tuner bus 10080ms`、`DM6302_init()` の中に 4.6 秒と 5.1 秒の空白 | [fork-changes.md 4.2](../fork-changes.md)、[known-issues.md 1.1](../known-issues.md) |
| `tuner-bus-800k-vs-1200k.log.gz` | `Tuner Bus` を 800k で 20 回、1.2M で 41 回初期化した回。TWI2 CCR の既定値と duty ビット不在の確認も含む | [measurements.md 5](../measurements.md) |
| `recording-fix-verified-2610ms.log.gz` | 録画中にソース切替と Wide/Narrow を操作し、`record stop collected took 1691/1953/2012ms` を確認した回 | [fork-changes.md 4.8](../fork-changes.md) |

**ゴーグルの時計は当てにならない。** RTC に電池が無く毎回同じ値に戻るので、日付は
どのログも同じに見える。起動間の比較は `boot total`（単調時計）で行うこと。

## 全部のログ

ゴーグルは 999 回分持つ (`HDZGOGGLE.log` と `boot-logs/HDZGOGGLE.NNNN.log`)。
それでも有限なので、カードを PC に挿したら

```sh
utilities/save-goggle-logs.sh
```

を実行する。チェックサムで重複を弾くので、何度実行しても増えない。保存先は
`logs/` (git 管理外)。あとから文書で引用することになったものを、ここへ移す。

他人のログは `utilities/save-goggle-logs.sh <ファイル>` で取り込む。ファイル名が
そのまま保存名のラベルになるので、誰のものか分かる名前にしてから渡すとよい。
ログの 1 行目 (`build: ...`) がどのバイナリの出力かを示す。
