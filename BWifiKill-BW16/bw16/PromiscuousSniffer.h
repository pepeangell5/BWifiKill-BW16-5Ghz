#pragma once

#include <Arduino.h>

#define WIFI_ANAL_HIST  60   // 60 bins de 100 ms = 6 s de historia
#define WIFI_BIN_MS     100  // muestreo a 10 Hz
#define WIFI_24_COUNT   13
#define WIFI_5G_COUNT   9

enum WifiAnalStatus : uint8_t {
  WIFI_ANAL_IDLE,
  WIFI_ANAL_NORMAL,
  WIFI_ANAL_LOW,
  WIFI_ANAL_HIGH
};

struct SnifferStats {
  bool active;
  uint32_t startedAt;
  uint32_t totalFrames;
  uint32_t totalBytes;
  uint32_t mgmtFrames;
  uint32_t ctrlFrames;
  uint32_t dataFrames;
  uint32_t beacons;
  uint32_t probeReqs;
  uint32_t probeResps;
  uint32_t deauths;
  uint32_t disassocs;
  uint32_t eapolFrames;
  uint8_t currentChannel;
  uint8_t band;                            // 2 o 5
  uint32_t framesPerChannel[14];           // 2.4G indices 1..13
  uint32_t frames5PerChannel[WIFI_5G_COUNT]; // 5G por posicion en HOP_CHANNELS_5
};

void sniffBegin();
bool sniffStart(uint8_t band = 2);         // 2 o 5
void sniffStop();
void sniffTick();
const SnifferStats &sniffGetStats();
void sniffResetStats();

// Analizador: tasa por bin de 100 ms con baseline movil
void wifiAnalyzerTick();
void wifiAnalyzerReset();
uint16_t wifiAnalyzerPps();           // suma de los ultimos 10 bins = fps efectivo
uint16_t wifiAnalyzerBaseline();      // baseline expresado como fps
WifiAnalStatus wifiAnalyzerStatus();
const uint8_t *wifiAnalyzerHistory();
uint8_t wifiAnalyzerHistoryHead();
uint8_t wifiAnalyzerBand();
const char *wifiAnalStatusLabel(WifiAnalStatus s);

// Helpers para la fila de barras por canal
uint8_t  wifiBandChannelCount();                       // 13 (2.4G) o 9 (5G)
uint8_t  wifiBandChannelNumber(uint8_t idx);           // numero de canal en pos idx
uint32_t wifiBandChannelFrames(uint8_t idx);           // total acumulado
int      wifiBandBusiestChannelIdx();                  // pos del canal con mas frames
