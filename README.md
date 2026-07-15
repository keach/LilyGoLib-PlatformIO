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

### Current features

- Clock display with hours, minutes, and seconds
- English weekday and date display
- RTC synchronization whenever the clock screen is shown
- Manual date and time settings screen
- Separate display brightness screen with persistent NVS storage
- Automatic display timeout
- Light Sleep after the display turns off
- Wake by touch panel or power button
- Battery percentage, charging, full, USB-powered, and low-battery states
- Multiple fixed Wi-Fi network registrations
- NTP synchronization with a persistent 24-hour interval
- Automatic Wi-Fi shutdown after time synchronization

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
- The clock screen reports states such as `WIFI 1/2`, `NTP SYNCING`,
  `NTP SYNCED`, and `NTP CURRENT`.

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

### 現在の機能

- 時・分・秒を表示する時計画面
- 英語の曜日・日付表示
- 時計画面が表示されるたびにRTCと同期
- 日付と時刻の手動設定画面
- NVSへ設定を保存する独立した画面明るさ設定
- 画面の自動消灯
- 消灯後のLight Sleep
- タッチパネルまたは電源ボタンによる復帰
- バッテリー残量、充電中、満充電、USB給電、残量低下の表示
- 複数の固定Wi-Fiネットワーク登録
- 前回同期時刻を保持する24時間間隔のNTP同期
- 時刻同期後のWi-Fi自動停止

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

### Wi-Fi・NTP動作

- 最後にNTP同期した時刻をESP32のNVSへ保存します。
- 同期履歴がない、RTC時刻が無効、前回同期から24時間以上経過、または時計が
  前回同期時刻より前へ戻った場合に同期が必要と判定します。
- 同期が必要な場合だけWi-Fiを起動します。
- 登録されたネットワークを順番に試し、1ネットワークにつき10秒待ちます。
- Wi-Fi接続後、NTPの応答を15秒待ちます。
- 全候補が失敗した場合は15分間再試行を抑制し、その後の画面復帰時に再試行します。
- NTP同期成功後はハードウェアRTCとNVSを更新し、省電力のためWi-Fiを停止します。
- 時計画面には`WIFI 1/2`、`NTP SYNCING`、`NTP SYNCED`、
  `NTP CURRENT`などの状態を表示します。

### セキュリティ上の注意

- `include/wifi_credentials.h`をコミットしないでください。
- サンプルファイルに実際のSSIDやパスワードを書かないでください。
- コミットやpushの前に`git status`を確認してください。
