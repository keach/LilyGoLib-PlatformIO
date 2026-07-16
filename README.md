# T-Watch S3 Custom Clock Firmware

Upstream project: [Xinyuan-LilyGO/LilyGoLib-PlatformIO](https://github.com/Xinyuan-LilyGO/LilyGoLib-PlatformIO)

[English](#english) | [日本語](#日本語)

## English

### Overview

This project develops a custom clock firmware for the LilyGo T-Watch S3 while
keeping the upstream factory firmware available as a separate PlatformIO
environment. Custom screens and features live in the local `src` directory and
can be developed without overwriting the factory application sources.

The current custom firmware uses English labels and Japan Standard Time (JST,
UTC+9).

### Implementation status

| Area | Status | Details |
| --- | --- | --- |
| Build foundation | Implemented and build-tested | Pinned Arduino-ESP32 3.3.8 toolchain and separate factory/custom PlatformIO environments |
| Clock face | Implemented and build-tested | Fixed-position `HH:MM:SS`, English weekday/date, and stable per-field centering |
| RTC | Implemented and device-tested | RTC refresh whenever the clock face appears and a separate manual date/time screen |
| Display timeout | Implemented and device-tested | 15-second clock timeout, 60-second settings timeout, and guarded touch wake |
| Light Sleep | Implemented and device-tested | Sleep 5 seconds after display-off and wake by touch or power button |
| Battery status | Implemented and device-tested | Compact always-visible upper-left battery, charging, USB-power, and low-battery state |
| Wi-Fi and NTP | Implemented; basic connection device-tested | A configured 2.4 GHz network connects on the device. Multi-network fallback, NTP persistence, and radio shutdown still need explicit verification |
| Brightness setting | Implemented and device-tested | Separate live-preview screen with `SAVE`/`CANCEL` and NVS persistence |
| Documentation | Implemented | Project-specific English and Japanese README |

### Roadmap

#### Verify the current firmware on the device

- [x] Create the Git-ignored `include/wifi_credentials.h` with real networks.
- [x] Deploy the current `t-watch-s3-custom` HEAD to the watch.
- [x] Verify connection to a configured 2.4 GHz Wi-Fi network.
- [x] Verify touch/power wake after Light Sleep.
- [ ] Verify multi-network Wi-Fi selection, NTP-to-RTC synchronization, and
  Wi-Fi shutdown.
- [x] Verify brightness persistence after reboot.
- [x] Verify the upper-left battery status.
- [ ] Verify the fixed clock layout and timed bottom notifications.

#### Next milestone: settings and synchronization

- [ ] Add a settings hub for navigating independent settings screens.
- [ ] Group the existing date/time and brightness screens under the settings
  hub.
- [ ] Add a time synchronization screen with `SYNC NOW`, automatic sync
  enable/disable, and the last synchronization result and time.
- [ ] Retry a failed Wi-Fi/NTP operation after 15 minutes without requiring a
  display wake.

#### Planned persistent settings

- [ ] Configurable clock-screen timeout
- [ ] Configurable settings-screen timeout
- [ ] Configurable delay before Light Sleep
- [ ] Automatic NTP synchronization enable/disable
- [ ] 12-hour/24-hour clock selection
- [ ] Restore-default-settings action

#### Later candidates

These are possible extensions and are not yet committed requirements.

- Japanese UI or selectable display language
- Selectable timezone instead of fixed JST
- Alarm, timer, and stopwatch functions

### PlatformIO environments

| Environment | Purpose | Source |
| --- | --- | --- |
| `twatchs3` | Upstream factory firmware | LilyGoLib factory example |
| `twatchs3_custom` | This custom clock firmware | `src/main.cpp` |

The factory and custom environments remain independent and can be built in
parallel.

### Requirements

- LilyGo T-Watch S3
- USB data cable
- Visual Studio Code with PlatformIO, or PlatformIO Core
- A supported Python installation for PlatformIO

The project pins the pioarduino ESP32 platform that provides Arduino-ESP32
3.3.8, as required by the current LilyGoLib display APIs.

### Wi-Fi configuration

Copy the example credentials file:

```sh
cp include/wifi_credentials.example.h include/wifi_credentials.h
```

Edit `include/wifi_credentials.h` and add trusted networks in priority order:

```cpp
inline constexpr WiFiCredential kWiFiCredentials[] = {
    {"HOME_WIFI", "home-password"},
    {"MOBILE_HOTSPOT", "hotspot-password"},
};
```

Add or remove entries as needed, then keep `kWiFiCredentialCount` as shown in
the example file. The real credentials file is excluded from Git. When it is
absent, the firmware still builds and operates with Wi-Fi/NTP disabled.

The ESP32-S3 supports 2.4 GHz Wi-Fi only. Configure a 2.4 GHz network or a
dual-band SSID that is also available on 2.4 GHz; a 5 GHz-only SSID cannot be
used.

### Build

Build the custom firmware:

```sh
pio run -e twatchs3_custom
```

The main output is generated at:

```text
.pio/build/twatchs3_custom/firmware.bin
```

The upstream factory firmware can still be built separately:

```sh
pio run -e twatchs3
```

### Upload to the watch

Connect the watch over USB and run:

```sh
pio run -e twatchs3_custom -t upload
```

If automatic port detection is unavailable, specify the port explicitly:

```sh
pio run -e twatchs3_custom -t upload --upload-port /dev/cu.usbmodemXXXXXX
```

If upload reports `No serial data received` while the installed firmware is in
Light Sleep, wake the display and run the upload command again.

### Clock and power behavior

- Use `BRI` on the clock screen to open the brightness screen.
- Brightness changes are previewed immediately; `SAVE` persists the value to
  NVS and `CANCEL` restores the previous value.
- The clock screen turns off after 15 seconds of inactivity.
- The settings screen turns off after 60 seconds of inactivity.
- Light Sleep begins 5 seconds after the display turns off.
- A touch or a short power-button press wakes the watch.
- The wake touch is consumed so it does not accidentally activate a control.
- RTC time and battery state are refreshed after wake.
- A compact battery or power status remains visible at the upper left.

### Wi-Fi and NTP behavior

- The last successful NTP synchronization epoch is stored in ESP32 NVS.
- Synchronization is due when no history exists, RTC time is invalid, at least
  24 hours have elapsed, or the clock has moved behind the previous sync time.
- Wi-Fi is powered only when synchronization is due.
- Registered networks are attempted in order, with 10 seconds per network.
- NTP is allowed 15 seconds after a Wi-Fi connection is established.
- Failed attempts are suppressed for 15 minutes and retried after a later
  display wake.
- A successful NTP result is written to the hardware RTC, persisted to NVS,
  and followed by Wi-Fi shutdown to reduce power consumption.
- Wi-Fi and NTP activity appears as a bottom notification, such as
  `CONNECTING WIFI 1/2`, `SYNCING TIME`, or `TIME SYNCED`.
- Success notifications disappear after 3 seconds; failure and configuration
  warnings disappear after 8 seconds. A current NTP state produces no notice.

### Security notes

- Never commit `include/wifi_credentials.h`.
- Do not put real SSIDs or passwords in the example file.
- Verify `git status` before committing or pushing changes.

---

## 日本語

### 概要

このプロジェクトでは、LilyGo T-Watch S3向けの独自時計ファームウェアを
開発します。上流の工場出荷ファームウェアは別のPlatformIO環境として維持し、
独自の画面と機能はローカルの`src`ディレクトリで開発します。そのため、工場出荷
アプリケーションのソースを上書きせず、平行して保守できます。

現在のカスタムファームウェアは英語表示で、日本標準時（JST、UTC+9）を使用します。

### 実装状況

| 領域 | 状況 | 内容 |
| --- | --- | --- |
| ビルド基盤 | 実装・ビルド確認済み | Arduino-ESP32 3.3.8の固定と、factory/customを分離したPlatformIO環境 |
| 時計画面 | 実装・ビルド確認済み | 位置を固定した`HH:MM:SS`、英語の曜日・日付、時・分・秒ごとの中央揃え |
| RTC | 実装・実機確認済み | 時計画面表示時のRTC再読込と、独立した日付・時刻手動設定画面 |
| 画面消灯 | 実装・実機確認済み | 時計15秒、設定60秒のタイムアウトと、誤操作を防ぐタッチ復帰 |
| Light Sleep | 実装・実機確認済み | 消灯5秒後に移行し、タッチまたは電源ボタンで復帰 |
| バッテリー状態 | 実装・実機確認済み | 左上に常時表示する簡潔な残量、充電、USB給電、低残量表示 |
| Wi-Fi・NTP | 実装済み、基本接続は実機確認済み | 設定した2.4 GHzネットワークへの接続を実機確認済み。複数ネットワークの切り替え、NTP履歴の永続化、Wi-Fi停止は明示的な確認が必要 |
| 明るさ設定 | 実装・実機確認済み | 即時プレビュー、`SAVE`/`CANCEL`、NVS永続化を備えた独立画面 |
| ドキュメント | 実装済み | このプロジェクト専用の英語・日本語README |

### ロードマップ

#### 現行ファームウェアの実機確認

- [x] Git管理外の`include/wifi_credentials.h`へ実際のネットワークを設定する。
- [x] 現在の`t-watch-s3-custom` HEADを実機へデプロイする。
- [x] 設定した2.4 GHz Wi-Fiネットワークへの接続を確認する。
- [x] Light Sleep後のタッチ・電源ボタン復帰を確認する。
- [ ] 複数Wi-Fiの選択、NTPからRTCへの同期、同期後のWi-Fi停止を確認する。
- [x] 再起動後も明るさ設定が保持されることを確認する。
- [x] 左上のバッテリー状態を確認する。
- [ ] 固定幅の時計表示と時間制御された下段通知を確認する。

#### 次のマイルストーン：設定と時刻同期

- [ ] 独立した設定画面へ移動するための設定ハブを追加する。
- [ ] 既存の日付・時刻設定と明るさ設定を設定ハブへまとめる。
- [ ] `SYNC NOW`、自動同期の有効・無効、最終同期結果・時刻を備えた
  時刻同期設定画面を追加する。
- [ ] Wi-Fi・NTP失敗から15分後、画面復帰を必要とせず再試行する。

#### 実装予定の設定永続化

- [ ] 時計画面の消灯時間設定
- [ ] 設定画面の消灯時間設定
- [ ] Light Sleepまでの待機時間設定
- [ ] NTP自動同期の有効・無効
- [ ] 12時間・24時間表示の選択
- [ ] 設定を初期値へ戻す操作

#### 将来の候補

以下は拡張候補であり、まだ確定要件ではありません。

- 日本語UIまたは表示言語の選択
- JST固定ではなくタイムゾーンを選択する設定
- アラーム、タイマー、ストップウォッチ

### PlatformIO環境

| 環境 | 用途 | ソース |
| --- | --- | --- |
| `twatchs3` | 上流の工場出荷ファームウェア | LilyGoLibのfactory example |
| `twatchs3_custom` | このプロジェクトの時計ファームウェア | `src/main.cpp` |

工場出荷版とカスタム版は独立しており、平行してビルドできます。

### 必要なもの

- LilyGo T-Watch S3
- データ通信対応USBケーブル
- Visual Studio CodeとPlatformIO、またはPlatformIO Core
- PlatformIOが対応するPython環境

現在のLilyGoLibの画面APIに必要なArduino-ESP32 3.3.8を使用するため、
pioarduinoのESP32プラットフォームを固定しています。

### Wi-Fi設定

認証情報のサンプルをコピーします。

```sh
cp include/wifi_credentials.example.h include/wifi_credentials.h
```

`include/wifi_credentials.h`を編集し、優先順位の高い順にネットワークを
登録します。

```cpp
inline constexpr WiFiCredential kWiFiCredentials[] = {
    {"HOME_WIFI", "home-password"},
    {"MOBILE_HOTSPOT", "hotspot-password"},
};
```

必要に応じて項目を追加・削除し、`kWiFiCredentialCount`はサンプルと同じ
計算式のまま使用してください。実際の認証情報ファイルはGit管理から除外されます。
ファイルが存在しない場合もビルドでき、Wi-Fi・NTP同期なしで時計が動作します。

ESP32-S3が対応するWi-Fiは2.4 GHz帯のみです。2.4 GHzのネットワーク、または
2.4 GHzでも提供される共通SSIDを設定してください。5 GHz専用SSIDには接続できません。

### ビルド

カスタムファームウェアをビルドします。

```sh
pio run -e twatchs3_custom
```

主な生成物は次の場所に作成されます。

```text
.pio/build/twatchs3_custom/firmware.bin
```

上流の工場出荷ファームウェアも別環境でビルドできます。

```sh
pio run -e twatchs3
```

### 実機への書き込み

時計をUSB接続して、次を実行します。

```sh
pio run -e twatchs3_custom -t upload
```

ポートが自動検出されない場合は明示的に指定します。

```sh
pio run -e twatchs3_custom -t upload --upload-port /dev/cu.usbmodemXXXXXX
```

インストール済みファームウェアがLight Sleep中で、書き込み時に
`No serial data received`と表示された場合は、画面を復帰させてから書き込み
コマンドを再実行してください。

### 時計・省電力動作

- 時計画面の`BRI`から明るさ設定画面を開きます。
- 明るさは操作中に即時反映され、`SAVE`でNVSへ保存、`CANCEL`で変更前の値へ
  戻ります。
- 時計画面は15秒間操作がないと消灯します。
- 設定画面は60秒間操作がないと消灯します。
- 消灯から5秒後にLight Sleepへ移行します。
- タッチまたは電源ボタンの短押しで復帰します。
- 復帰に使ったタッチは吸収され、背後のボタンを誤操作しません。
- 復帰時にRTC時刻とバッテリー状態を更新します。
- 左上には簡潔なバッテリー・給電状態を常時表示します。

### Wi-Fi・NTP動作

- 最後にNTP同期した時刻をESP32のNVSへ保存します。
- 同期履歴がない、RTC時刻が無効、前回同期から24時間以上経過、または時計が
  前回同期時刻より前へ戻った場合に同期が必要と判定します。
- 同期が必要な場合だけWi-Fiを起動します。
- 登録されたネットワークを順番に試し、1ネットワークにつき10秒待ちます。
- Wi-Fi接続後、NTPの応答を15秒待ちます。
- 全候補が失敗した場合は15分間再試行を抑制し、その後の画面復帰時に再試行します。
- NTP同期成功後はハードウェアRTCとNVSを更新し、省電力のためWi-Fiを停止します。
- Wi-Fi・NTPの動作は下段へ`CONNECTING WIFI 1/2`、`SYNCING TIME`、
  `TIME SYNCED`などの通知として表示します。
- 成功通知は3秒、失敗・未設定通知は8秒で消えます。NTPが最新の定常状態では
  通知を表示しません。

### セキュリティ上の注意

- `include/wifi_credentials.h`をコミットしないでください。
- サンプルファイルに実際のSSIDやパスワードを書かないでください。
- コミットやpushの前に`git status`を確認してください。
