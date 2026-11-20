#!/usr/bin/env python3
"""0xorin host CLI — kontrol firmware 0xorin (Maruder port) via serial.

Usage:
  python3 0xorin.py --port /dev/ttyUSB0                # menu interaktif
  python3 0xorin.py --port /dev/ttyUSB0 --cmd scanap   # sekali jalan

Dependency: pip install pyserial
"""
import argparse
import time
import sys

import serial  # pyserial

COMMANDS = {
    "scanap": "scan access point",
    "ssids": "daftar SSID hasil scan",
    "select  <n>": "pilih AP/client",
    "attack -d": "deauth semua",
    "attack -t": "deauth targeted (per SSID terpilih)",
    "attack -r": "rogue AP",
    "attack -b": "beacon flood",
    "scansta": "scan station/client",
    "attack -c": "deauth client",
    "blespam": "BLE spam (sour apple)",
    "stopscan": "stop scan",
    "clearap": "reset hasil scan",
    "help": "daftar perintah firmware",
}


def open_port(port: str, baud: int = 115200) -> serial.Serial:
    try:
        return serial.Serial(port, baud, timeout=1)
    except serial.SerialException as e:
        sys.exit(f"[!] gagal buka {port}: {e}")


def read_serial(ser: serial.Serial, seconds: float = 2.0) -> str:
    ser.timeout = 0.2
    end = time.time() + seconds
    out = []
    while time.time() < end:
        line = ser.readline()
        if line:
            out.append(line.decode(errors="replace").rstrip())
            end = time.time() + 0.5
    return "\n".join(out)


def main() -> None:
    p = argparse.ArgumentParser(description="0xorin host controller")
    p.add_argument("--port", required=True)
    p.add_argument("--cmd", help="kirim satu perintah lalu keluar")
    p.add_argument("--baud", type=int, default=115200)
    args = p.parse_args()

    ser = open_port(args.port, args.baud)
    time.sleep(2)  # tunggu board reset saat port dibuka
    read_serial(ser, 1)

    if args.cmd:
        ser.write((args.cmd + "\n").encode())
        print(read_serial(ser, 3.0))
        return

    print("0xorin @ " + args.port + " | perintah: " +
          ", ".join(f"{k}" for k in COMMANDS) + " | q=keluar")
    while True:
        c = input("> ").strip()
        if c == "q":
            break
        ser.write((c + "\n").encode())
        print(read_serial(ser, 3.0))


if __name__ == "__main__":
    main()
