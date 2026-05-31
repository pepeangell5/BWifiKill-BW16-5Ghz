#include "BeaconSpam.h"

#include <WiFi.h>
#include <wifi_conf.h>
#include <string.h>

#include "packet-injection.h"

// ===========================================================================
// Lista de SSIDs para el beacon spam.
// Editable: agrega/quita/cambia lo que quieras antes de flashear.
// Maximo 32 bytes en UTF-8 cada uno (los emojis usan 3-4 bytes).
// ===========================================================================
static const char *const SSID_LIST[] = {
  "\xF0\x9F\x91\xB9" "Eres_Un_Pendejo",
  "\xF0\x9F\x92\x80" "Wifi_Para_Pendejos",
  "\xF0\x9F\x98\x88" "Wifi_Gratis",
  "\xF0\x9F\x93\xA1" "Ey_Tu_La_De_Negro",
  "\xF0\x9F\xA4\x96" "Robot_Sexual_Activado",
  "\xF0\x9F\x91\xBE" "Te_Estoy_Viendo",
  "\xF0\x9F\x94\x9E" "Sin_Adultos_Aqui",
  "\xF0\x9F\x8C\xAE" "Sin_Salsa_No_Sirve",
  "\xF0\x9F\xA7\xA0" "Cerebro_De_Mocos",
  "\xF0\x9F\x90\x8D" "Conectate_y_te_hackeo",
  "\xF0\x9F\x92\xA5" "El_Diablo_Te_Bendiga",
  "\xF0\x9F\x91\xBD" "Paga_tu_internet",
  "\xF0\x9F\x94\xA5" "Pinche_Pobre",
  "\xF0\x9F\x96\x95" "Chupa_Limon_Kbron",
  "\xF0\x9F\xA4\xA1" "Payaso_El_Que_Se_Conecte",
  "\xF0\x9F\xA7\xBC" "Banate_Cochino",
  "\xF0\x9F\xA7\xA8" "Cuidado_Pedo_Toxico",
  "\xF0\x9F\x91\xAE" "Patrulla_Espacial_69",
  "\xF0\x9F\xA4\xAE" "Tu_Cara_Da_Asquito",
  "\xF0\x9F\x8C\x9A" "Me_Gustas_Cuando_Callas",
  "\xF0\x9F\x90\xA2" "Mi_Wifi_Es_Mas_Lento",
  "\xF0\x9F\x91\xBB" "Wifi_Embrujado_2010",
  "\xF0\x9F\xAA\xA9" "Discoteca_Sin_Musica",
  "\xF0\x9F\xA5\x92" "Pepino_Master_Hacker",
  "\xF0\x9F\xA6\x96" "Internet_Jurasico",
  "\xF0\x9F\x9B\xB8" "Ovni_En_Tu_Patio",
  "\xF0\x9F\x9A\x9C" "Tractor_Con_Wifi",
  "\xF0\x9F\x8E\x82" "Pastel_Sin_Velitas",
  "\xF0\x9F\x93\xBA" "Solo_Tengo_Canal_5",
  "\xF0\x9F\x9A\xBD" "Wifi_Del_Bano",
  "\xF0\x9F\x8D\x94" "Hamburguesas_Tristes",
  "\xF0\x9F\x8E\xAF" "Casi_Le_Atine_Al_Pin",
  "\xF0\x9F\xA5\x80" "Virgen_A_Los_40",
  "\xF0\x9F\x8D\x97" "Pollo_Frito_Gratis",
  "\xF0\x9F\x8D\x96" "Huele_A_Obito",
  "\xF0\x9F\xA6\xB6" "Amo_Tus_Patas",
  "\xF0\x9F\xA7\x9F" "Zombi_En_Tu_Cochera",
  "\xF0\x9F\x91\xBA" "Soy_Tu_Padre_HDP",
  "\xF0\x9F\x8D\x95" "Pizza_Con_Pina_Sux",
  "\xF0\x9F\x92\x8A" "Toma_Tu_Medicina",
};
static const uint16_t SSID_COUNT = sizeof(SSID_LIST) / sizeof(SSID_LIST[0]);

// ===========================================================================
// Canales: en 2.4G usamos los 3 no solapados; en 5G non-DFS.
// ===========================================================================
static const uint8_t CHANNELS_24[] = { 1, 6, 11 };
static const uint8_t CHANNELS_5[]  = { 36, 40, 44, 48, 149, 153, 157, 161 };

#define CH_COUNT_24 (sizeof(CHANNELS_24) / sizeof(CHANNELS_24[0]))
#define CH_COUNT_5  (sizeof(CHANNELS_5)  / sizeof(CHANNELS_5[0]))

// ===========================================================================
// Estado interno
// ===========================================================================
static BeaconSpamStats stats;
static uint8_t  hopIdx = 0;
static uint32_t lastHopAt = 0;
static uint32_t lastBurstAt = 0;

static const uint32_t HOP_MS   = 1500;  // hop lento: da tiempo a scanners
static const uint32_t BURST_MS = 80;    // burst de todos los SSIDs cada 80ms

// ===========================================================================
// Construye un beacon completo conforme a 802.11.
// La estructura BeaconFrame original en packet-injection.h NO incluye los IEs
// obligatorios Supported Rates ni DS Parameter Set, por eso los scanners
// tipo Windows/macOS rechazaban los beacons como malformados.
// ===========================================================================
static int buildBeacon(uint8_t *buf, const uint8_t *bssid,
                       const char *ssid, uint8_t channel, bool is5g) {
  uint8_t *p = buf;

  // ----- Header 802.11 (24 bytes) -----
  *p++ = 0x80; *p++ = 0x00;          // FC: type=mgmt(0), subtype=beacon(8)
  *p++ = 0x00; *p++ = 0x00;          // Duration
  for (int i = 0; i < 6; i++) *p++ = 0xFF;          // DA: broadcast
  for (int i = 0; i < 6; i++) *p++ = bssid[i];      // SA: nuestro BSSID
  for (int i = 0; i < 6; i++) *p++ = bssid[i];      // BSSID
  *p++ = 0x00; *p++ = 0x00;          // Sequence

  // ----- Fixed parameters (12 bytes) -----
  for (int i = 0; i < 8; i++) *p++ = 0x00;          // Timestamp
  *p++ = 0x64; *p++ = 0x00;          // Beacon interval: 100 TUs (~102.4 ms)
  *p++ = 0x01; *p++ = 0x04;          // Capability: ESS + Short Slot Time

  // ----- IE 0: SSID -----
  uint8_t ssidLen = 0;
  while (ssid[ssidLen] && ssidLen < 32) ssidLen++;
  *p++ = 0x00;
  *p++ = ssidLen;
  for (uint8_t i = 0; i < ssidLen; i++) *p++ = (uint8_t)ssid[i];

  // ----- IE 1: Supported Rates (MANDATORIO) -----
  // bit 7 marca rate basico
  *p++ = 0x01;
  *p++ = 0x08;
  if (!is5g) {
    *p++ = 0x82; *p++ = 0x84; *p++ = 0x8B; *p++ = 0x96;  // 1, 2, 5.5, 11 basicos
    *p++ = 0x24; *p++ = 0x30; *p++ = 0x48; *p++ = 0x6C;  // 18, 24, 36, 54
  } else {
    *p++ = 0x8C; *p++ = 0x12; *p++ = 0x98; *p++ = 0x24;  // 6 basic, 9, 12 basic, 18
    *p++ = 0xB0; *p++ = 0x48; *p++ = 0x60; *p++ = 0x6C;  // 24 basic, 36, 48, 54
  }

  // ----- IE 3: DS Parameter Set -----
  *p++ = 0x03;
  *p++ = 0x01;
  *p++ = channel;

  return (int)(p - buf);
}

// ===========================================================================
// API publica
// ===========================================================================
void beaconSpamBegin() {
  memset(&stats, 0, sizeof(stats));
}

bool beaconSpamStart(uint8_t band) {
  memset(&stats, 0, sizeof(stats));
  stats.band = (band == 5) ? 5 : 2;
  stats.active = true;
  stats.startedAt = millis();
  hopIdx = 0;

  wifi_off();
  delay(150);
  if (wifi_on(RTW_MODE_STA) != RTW_SUCCESS) {
    stats.active = false;
    return false;
  }
  delay(200);

  stats.currentChannel = (stats.band == 5) ? CHANNELS_5[0] : CHANNELS_24[0];
  if (wifi_set_channel(stats.currentChannel) != RTW_SUCCESS) {
    stats.active = false;
    return false;
  }
  delay(50);

  lastHopAt = millis();
  lastBurstAt = 0;
  return true;
}

void beaconSpamStop() {
  stats.active = false;
  wifi_off();
  delay(100);
  wifi_on(RTW_MODE_STA);
  delay(150);
}

void beaconSpamTick() {
  if (!stats.active) return;
  uint32_t now = millis();

  // Hop entre canales (lento, 1.5s)
  if (now - lastHopAt >= HOP_MS) {
    uint8_t count = (stats.band == 5) ? (uint8_t)CH_COUNT_5 : (uint8_t)CH_COUNT_24;
    const uint8_t *channels = (stats.band == 5) ? CHANNELS_5 : CHANNELS_24;
    hopIdx = (hopIdx + 1) % count;
    stats.currentChannel = channels[hopIdx];
    wifi_set_channel(stats.currentChannel);
    lastHopAt = now;
  }

  // BURST: cada ~80ms mandamos TODOS los SSIDs en rafaga
  // Asi cada SSID se transmite ~12 veces/s (vs 1.6/s del modelo anterior)
  if (now - lastBurstAt >= BURST_MS) {
    bool is5g = (stats.band == 5);
    uint8_t buf[160];

    for (uint16_t i = 0; i < SSID_COUNT; i++) {
      // MAC source unica por SSID, bit local-administrado en 0x02
      uint8_t srcMac[6] = {
        0x02,
        (uint8_t)((i * 17) & 0xFF),
        (uint8_t)((i * 53) & 0xFF),
        stats.currentChannel,
        (uint8_t)0xAB,
        (uint8_t)0xCD
      };

      int len = buildBeacon(buf, srcMac, SSID_LIST[i],
                            stats.currentChannel, is5g);
      if (wifi_tx_raw_frame(buf, (size_t)len)) {
        stats.totalTx++;
      }
      stats.currentSsidIdx = i;
    }
    lastBurstAt = now;
  }
}

bool beaconSpamActive() {
  return stats.active;
}

const BeaconSpamStats &beaconSpamGetStats() {
  return stats;
}

uint16_t beaconSpamSsidCount() {
  return SSID_COUNT;
}

const char *beaconSpamCurrentSsid() {
  return SSID_LIST[stats.currentSsidIdx];
}