#pragma once

#include <Arduino.h>
#include "WifiScanner.h"

struct LabStats {
  bool active;
  char bssid[18];
  uint16_t samples;
  uint16_t found;
  uint16_t missed;
  int32_t minRssi;
  int32_t maxRssi;
  int32_t sumRssi;
  uint8_t lastChannel;
};

void labStatsReset(const char *bssid);
void labStatsAdd(bool found, const NetworkInfo *network);
bool labStatsActive();
const LabStats &labStatsGet();
int32_t labStatsAverageRssi();
void labStatsPrintToSerial();
