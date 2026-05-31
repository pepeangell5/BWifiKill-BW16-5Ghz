#include "LabTestEngine.h"

static LabTestReport report = {
  LAB_TEST_IDLE,
  "Prueba principal",
  "Sin objetivo",
  "Selecciona una red",
  "desde WiFi > Scan",
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
  strncpy(report.title, "Prueba principal", sizeof(report.title));
  report.title[sizeof(report.title) - 1] = '\0';

  snprintf(report.line1, sizeof(report.line1), "CH %u %s",
           target.channel,
           wifiScannerIs5GHz(target.channel) ? "5GHz" : "2.4GHz");

  if (isLikelyPmfProtected(target.security)) {
    report.state = LAB_TEST_BLOCKED;
    strncpy(report.line2, "PMF probable", sizeof(report.line2));
    strncpy(report.line3, "Solo diagnostico", sizeof(report.line3));
  } else {
    report.state = LAB_TEST_READY;
    strncpy(report.line2, "Objetivo compatible", sizeof(report.line2));
    strncpy(report.line3, "TX no habilitado", sizeof(report.line3));
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
    strncpy(report.line2, "PMF/WPA3 probable", sizeof(report.line2));
    strncpy(report.line3, "Sin efecto esperado", sizeof(report.line3));
  } else if (!found) {
    report.state = LAB_TEST_MEASURED;
    strncpy(report.line2, "Objetivo no visto", sizeof(report.line2));
    snprintf(report.line3, sizeof(report.line3), "Perdidas %u%%", missPercent(stats));
  } else {
    report.state = LAB_TEST_MEASURED;
    snprintf(report.line2, sizeof(report.line2), "Visto %u/%u",
             stats.found,
             stats.samples);
    snprintf(report.line3, sizeof(report.line3), "RSSI prom %ld",
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
    strncpy(report.line2, "PMF probable", sizeof(report.line2));
    strncpy(report.line3, "Deauth bloqueado", sizeof(report.line3));
  } else {
    report.state = txInvoked ? LAB_TEST_MEASURED : LAB_TEST_SIMULATED;
    snprintf(report.line2, sizeof(report.line2), foundAfter ? "Objetivo visible" : "Objetivo perdido");
    snprintf(report.line3, sizeof(report.line3), txInvoked ? "TX invocado" : "Driver no invocado");
  }

  report.line1[sizeof(report.line1) - 1] = '\0';
  report.line2[sizeof(report.line2) - 1] = '\0';
  report.line3[sizeof(report.line3) - 1] = '\0';
}

void labTestReset() {
  report.state = LAB_TEST_IDLE;
  strncpy(report.line1, "Sin objetivo", sizeof(report.line1));
  strncpy(report.line2, "Selecciona una red", sizeof(report.line2));
  strncpy(report.line3, "desde WiFi > Scan", sizeof(report.line3));
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
  Serial.println("=== BWifiKill Principal ===");
  Serial.print("Estado: ");
  Serial.println(report.state == LAB_TEST_READY ? "READY" : report.state == LAB_TEST_MEASURED ? "MEASURED" : report.state == LAB_TEST_SIMULATED ? "SIMULATED" : report.state == LAB_TEST_BLOCKED ? "BLOCKED" : "IDLE");
  Serial.print("Intentos: ");
  Serial.println(report.attempts);
  Serial.println(report.line1);
  Serial.println(report.line2);
  Serial.println(report.line3);
  Serial.println("===========================");
}
