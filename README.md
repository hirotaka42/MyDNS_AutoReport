# How-To

https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html

エイリアス登録済みなら、最初に実行
```
get_idf
```

```
cd ./MyDNS_AutoReport
idf.py set-target esp32 // ターゲットを設定
idf.py menuconfig
```

必須設定
まず、menuconfig で以下の設定を有効にする:
```
idf.py menuconfig
Component config → LWIP → Enable IPv6
Component config → LWIP → Enable IPv6 autoconfiguration
Component config → mbedTLS → TLS versions supported → Enable TLS 1.2
```

よく使うコマンドまとめ

|コマンド|説明|
|--|--|
|idf.py build|ビルドのみ|
|idf.py flash|書き込み|
|idf.py monitor|シリアルログ表示|
|idf.py flash monitor|一括書き込み＋ログ表示|
|idf.py menuconfig|設定UIを開く|



実際に使用するコマンド

```
idf.py build
idf.py -p /dev/tty.usbserial-71D69AA2AD -b 115200 flash
idf.py -p /dev/tty.usbserial-71D69AA2AD monitor
```