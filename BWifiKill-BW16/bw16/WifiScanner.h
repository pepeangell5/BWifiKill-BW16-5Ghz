#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <wifi_conf.h>

#define MAX_NETWORKS WL_NETWORKS_LIST_MAXNUM
#define SCAN_WAIT_MS 6000

struct NetworkInfo {
  char ssid[WL_SSID_MAX_LENGTH];
  int32_t rssi;
  uint32_t security;
  uint8_t channel;
  char bssid[18];
};

void wifiScannerBegin();
bool wifiScannerScan();
uint8_t wifiScannerCount();
const NetworkInfo &wifiScannerNetwork(uint8_t index);
bool wifiScannerIs5GHz(uint8_t channel);
bool wifiScannerNetworkInBand(uint8_t index, uint8_t band);
uint8_t wifiScannerCountBand(uint8_t band);
uint8_t wifiScannerCountChannel(uint8_t channel);
int wifiScannerStrongestIndex(uint8_t band);
int wifiScannerFindBssid(const char *bssid);
uint8_t wifiScannerBusiestChannel(uint8_t band);
const char *wifiScannerSecurityName(uint32_t security);
void wifiScannerPrintToSerial();
