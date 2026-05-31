#pragma once

#include <Arduino.h>
#include "LabStats.h"
#include "WifiScanner.h"

enum LabTestState {
  LAB_TEST_IDLE,
  LAB_TEST_READY,
  LAB_TEST_MEASURED,
  LAB_TEST_SIMULATED,
  LAB_TEST_BLOCKED
};

struct LabTestReport {
  LabTestState state;
  char title[20];
  char line1[28];
  char line2[28];
  char line3[28];
  uint16_t attempts;
};

void labTestPrepare(const NetworkInfo &target);
void labTestEvaluate(const NetworkInfo &target, bool found, const LabStats &stats);
void labTestSimulateDeauth(const NetworkInfo &target, bool foundAfter, const LabStats &stats, bool txInvoked = false);
void labTestReset();
const LabTestReport &labTestGetReport();
void labTestPrintToSerial();
