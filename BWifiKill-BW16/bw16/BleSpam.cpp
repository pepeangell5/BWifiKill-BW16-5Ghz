#include "BleSpam.h"

#include <BLEDevice.h>
#include <BLEAdvert.h>
#include <BLEAdvertData.h>
#include <string.h>

#include "BluetoothScanner.h"

// GAP low-level: necesario para cambiar la random address en runtime
extern "C" {
  #include "gap.h"
  #include "gap_le.h"
  #include "gap_adv.h"
}

// ===========================================================================
// Lista de nombres BLE
// ===========================================================================
static const char *const BLE_NAMES[] = {
  "\xF0\x9F\x8D\x8E" "AirPods_De_Tu_Ex",
  "\xE2\x8C\x9A" "Watch_Mata_Pulgas",
  "\xF0\x9F\x8E\xAE" "Joycon_Del_Diablo",
  "\xF0\x9F\x93\xBA" "Apple_TV_Hackeada",
  "\xF0\x9F\xA5\xA4" "Yeti_Con_Tepache",
  "\xF0\x9F\x93\xB1" "iPhone_Robado_Tepito",
  "\xF0\x9F\x9A\x97" "Tesla_Sin_Llantas",
  "\xF0\x9F\x9B\xB9" "Skate_Pendejo",
  "\xE2\x9A\x96" "Bascula_Mentirosa",
  "\xF0\x9F\x8E\xA7" "Audifonos_2003",
  "\xF0\x9F\x92\x8A" "Pastillas_Bluetooth",
  "\xF0\x9F\x8D\xBF" "Microondas_PopOS",
  "\xF0\x9F\x94\x8B" "Pila_Casi_Muerta",
  "\xF0\x9F\x95\xB6" "RayBan_Pirata",
  "\xF0\x9F\x9A\xB2" "Bici_Sin_Cadena",
  "\xF0\x9F\x8F\x83" "Fitbit_Floja",
  "\xF0\x9F\x90\x95" "Collar_Perro_Perdido",
  "\xF0\x9F\xA7\xB8" "Peluche_Hackeable",
  "\xF0\x9F\x8D\xBA" "Cerveza_Inteligente",
  "\xF0\x9F\xAA\xA5" "Cepillo_Roto",
  "\xE2\x98\x95" "Cafetera_Sin_Agua",
  "\xF0\x9F\x9B\x8F" "Cama_Que_Ronca",
  "\xF0\x9F\x8E\xA4" "Karaoke_Maldito",
  "\xF0\x9F\xAA\x91" "Silla_Con_Wifi",
  "\xF0\x9F\x8E\xAF" "Dardo_Sin_Punteria",
  "\xF0\x9F\xAA\x9E" "Espejo_Que_Habla",
  "\xF0\x9F\xA7\x8A" "Hielera_Caliente",
  "\xF0\x9F\x94\xA5" "Encendedor_Sin_Gas",
  "\xF0\x9F\x8E\xBA" "Trompeta_3am",
  "\xF0\x9F\x93\xA1" "Antena_Chismosa",
};
static const uint16_t BLE_NAME_COUNT = sizeof(BLE_NAMES) / sizeof(BLE_NAMES[0]);

// ===========================================================================
// Estado interno
// ===========================================================================
static BleSpamStats stats;
static uint16_t cursor = 0;
static uint32_t lastChangeAt = 0;
static const uint32_t CHANGE_MS = 350;

static BLEAdvertData advdata;
static BLEAdvertData scnrsp;
static bool peripheralUp = false;

// ===========================================================================
// Helpers
// ===========================================================================
static void genRandomMac(uint8_t mac[6], uint16_t idx) {
  mac[0] = (uint8_t)((idx * 17  + 3)  & 0xFF);
  mac[1] = (uint8_t)((idx * 53  + 7)  & 0xFF);
  mac[2] = (uint8_t)((idx * 97  + 11) & 0xFF);
  mac[3] = (uint8_t)((idx * 131 + 19) & 0xFF);
  mac[4] = (uint8_t)((idx * 167 + 23) & 0xFF);
  mac[5] = (uint8_t)(((idx * 211 + 29) & 0x3F) | 0xC0);  // static random
}

static void applyAdvertAndAddr(uint16_t idx) {
  uint8_t mac[6];
  genRandomMac(mac, idx);
  le_set_gap_param(GAP_PARAM_RANDOM_ADDR, 6, mac);

  advdata.clear();
  advdata.addFlags();
  advdata.addCompleteName(BLE_NAMES[idx]);
  BLE.configAdvert()->setAdvData(advdata);

  scnrsp.clear();
  scnrsp.addCompleteName(BLE_NAMES[idx]);
  BLE.configAdvert()->setScanRspData(scnrsp);
}

// ===========================================================================
// API publica
// ===========================================================================
void bleSpamBegin() {
  memset(&stats, 0, sizeof(stats));
}

bool bleSpamStart() {
  Serial.println("[BLE_SPAM] start");

  if (bleActive()) {
    Serial.println("[BLE_SPAM] paro scanner central");
    bleStop();
    delay(300);
  }

  memset(&stats, 0, sizeof(stats));
  stats.active = true;
  stats.startedAt = millis();
  cursor = 0;

  Serial.println("[BLE_SPAM] BLE.init");
  BLE.init();
  delay(200);

  // ADV_NONCONN_IND = 3. Non-connectable beacon mode. No requiere servicios.
  // Los devices aparecen en BLE scanner apps tipo nRF Connect, no en el
  // Settings nativo del SO (que filtra a connectables con servicios).
  Serial.println("[BLE_SPAM] setAdvType NONCONN_IND");
  BLE.configAdvert()->setAdvType(GAP_ADTYPE_ADV_NONCONN_IND);

  // Random local address type (1)
  uint8_t addrType = GAP_LOCAL_ADDR_LE_RANDOM;
  le_adv_set_param(GAP_PARAM_ADV_LOCAL_ADDR_TYPE, sizeof(addrType), &addrType);

  BLE.configAdvert()->setMinInterval(0xA0);   // 100 ms
  BLE.configAdvert()->setMaxInterval(0x140);  // 200 ms

  applyAdvertAndAddr(0);

  Serial.println("[BLE_SPAM] beginPeripheral");
  BLE.beginPeripheral();
  delay(400);
  peripheralUp = true;

  stats.currentIdx = 0;
  stats.totalTx = 1;
  lastChangeAt = millis();

  Serial.print("[BLE_SPAM] anunciando ");
  Serial.println(BLE_NAMES[0]);
  return true;
}

void bleSpamStop() {
  Serial.println("[BLE_SPAM] stop");
  if (peripheralUp) {
    BLE.configAdvert()->stopAdv();
    delay(50);
    BLE.end();
    peripheralUp = false;
    delay(300);
  }
  stats.active = false;
}

void bleSpamTick() {
  if (!stats.active) return;
  uint32_t now = millis();
  if (now - lastChangeAt < CHANGE_MS) return;

  cursor = (cursor + 1) % BLE_NAME_COUNT;

  BLE.configAdvert()->stopAdv();
  delay(30);
  applyAdvertAndAddr(cursor);
  delay(20);
  BLE.configAdvert()->updateAdvertParams();
  BLE.configAdvert()->startAdv();

  stats.currentIdx = cursor;
  stats.totalTx++;
  lastChangeAt = now;

  if ((stats.totalTx % 10) == 0) {
    Serial.print("[BLE_SPAM] cursor=");
    Serial.print(cursor);
    Serial.print(" name=");
    Serial.println(BLE_NAMES[cursor]);
  }
}

bool bleSpamActive() {
  return stats.active;
}

const BleSpamStats &bleSpamGetStats() {
  return stats;
}

uint16_t bleSpamCount() {
  return BLE_NAME_COUNT;
}

const char *bleSpamCurrent() {
  return BLE_NAMES[stats.currentIdx];
}