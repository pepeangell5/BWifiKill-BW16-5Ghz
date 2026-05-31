#pragma once

#include <Arduino.h>

struct BleSpamStats {
  bool active;
  uint16_t currentIdx;
  uint32_t totalTx;
  uint32_t startedAt;
};

void bleSpamBegin();
bool bleSpamStart();
void bleSpamStop();
void bleSpamTick();
bool bleSpamActive();
const BleSpamStats &bleSpamGetStats();
uint16_t bleSpamCount();
const char *bleSpamCurrent();
