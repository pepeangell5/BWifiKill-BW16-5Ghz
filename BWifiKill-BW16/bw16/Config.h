#pragma once

#include <Arduino.h>

#define FIRMWARE_NAME "BWifiKill"
#define FIRMWARE_VERSION "v0.2"
#define FIRMWARE_BOARD "BW16 RTL8720DN"

// TFT ST7735 wiring
#define TFT_CS   PA27
#define TFT_DC   PA25
#define TFT_RST  PA26
#define TFT_BL   PA30

// Button wiring. Buttons should pull the pin to GND when pressed.
#define BTN_UP   PB1
#define BTN_OK   PB3
#define BTN_DOWN PB2
