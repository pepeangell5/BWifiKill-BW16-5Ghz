#pragma once

#include <Arduino.h>
#include "LabTestEngine.h"
#include "LabStats.h"
#include "WifiScanner.h"
#include "PromiscuousSniffer.h"
#include "BluetoothScanner.h"
#include "BeaconSpam.h"
#include "BleSpam.h"

void uiBegin();
void uiDrawSplash();
void uiDrawHome();
void uiDrawStatus(const char *message);
void uiDrawTxCounter(uint32_t packetCount);
void uiDrawMenu(const char *title, const char *const items[], uint8_t itemCount, uint8_t selected, const char *footer);
void uiTickMenuAnimation();
void uiDrawBandMenu(uint8_t selectedBand);
void uiDrawRadarBandMenu(uint8_t selectedBand);
void uiDrawNetworkList(uint8_t selectedBand, int selectedNetwork, int listTop);
void uiDrawNetworkDetails(const NetworkInfo &network, bool saved);
void uiDrawTargetDetails(const NetworkInfo &network);
void uiDrawAnalyzer(uint8_t band);
void uiDrawWifiRadar(const NetworkInfo *network, bool found);
void uiTickWifiRadar();
void uiDrawSystemInfo(bool hasTarget, uint8_t scanCount, uint8_t count24, uint8_t count5);
void uiDrawLabPrecheck(bool hasTarget, const NetworkInfo *network);
void uiDrawTargetMonitor(const NetworkInfo &network, bool found);
void uiDrawLabStats(const LabStats &stats);
void uiDrawPrincipalTest(const LabTestReport &report);
void uiDrawBleList(int selected, int listTop);
void uiDrawBleDetails(const BleDeviceInfo &dev);
void uiRefreshBleList(int selected, int listTop);
void uiDrawBleAnalyzer();
void uiRefreshBleAnalyzer();
void uiDrawWifiAnalyzer();
void uiRefreshWifiAnalyzer();
void uiDrawBeaconSpam();
void uiRefreshBeaconSpam();
void uiDrawBleSpam();
void uiRefreshBleSpam();
void uiDrawSniffer(const SnifferStats &stats);
void uiRefreshSniffer(const SnifferStats &stats);
