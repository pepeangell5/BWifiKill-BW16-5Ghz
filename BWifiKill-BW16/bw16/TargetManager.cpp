#include "TargetManager.h"

static bool hasTarget = false;
static NetworkInfo selectedTarget;

void targetClear() {
  hasTarget = false;
}

void targetSet(const NetworkInfo &network) {
  selectedTarget = network;
  hasTarget = true;
}

bool targetHasSelection() {
  return hasTarget;
}

const NetworkInfo &targetGet() {
  return selectedTarget;
}

