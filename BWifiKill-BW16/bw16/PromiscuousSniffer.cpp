#include "PromiscuousSniffer.h"

#include <WiFi.h>
#include <wifi_conf.h>
#include <wifi_constants.h>
#include <string.h>

#ifndef RTW_PROMISC_DISABLE
#define RTW_PROMISC_DISABLE   0
#define RTW_PROMISC_ENABLE    1
#define RTW_PROMISC_ENABLE_1  2
#define RTW_PROMISC_ENABLE_2  3
#endif

static SnifferStats stats;

// ---------------------------------------------------------------------------
// Listas de canales por banda
// ---------------------------------------------------------------------------
static const uint8_t HOP_CHANNELS_24[WIFI_24_COUNT] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
};
static const uint8_t HOP_CHANNELS_5[WIFI_5G_COUNT] = {
  36, 40, 44, 48, 149, 153, 157, 161, 165
};
static const uint32_t HOP_INTERVAL_MS = 300;
static uint8_t hopIdx = 0;
static uint32_t lastHopAt = 0;

// ---------------------------------------------------------------------------
// Analizador: contador por bin y buffer de historial
// ---------------------------------------------------------------------------
static volatile uint32_t curBinFrames = 0;
static uint32_t lastBinTick = 0;
static uint8_t  histBuf[WIFI_ANAL_HIST] = {0};
static uint8_t  histHead = 0;
static bool     histPrimed = false;

// ---------------------------------------------------------------------------
// Indice de canal 5G por numero
// ---------------------------------------------------------------------------
static int channel5IndexOf(uint8_t channel) {
  for (int i = 0; i < WIFI_5G_COUNT; i++) {
    if (HOP_CHANNELS_5[i] == channel) return i;
  }
  return -1;
}

// ---------------------------------------------------------------------------
// Callback (context: WiFi task) - corto, solo cuenta
// ---------------------------------------------------------------------------
extern "C" void promiscOnFrame(unsigned char *buf, unsigned int len, void *userdata) {
  (void)userdata;
  if (!stats.active || buf == NULL || len < 2) return;

  uint8_t fc0 = buf[0];
  uint8_t type = (fc0 >> 2) & 0x3;
  uint8_t subtype = (fc0 >> 4) & 0xF;

  stats.totalFrames++;
  stats.totalBytes += len;
  curBinFrames++;

  uint8_t ch = stats.currentChannel;
  if (stats.band == 5) {
    int idx = channel5IndexOf(ch);
    if (idx >= 0) stats.frames5PerChannel[idx]++;
  } else {
    if (ch >= 1 && ch <= 13) {
      stats.framesPerChannel[ch]++;
    }
  }

  if (type == 0x0) {
    stats.mgmtFrames++;
    switch (subtype) {
      case 0x4: stats.probeReqs++;  break;
      case 0x5: stats.probeResps++; break;
      case 0x8: stats.beacons++;    break;
      case 0xA: stats.disassocs++;  break;
      case 0xC: stats.deauths++;    break;
    }
  } else if (type == 0x1) {
    stats.ctrlFrames++;
  } else if (type == 0x2) {
    stats.dataFrames++;
    if (len >= 34 && buf[30] == 0x88 && buf[31] == 0x8E) {
      stats.eapolFrames++;
    }
  }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void sniffBegin() {
  memset(&stats, 0, sizeof(stats));
  stats.band = 2;
}

void sniffResetStats() {
  bool wasActive = stats.active;
  uint8_t currentCh = stats.currentChannel;
  uint8_t currentBand = stats.band;
  memset(&stats, 0, sizeof(stats));
  stats.active = wasActive;
  stats.currentChannel = currentCh;
  stats.band = currentBand;
}

bool sniffStart(uint8_t band) {
  sniffResetStats();
  stats.band = (band == 5) ? 5 : 2;
  stats.active = true;
  stats.startedAt = millis();

  wifi_off();
  delay(150);
  if (wifi_on(RTW_MODE_STA) != RTW_SUCCESS) {
    stats.active = false;
    return false;
  }
  delay(200);

  hopIdx = 0;
  stats.currentChannel = (stats.band == 5) ? HOP_CHANNELS_5[0] : HOP_CHANNELS_24[0];
  wifi_set_channel(stats.currentChannel);
  delay(50);

  wifi_set_promisc(RTW_PROMISC_ENABLE_2, promiscOnFrame, 0);

  wifiAnalyzerReset();
  lastHopAt = millis();
  return true;
}

void sniffStop() {
  wifi_set_promisc(RTW_PROMISC_DISABLE, NULL, 0);
  stats.active = false;

  wifi_off();
  delay(100);
  wifi_on(RTW_MODE_STA);
  delay(150);
}

void sniffTick() {
  if (!stats.active) return;
  if (millis() - lastHopAt < HOP_INTERVAL_MS) return;

  uint8_t count = (stats.band == 5) ? WIFI_5G_COUNT : WIFI_24_COUNT;
  const uint8_t *channels = (stats.band == 5) ? HOP_CHANNELS_5 : HOP_CHANNELS_24;

  hopIdx = (hopIdx + 1) % count;
  stats.currentChannel = channels[hopIdx];
  wifi_set_channel(stats.currentChannel);
  lastHopAt = millis();
}

const SnifferStats &sniffGetStats() {
  return stats;
}

// ===========================================================================
// Analizador
// ===========================================================================

void wifiAnalyzerReset() {
  memset(histBuf, 0, sizeof(histBuf));
  histHead = 0;
  histPrimed = false;
  curBinFrames = 0;
  lastBinTick = millis();
}

void wifiAnalyzerTick() {
  if (!stats.active) return;
  uint32_t now = millis();
  if (lastBinTick == 0) lastBinTick = now;
  if (now - lastBinTick < WIFI_BIN_MS) return;
  lastBinTick = now;

  uint32_t v = curBinFrames;
  curBinFrames = 0;
  uint8_t clamped = (v > 255) ? 255 : (uint8_t)v;

  histBuf[histHead] = clamped;
  histHead = (histHead + 1) % WIFI_ANAL_HIST;
  if (histHead == 0) histPrimed = true;
}

uint16_t wifiAnalyzerPps() {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < 10 && i < WIFI_ANAL_HIST; i++) {
    uint8_t idx = (histHead + WIFI_ANAL_HIST - 1 - i) % WIFI_ANAL_HIST;
    sum += histBuf[idx];
  }
  return sum;
}

uint16_t wifiAnalyzerBaseline() {
  uint32_t sum = 0;
  uint8_t count = 0;
  uint8_t limit = histPrimed ? WIFI_ANAL_HIST : histHead;
  const uint8_t SKIP = 10;
  if (limit <= SKIP + 5) return 0;

  for (uint8_t i = SKIP; i < limit; i++) {
    uint8_t idx = (histHead + WIFI_ANAL_HIST - 1 - i) % WIFI_ANAL_HIST;
    sum += histBuf[idx];
    count++;
  }
  if (count == 0) return 0;
  return (uint16_t)((sum * 10) / count);
}

WifiAnalStatus wifiAnalyzerStatus() {
  uint16_t base = wifiAnalyzerBaseline();
  uint16_t cur  = wifiAnalyzerPps();
  uint8_t filled = histPrimed ? WIFI_ANAL_HIST : histHead;
  if (filled < 25 || base < 6) return WIFI_ANAL_IDLE;
  if ((uint32_t)cur * 3 < (uint32_t)base) return WIFI_ANAL_LOW;
  if ((uint32_t)cur > (uint32_t)base * 2 + 12) return WIFI_ANAL_HIGH;
  return WIFI_ANAL_NORMAL;
}

const uint8_t *wifiAnalyzerHistory() { return histBuf; }
uint8_t wifiAnalyzerHistoryHead() { return histHead; }
uint8_t wifiAnalyzerBand() { return stats.band; }

const char *wifiAnalStatusLabel(WifiAnalStatus s) {
  switch (s) {
    case WIFI_ANAL_NORMAL: return "Normal";
    case WIFI_ANAL_LOW:    return "Bajo";
    case WIFI_ANAL_HIGH:   return "Alto";
    default:               return "Calibrando";
  }
}

// ---------------------------------------------------------------------------
// Helpers de canales por banda
// ---------------------------------------------------------------------------
uint8_t wifiBandChannelCount() {
  return (stats.band == 5) ? WIFI_5G_COUNT : WIFI_24_COUNT;
}

uint8_t wifiBandChannelNumber(uint8_t idx) {
  if (stats.band == 5) {
    return (idx < WIFI_5G_COUNT) ? HOP_CHANNELS_5[idx] : 0;
  }
  return (idx < WIFI_24_COUNT) ? HOP_CHANNELS_24[idx] : 0;
}

uint32_t wifiBandChannelFrames(uint8_t idx) {
  if (stats.band == 5) {
    return (idx < WIFI_5G_COUNT) ? stats.frames5PerChannel[idx] : 0;
  }
  // 2.4G: idx 0..12 -> CH 1..13
  if (idx >= WIFI_24_COUNT) return 0;
  return stats.framesPerChannel[idx + 1];
}

int wifiBandBusiestChannelIdx() {
  uint8_t count = wifiBandChannelCount();
  int best = -1;
  uint32_t bestVal = 0;
  for (uint8_t i = 0; i < count; i++) {
    uint32_t v = wifiBandChannelFrames(i);
    if (v > bestVal) {
      bestVal = v;
      best = i;
    }
  }
  return best;
}
