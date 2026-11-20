# 0xorin

Toolkit pentest ESP32 untuk WiFi, Bluetooth/BLE, dan perangkat IoT — dikembangkan dari nol, meniru perilaku toolkit serupa (clean-room, tanpa kode pihak lain).

## Isi

| Folder | Board | Fitur |
|---|---|---|
| `esp32_jammer` | ESP32 classic (WROOM-32D) | WiFi scan + deauth (broadcast/per-client), channel hop, BT classic inquiry spam |
| `esp32s3_multitool` | ESP32-S3 | BadUSB (HID), U2F (stub), BLE spam, IR, NFC (PN532), Sub-GHz (CC1101), iButton, GPIO |
| `0xorin.py` | PC (host) | CLI serial untuk mengendalikan kedua board |

## Kontrol serial

**Jammer (115200):** `s`=scan, `a`=deauth semua AP, `0-9`=pilih AP, `c`=deauth client, `h`=hop, `j`=BT on/off, `x`=stop.

**S3 (115200):** `b`=BadUSB, `u`=U2F, `l`=BLE spam, `i`=IR, `n`=NFC, `s`=Sub-GHz, `w`=iButton, `g`=GPIO.

**Host:** `python3 0xorin.py --port /dev/ttyUSB0 --fw jammer` (perlu `pip install pyserial`).

## Build

Arduino IDE, board "ESP32 Dev Module" (classic) / "ESP32S3 Dev Module" (USB CDC on boot).

## Legal

Jamming RF butuh izin. Uji hanya pada perangkat sendiri/yang diberi izin.
