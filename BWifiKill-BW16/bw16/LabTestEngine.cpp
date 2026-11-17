#include "LabTestEngine.h"
#include "ui_lang.h"

static LabTestReport report = {
  LAB_TEST_IDLE,
  UIS_MAIN_TEST,
  UIS_NO_TARGET,
  UIS_SELECT_A_NETWORK,
  UIS_FROM_WIFI_SCAN,
  0
};

static bool isLikelyPmfProtected(uint32_t security) {
  return security == RTW_SECURITY_WPA3_AES_PSK ||
         security == RTW_SECURITY_WPA2_WPA3_MIXED ||
         security == RTW_SECURITY_WPA2_AES_CMAC;
}

static uint8_t missPercent(const LabStats &stats) {
  if (stats.samples == 0) {
    return 0;
  }

  return (stats.missed * 100UL) / stats.samples;
}

void labTestPrepare(const NetworkInfo &target) {
  report.attempts++;
  strncpy(report.title, UIS_MAIN_TEST, sizeof(report.title));
  report.title[sizeof(report.title) - 1] = '\0';

  snprintf(report.line1, sizeof(report.line1), "CH %u %s",
           target.channel,
           wifiScannerIs5GHz(target.channel) ? "5GHz" : "2.4GHz");

  if (isLikelyPmfProtected(target.security)) {
    report.state = LAB_TEST_BLOCKED;
    strncpy(report.line2, UIS_PMF_LIKELY, sizeof(report.line2));
    strncpy(report.line3, UIS_DIAGNOSTICS_ONLY, sizeof(report.line3));
  } else {
    report.state = LAB_TEST_READY;
    strncpy(report.line2, UIS_TARGET_COMPATIBLE, sizeof(report.line2));
    strncpy(report.line3, UIS_TX_NOT_ENABLED, sizeof(report.line3));
  }

  report.line2[sizeof(report.line2) - 1] = '\0';
  report.line3[sizeof(report.line3) - 1] = '\0';
}

void labTestEvaluate(const NetworkInfo &target, bool found, const LabStats &stats) {
  snprintf(report.line1, sizeof(report.line1), "CH %u %s RSSI %ld",
           target.channel,
           wifiScannerIs5GHz(target.channel) ? "5G" : "2.4G",
           (long)target.rssi);

  if (isLikelyPmfProtected(target.security)) {
    report.state = LAB_TEST_BLOCKED;
    strncpy(report.line2, UIS_PMF_WPA3_LIKELY, sizeof(report.line2));
    strncpy(report.line3, "Sin efecto esperado", sizeof(report.line3));
  } else if (!found) {
    report.state = LAB_TEST_MEASURED;
    strncpy(report.line2, UIS_TARGET_NOT_SEEN, sizeof(report.line2));
    snprintf(report.line3, sizeof(report.line3), UIS_LOST_U, missPercent(stats));
  } else {
    report.state = LAB_TEST_MEASURED;
    snprintf(report.line2, sizeof(report.line2), UIS_SEEN_U_U,
             stats.found,
             stats.samples);
    snprintf(report.line3, sizeof(report.line3), UIS_RSSI_AVG_LD,
             (long)labStatsAverageRssi());
  }

  report.line1[sizeof(report.line1) - 1] = '\0';
  report.line2[sizeof(report.line2) - 1] = '\0';
  report.line3[sizeof(report.line3) - 1] = '\0';
}

void labTestSimulateDeauth(const NetworkInfo &target, bool foundAfter, const LabStats &stats, bool txInvoked) {
  (void)stats;
  report.attempts++;
  strncpy(report.title, "Deauth lab", sizeof(report.title));
  report.title[sizeof(report.title) - 1] = '\0';

  snprintf(report.line1, sizeof(report.line1), "CH %u %s",
           target.channel,
           wifiScannerIs5GHz(target.channel) ? "5GHz" : "2.4GHz");

  if (isLikelyPmfProtected(target.security)) {
    report.state = LAB_TEST_BLOCKED;
    strncpy(report.line2, UIS_PMF_LIKELY, sizeof(report.line2));
    strncpy(report.line3, UIS_DEAUTH_BLOCKED, sizeof(report.line3));
  } else {
    report.state = txInvoked ? LAB_TEST_MEASURED : LAB_TEST_SIMULATED;
    snprintf(report.line2, sizeof(report.line2), foundAfter ? UIS_TARGET_VISIBLE : UIS_TARGET_LOST);
    snprintf(report.line3, sizeof(report.line3), txInvoked ? UIS_TX_INVOKED : UIS_DRIVER_NOT_INVOKED);
  }

  report.line1[sizeof(report.line1) - 1] = '\0';
  report.line2[sizeof(report.line2) - 1] = '\0';
  report.line3[sizeof(report.line3) - 1] = '\0';
}

void labTestReset() {
  report.state = LAB_TEST_IDLE;
  strncpy(report.line1, UIS_NO_TARGET, sizeof(report.line1));
  strncpy(report.line2, UIS_SELECT_A_NETWORK, sizeof(report.line2));
  strncpy(report.line3, UIS_FROM_WIFI_SCAN, sizeof(report.line3));
  report.line1[sizeof(report.line1) - 1] = '\0';
  report.line2[sizeof(report.line2) - 1] = '\0';
  report.line3[sizeof(report.line3) - 1] = '\0';
  report.attempts = 0;
}

const LabTestReport &labTestGetReport() {
  return report;
}

void labTestPrintToSerial() {
  Serial.println();
  Serial.println(UIS_BWIFIKILL_MAIN);
  Serial.print(UIS_STATE);
  Serial.println(report.state == LAB_TEST_READY ? "READY" : report.state == LAB_TEST_MEASURED ? "MEASURED" : report.state == LAB_TEST_SIMULATED ? "SIMULATED" : report.state == LAB_TEST_BLOCKED ? "BLOCKED" : "IDLE");
  Serial.print(UIS_ATTEMPTS);
  Serial.println(report.attempts);
  Serial.println(report.line1);
  Serial.println(report.line2);
  Serial.println(report.line3);
  Serial.println("===========================");
}
