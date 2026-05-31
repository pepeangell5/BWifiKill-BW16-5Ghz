#pragma once

#include <Arduino.h>

struct BeaconSpamStats {
  bool active;
  uint8_t band;            // 2 o 5
  uint8_t currentChannel;
  uint16_t currentSsidIdx;
  uint32_t totalTx;
  uint32_t startedAt;
};

void beaconSpamBegin();
bool beaconSpamStart(uint8_t band);
void beaconSpamStop();
void beaconSpamTick();
bool beaconSpamActive();
const BeaconSpamStats &beaconSpamGetStats();
uint16_t beaconSpamSsidCount();
const char *beaconSpamCurrentSsid();
