#include "LabStats.h"
#include "ui_lang.h"

static LabStats stats = { false, "", 0, 0, 0, 0, 0, 0, 0 };

void labStatsReset(const char *bssid) {
  stats.active = true;
  strncpy(stats.bssid, bssid, sizeof(stats.bssid));
  stats.bssid[sizeof(stats.bssid) - 1] = '\0';
  stats.samples = 0;
  stats.found = 0;
  stats.missed = 0;
  stats.minRssi = 0;
  stats.maxRssi = 0;
  stats.sumRssi = 0;
  stats.lastChannel = 0;
}

void labStatsAdd(bool found, const NetworkInfo *network) {
  if (!stats.active) {
    return;
  }

  stats.samples++;

  if (!found || network == NULL) {
    stats.missed++;
    return;
  }

  stats.found++;
  stats.lastChannel = network->channel;
  stats.sumRssi += network->rssi;

  if (stats.found == 1 || network->rssi < stats.minRssi) {
    stats.minRssi = network->rssi;
  }

  if (stats.found == 1 || network->rssi > stats.maxRssi) {
    stats.maxRssi = network->rssi;
  }
}

bool labStatsActive() {
  return stats.active;
}

const LabStats &labStatsGet() {
  return stats;
}

int32_t labStatsAverageRssi() {
  if (stats.found == 0) {
    return 0;
  }

  return stats.sumRssi / stats.found;
}

void labStatsPrintToSerial() {
  Serial.println();
  Serial.println("=== BWifiKill Lab Stats ===");
  Serial.print("BSSID: ");
  Serial.println(stats.bssid);
  Serial.print(UIS_SAMPLES_2);
  Serial.println(stats.samples);
  Serial.print(UIS_DETECTED);
  Serial.println(stats.found);
  Serial.print(UIS_LOST);
  Serial.println(stats.missed);
  Serial.print(UIS_RSSI_MIN_AVG_MAX_2);
  Serial.print(stats.minRssi);
  Serial.print("/");
  Serial.print(labStatsAverageRssi());
  Serial.print("/");
  Serial.println(stats.maxRssi);
  Serial.print(UIS_LAST_CHANNEL);
  Serial.println(stats.lastChannel);
  Serial.println("===========================");
}
