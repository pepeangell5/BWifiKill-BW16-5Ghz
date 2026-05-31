#pragma once

#include "WifiScanner.h"

void targetClear();
void targetSet(const NetworkInfo &network);
bool targetHasSelection();
const NetworkInfo &targetGet();

