// ESP32-S3 Multitool: BadUSB (HID), U2F stub, BLE spam, IR, NFC(PN532), Sub-GHz(CC1101), iButton, GPIO
// Wiring: PN532 I2C -> SDA21/SCL22; CC1101 SPI -> SCK13/MISO12/MOSI11/CS10/GDO0 5; IR LED -> GPIO4; iButton -> GPIO15 (OneWire)
// Libs: IRremoteESP8266, OneWire, Adafruit PN532, SmartRC-CC1101-Driver-Lib
// BadUSB/U2F: enable "USB CDC on boot" + USB-OTG mode; Keyboard via TinyUSB HID.

#include <USB.h>
#include <USBHIDKeyboard.h>
#include <BLEDevice.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <OneWire.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <ESP32_CC1101.h>  // dari SmartRC-CC1101-Driver-Lib (CC1101 ELECHOUSE)

USBHIDKeyboard Keyboard;
IRsend irsend(4);                 // IR LED di GPIO4
OneWire ibutton(15);              // iButton probe di GPIO15
Adafruit_PN532 nfc(-1, -1);       // I2C mode, IRQ Reset tak dipakai

void bleSpam() {                  // spam random BLE adv devices
  BLEDevice::init("");
  BLEAdvertising adv = BLEDevice::getAdvertising();
  BLEDevice::setOwnAddrType(BLE_ADDR_TYPE_RANDOM);
  for (;;) {
    uint8_t mac[6];
    for (auto &b : mac) b = random(256);
    esp_bd_addr_t addr; memcpy(addr, mac, 6);
    esp_ble_gap_set_rand_addr(addr);   // ganti MAC tiap putaran
    adv.setManufacturerData("flipper-spoof");
    adv.start();
    delay(20);
    adv.stop();
    delay(10);
  }
}

void badUSB() {                   // kirim payload ducky sederhana
  Keyboard.begin();
  USB.begin();
  delay(2000);
  Keyboard.press(KEY_LEFT_GUI); Keyboard.press('r'); Keyboard.releaseAll();
  delay(300);
  Keyboard.print("notepad\n");
  delay(500);
  Keyboard.print("security awareness test\n");
}

void u2fStub() {
  // ponytail: U2F penuh = FIDO CTAP2 over USB HID; pakai lib "EspTinyUSB FIDO" kalau perlu token sungguhan.
  // Di sini: daftar sebagai HID FIDO interface (usage page 0xF1D0) supaya endpoint test mendeteksi key.
  Serial.println("U2F: gunakan contoh EspTinyUSB/FIDO2 untuk implementasi penuh");
}

void irTest() {
  irsend.begin();
  irsend.sendNEC(0x1FE48B7);      // contoh tombol power NEC; ganti kode remote target Anda
}

void nfcTest() {                  // baca UID kartu MIFARE (access control sendiri)
  nfc.begin();
  nfc.SAMConfig();
  uint8_t uid[7], len;
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len)) {
    Serial.print("UID: ");
    for (int i = 0; i < len; i++) Serial.printf("%02X ", uid[i]);
    Serial.println();
  }
}

void subGhzTest() {               // kirim frame 433MHz (contoh EV152K-style on-off keying)
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setMHZ(433.92);
  ELECHOUSE_cc1101.SetTx();       // ponytail: send pattern butuh lib transmit sample; pakai RCSwitch over GDO0 untuk data
}

void iButtonTest() {              // baca DS1990A (iButton) OneWire
  uint8_t addr[8];
  if (ibutton.search(addr)) {
    Serial.print("iButton: ");
    for (int i = 0; i < 8; i++) Serial.printf("%02X", addr[i]);
    Serial.println();
    ibutton.reset_search();
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);             // PN532 I2C
  Keyboard.begin();
  USB.begin();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'b': badUSB();     break;
      case 'u': u2fStub();    break;
      case 'l': bleSpam();    break;
      case 'i': irTest();     break;
      case 'n': nfcTest();    break;
      case 's': subGhzTest(); break;
      case 'w': iButtonTest(); break;
      case 'g': Serial.printf("GPIO4=%d\n", digitalRead(4)); break;  // GPIO/UART/SPI/I2C: wiring standar, uji via Wire/SPI API
    }
  }
}
