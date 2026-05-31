#pragma once

#include <Adafruit_ST7735.h>

// === Estructura y superficies ===
static constexpr uint16_t UI_BG          = 0x0841; // navy oscuro
static constexpr uint16_t UI_PANEL       = 0x18C3; // panel / card sobre BG
static constexpr uint16_t UI_PANEL_DARK  = 0x1082; // panel mas oscuro
static constexpr uint16_t UI_LINE        = 0x39E7; // separador
static constexpr uint16_t UI_LINE_SOFT   = 0x2104; // separador muy sutil

// === Texto ===
static constexpr uint16_t UI_TEXT        = 0xFFFF; // blanco
static constexpr uint16_t UI_TEXT_DIM    = 0xC618; // gris claro
static constexpr uint16_t UI_MUTED       = 0x73AE; // gris medio (calido)

// === Acentos / estado ===
static constexpr uint16_t UI_BRAND       = 0xFB60; // naranja
static constexpr uint16_t UI_INFO        = 0x05FF; // cyan
static constexpr uint16_t UI_OK          = 0x2FE6; // verde menta
static constexpr uint16_t UI_WARN        = 0xFD20; // ambar
static constexpr uint16_t UI_DANGER      = 0xE104; // carmin (no rojo puro)
static constexpr uint16_t UI_SELECT      = 0x6800; // borgona (fondo seleccion)
static constexpr uint16_t UI_ACCENT_ALT  = 0xF81F; // magenta (uso ocasional)

// === Spacing tokens ===
static constexpr int8_t   UI_PAD         = 4;
static constexpr int8_t   UI_STATUSBAR_H = 14;