#include "BluetoothScanner.h"

#include "BLEDevice.h"

#include <string.h>

static BleDeviceInfo devices[MAX_BLE_DEVICES];
static uint8_t deviceCount = 0;

static volatile uint32_t totalPackets = 0;
static volatile uint32_t appleCount = 0;
static volatile uint32_t microsoftCount = 0;
static volatile uint32_t namedCount = 0;

// Analizador: contadores del segundo actual + historial
static volatile uint32_t curBinPackets = 0;
static volatile int32_t  curBinRssiSum = 0;
static volatile uint32_t curBinRssiCount = 0;
static uint32_t lastBinTick = 0;
static uint8_t  ppsHistory[BLE_HISTORY_SIZE]   = {0};
static int8_t   rssiHistory[BLE_HISTORY_SIZE]  = {0};
static uint8_t  historyHead = 0;
static bool     historyPrimed = false;

static bool initialized = false;
static bool scanning = false;

// ---------------------------------------------------------------------------
// Clasificacion
// ---------------------------------------------------------------------------
static BleDeviceKind classifyDevice(uint16_t manufacturer,
                                    const uint8_t *data,
                                    uint8_t len,
                                    uint8_t &subtypeOut) {
  subtypeOut = 0;

  if (manufacturer == 0x004C) {
    // Apple: el primer byte despues del company ID es el subtipo,
    // el segundo es la longitud del payload del subtipo.
    if (len >= 2) {
      subtypeOut = data[0];
      // iBeacon clasico: subtipo 0x02, length 0x15 (21 bytes)
      if (data[0] == 0x02 && data[1] == 0x15) {
        return BLE_KIND_IBEACON;
      }
      return BLE_KIND_APPLE_CONTINUITY;
    }
    return BLE_KIND_APPLE_CONTINUITY;
  }

  if (manufacturer == 0x0006) return BLE_KIND_MICROSOFT;
  if (manufacturer == 0x00E0) return BLE_KIND_GOOGLE;
  if (manufacturer == 0x0075) return BLE_KIND_SAMSUNG;

  return BLE_KIND_UNKNOWN;
}

// ---------------------------------------------------------------------------
// Busqueda interna
// ---------------------------------------------------------------------------
static int findDevice(const char *addr) {
  for (uint8_t i = 0; i < deviceCount; i++) {
    if (strcmp(devices[i].addr, addr) == 0) return i;
  }
  return -1;
}

static int findWeakestIndex() {
  int weakest = 0;
  for (uint8_t i = 1; i < deviceCount; i++) {
    if (devices[i].rssi < devices[weakest].rssi) weakest = i;
  }
  return weakest;
}

// ---------------------------------------------------------------------------
// Callback de scan (corre en task del stack BLE; mantener corto)
// ---------------------------------------------------------------------------
static void bleScanCallback(T_LE_CB_DATA *p_data) {
  totalPackets++;
  curBinPackets++;

  BLEAdvertData advert;
  advert.parseScanInfo(p_data);

  BLEAddr addr = advert.getAddr();
  const char *addrStr = addr.str();
  if (addrStr == NULL) return;

  int8_t rssi = advert.getRSSI();
  curBinRssiSum += rssi;
  curBinRssiCount++;

  uint16_t mfg = advert.hasManufacturer() ? advert.getManufacturer() : 0;
  uint8_t mfgLen = advert.getManufacturerDataLength();
  uint8_t *mfgData = advert.getManufacturerData();

  int idx = findDevice(addrStr);
  bool isNew = false;

  if (idx < 0) {
    if (deviceCount < MAX_BLE_DEVICES) {
      idx = deviceCount++;
      isNew = true;
    } else {
      // Lista llena: reemplazar el mas debil si este es claramente mas fuerte
      int weakest = findWeakestIndex();
      if (rssi > devices[weakest].rssi + 5) {
        idx = weakest;
        memset(&devices[idx], 0, sizeof(BleDeviceInfo));
        isNew = true;
      } else {
        return;
      }
    }
    strncpy(devices[idx].addr, addrStr, 17);
    devices[idx].addr[17] = '\0';
    devices[idx].seenCount = 0;
    devices[idx].name[0] = '\0';
  }

  BleDeviceInfo &dev = devices[idx];
  dev.rssi = rssi;
  dev.lastSeen = millis();
  dev.seenCount++;
  dev.manufacturer = mfg;
  dev.mfgDataLen = (mfgLen > BLE_MFG_MAX) ? BLE_MFG_MAX : mfgLen;

  if (mfgData != NULL && dev.mfgDataLen > 0) {
    memcpy(dev.mfgData, mfgData, dev.mfgDataLen);
  }

  if (advert.hasName()) {
    String n = advert.getName();
    bool hadName = dev.name[0] != '\0';
    strncpy(dev.name, n.c_str(), BLE_NAME_LEN - 1);
    dev.name[BLE_NAME_LEN - 1] = '\0';
    if (!hadName) namedCount++;
  }

  uint8_t subtype = 0;
  BleDeviceKind kind = classifyDevice(mfg, dev.mfgData, dev.mfgDataLen, subtype);

  if (kind == BLE_KIND_UNKNOWN && dev.name[0] != '\0') {
    kind = BLE_KIND_NAMED;
  }

  dev.kind = kind;
  dev.appleSubtype = subtype;

  if (isNew) {
    if (mfg == 0x004C) appleCount++;
    if (mfg == 0x0006) microsoftCount++;
  }
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------
void bleBegin() {
  // Inicializacion perezosa: BLE.init() se llama en el primer bleStart().
  // Esto evita gastar RAM si el usuario nunca entra a la pantalla BLE.
}

bool bleStart() {
  if (!initialized) {
    BLE.init();
    BLE.configScan()->setScanMode(GAP_SCAN_MODE_ACTIVE);
    BLE.configScan()->setScanInterval(500);
    BLE.configScan()->setScanWindow(250);
    BLE.configScan()->updateScanParams();
    BLE.setScanCallback(bleScanCallback);
    BLE.beginCentral(0);
    initialized = true;
  }

  bleResetList();
  BLE.configScan()->startScan();   // continuo, lo detiene bleStop()
  scanning = true;
  return true;
}

void bleStop() {
  if (scanning) {
    BLE.configScan()->stopScan();
    scanning = false;
  }
}

bool bleActive() {
  return scanning;
}

void bleResetList() {
  deviceCount = 0;
  totalPackets = 0;
  appleCount = 0;
  microsoftCount = 0;
  namedCount = 0;
  bleAnalyzerReset();
}

uint8_t bleCount() { return deviceCount; }

const BleDeviceInfo &bleDevice(uint8_t index) {
  return devices[index];
}

uint32_t bleTotalPackets()   { return totalPackets; }
uint32_t bleAppleCount()     { return appleCount; }
uint32_t bleMicrosoftCount() { return microsoftCount; }
uint32_t bleNamedCount()     { return namedCount; }

const char *bleKindLabel(BleDeviceKind kind) {
  switch (kind) {
    case BLE_KIND_IBEACON:          return "iBeacon";
    case BLE_KIND_APPLE_CONTINUITY: return "Apple";
    case BLE_KIND_MICROSOFT:        return "Microsoft";
    case BLE_KIND_GOOGLE:           return "Google";
    case BLE_KIND_SAMSUNG:          return "Samsung";
    case BLE_KIND_NAMED:            return "Named";
    default:                        return "BLE";
  }
}

const char *bleAppleSubtypeLabel(uint8_t subtype) {
  switch (subtype) {
    case 0x02: return "iBeacon";
    case 0x03: return "AirPrint";
    case 0x05: return "AirDrop";
    case 0x06: return "HomeKit";
    case 0x07: return "Proximity";
    case 0x08: return "Hey Siri";
    case 0x09: return "AirPlay Src";
    case 0x0A: return "AirPlay Dst";
    case 0x0B: return "MagicSwitch";
    case 0x0C: return "Handoff";
    case 0x0D: return "Teth Target";
    case 0x0E: return "Teth Source";
    case 0x0F: return "Nearby Action";
    case 0x10: return "Nearby Info";
    case 0x12: return "Find My";
    default:   return "Apple ?";
  }
}

const char *bleManufacturerName(uint16_t companyId) {
  switch (companyId) {
    case 0x004C: return "Apple";
    case 0x0006: return "Microsoft";
    case 0x00E0: return "Google";
    case 0x0075: return "Samsung";
    case 0x0001: return "Ericsson";
    case 0x000F: return "Broadcom";
    case 0x0046: return "Sennheiser";
    case 0x0059: return "Nordic";
    case 0x0087: return "Garmin";
    case 0x015D: return "Xiaomi";
    case 0x0157: return "Huami";
    case 0x0131: return "Cypress";
    case 0x0399: return "Nintendo";
    case 0x0822: return "Realtek";
    case 0x01DA: return "Logitech";
    case 0x000D: return "Texas Instr";
    default:     return NULL;
  }
}

// ===========================================================================
// Analizador: muestreo de tasa por segundo y baseline movil
// ===========================================================================

void bleAnalyzerReset() {
  for (uint8_t i = 0; i < BLE_HISTORY_SIZE; i++) {
    ppsHistory[i] = 0;
    rssiHistory[i] = 0;
  }
  historyHead = 0;
  historyPrimed = false;
  curBinPackets = 0;
  curBinRssiSum = 0;
  curBinRssiCount = 0;
  lastBinTick = millis();
}

void bleAnalyzerTick() {
  uint32_t now = millis();
  if (lastBinTick == 0) lastBinTick = now;
  if (now - lastBinTick < BLE_BIN_MS) return;
  lastBinTick = now;

  // Snapshot rapido del bin actual (carrera benigna en BLE task)
  uint32_t bin = curBinPackets;
  int32_t  rsum = curBinRssiSum;
  uint32_t rcount = curBinRssiCount;
  curBinPackets = 0;
  curBinRssiSum = 0;
  curBinRssiCount = 0;

  uint8_t binClamped = (bin > 255) ? 255 : (uint8_t)bin;
  int8_t  avgRssi = (rcount > 0) ? (int8_t)(rsum / (int32_t)rcount) : 0;

  ppsHistory[historyHead] = binClamped;
  rssiHistory[historyHead] = avgRssi;
  historyHead = (historyHead + 1) % BLE_HISTORY_SIZE;
  if (historyHead == 0) historyPrimed = true;
}

// blePps: tasa efectiva en paquetes por segundo (suma de ultimos 10 bins de 100 ms)
uint16_t blePps() {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < 10 && i < BLE_HISTORY_SIZE; i++) {
    uint8_t idx = (historyHead + BLE_HISTORY_SIZE - 1 - i) % BLE_HISTORY_SIZE;
    sum += ppsHistory[idx];
  }
  return sum;
}

int8_t bleAvgRssi() {
  // RSSI del bin mas reciente con datos validos
  for (uint8_t i = 0; i < 5 && i < BLE_HISTORY_SIZE; i++) {
    uint8_t idx = (historyHead + BLE_HISTORY_SIZE - 1 - i) % BLE_HISTORY_SIZE;
    if (rssiHistory[idx] != 0) return rssiHistory[idx];
  }
  return 0;
}

// bleBaseline: promedio expresado como pps, excluyendo los 10 bins (1 s) mas recientes
uint16_t bleBaseline() {
  uint32_t sum = 0;
  uint8_t count = 0;
  uint8_t limit = historyPrimed ? BLE_HISTORY_SIZE : historyHead;
  const uint8_t SKIP_RECENT = 10;
  if (limit <= SKIP_RECENT + 5) return 0;

  for (uint8_t i = SKIP_RECENT; i < limit; i++) {
    uint8_t idx = (historyHead + BLE_HISTORY_SIZE - 1 - i) % BLE_HISTORY_SIZE;
    sum += ppsHistory[idx];
    count++;
  }
  if (count == 0) return 0;
  // sum es total de paquetes en (count * 100 ms) -> escalar a pps
  return (uint16_t)((sum * 10) / count);
}

BleAnalyzerStatus bleAnalyzerStatus() {
  uint16_t baseline = bleBaseline();
  uint16_t current  = blePps();
  uint8_t filled    = historyPrimed ? BLE_HISTORY_SIZE : historyHead;

  // Necesitamos al menos 2.5 s de historia y un baseline minimo para decidir
  if (filled < 25 || baseline < 4) return BLE_STATUS_IDLE;

  // Caida >= 2/3 vs baseline: posible interferencia
  if ((uint32_t)current * 3 < (uint32_t)baseline) return BLE_STATUS_LOW;

  // Pico > 2x baseline + margen: posible flood / spam
  if ((uint32_t)current > (uint32_t)baseline * 2 + 8) return BLE_STATUS_HIGH;

  return BLE_STATUS_NORMAL;
}

const uint8_t *blePpsHistory() {
  return ppsHistory;
}

uint8_t blePpsHistoryHead() {
  return historyHead;
}

const char *bleAnalyzerStatusLabel(BleAnalyzerStatus s) {
  switch (s) {
    case BLE_STATUS_NORMAL: return "Normal";
    case BLE_STATUS_LOW:    return "Bajo";
    case BLE_STATUS_HIGH:   return "Alto";
    default:                return "Calibrando";
  }
}
