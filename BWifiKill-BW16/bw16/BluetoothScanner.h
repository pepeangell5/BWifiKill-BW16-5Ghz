#pragma once

#include <Arduino.h>

#define MAX_BLE_DEVICES 24
#define BLE_NAME_LEN    20
#define BLE_MFG_MAX     27

enum BleDeviceKind : uint8_t {
  BLE_KIND_UNKNOWN = 0,
  BLE_KIND_NAMED,
  BLE_KIND_IBEACON,
  BLE_KIND_APPLE_CONTINUITY,
  BLE_KIND_MICROSOFT,
  BLE_KIND_GOOGLE,
  BLE_KIND_SAMSUNG
};

struct BleDeviceInfo {
  char addr[18];                  // "XX:XX:XX:XX:XX:XX"
  char name[BLE_NAME_LEN];        // truncado, "" si no anuncia nombre
  int8_t rssi;
  uint16_t manufacturer;          // company ID (BT SIG)
  uint8_t mfgData[BLE_MFG_MAX];   // datos manuf despues del company ID
  uint8_t mfgDataLen;
  BleDeviceKind kind;
  uint8_t appleSubtype;           // subtipo de Apple Continuity (0 si no aplica)
  uint32_t lastSeen;
  uint16_t seenCount;
};

enum BleAnalyzerStatus : uint8_t {
  BLE_STATUS_IDLE,
  BLE_STATUS_NORMAL,
  BLE_STATUS_LOW,
  BLE_STATUS_HIGH
};

#define BLE_HISTORY_SIZE 60     // 60 bins de 100 ms = 6 s de historia
#define BLE_BIN_MS       100    // muestreo a 10 Hz para feel de osciloscopio

void bleBegin();
bool bleStart();
void bleStop();
void bleRelease();
void bleMarkStackStopped();
bool bleActive();
void bleResetList();

uint8_t bleCount();
bool bleCopyDevice(uint8_t index, BleDeviceInfo &out);

uint32_t bleTotalPackets();
uint32_t bleAppleCount();
uint32_t bleMicrosoftCount();
uint32_t bleNamedCount();

const char *bleKindLabel(BleDeviceKind kind);
const char *bleAppleSubtypeLabel(uint8_t subtype);
const char *bleManufacturerName(uint16_t companyId);

// Analizador: muestreo a 10 Hz con baseline movil
void bleAnalyzerTick();
void bleAnalyzerReset();
uint16_t blePps();                       // suma de los ultimos 10 bins = pps efectivo
int8_t   bleAvgRssi();
uint16_t bleBaseline();                  // baseline expresado como pps
BleAnalyzerStatus bleAnalyzerStatus();
const uint8_t *blePpsHistory();          // historial por bin (100 ms cada uno)
uint8_t blePpsHistoryHead();
const char *bleAnalyzerStatusLabel(BleAnalyzerStatus s);
