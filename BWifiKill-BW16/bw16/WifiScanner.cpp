#include "WifiScanner.h"

static uint8_t networkCount = 0;
static NetworkInfo networks[MAX_NETWORKS];

static rtw_result_t scanResultHandler(rtw_scan_handler_result_t *scanResult) {
  if (scanResult->scan_complete == RTW_TRUE) {
    return RTW_SUCCESS;
  }

  if (networkCount >= MAX_NETWORKS) {
    return RTW_SUCCESS;
  }

  rtw_scan_result_t *record = &scanResult->ap_details;
  record->SSID.val[record->SSID.len] = 0;

  NetworkInfo &network = networks[networkCount];
  strncpy(network.ssid, (char *)record->SSID.val, WL_SSID_MAX_LENGTH - 1);
  network.ssid[WL_SSID_MAX_LENGTH - 1] = '\0';
  network.rssi = record->signal_strength;
  network.security = record->security;
  network.channel = record->channel;
  snprintf(network.bssid, sizeof(network.bssid),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           record->BSSID.octet[0], record->BSSID.octet[1], record->BSSID.octet[2],
           record->BSSID.octet[3], record->BSSID.octet[4], record->BSSID.octet[5]);
  networkCount++;

  return RTW_SUCCESS;
}

void wifiScannerBegin() {
  WiFi.status();
}

bool wifiScannerScan() {
  networkCount = 0;

  if (wifi_scan_networks(scanResultHandler, NULL) != RTW_SUCCESS) {
    return false;
  }

  delay(SCAN_WAIT_MS);
  return true;
}

uint8_t wifiScannerCount() {
  return networkCount;
}

const NetworkInfo &wifiScannerNetwork(uint8_t index) {
  return networks[index];
}

bool wifiScannerIs5GHz(uint8_t channel) {
  return channel >= 14;
}

bool wifiScannerNetworkInBand(uint8_t index, uint8_t band) {
  return band == 5 ? wifiScannerIs5GHz(networks[index].channel) : !wifiScannerIs5GHz(networks[index].channel);
}

uint8_t wifiScannerCountBand(uint8_t band) {
  uint8_t count = 0;

  for (uint8_t i = 0; i < networkCount; i++) {
    if (wifiScannerNetworkInBand(i, band)) {
      count++;
    }
  }

  return count;
}

uint8_t wifiScannerCountChannel(uint8_t channel) {
  uint8_t count = 0;

  for (uint8_t i = 0; i < networkCount; i++) {
    if (networks[i].channel == channel) {
      count++;
    }
  }

  return count;
}

int wifiScannerStrongestIndex(uint8_t band) {
  int strongest = -1;

  for (uint8_t i = 0; i < networkCount; i++) {
    if (!wifiScannerNetworkInBand(i, band)) {
      continue;
    }

    if (strongest < 0 || networks[i].rssi > networks[strongest].rssi) {
      strongest = i;
    }
  }

  return strongest;
}

int wifiScannerFindBssid(const char *bssid) {
  for (uint8_t i = 0; i < networkCount; i++) {
    if (strcmp(networks[i].bssid, bssid) == 0) {
      return i;
    }
  }

  return -1;
}

uint8_t wifiScannerBusiestChannel(uint8_t band) {
  uint8_t bestChannel = 0;
  uint8_t bestCount = 0;

  for (uint8_t i = 0; i < networkCount; i++) {
    if (!wifiScannerNetworkInBand(i, band)) {
      continue;
    }

    uint8_t channel = networks[i].channel;
    uint8_t count = wifiScannerCountChannel(channel);
    if (count > bestCount) {
      bestCount = count;
      bestChannel = channel;
    }
  }

  return bestChannel;
}

const char *wifiScannerSecurityName(uint32_t security) {
  switch (security) {
    case RTW_SECURITY_OPEN:
      return "OPEN";
    case RTW_SECURITY_WEP_PSK:
      return "WEP";
    case RTW_SECURITY_WPA_TKIP_PSK:
      return "WPA TKIP";
    case RTW_SECURITY_WPA_AES_PSK:
      return "WPA AES";
    case RTW_SECURITY_WPA2_AES_PSK:
      return "WPA2 AES";
    case RTW_SECURITY_WPA2_TKIP_PSK:
      return "WPA2 TKIP";
    case RTW_SECURITY_WPA2_MIXED_PSK:
      return "WPA2 MIX";
    case RTW_SECURITY_WPA_WPA2_MIXED_PSK:
      return "WPA/WPA2";
    case RTW_SECURITY_WPA3_AES_PSK:
      return "WPA3 AES";
    case RTW_SECURITY_WPA2_WPA3_MIXED:
      return "WPA2/WPA3";
    default:
      return "SEC?";
  }
}

void wifiScannerPrintToSerial() {
  Serial.print("Redes encontradas: ");
  Serial.println(networkCount);

  for (uint8_t i = 0; i < networkCount; i++) {
    const NetworkInfo &network = networks[i];

    Serial.print(i);
    Serial.print(" SSID=");
    Serial.print(network.ssid[0] ? network.ssid : "<oculta>");
    Serial.print(" CH=");
    Serial.print(network.channel);
    Serial.print(" BAND=");
    Serial.print(wifiScannerIs5GHz(network.channel) ? "5G" : "2.4G");
    Serial.print(" RSSI=");
    Serial.print(network.rssi);
    Serial.print(" BSSID=");
    Serial.print(network.bssid);
    Serial.print(" SEC=");
    Serial.println(wifiScannerSecurityName(network.security));
  }
}
