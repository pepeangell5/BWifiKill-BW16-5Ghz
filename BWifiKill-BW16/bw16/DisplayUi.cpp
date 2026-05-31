#include "DisplayUi.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <math.h>
#include <string.h>

#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/Picopixel.h>

#include "Config.h"
#include "SplashScreen.h"
#include "Theme.h"

static Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// Cache para animar el icono del menu
static const char *lastMenuTitle = NULL;
static const char *const *lastMenuItems = NULL;
static const char *lastMenuFooter = NULL;
static uint8_t lastMenuItemCount = 0;
static uint8_t lastMenuSelected = 0;
static uint8_t menuAnimFrame = 0;
static uint32_t lastMenuAnimAt = 0;
static const int MENU_ICON_X = 64;
static const int MENU_ICON_Y = 58;

// ==========================================================================
// Font helpers
// ==========================================================================
static void useDefault()    { tft.setFont(NULL);                 tft.setTextSize(1); }
static void useDefaultBig() { tft.setFont(NULL);                 tft.setTextSize(2); }
static void useTiny()       { tft.setFont(&Picopixel);           tft.setTextSize(1); }
static void useHero()       { tft.setFont(&FreeSansBold9pt7b);   tft.setTextSize(1); }

// Todos los helpers reciben yTop. Las fuentes GFX necesitan baseline, el default no.
static void textDefault(int x, int yTop, uint16_t color, const char *txt) {
  useDefault();
  tft.setTextColor(color);
  tft.setCursor(x, yTop);
  tft.print(txt);
}

static void textTiny(int x, int yTop, uint16_t color, const char *txt) {
  useTiny();
  tft.setTextColor(color);
  tft.setCursor(x, yTop + 5);
  tft.print(txt);
}

static uint16_t getStrWidth(const char *txt) {
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  return w;
}

// ==========================================================================
// SSID con truncado + elipsis
// ==========================================================================
static void printSsidSafe(const char *ssid, uint8_t maxChars) {
  if (ssid == NULL || ssid[0] == '\0') {
    tft.print("<oculta>");
    return;
  }
  uint8_t len = strlen(ssid);
  if (len <= maxChars) {
    tft.print(ssid);
    return;
  }
  for (uint8_t i = 0; i < maxChars - 1; i++) tft.write(ssid[i]);
  tft.print((char)'.');
}

// ==========================================================================
// Status bar (siempre con fuente default, cabe en 14px)
// ==========================================================================
static void drawStatusBar(const char *title) {
  tft.fillRect(0, 0, tft.width(), UI_STATUSBAR_H, UI_PANEL);
  tft.drawFastHLine(0, UI_STATUSBAR_H, tft.width(), UI_LINE);
  textDefault(UI_PAD, 4, UI_TEXT, title);
}

static void statusBarRight(const char *text, uint16_t color) {
  useDefault();
  uint16_t w = strlen(text) * 6;
  tft.setTextColor(color);
  tft.setCursor(tft.width() - w - 4, 4);
  tft.print(text);
}

static void statusBarTarget(bool hasTarget) {
  int rx = tft.width() - 8;
  if (hasTarget) tft.fillCircle(rx, 7, 3, UI_OK);
  else           tft.drawCircle(rx, 7, 3, UI_MUTED);
}

static void statusBarBusy() {
  uint8_t pulse = (millis() / 250) & 1;
  tft.fillCircle(tft.width() - 18, 7, 2, pulse ? UI_BRAND : UI_LINE);
}

// ==========================================================================
// Componentes
// ==========================================================================
static void drawCard(int x, int y, int w, int h) {
  tft.fillRoundRect(x, y, w, h, 4, UI_PANEL);
  tft.drawRoundRect(x, y, w, h, 4, UI_LINE);
}

static void drawRssiBars(int x, int y, int rssi) {
  int level = constrain(map(rssi, -90, -40, 0, 4), 0, 4);
  uint16_t fg;
  if (level >= 3)      fg = UI_OK;
  else if (level >= 2) fg = UI_WARN;
  else if (level >= 1) fg = UI_BRAND;
  else                 fg = UI_DANGER;
  for (int i = 0; i < 4; i++) {
    int bh = 2 + i * 2;
    int bx = x + i * 4;
    int by = y + (8 - bh);
    uint16_t c = (i < level) ? fg : UI_LINE_SOFT;
    tft.fillRect(bx, by, 3, bh, c);
  }
}

static void drawSecBadge(int x, int y, uint32_t sec) {
  const char *txt;
  uint16_t bg;
  switch (sec) {
    case RTW_SECURITY_OPEN:           txt = "OPEN"; bg = UI_DANGER; break;
    case RTW_SECURITY_WEP_PSK:        txt = "WEP";  bg = UI_DANGER; break;
    case RTW_SECURITY_WPA3_AES_PSK:
    case RTW_SECURITY_WPA2_WPA3_MIXED:
    case RTW_SECURITY_WPA2_AES_CMAC:  txt = "WPA3"; bg = UI_OK;     break;
    case RTW_SECURITY_WPA_TKIP_PSK:
    case RTW_SECURITY_WPA_AES_PSK:    txt = "WPA";  bg = UI_WARN;   break;
    default:                          txt = "WPA2"; bg = UI_INFO;   break;
  }
  useTiny();
  uint16_t w = getStrWidth(txt) + 8;
  tft.fillRoundRect(x, y, w, 9, 2, bg);
  tft.setTextColor(UI_BG);
  tft.setCursor(x + 4, y + 7);
  tft.print(txt);
}

static void drawBandTag(int x, int y, uint8_t channel) {
  bool is5 = channel >= 14;
  const char *txt = is5 ? "5G" : "2.4G";
  uint16_t color = is5 ? UI_BRAND : UI_INFO;
  useTiny();
  uint16_t w = getStrWidth(txt) + 6;
  tft.drawRoundRect(x, y, w, 9, 2, color);
  tft.setTextColor(color);
  tft.setCursor(x + 3, y + 7);
  tft.print(txt);
}

static void drawChannelTag(int x, int y, uint8_t channel) {
  char buf[8];
  snprintf(buf, sizeof(buf), "CH%u", channel);
  useTiny();
  uint16_t w = getStrWidth(buf) + 6;
  tft.drawRoundRect(x, y, w, 9, 2, UI_TEXT_DIM);
  tft.setTextColor(UI_TEXT_DIM);
  tft.setCursor(x + 3, y + 7);
  tft.print(buf);
}

// ==========================================================================
// Icons (sin cambios)
// ==========================================================================
static bool textHas(const char *text, const char *needle) {
  return strstr(text, needle) != NULL;
}

static void drawWifiIcon(int cx, int cy, uint8_t pulse) {
  uint16_t c = pulse ? UI_OK : UI_INFO;
  tft.drawCircle(cx, cy + 13, 2 + pulse, UI_TEXT);
  tft.drawCircle(cx, cy + 13, 8 + pulse, c);
  tft.drawCircle(cx, cy + 13, 16 + pulse, c);
  tft.fillRect(cx - 20, cy + 13, 40, 20, UI_BG);
  tft.fillCircle(cx, cy + 13, 3 + pulse, UI_TEXT);
}

static void drawTargetIcon(int cx, int cy, uint8_t pulse) {
  uint16_t c = pulse ? UI_WARN : UI_OK;
  tft.drawCircle(cx, cy, 17 + pulse, c);
  tft.drawCircle(cx, cy, 8 + pulse, UI_TEXT);
  tft.drawFastHLine(cx - 23, cy, 46, UI_DANGER);
  tft.drawFastVLine(cx, cy - 23, 46, UI_DANGER);
  tft.fillCircle(cx, cy, 3, UI_WARN);
}

static void drawLabIcon(int cx, int cy, uint8_t pulse) {
  uint16_t c = pulse ? UI_WARN : UI_BRAND;
  tft.drawRect(cx - 8, cy - 20, 16, 7, UI_TEXT);
  tft.drawLine(cx - 6, cy - 13, cx - 18, cy + 18, UI_TEXT);
  tft.drawLine(cx + 6, cy - 13, cx + 18, cy + 18, UI_TEXT);
  tft.drawFastHLine(cx - 18, cy + 18, 37, UI_TEXT);
  tft.fillTriangle(cx - 12, cy + 8, cx + 12, cy + 8, cx + 16, cy + 17, c);
  tft.fillCircle(cx - 4, cy + 6, 2 + pulse, UI_INFO);
  tft.fillCircle(cx + 8, cy + 13, 2, UI_OK);
}

static void drawDeauthIcon(int cx, int cy, uint8_t pulse) {
  uint16_t c = pulse ? UI_DANGER : UI_WARN;
  tft.drawTriangle(cx, cy - 22, cx - 23, cy + 20, cx + 23, cy + 20, c);
  tft.drawFastVLine(cx, cy - 8, 16, UI_TEXT);
  tft.fillCircle(cx, cy + 13, 3 + pulse, UI_TEXT);
  tft.drawCircle(cx, cy, 25 + pulse, UI_DANGER);
}

static void drawAnalyzerIcon(int cx, int cy, uint8_t pulse) {
  uint16_t c = pulse ? UI_INFO : UI_BRAND;
  tft.fillRect(cx - 22, cy + 8, 8, 16 + pulse, UI_OK);
  tft.fillRect(cx - 5, cy - 2, 8, 26 + pulse, c);
  tft.fillRect(cx + 12, cy - 12, 8, 36 + pulse, UI_WARN);
  tft.drawFastHLine(cx - 27, cy + 26, 54, UI_TEXT);
}

static void drawBackIcon(int cx, int cy, uint8_t pulse) {
  uint16_t c = pulse ? UI_OK : UI_INFO;
  tft.drawLine(cx - 23, cy, cx - 4, cy - 18, c);
  tft.drawLine(cx - 23, cy, cx - 4, cy + 18, c);
  tft.drawFastHLine(cx - 21, cy, 48, c);
}

static void drawSystemIcon(int cx, int cy, uint8_t pulse) {
  tft.drawCircle(cx, cy, 17 + pulse, UI_TEXT);
  tft.drawCircle(cx, cy, 6 + pulse, UI_INFO);
  for (uint8_t i = 0; i < 8; i++) {
    float a = i * 0.785398f;
    int x1 = cx + cos(a) * 21;
    int y1 = cy + sin(a) * 21;
    int x2 = cx + cos(a) * 26;
    int y2 = cy + sin(a) * 26;
    tft.drawLine(x1, y1, x2, y2, UI_BRAND);
  }
}

static void drawBluetoothIcon(int cx, int cy, uint8_t pulse) {
  uint16_t c = pulse ? UI_INFO : UI_TEXT;
  tft.drawFastVLine(cx, cy - 22, 44, c);
  tft.drawLine(cx, cy - 22, cx + 18, cy - 10, UI_INFO);
  tft.drawLine(cx + 18, cy - 10, cx, cy + 3, UI_INFO);
  tft.drawLine(cx, cy + 3, cx + 18, cy + 16, UI_INFO);
  tft.drawLine(cx + 18, cy + 16, cx, cy + 22, UI_INFO);
  tft.drawLine(cx, cy, cx - 15, cy - 13, UI_BRAND);
  tft.drawLine(cx, cy, cx - 15, cy + 13, UI_BRAND);
}

static void drawDefaultIcon(int cx, int cy, uint8_t pulse) {
  tft.drawRoundRect(cx - 26, cy - 22, 52, 44, 5, UI_INFO);
  tft.fillCircle(cx, cy, 9 + pulse, UI_BRAND);
}

static void drawSnifferIcon(int cx, int cy, uint8_t pulse) {
  // Antena con ondas RF arriba + caja de captura con "datos" abajo
  uint16_t c = pulse ? UI_OK : UI_INFO;
  // Antena
  tft.drawFastVLine(cx, cy - 22, 12, UI_TEXT);
  tft.fillCircle(cx, cy - 22, 2, UI_BRAND);
  // Dos arcos de ondas RF
  tft.drawCircle(cx - 8,  cy - 18, 3 + pulse, c);
  tft.drawCircle(cx + 8,  cy - 18, 3 + pulse, c);
  // Caja de captura
  tft.drawRoundRect(cx - 22, cy - 8, 44, 26, 3, UI_BRAND);
  // Datos como lineas tipo terminal
  for (int i = 0; i < 4; i++) {
    int yLine = cy - 4 + i * 5;
    int w1 = 10 + ((i + pulse) * 3) % 12;
    int w2 = 6 + ((i * 2 + pulse) * 2) % 10;
    tft.drawFastHLine(cx - 18, yLine, w1, UI_OK);
    tft.drawFastHLine(cx - 18 + w1 + 2, yLine, w2, UI_WARN);
  }
}

static void drawSniffer5Icon(int cx, int cy, uint8_t pulse) {
  // Mismo layout que Sniffer 2.4G pero paleta calida para distinguir banda
  uint16_t c = pulse ? UI_BRAND : UI_WARN;
  // Antena
  tft.drawFastVLine(cx, cy - 22, 12, UI_TEXT);
  tft.fillCircle(cx, cy - 22, 2, UI_DANGER);
  // Ondas RF
  tft.drawCircle(cx - 8,  cy - 18, 3 + pulse, c);
  tft.drawCircle(cx + 8,  cy - 18, 3 + pulse, c);
  // Caja de captura con borde calido
  tft.drawRoundRect(cx - 22, cy - 8, 44, 26, 3, UI_WARN);
  // Datos
  for (int i = 0; i < 4; i++) {
    int yLine = cy - 4 + i * 5;
    int w1 = 10 + ((i + pulse) * 3) % 12;
    int w2 = 6 + ((i * 2 + pulse) * 2) % 10;
    tft.drawFastHLine(cx - 18, yLine, w1, UI_BRAND);
    tft.drawFastHLine(cx - 18 + w1 + 2, yLine, w2, UI_DANGER);
  }
}

static void drawBeacon24Icon(int cx, int cy, uint8_t pulse) {
  // Antena central transmitiendo ondas concentricas + SSIDs flotantes (cuadritos)
  uint16_t c1 = pulse ? UI_OK : UI_INFO;
  uint16_t c2 = pulse ? UI_INFO : UI_OK;
  // Antena
  tft.drawFastVLine(cx, cy - 14, 18, UI_TEXT);
  tft.fillCircle(cx, cy - 14, 3, UI_BRAND);
  // Ondas concentricas
  tft.drawCircle(cx, cy - 10, 6 + pulse,  c1);
  tft.drawCircle(cx, cy - 10, 12 + pulse, c2);
  tft.drawCircle(cx, cy - 10, 18 + pulse, c1);
  // Cuadritos como "SSIDs" flotantes
  tft.fillRect(cx - 24, cy + 8,  7, 3, UI_WARN);
  tft.fillRect(cx + 17, cy + 8,  7, 3, UI_WARN);
  tft.fillRect(cx - 14, cy + 15, 9, 3, UI_BRAND);
  tft.fillRect(cx + 5,  cy + 15, 9, 3, UI_BRAND);
  tft.fillRect(cx - 5,  cy + 21, 10, 3, UI_OK);
}

static void drawBeacon5Icon(int cx, int cy, uint8_t pulse) {
  // Mismo que beacon 2.4 pero paleta calida
  uint16_t c1 = pulse ? UI_BRAND : UI_WARN;
  uint16_t c2 = pulse ? UI_WARN : UI_BRAND;
  tft.drawFastVLine(cx, cy - 14, 18, UI_TEXT);
  tft.fillCircle(cx, cy - 14, 3, UI_DANGER);
  tft.drawCircle(cx, cy - 10, 6 + pulse,  c1);
  tft.drawCircle(cx, cy - 10, 12 + pulse, c2);
  tft.drawCircle(cx, cy - 10, 18 + pulse, c1);
  tft.fillRect(cx - 24, cy + 8,  7, 3, UI_DANGER);
  tft.fillRect(cx + 17, cy + 8,  7, 3, UI_DANGER);
  tft.fillRect(cx - 14, cy + 15, 9, 3, UI_WARN);
  tft.fillRect(cx + 5,  cy + 15, 9, 3, UI_WARN);
  tft.fillRect(cx - 5,  cy + 21, 10, 3, UI_BRAND);
}

static void drawBleSpamIcon(int cx, int cy, uint8_t pulse) {
  // Logo BT central + multiples devices fantasma orbitando
  uint16_t c = pulse ? UI_INFO : UI_BRAND;
  // Logo BT (mismo trazo que drawBluetoothIcon pero pequeno)
  tft.drawFastVLine(cx, cy - 12, 24, UI_TEXT);
  tft.drawLine(cx, cy - 12, cx + 8, cy - 6, UI_INFO);
  tft.drawLine(cx + 8, cy - 6, cx, cy + 1, UI_INFO);
  tft.drawLine(cx, cy + 1, cx + 8, cy + 8, UI_INFO);
  tft.drawLine(cx + 8, cy + 8, cx, cy + 12, UI_INFO);
  tft.drawLine(cx, cy, cx - 7, cy - 7, UI_BRAND);
  tft.drawLine(cx, cy, cx - 7, cy + 7, UI_BRAND);
  // Ondas saliendo
  tft.drawCircle(cx, cy, 20 + pulse, c);
  // Devices fantasma alrededor
  tft.fillCircle(cx - 24, cy - 16, 2, UI_OK);
  tft.fillCircle(cx + 22, cy - 14, 2, UI_WARN);
  tft.fillCircle(cx - 22, cy + 18, 2, UI_DANGER);
  tft.fillCircle(cx + 23, cy + 17, 2, UI_INFO);
  tft.fillCircle(cx - 26, cy + 4,  2, UI_BRAND);
  tft.fillCircle(cx + 25, cy + 2,  2, UI_OK);
}

static void drawTraffic24Icon(int cx, int cy, uint8_t pulse) {
  // Bar chart de 4 barras (la cantidad alude a "2.4")
  uint16_t cBase = pulse ? UI_OK : UI_INFO;
  int baseY = cy + 18;
  // Ejes
  tft.drawFastVLine(cx - 22, cy - 22, baseY - (cy - 22), UI_TEXT_DIM);
  tft.drawFastHLine(cx - 22, baseY,    44,                UI_TEXT);
  // Barras
  int heights[4] = { 14, 26, 12, 22 };
  uint16_t cols[4] = { cBase, UI_BRAND, UI_WARN, cBase };
  int x0 = cx - 16;
  for (int i = 0; i < 4; i++) {
    int barH = heights[i] + pulse;
    tft.fillRect(x0 + i * 10, baseY - barH, 7, barH, cols[i]);
  }
}

static void drawTraffic5Icon(int cx, int cy, uint8_t pulse) {
  // Bar chart de 5 barras (la cantidad alude a "5G"), paleta calida
  uint16_t cBase = pulse ? UI_WARN : UI_BRAND;
  int baseY = cy + 18;
  // Ejes
  tft.drawFastVLine(cx - 22, cy - 22, baseY - (cy - 22), UI_TEXT_DIM);
  tft.drawFastHLine(cx - 22, baseY,    44,                UI_TEXT);
  // Barras (mas finas para que entren 5)
  int heights[5] = { 18, 28, 14, 24, 10 };
  uint16_t cols[5] = { cBase, UI_OK, cBase, UI_INFO, UI_WARN };
  int x0 = cx - 18;
  for (int i = 0; i < 5; i++) {
    int barH = heights[i] + pulse;
    tft.fillRect(x0 + i * 8, baseY - barH, 6, barH, cols[i]);
  }
}

static void drawMenuIcon(const char *item, int cx, int cy, uint8_t pulse) {
  // Chequeos especificos primero (Trafico 2.4G/5G y Sniffer 2.4/5)
  if (textHas(item, "Trafico 2.4")) {
    drawTraffic24Icon(cx, cy, pulse);
  } else if (textHas(item, "Trafico 5")) {
    drawTraffic5Icon(cx, cy, pulse);
  } else if (textHas(item, "Beacon 2.4")) {
    drawBeacon24Icon(cx, cy, pulse);
  } else if (textHas(item, "Beacon 5")) {
    drawBeacon5Icon(cx, cy, pulse);
  } else if (textHas(item, "Sniffer 5")) {
    drawSniffer5Icon(cx, cy, pulse);
  } else if (textHas(item, "Sniffer")) {
    drawSnifferIcon(cx, cy, pulse);
  } else if (textHas(item, "BLE spam")) {
    drawBleSpamIcon(cx, cy, pulse);
  } else if (textHas(item, "WiFi") || textHas(item, "Scan") || textHas(item, "GHz")) {
    drawWifiIcon(cx, cy, pulse);
  } else if (textHas(item, "Objetivo") || textHas(item, "Monitor")) {
    drawTargetIcon(cx, cy, pulse);
  } else if (textHas(item, "Deauth")) {
    drawDeauthIcon(cx, cy, pulse);
  } else if (textHas(item, "Pruebas") || textHas(item, "Precheck") || textHas(item, "Prueba")) {
    drawLabIcon(cx, cy, pulse);
  } else if (textHas(item, "Analizador") || textHas(item, "Resultados") || textHas(item, "stats")) {
    drawAnalyzerIcon(cx, cy, pulse);
  } else if (textHas(item, "Sistema") || textHas(item, "Reset")) {
    drawSystemIcon(cx, cy, pulse);
  } else if (textHas(item, "Bluetooth")) {
    drawBluetoothIcon(cx, cy, pulse);
  } else if (textHas(item, "Volver")) {
    drawBackIcon(cx, cy, pulse);
  } else {
    drawDefaultIcon(cx, cy, pulse);
  }
}

// ==========================================================================
// API
// ==========================================================================
void uiBegin() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(UI_BG);
}

void uiDrawSplash() {
  splashDraw(tft);
}

void uiDrawHome() {
  tft.fillScreen(UI_BG);
  drawStatusBar(FIRMWARE_NAME);
  textDefault(UI_PAD, 25, UI_TEXT, "BW16 2.4 / 5GHz");
  textDefault(UI_PAD, 50, UI_MUTED, "UP    OK    DOWN");
  textDefault(UI_PAD, 65, UI_MUTED, "OK inicia scan");
}

void uiDrawStatus(const char *message) {
  tft.fillScreen(UI_BG);
  drawStatusBar(FIRMWARE_NAME);
  statusBarBusy();
  textDefault(UI_PAD, 60, UI_WARN, message);
}

void uiDrawTxCounter(uint32_t packetCount) {
  tft.fillScreen(UI_BG);
  drawStatusBar("Deauth lab");
  statusBarTarget(true);

  textDefault(UI_PAD, 22, UI_TEXT_DIM, "Paquetes enviados");

  uint8_t pulse = (millis() / 200) & 1;
  tft.fillCircle(tft.width() - 12, 25, 3, pulse ? UI_DANGER : UI_PANEL);

  char buf[12];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)packetCount);
  useDefaultBig();
  tft.setTextColor(UI_OK);
  tft.setCursor(UI_PAD, 50);
  tft.print(buf);

  textDefault(UI_PAD, tft.height() - 10, UI_MUTED, "OK detiene");
}

void uiDrawMenu(const char *title, const char *const items[], uint8_t itemCount,
                uint8_t selected, const char *footer) {
  lastMenuTitle = title;
  lastMenuItems = items;
  lastMenuItemCount = itemCount;
  lastMenuSelected = selected;
  lastMenuFooter = footer;

  tft.fillScreen(UI_BG);
  drawStatusBar(title);

  char pos[8];
  snprintf(pos, sizeof(pos), "%u/%u", selected + 1, itemCount);
  statusBarRight(pos, UI_BRAND);

  // Previous item
  useDefault();
  tft.setTextColor(UI_MUTED);
  tft.setCursor(UI_PAD, 22);
  tft.print("^ ");
  tft.print(selected == 0 ? items[itemCount - 1] : items[selected - 1]);

  // Icon
  uint8_t pulse = menuAnimFrame & 0x01;
  drawMenuIcon(items[selected], MENU_ICON_X, MENU_ICON_Y, pulse);

  // Selected pill - tamano segun longitud
  int pillY = 96;
  int pillH = 22;
  tft.fillRoundRect(6, pillY, tft.width() - 12, pillH, 4, UI_SELECT);
  tft.drawRoundRect(6, pillY, tft.width() - 12, pillH, 4, UI_BRAND);

  uint8_t labelLen = strlen(items[selected]);
  bool isShort = labelLen <= 9;
  useDefault();
  tft.setTextSize(isShort ? 2 : 1);
  uint8_t charW = isShort ? 12 : 6;
  uint16_t lw = labelLen * charW;
  int lx = (tft.width() - lw) / 2;
  if (lx < UI_PAD) lx = UI_PAD;
  tft.setTextColor(UI_TEXT);
  tft.setCursor(lx, pillY + (isShort ? 4 : 8));
  tft.print(items[selected]);
  tft.setTextSize(1);

  // Next item
  useDefault();
  tft.setTextColor(UI_MUTED);
  tft.setCursor(UI_PAD, 128);
  tft.print("v ");
  tft.print(selected + 1 >= itemCount ? items[0] : items[selected + 1]);

  // Footer
  tft.drawFastHLine(UI_PAD, 142, tft.width() - UI_PAD * 2, UI_LINE_SOFT);
  textDefault(UI_PAD, 148, UI_MUTED, footer);
}

void uiTickMenuAnimation() {
  if (lastMenuTitle == NULL || lastMenuItems == NULL || lastMenuItemCount == 0) return;
  uint32_t now = millis();
  if (now - lastMenuAnimAt < 260) return;
  lastMenuAnimAt = now;
  menuAnimFrame++;
  tft.fillRect(MENU_ICON_X - 30, MENU_ICON_Y - 28, 60, 60, UI_BG);
  drawMenuIcon(lastMenuItems[lastMenuSelected], MENU_ICON_X, MENU_ICON_Y, menuAnimFrame & 0x01);
}

static const char *const BAND_MENU_ITEMS[] = { "5GHz", "2.4GHz", "Volver" };

void uiDrawBandMenu(uint8_t selectedBand) {
  uint8_t selected = 0;
  if (selectedBand == 2) selected = 1;
  else if (selectedBand == 0) selected = 2;
  uiDrawMenu("Elegir banda", BAND_MENU_ITEMS, 3, selected, "OK entra");
}

void uiDrawNetworkList(uint8_t selectedBand, int selectedNetwork, int listTop) {
  tft.fillScreen(UI_BG);

  char title[18];
  snprintf(title, sizeof(title), "WiFi %s", selectedBand == 5 ? "5GHz" : "2.4GHz");
  drawStatusBar(title);

  uint8_t visibleCount = wifiScannerCountBand(selectedBand);
  char countTxt[8];
  snprintf(countTxt, sizeof(countTxt), "%u", visibleCount);
  statusBarRight(countTxt, UI_BRAND);

  // Fila fija de "Volver" arriba (selectedNetwork == -1)
  bool backSel = (selectedNetwork == -1);
  int backY = 17;
  if (backSel) {
    tft.fillRoundRect(2, backY - 1, tft.width() - 4, 11, 2, UI_SELECT);
  }
  useDefault();
  tft.setTextColor(backSel ? UI_TEXT : UI_INFO);
  tft.setCursor(UI_PAD, backY + 2);
  tft.print("< Volver a bandas");

  tft.drawFastHLine(2, backY + 12, tft.width() - 4, UI_LINE_SOFT);

  if (visibleCount == 0) {
    textDefault(UI_PAD, 45, UI_WARN, "Sin redes en esta banda");
    textDefault(UI_PAD, 60, UI_MUTED, "OK vuelve");
    return;
  }

  // 4 filas de red visibles
  int rowY = 33;
  uint8_t shown = 0;
  for (uint8_t idx = listTop; idx < wifiScannerCount() && shown < 4; idx++) {
    if (!wifiScannerNetworkInBand(idx, selectedBand)) continue;
    const NetworkInfo &net = wifiScannerNetwork(idx);
    bool isSel = ((int)idx == selectedNetwork);

    if (isSel) {
      tft.fillRoundRect(2, rowY - 1, tft.width() - 4, 10, 2, UI_SELECT);
    }
    useDefault();
    tft.setTextColor(isSel ? UI_TEXT : UI_TEXT_DIM);
    tft.setCursor(UI_PAD, rowY + 1);
    printSsidSafe(net.ssid, 15);

    drawRssiBars(tft.width() - 22, rowY, net.rssi);
    rowY += 11;
    shown++;
  }

  // Card de detalle si hay red real seleccionada
  if (selectedNetwork >= 0 && selectedNetwork < (int)wifiScannerCount() &&
      wifiScannerNetworkInBand(selectedNetwork, selectedBand)) {
    const NetworkInfo &net = wifiScannerNetwork(selectedNetwork);

    int cardY = 80;
    drawCard(2, cardY, tft.width() - 4, 76);

    int bx = 6, by = cardY + 5;
    drawChannelTag(bx, by, net.channel); bx += 26;
    drawBandTag(bx, by, net.channel);    bx += 22;
    drawSecBadge(bx, by, net.security);

    char rbuf[16];
    snprintf(rbuf, sizeof(rbuf), "%ld dBm", (long)net.rssi);
    textDefault(6, cardY + 22, UI_TEXT, rbuf);
    drawRssiBars(tft.width() - 26, cardY + 22, net.rssi);

    textDefault(6, cardY + 38, UI_TEXT_DIM, net.bssid);
    textDefault(6, cardY + 54, UI_MUTED, "OK guarda objetivo");
  } else if (backSel) {
    int cardY = 80;
    drawCard(2, cardY, tft.width() - 4, 50);
    textTiny(UI_PAD + 4, cardY + 6, UI_MUTED, "REGRESAR");
    textDefault(UI_PAD + 4, cardY + 18, UI_INFO, "OK vuelve atras");
    textDefault(UI_PAD + 4, cardY + 34, UI_MUTED, "DOWN ve redes");
  }
}

static void drawDetailScreen(const char *header, const NetworkInfo &net,
                             const char *hint, uint16_t headerColor) {
  tft.fillScreen(UI_BG);
  drawStatusBar(header);
  statusBarTarget(true);

  // SSID prominente (FreeSansBold9pt7b size 1, ~14px de alto)
  useHero();
  tft.setTextColor(headerColor);
  tft.setCursor(UI_PAD, 30); // baseline; top ~y=18
  printSsidSafe(net.ssid, 14);

  // Fila de badges
  int bx = UI_PAD, by = 40;
  drawChannelTag(bx, by, net.channel); bx += 26;
  drawBandTag(bx, by, net.channel);    bx += 22;
  drawSecBadge(bx, by, net.security);

  // Card RSSI
  drawCard(UI_PAD, 56, tft.width() - UI_PAD * 2, 28);
  textTiny(UI_PAD + 4, 60, UI_MUTED, "RSSI");
  char rbuf[16];
  snprintf(rbuf, sizeof(rbuf), "%ld dBm", (long)net.rssi);
  textDefault(UI_PAD + 4, 72, UI_TEXT, rbuf);
  drawRssiBars(tft.width() - UI_PAD - 24, 72, net.rssi);

  // Card BSSID
  drawCard(UI_PAD, 90, tft.width() - UI_PAD * 2, 24);
  textTiny(UI_PAD + 4, 94, UI_MUTED, "BSSID");
  textDefault(UI_PAD + 4, 104, UI_TEXT, net.bssid);

  // Pill de accion
  tft.fillRoundRect(UI_PAD, 122, tft.width() - UI_PAD * 2, 16, 3, UI_PANEL_DARK);
  tft.drawRoundRect(UI_PAD, 122, tft.width() - UI_PAD * 2, 16, 3, UI_BRAND);
  useDefault();
  uint16_t hw = strlen(hint) * 6;
  tft.setTextColor(UI_BRAND);
  tft.setCursor((tft.width() - hw) / 2, 126);
  tft.print(hint);

  textDefault(UI_PAD, 146, UI_MUTED, "UP/DOWN vuelve");
}

void uiDrawNetworkDetails(const NetworkInfo &network, bool saved) {
  drawDetailScreen(saved ? "Objetivo OK" : "Detalle AP",
                   network,
                   saved ? "OK  Deauth" : "OK  Guardar",
                   saved ? UI_OK : UI_BRAND);
}

void uiDrawTargetDetails(const NetworkInfo &network) {
  drawDetailScreen("Objetivo", network, "OK  Deauth", UI_OK);
}

void uiDrawAnalyzer(uint8_t band) {
  tft.fillScreen(UI_BG);
  char title[18];
  snprintf(title, sizeof(title), "Analizador %s", band == 5 ? "5G" : "2.4G");
  drawStatusBar(title);

  uint8_t bandCount = wifiScannerCountBand(band);
  int strongestIndex = wifiScannerStrongestIndex(band);
  uint8_t busyChannel = wifiScannerBusiestChannel(band);

  char buf[28];
  snprintf(buf, sizeof(buf), "Total: %u redes", bandCount);
  textDefault(UI_PAD, 22, UI_TEXT, buf);

  snprintf(buf, sizeof(buf), "2.4G %u   5G %u",
           wifiScannerCountBand(2), wifiScannerCountBand(5));
  textDefault(UI_PAD, 36, UI_TEXT_DIM, buf);

  // Card "mas fuerte"
  drawCard(UI_PAD, 50, tft.width() - UI_PAD * 2, 44);
  textTiny(UI_PAD + 4, 54, UI_MUTED, "MAS FUERTE");
  if (strongestIndex >= 0) {
    const NetworkInfo &net = wifiScannerNetwork(strongestIndex);
    useDefault();
    tft.setTextColor(UI_TEXT);
    tft.setCursor(UI_PAD + 4, 66);
    printSsidSafe(net.ssid, 16);
    drawRssiBars(tft.width() - UI_PAD - 24, 66, net.rssi);
    char chBuf[20];
    snprintf(chBuf, sizeof(chBuf), "CH %u   %ld dBm", net.channel, (long)net.rssi);
    textDefault(UI_PAD + 4, 80, UI_INFO, chBuf);
  } else {
    textDefault(UI_PAD + 4, 68, UI_MUTED, "Sin datos");
  }

  // Card "canal mas lleno"
  drawCard(UI_PAD, 100, tft.width() - UI_PAD * 2, 28);
  textTiny(UI_PAD + 4, 104, UI_MUTED, "CANAL MAS LLENO");
  useDefault();
  tft.setTextColor(UI_WARN);
  tft.setCursor(UI_PAD + 4, 116);
  if (busyChannel > 0) {
    tft.print("CH ");
    tft.print(busyChannel);
    tft.print("  (");
    tft.print(wifiScannerCountChannel(busyChannel));
    tft.print(" APs)");
  } else {
    tft.print("N/A");
  }

  textDefault(UI_PAD, 138, UI_MUTED, "UP/DOWN banda  OK vuelve");
}

void uiDrawSystemInfo(bool hasTarget, uint8_t scanCount, uint8_t count24, uint8_t count5) {
  tft.fillScreen(UI_BG);
  drawStatusBar("Sistema");
  statusBarTarget(hasTarget);

  useDefault();
  tft.setTextColor(UI_BRAND);
  tft.setCursor(UI_PAD, 22);
  tft.print(FIRMWARE_NAME);
  tft.print(" ");
  tft.print(FIRMWARE_VERSION);

  textDefault(UI_PAD, 34, UI_TEXT_DIM, FIRMWARE_BOARD);
  textDefault(UI_PAD, 46, UI_MUTED, "ST7735 vertical");

  drawCard(UI_PAD, 60, tft.width() - UI_PAD * 2, 50);
  textTiny(UI_PAD + 4, 64, UI_MUTED, "ULTIMO SCAN");
  char buf[24];
  snprintf(buf, sizeof(buf), "%u redes", scanCount);
  textDefault(UI_PAD + 4, 76, UI_TEXT, buf);
  snprintf(buf, sizeof(buf), "2.4G %u   /   5G %u", count24, count5);
  textDefault(UI_PAD + 4, 90, UI_INFO, buf);

  textDefault(UI_PAD, 118, hasTarget ? UI_OK : UI_WARN,
              hasTarget ? "Objetivo listo" : "Sin objetivo");
  textDefault(UI_PAD, 146, UI_MUTED, "OK vuelve");
}

void uiDrawLabPrecheck(bool hasTarget, const NetworkInfo *network) {
  tft.fillScreen(UI_BG);
  drawStatusBar("Precheck");
  statusBarTarget(hasTarget);

  if (!hasTarget || network == NULL) {
    textDefault(UI_PAD, 40, UI_WARN, "Sin objetivo");
    textDefault(UI_PAD, 60, UI_MUTED, "Selecciona red");
    textDefault(UI_PAD, 75, UI_MUTED, "desde WiFi > Scan");
    textDefault(UI_PAD, 146, UI_MUTED, "OK vuelve");
    return;
  }

  textTiny(UI_PAD, 20, UI_MUTED, "OBJETIVO");
  useHero();
  tft.setTextColor(UI_OK);
  tft.setCursor(UI_PAD, 40);
  printSsidSafe(network->ssid, 14);

  int bx = UI_PAD, by = 50;
  drawChannelTag(bx, by, network->channel); bx += 26;
  drawBandTag(bx, by, network->channel);    bx += 22;
  drawSecBadge(bx, by, network->security);

  drawCard(UI_PAD, 66, tft.width() - UI_PAD * 2, 28);
  textTiny(UI_PAD + 4, 70, UI_MUTED, "RSSI");
  char rbuf[16];
  snprintf(rbuf, sizeof(rbuf), "%ld dBm", (long)network->rssi);
  textDefault(UI_PAD + 4, 82, UI_TEXT, rbuf);
  drawRssiBars(tft.width() - UI_PAD - 24, 82, network->rssi);

  drawCard(UI_PAD, 100, tft.width() - UI_PAD * 2, 24);
  textTiny(UI_PAD + 4, 104, UI_MUTED, "BSSID");
  textDefault(UI_PAD + 4, 114, UI_TEXT, network->bssid);

  textDefault(UI_PAD, 132, UI_BRAND, "Modo diagnostico");
  textDefault(UI_PAD, 146, UI_MUTED, "OK vuelve");
}

void uiDrawTargetMonitor(const NetworkInfo &network, bool found) {
  tft.fillScreen(UI_BG);
  drawStatusBar(found ? "Activo" : "No visto");
  statusBarTarget(true);
  statusBarBusy();

  useHero();
  tft.setTextColor(found ? UI_OK : UI_WARN);
  tft.setCursor(UI_PAD, 30);
  printSsidSafe(network.ssid, 14);

  int bx = UI_PAD, by = 40;
  drawChannelTag(bx, by, network.channel); bx += 26;
  drawBandTag(bx, by, network.channel);

  drawCard(UI_PAD, 56, tft.width() - UI_PAD * 2, 40);
  textTiny(UI_PAD + 4, 60, UI_MUTED, "RSSI");
  char rbuf[16];
  snprintf(rbuf, sizeof(rbuf), "%ld dBm", (long)network.rssi);
  useDefaultBig();
  tft.setTextColor(found ? UI_OK : UI_MUTED);
  tft.setCursor(UI_PAD + 4, 74);
  tft.print(rbuf);
  drawRssiBars(tft.width() - UI_PAD - 24, 78, network.rssi);

  drawCard(UI_PAD, 102, tft.width() - UI_PAD * 2, 24);
  textTiny(UI_PAD + 4, 106, UI_MUTED, "BSSID");
  textDefault(UI_PAD + 4, 116, UI_TEXT, network.bssid);

  textDefault(UI_PAD, 134, UI_INFO, "OK  re-scan");
  textDefault(UI_PAD, 146, UI_MUTED, "UP/DOWN vuelve");
}

void uiDrawLabStats(const LabStats &stats) {
  tft.fillScreen(UI_BG);
  drawStatusBar("Resultados");

  if (!stats.active || stats.samples == 0) {
    textDefault(UI_PAD, 40, UI_WARN, "Sin muestras");
    textDefault(UI_PAD, 60, UI_MUTED, "Usa Re-scan o");
    textDefault(UI_PAD, 75, UI_MUTED, "Prueba principal");
    textDefault(UI_PAD, 146, UI_MUTED, "OK vuelve");
    return;
  }

  drawCard(UI_PAD, 20, tft.width() - UI_PAD * 2, 36);
  textTiny(UI_PAD + 4, 24, UI_MUTED, "MUESTRAS");
  char buf[24];
  snprintf(buf, sizeof(buf), "%u total", stats.samples);
  textDefault(UI_PAD + 4, 34, UI_TEXT, buf);
  snprintf(buf, sizeof(buf), "OK %u   Perd %u", stats.found, stats.missed);
  textDefault(UI_PAD + 4, 46, UI_INFO, buf);

  drawCard(UI_PAD, 62, tft.width() - UI_PAD * 2, 52);
  textTiny(UI_PAD + 4, 66, UI_MUTED, "RSSI MIN / PROM / MAX");
  useDefault();
  tft.setTextColor(UI_DANGER);
  tft.setCursor(UI_PAD + 4, 80);
  tft.print(stats.minRssi);
  tft.setTextColor(UI_BRAND);
  tft.setCursor(UI_PAD + 42, 80);
  tft.print(labStatsAverageRssi());
  tft.setTextColor(UI_OK);
  tft.setCursor(UI_PAD + 80, 80);
  tft.print(stats.maxRssi);

  snprintf(buf, sizeof(buf), "Ult CH %u", stats.lastChannel);
  textDefault(UI_PAD + 4, 96, UI_TEXT_DIM, buf);

  textDefault(UI_PAD, 122, UI_MUTED, stats.bssid);
  textDefault(UI_PAD, 146, UI_MUTED, "OK vuelve");
}

void uiDrawPrincipalTest(const LabTestReport &report) {
  tft.fillScreen(UI_BG);
  drawStatusBar(report.title);
  statusBarTarget(true);

  const char *stateTxt = "IDLE";
  uint16_t stateColor = UI_MUTED;
  if (report.state == LAB_TEST_READY)         { stateTxt = "READY";    stateColor = UI_OK; }
  else if (report.state == LAB_TEST_MEASURED) { stateTxt = "MEDIDO";   stateColor = UI_INFO; }
  else if (report.state == LAB_TEST_SIMULATED){ stateTxt = "SIMULADO"; stateColor = UI_BRAND; }
  else if (report.state == LAB_TEST_BLOCKED)  { stateTxt = "BLOCKED";  stateColor = UI_WARN; }

  // Estado como pill
  useDefault();
  uint16_t sw = strlen(stateTxt) * 6 + 8;
  tft.fillRoundRect(UI_PAD, 20, sw, 12, 3, stateColor);
  tft.setTextColor(UI_BG);
  tft.setCursor(UI_PAD + 4, 23);
  tft.print(stateTxt);

  drawCard(UI_PAD, 38, tft.width() - UI_PAD * 2, 82);
  textDefault(UI_PAD + 4, 44, UI_TEXT, report.line1);
  textDefault(UI_PAD + 4, 64, UI_TEXT_DIM, report.line2);
  textDefault(UI_PAD + 4, 84, UI_TEXT_DIM, report.line3);

  char buf[16];
  snprintf(buf, sizeof(buf), "Intentos: %u", report.attempts);
  textDefault(UI_PAD, 126, UI_BRAND, buf);
  textDefault(UI_PAD, 146, UI_MUTED, "OK vuelve");
}

void uiDrawBleList(int selected, int listTop) {
  tft.fillScreen(UI_BG);
  drawStatusBar("BLE Scan");

  uint8_t count = bleCount();
  char buf[24];
  snprintf(buf, sizeof(buf), "%u", count);
  statusBarRight(buf, UI_BRAND);

  // Volver row arriba (selected == -1)
  bool backSel = (selected < 0);
  int backY = 17;
  if (backSel) {
    tft.fillRoundRect(2, backY - 1, tft.width() - 4, 11, 2, UI_SELECT);
  }
  useDefault();
  tft.setTextColor(backSel ? UI_TEXT : UI_INFO);
  tft.setCursor(UI_PAD, backY + 2);
  tft.print("< Volver al menu");

  // Stats line
  snprintf(buf, sizeof(buf), "%lu pkt  Ap %lu  Ms %lu",
           (unsigned long)bleTotalPackets(),
           (unsigned long)bleAppleCount(),
           (unsigned long)bleMicrosoftCount());
  textTiny(UI_PAD, 30, UI_MUTED, buf);

  tft.drawFastHLine(2, 39, tft.width() - 4, UI_LINE_SOFT);

  if (count == 0) {
    textDefault(UI_PAD, 60, UI_WARN, "Escaneando...");
    textDefault(UI_PAD, 75, UI_MUTED, "Sin dispositivos aun");
    textDefault(UI_PAD, 146, UI_MUTED, "OK vuelve");
    return;
  }

  // Hasta 7 filas a partir de listTop
  int rowY = 43;
  const int rowH = 12;
  const int VISIBLE_ROWS = 7;
  for (int i = 0; i < VISIBLE_ROWS && (listTop + i) < (int)count; i++) {
    int idx = listTop + i;
    const BleDeviceInfo &dev = bleDevice(idx);
    bool isSel = (idx == selected);

    if (isSel) {
      tft.fillRoundRect(2, rowY - 1, tft.width() - 4, rowH - 1, 2, UI_SELECT);
    }

    // Tag de tipo (2 caracteres, color por categoria)
    const char *tag = "?";
    uint16_t tagColor = UI_TEXT_DIM;
    switch (dev.kind) {
      case BLE_KIND_IBEACON:          tag = "iB"; tagColor = UI_OK;       break;
      case BLE_KIND_APPLE_CONTINUITY: tag = "Ap"; tagColor = UI_BRAND;    break;
      case BLE_KIND_MICROSOFT:        tag = "Ms"; tagColor = UI_INFO;     break;
      case BLE_KIND_GOOGLE:           tag = "Go"; tagColor = UI_WARN;     break;
      case BLE_KIND_SAMSUNG:          tag = "Sa"; tagColor = UI_WARN;     break;
      case BLE_KIND_NAMED:            tag = "N ";  tagColor = UI_TEXT_DIM; break;
      default:                        tag = "? ";  tagColor = UI_MUTED;    break;
    }

    useDefault();
    tft.setTextColor(tagColor);
    tft.setCursor(UI_PAD, rowY + 2);
    tft.print(tag);

    // Nombre o ultimos 3 octetos de la MAC
    tft.setTextColor(isSel ? UI_TEXT : UI_TEXT_DIM);
    tft.setCursor(UI_PAD + 16, rowY + 2);
    if (dev.name[0] != '\0') {
      char nameBuf[12];
      strncpy(nameBuf, dev.name, 11);
      nameBuf[11] = '\0';
      tft.print(nameBuf);
    } else {
      tft.print(dev.addr + 9);   // "DD:EE:FF"
    }

    // RSSI a la derecha
    char rssiBuf[8];
    snprintf(rssiBuf, sizeof(rssiBuf), "%d", (int)dev.rssi);
    uint16_t rssiW = strlen(rssiBuf) * 6;
    tft.setTextColor(isSel ? UI_TEXT : UI_INFO);
    tft.setCursor(tft.width() - UI_PAD - rssiW, rowY + 2);
    tft.print(rssiBuf);

    rowY += rowH;
  }

  textDefault(UI_PAD, 146, UI_MUTED, "OK detalle / Volver");
}

void uiDrawBleDetails(const BleDeviceInfo &dev) {
  tft.fillScreen(UI_BG);
  drawStatusBar("BLE Detalle");

  // Nombre prominente o "<sin nombre>"
  useHero();
  tft.setTextColor(UI_OK);
  tft.setCursor(UI_PAD, 30);
  if (dev.name[0] != '\0') {
    char nameBuf[13];
    strncpy(nameBuf, dev.name, 12);
    nameBuf[12] = '\0';
    tft.print(nameBuf);
  } else {
    tft.print("<sin nombre>");
  }

  // Pill con kind
  const char *kindLabel = bleKindLabel(dev.kind);
  useTiny();
  uint16_t kw = getStrWidth(kindLabel) + 8;
  tft.fillRoundRect(UI_PAD, 38, kw, 10, 2, UI_BRAND);
  tft.setTextColor(UI_BG);
  tft.setCursor(UI_PAD + 4, 45);
  tft.print(kindLabel);

  // Card direccion
  drawCard(UI_PAD, 52, tft.width() - UI_PAD * 2, 22);
  textTiny(UI_PAD + 4, 56, UI_MUTED, "ADDR");
  textDefault(UI_PAD + 4, 64, UI_TEXT, dev.addr);

  // Card RSSI / seen count
  drawCard(UI_PAD, 78, tft.width() - UI_PAD * 2, 24);
  textTiny(UI_PAD + 4, 82, UI_MUTED, "RSSI");
  char rbuf[16];
  snprintf(rbuf, sizeof(rbuf), "%d dBm", (int)dev.rssi);
  textDefault(UI_PAD + 4, 92, UI_TEXT, rbuf);
  char seenBuf[10];
  snprintf(seenBuf, sizeof(seenBuf), "x%u", dev.seenCount);
  uint16_t sw = strlen(seenBuf) * 6;
  useDefault();
  tft.setTextColor(UI_TEXT_DIM);
  tft.setCursor(tft.width() - UI_PAD - sw - 4, 92);
  tft.print(seenBuf);

  // Card fabricante / subtipo Apple
  drawCard(UI_PAD, 106, tft.width() - UI_PAD * 2, 32);
  textTiny(UI_PAD + 4, 110, UI_MUTED, "MANUFACTURER");

  const char *mfgName = bleManufacturerName(dev.manufacturer);
  char mfgBuf[28];
  if (mfgName != NULL) {
    snprintf(mfgBuf, sizeof(mfgBuf), "%s (%04X)", mfgName, dev.manufacturer);
  } else if (dev.manufacturer != 0) {
    snprintf(mfgBuf, sizeof(mfgBuf), "%04X", dev.manufacturer);
  } else {
    strcpy(mfgBuf, "(ninguno)");
  }
  textDefault(UI_PAD + 4, 120, UI_TEXT, mfgBuf);

  if (dev.manufacturer == 0x004C && dev.appleSubtype != 0) {
    char subBuf[26];
    snprintf(subBuf, sizeof(subBuf), "%s %02X",
             bleAppleSubtypeLabel(dev.appleSubtype), dev.appleSubtype);
    textDefault(UI_PAD + 4, 130, UI_INFO, subBuf);
  }

  textDefault(UI_PAD, 146, UI_MUTED, "UP/DOWN/OK vuelve");
}


void uiDrawSniffer(const SnifferStats &stats) {
  tft.fillScreen(UI_BG);
  drawStatusBar(stats.band == 5 ? "Sniffer 5G" : "Sniffer 2.4G");
  statusBarRight(stats.active ? "RUN" : "STOP", stats.active ? UI_OK : UI_MUTED);

  char buf[28];
  snprintf(buf, sizeof(buf), "CH %u", stats.currentChannel);
  textDefault(UI_PAD, 22, UI_TEXT, buf);

  // Card totales
  drawCard(UI_PAD, 36, tft.width() - UI_PAD * 2, 42);
  textTiny(UI_PAD + 4, 40, UI_MUTED, "TOTALES");
  snprintf(buf, sizeof(buf), "%lu frames", (unsigned long)stats.totalFrames);
  textDefault(UI_PAD + 4, 52, UI_TEXT, buf);
  snprintf(buf, sizeof(buf), "M %lu C %lu D %lu",
           (unsigned long)stats.mgmtFrames,
           (unsigned long)stats.ctrlFrames,
           (unsigned long)stats.dataFrames);
  textDefault(UI_PAD + 4, 65, UI_INFO, buf);

  // Card subtipos
  drawCard(UI_PAD, 82, tft.width() - UI_PAD * 2, 56);
  textTiny(UI_PAD + 4, 86, UI_MUTED, "MGMT SUBTIPOS");
  snprintf(buf, sizeof(buf), "Beacon %lu", (unsigned long)stats.beacons);
  textDefault(UI_PAD + 4, 96, UI_TEXT_DIM, buf);
  snprintf(buf, sizeof(buf), "ProbeR %lu", (unsigned long)stats.probeReqs);
  textDefault(UI_PAD + 4, 108, UI_TEXT_DIM, buf);
  snprintf(buf, sizeof(buf), "Deauth %lu", (unsigned long)stats.deauths);
  textDefault(UI_PAD + 4, 120, stats.deauths > 0 ? UI_DANGER : UI_TEXT_DIM, buf);

  textDefault(UI_PAD, 148, UI_MUTED, "OK sale");
}
// ===========================================================================
// Refresh parcial: redibuja solo zonas dinamicas (sin fillScreen).
// Evita parpadeo en pantallas que se actualizan periodicamente.
// ===========================================================================

void uiRefreshBleList(int selected, int listTop) {
  uint8_t count = bleCount();
  char buf[28];

  // Contador en la status bar (derecha)
  tft.fillRect(tft.width() - 28, 2, 26, 11, UI_PANEL);
  snprintf(buf, sizeof(buf), "%u", count);
  statusBarRight(buf, UI_BRAND);

  // Linea de stats a y=30 (justo arriba del separador en y=39)
  tft.fillRect(UI_PAD, 28, tft.width() - UI_PAD * 2, 10, UI_BG);
  snprintf(buf, sizeof(buf), "%lu pkt  Ap %lu  Ms %lu",
           (unsigned long)bleTotalPackets(),
           (unsigned long)bleAppleCount(),
           (unsigned long)bleMicrosoftCount());
  textTiny(UI_PAD, 30, UI_MUTED, buf);

  // Si no hay dispositivos aun, limpiar el area y dejar el mensaje
  if (count == 0) {
    tft.fillRect(2, 42, tft.width() - 4, 90, UI_BG);
    textDefault(UI_PAD, 60, UI_WARN, "Escaneando...");
    textDefault(UI_PAD, 75, UI_MUTED, "Sin dispositivos aun");
    return;
  }

  // Filas de dispositivos: limpiar cada una y redibujarla.
  // Esto evita el parpadeo del fillScreen.
  int rowY = 43;
  const int rowH = 12;
  const int VISIBLE_ROWS = 7;
  for (int i = 0; i < VISIBLE_ROWS; i++) {
    int idx = listTop + i;

    // Limpiar el strip de esta fila
    tft.fillRect(2, rowY - 1, tft.width() - 4, rowH - 1, UI_BG);

    if (idx >= (int)count) {
      rowY += rowH;
      continue;
    }

    const BleDeviceInfo &dev = bleDevice(idx);
    bool isSel = (idx == selected);

    if (isSel) {
      tft.fillRoundRect(2, rowY - 1, tft.width() - 4, rowH - 1, 2, UI_SELECT);
    }

    // Tag de tipo
    const char *tag = "?";
    uint16_t tagColor = UI_TEXT_DIM;
    switch (dev.kind) {
      case BLE_KIND_IBEACON:          tag = "iB"; tagColor = UI_OK;       break;
      case BLE_KIND_APPLE_CONTINUITY: tag = "Ap"; tagColor = UI_BRAND;    break;
      case BLE_KIND_MICROSOFT:        tag = "Ms"; tagColor = UI_INFO;     break;
      case BLE_KIND_GOOGLE:           tag = "Go"; tagColor = UI_WARN;     break;
      case BLE_KIND_SAMSUNG:          tag = "Sa"; tagColor = UI_WARN;     break;
      case BLE_KIND_NAMED:            tag = "N ";  tagColor = UI_TEXT_DIM; break;
      default:                        tag = "? ";  tagColor = UI_MUTED;    break;
    }

    useDefault();
    tft.setTextColor(tagColor);
    tft.setCursor(UI_PAD, rowY + 2);
    tft.print(tag);

    // Nombre o ultimos 3 octetos
    tft.setTextColor(isSel ? UI_TEXT : UI_TEXT_DIM);
    tft.setCursor(UI_PAD + 16, rowY + 2);
    if (dev.name[0] != '\0') {
      char nameBuf[12];
      strncpy(nameBuf, dev.name, 11);
      nameBuf[11] = '\0';
      tft.print(nameBuf);
    } else {
      tft.print(dev.addr + 9);
    }

    // RSSI
    char rssiBuf[8];
    snprintf(rssiBuf, sizeof(rssiBuf), "%d", (int)dev.rssi);
    uint16_t rssiW = strlen(rssiBuf) * 6;
    tft.setTextColor(isSel ? UI_TEXT : UI_INFO);
    tft.setCursor(tft.width() - UI_PAD - rssiW, rowY + 2);
    tft.print(rssiBuf);

    rowY += rowH;
  }
}

void uiRefreshSniffer(const SnifferStats &stats) {
  char buf[28];

  // Status bar derecha: RUN / STOP
  tft.fillRect(tft.width() - 30, 2, 28, 11, UI_PANEL);
  statusBarRight(stats.active ? "RUN" : "STOP", stats.active ? UI_OK : UI_MUTED);

  // Linea de canal a y=22
  tft.fillRect(UI_PAD, 20, tft.width() - UI_PAD * 2, 12, UI_BG);
  snprintf(buf, sizeof(buf), "CH %u", stats.currentChannel);
  textDefault(UI_PAD, 22, UI_TEXT, buf);

  // Contenido dinamico dentro del card TOTALES (y=36..78).
  // Limpio solo el area de los numeros (no la etiqueta TOTALES en y=40).
  tft.fillRect(UI_PAD + 4, 50, tft.width() - UI_PAD * 2 - 8, 24, UI_PANEL);
  snprintf(buf, sizeof(buf), "%lu frames", (unsigned long)stats.totalFrames);
  textDefault(UI_PAD + 4, 52, UI_TEXT, buf);
  snprintf(buf, sizeof(buf), "M %lu C %lu D %lu",
           (unsigned long)stats.mgmtFrames,
           (unsigned long)stats.ctrlFrames,
           (unsigned long)stats.dataFrames);
  textDefault(UI_PAD + 4, 65, UI_INFO, buf);

  // Contenido dinamico dentro del card MGMT SUBTIPOS (y=82..138).
  // Limpio bajo la etiqueta (en y=86).
  tft.fillRect(UI_PAD + 4, 94, tft.width() - UI_PAD * 2 - 8, 38, UI_PANEL);
  snprintf(buf, sizeof(buf), "Beacon %lu", (unsigned long)stats.beacons);
  textDefault(UI_PAD + 4, 96, UI_TEXT_DIM, buf);
  snprintf(buf, sizeof(buf), "ProbeR %lu", (unsigned long)stats.probeReqs);
  textDefault(UI_PAD + 4, 108, UI_TEXT_DIM, buf);
  snprintf(buf, sizeof(buf), "Deauth %lu", (unsigned long)stats.deauths);
  textDefault(UI_PAD + 4, 120, stats.deauths > 0 ? UI_DANGER : UI_TEXT_DIM, buf);
}

// ===========================================================================
// Analizador BLE: sparkline de actividad + stats + estado vs baseline
// ===========================================================================

static const int ANAL_SPARK_X = 4;
static const int ANAL_SPARK_Y = 30;
static const int ANAL_SPARK_W = 120;
static const int ANAL_SPARK_H = 56;

static uint16_t analStatusColor(BleAnalyzerStatus s) {
  switch (s) {
    case BLE_STATUS_NORMAL: return UI_OK;
    case BLE_STATUS_LOW:    return UI_DANGER;
    case BLE_STATUS_HIGH:   return UI_WARN;
    default:                return UI_MUTED;
  }
}

static void drawBleScope(int x, int y, int w, int h, uint16_t baselinePps) {
  // Marco
  tft.drawRect(x, y, w, h, UI_LINE_SOFT);

  const uint8_t *hist = blePpsHistory();
  uint8_t head = blePpsHistoryHead();

  // Encuentra el maximo por bin para escalar el eje Y (piso para que no se vea plano)
  uint8_t maxBin = 3;
  for (uint8_t i = 0; i < BLE_HISTORY_SIZE; i++) {
    if (hist[i] > maxBin) maxBin = hist[i];
  }
  // El baseline esta en pps; dividir entre 10 da el equivalente por bin
  uint8_t baseBin = (uint8_t)(baselinePps / 10);
  if (baseBin > maxBin) maxBin = baseBin;

  int innerW = w - 2;
  int innerH = h - 2;
  int x0Inner = x + 1;
  int yBottom = y + h - 1;

  // Linea baseline punteada al fondo (debajo de la curva)
  if (baseBin >= 1) {
    int baseY = yBottom - ((int)baseBin * innerH) / maxBin;
    if (baseY > y + 1 && baseY < yBottom) {
      for (int px = x + 2; px < x + w - 2; px += 3) {
        tft.drawPixel(px, baseY, UI_INFO);
      }
    }
  }

  // Color global segun status (verde normal, rojo bajo, ambar alto)
  uint16_t lineColor = UI_OK;
  if (baselinePps >= 4) {
    uint16_t cur = blePps();
    if ((uint32_t)cur * 3 < (uint32_t)baselinePps)            lineColor = UI_DANGER;
    else if ((uint32_t)cur > (uint32_t)baselinePps * 2 + 8)   lineColor = UI_WARN;
  }
  // Relleno tenue debajo de la onda
  uint16_t fillColor = UI_PANEL;

  // Mapear cada bin a una X y conectar con drawLine -> osciloscopio
  int prevX = x0Inner;
  uint8_t v0 = hist[head];
  int prevY = yBottom - ((int)v0 * innerH) / maxBin;

  for (uint8_t i = 1; i < BLE_HISTORY_SIZE; i++) {
    uint8_t idx = (head + i) % BLE_HISTORY_SIZE;
    uint8_t v = hist[idx];

    int curX = x0Inner + ((int)i * (innerW - 1)) / (BLE_HISTORY_SIZE - 1);
    int curY = yBottom - ((int)v * innerH) / maxBin;

    // Relleno: cada x entre prevX y curX interpola Y y pinta hasta el fondo
    int dx = curX - prevX;
    if (dx > 0) {
      for (int xp = prevX + 1; xp <= curX; xp++) {
        int yp = prevY + ((curY - prevY) * (xp - prevX)) / dx;
        int fillH = yBottom - yp - 1;
        if (fillH > 0) {
          tft.drawFastVLine(xp, yp + 1, fillH, fillColor);
        }
      }
    }

    // Linea principal (2 px de grosor para que se vea brillante)
    tft.drawLine(prevX, prevY, curX, curY, lineColor);
    if (curY < yBottom - 1) {
      tft.drawLine(prevX, prevY + 1, curX, curY + 1, lineColor);
    }

    prevX = curX;
    prevY = curY;
  }
}

void uiDrawBleAnalyzer() {
  tft.fillScreen(UI_BG);
  drawStatusBar("Analizador BLE");

  // Osciloscopio
  drawBleScope(ANAL_SPARK_X, ANAL_SPARK_Y, ANAL_SPARK_W, ANAL_SPARK_H, bleBaseline());

  // Card con datos numericos (y=92 a y=138)
  drawCard(UI_PAD, 92, tft.width() - UI_PAD * 2, 46);
  textTiny(UI_PAD + 4, 96, UI_MUTED, "ACTIVIDAD");

  // Footer
  textDefault(UI_PAD, 148, UI_MUTED, "OK vuelve  UP reset");

  // Llenar zonas dinamicas
  uiRefreshBleAnalyzer();
}

void uiRefreshBleAnalyzer() {
  char buf[28];
  BleAnalyzerStatus status = bleAnalyzerStatus();

  // Status pill en la status bar (derecha)
  const char *statusTxt = bleAnalyzerStatusLabel(status);
  uint16_t statusColor = analStatusColor(status);
  tft.fillRect(tft.width() - 58, 2, 56, 11, UI_PANEL);
  useDefault();
  uint16_t sw = strlen(statusTxt) * 6;
  tft.setTextColor(statusColor);
  tft.setCursor(tft.width() - sw - 4, 4);
  tft.print(statusTxt);

  // Linea de stats sobre el scope (y=18)
  uint16_t pps = blePps();
  uint16_t base = bleBaseline();
  tft.fillRect(UI_PAD, 16, tft.width() - UI_PAD * 2, 12, UI_BG);
  snprintf(buf, sizeof(buf), "now %u/s  base %u/s", pps, base);
  textTiny(UI_PAD, 20, UI_TEXT_DIM, buf);

  // Osciloscopio: limpia el interior y repinta
  tft.fillRect(ANAL_SPARK_X + 1, ANAL_SPARK_Y + 1,
               ANAL_SPARK_W - 2, ANAL_SPARK_H - 2, UI_BG);
  drawBleScope(ANAL_SPARK_X, ANAL_SPARK_Y, ANAL_SPARK_W, ANAL_SPARK_H, base);

  // Datos dentro del card
  tft.fillRect(UI_PAD + 4, 104, tft.width() - UI_PAD * 2 - 8, 32, UI_PANEL);

  snprintf(buf, sizeof(buf), "Rate    %u/s", pps);
  textDefault(UI_PAD + 4, 106, UI_TEXT, buf);

  int8_t rssi = bleAvgRssi();
  if (rssi == 0) {
    strcpy(buf, "RSSI    --");
  } else {
    snprintf(buf, sizeof(buf), "RSSI    %d dBm", (int)rssi);
  }
  textDefault(UI_PAD + 4, 118, UI_INFO, buf);

  snprintf(buf, sizeof(buf), "Devs %u  Pkt %lu",
           bleCount(), (unsigned long)bleTotalPackets());
  textDefault(UI_PAD + 4, 130, UI_TEXT_DIM, buf);
}

// ===========================================================================
// Analizador WiFi: scope de actividad + barras por canal de la banda activa
// ===========================================================================

static const int WIFI_SCOPE_X = 4;
static const int WIFI_SCOPE_Y = 28;
static const int WIFI_SCOPE_W = 120;
static const int WIFI_SCOPE_H = 50;

static const int WIFI_BARS_X = 4;
static const int WIFI_BARS_Y = 82;
static const int WIFI_BARS_W = 120;
static const int WIFI_BARS_H = 28;

static uint16_t wifiStatusColor(WifiAnalStatus s) {
  switch (s) {
    case WIFI_ANAL_NORMAL: return UI_OK;
    case WIFI_ANAL_LOW:    return UI_DANGER;
    case WIFI_ANAL_HIGH:   return UI_WARN;
    default:               return UI_MUTED;
  }
}

static void drawWifiScope(int x, int y, int w, int h, uint16_t basePps) {
  tft.drawRect(x, y, w, h, UI_LINE_SOFT);

  const uint8_t *hist = wifiAnalyzerHistory();
  uint8_t head = wifiAnalyzerHistoryHead();

  uint8_t maxBin = 3;
  for (uint8_t i = 0; i < WIFI_ANAL_HIST; i++) {
    if (hist[i] > maxBin) maxBin = hist[i];
  }
  uint8_t baseBin = (uint8_t)(basePps / 10);
  if (baseBin > maxBin) maxBin = baseBin;

  int innerW = w - 2;
  int innerH = h - 2;
  int x0 = x + 1;
  int yBottom = y + h - 1;

  // Baseline dotted
  if (baseBin >= 1) {
    int baseY = yBottom - ((int)baseBin * innerH) / maxBin;
    if (baseY > y + 1 && baseY < yBottom) {
      for (int px = x + 2; px < x + w - 2; px += 3) {
        tft.drawPixel(px, baseY, UI_INFO);
      }
    }
  }

  // Color global segun status
  uint16_t lineColor = UI_OK;
  if (basePps >= 6) {
    uint16_t cur = wifiAnalyzerPps();
    if ((uint32_t)cur * 3 < (uint32_t)basePps)            lineColor = UI_DANGER;
    else if ((uint32_t)cur > (uint32_t)basePps * 2 + 12)  lineColor = UI_WARN;
  }

  uint16_t fillColor = UI_PANEL;

  int prevX = x0;
  uint8_t v0 = hist[head];
  int prevY = yBottom - ((int)v0 * innerH) / maxBin;

  for (uint8_t i = 1; i < WIFI_ANAL_HIST; i++) {
    uint8_t idx = (head + i) % WIFI_ANAL_HIST;
    uint8_t v = hist[idx];

    int curX = x0 + ((int)i * (innerW - 1)) / (WIFI_ANAL_HIST - 1);
    int curY = yBottom - ((int)v * innerH) / maxBin;

    int dx = curX - prevX;
    if (dx > 0) {
      for (int xp = prevX + 1; xp <= curX; xp++) {
        int yp = prevY + ((curY - prevY) * (xp - prevX)) / dx;
        int fillH = yBottom - yp - 1;
        if (fillH > 0) {
          tft.drawFastVLine(xp, yp + 1, fillH, fillColor);
        }
      }
    }

    tft.drawLine(prevX, prevY, curX, curY, lineColor);
    if (curY < yBottom - 1) {
      tft.drawLine(prevX, prevY + 1, curX, curY + 1, lineColor);
    }

    prevX = curX;
    prevY = curY;
  }
}

static void drawWifiChannelBars(int x, int y, int w, int h) {
  tft.drawRect(x, y, w, h, UI_LINE_SOFT);

  uint8_t count = wifiBandChannelCount();
  if (count == 0) return;

  // Max para escalar
  uint32_t maxFrames = 1;
  for (uint8_t i = 0; i < count; i++) {
    uint32_t v = wifiBandChannelFrames(i);
    if (v > maxFrames) maxFrames = v;
  }

  int innerW = w - 2;
  int innerH = h - 2;
  int barAreaW = innerW / count;
  if (barAreaW < 2) barAreaW = 2;
  int gap = 1;
  int barW = barAreaW - gap;
  if (barW < 1) barW = 1;

  int busiest = wifiBandBusiestChannelIdx();
  uint8_t currentCh = sniffGetStats().currentChannel;

  for (uint8_t i = 0; i < count; i++) {
    uint32_t v = wifiBandChannelFrames(i);
    int barH = (int)((v * (uint32_t)innerH) / maxFrames);
    if (barH < 1 && v > 0) barH = 1;

    int barX = x + 1 + i * barAreaW;
    int barY = y + h - 1 - barH;

    uint16_t color = UI_INFO;
    if ((int)i == busiest && v > 0) color = UI_BRAND;
    if (barH > 0) {
      tft.fillRect(barX, barY, barW, barH, color);
    }

    // Marcador del canal actualmente sintonizado
    if (wifiBandChannelNumber(i) == currentCh) {
      tft.drawFastHLine(barX, y + 1, barW, UI_WARN);
    }
  }
}

void uiDrawWifiAnalyzer() {
  tft.fillScreen(UI_BG);
  uint8_t band = wifiAnalyzerBand();
  drawStatusBar(band == 5 ? "Trafico 5G" : "Trafico 2.4G");

  // Marcos del scope y las barras (los rellenos se hacen en refresh)
  drawWifiScope(WIFI_SCOPE_X, WIFI_SCOPE_Y, WIFI_SCOPE_W, WIFI_SCOPE_H, wifiAnalyzerBaseline());
  drawWifiChannelBars(WIFI_BARS_X, WIFI_BARS_Y, WIFI_BARS_W, WIFI_BARS_H);

  textTiny(UI_PAD, 116, UI_MUTED, "TOP CH / ACTUAL");

  // Footer
  textDefault(UI_PAD, 148, UI_MUTED, "OK vuelve  UP reset");

  uiRefreshWifiAnalyzer();
}

void uiRefreshWifiAnalyzer() {
  char buf[32];
  WifiAnalStatus status = wifiAnalyzerStatus();

  // Status pill: solo dibujamos algo si NO esta IDLE (calibrando se encimaba con el titulo)
  tft.fillRect(tft.width() - 58, 2, 56, 11, UI_PANEL);
  if (status != WIFI_ANAL_IDLE) {
    const char *statusTxt = wifiAnalStatusLabel(status);
    uint16_t statusColor = wifiStatusColor(status);
    useDefault();
    uint16_t sw = strlen(statusTxt) * 6;
    tft.setTextColor(statusColor);
    tft.setCursor(tft.width() - sw - 4, 4);
    tft.print(statusTxt);
  }

  // Linea de stats
  uint16_t fps = wifiAnalyzerPps();
  uint16_t base = wifiAnalyzerBaseline();
  uint8_t ch = sniffGetStats().currentChannel;
  tft.fillRect(UI_PAD, 16, tft.width() - UI_PAD * 2, 10, UI_BG);
  snprintf(buf, sizeof(buf), "CH%u  now %u  base %u", ch, fps, base);
  textTiny(UI_PAD, 20, UI_TEXT_DIM, buf);

  // Scope: limpia interior y repinta
  tft.fillRect(WIFI_SCOPE_X + 1, WIFI_SCOPE_Y + 1,
               WIFI_SCOPE_W - 2, WIFI_SCOPE_H - 2, UI_BG);
  drawWifiScope(WIFI_SCOPE_X, WIFI_SCOPE_Y, WIFI_SCOPE_W, WIFI_SCOPE_H, base);

  // Channel bars: limpia interior y repinta
  tft.fillRect(WIFI_BARS_X + 1, WIFI_BARS_Y + 1,
               WIFI_BARS_W - 2, WIFI_BARS_H - 2, UI_BG);
  drawWifiChannelBars(WIFI_BARS_X, WIFI_BARS_Y, WIFI_BARS_W, WIFI_BARS_H);

  // Linea inferior con top channel y total
  tft.fillRect(UI_PAD, 124, tft.width() - UI_PAD * 2, 12, UI_BG);
  int topIdx = wifiBandBusiestChannelIdx();
  if (topIdx >= 0) {
    uint8_t topCh = wifiBandChannelNumber((uint8_t)topIdx);
    uint32_t topFrames = wifiBandChannelFrames((uint8_t)topIdx);
    snprintf(buf, sizeof(buf), "Top CH%u  %lu fr",
             topCh, (unsigned long)topFrames);
    textDefault(UI_PAD, 126, UI_BRAND, buf);
  } else {
    textDefault(UI_PAD, 126, UI_MUTED, "Sin datos aun");
  }
}

// ===========================================================================
// Beacon spam: contador TX + canal + SSID actual (lab framing)
// ===========================================================================

void uiDrawBeaconSpam() {
  tft.fillScreen(UI_BG);
  const BeaconSpamStats &s = beaconSpamGetStats();
  drawStatusBar(s.band == 5 ? "Beacon 5G" : "Beacon 2.4G");

  // Estatus en la derecha (activo / detenido)
  statusBarRight(s.active ? "TX" : "OFF", s.active ? UI_DANGER : UI_MUTED);

  // Etiqueta de contador
  textTiny(UI_PAD, 22, UI_MUTED, "PAQUETES ENVIADOS");

  // Card grande con el contador (se llena en refresh)
  drawCard(UI_PAD, 32, tft.width() - UI_PAD * 2, 38);

  // Card con canal + SSID actual
  drawCard(UI_PAD, 76, tft.width() - UI_PAD * 2, 60);
  textTiny(UI_PAD + 4, 80, UI_MUTED, "CANAL / SSID ACTUAL");

  // Aviso lab framing
  textDefault(UI_PAD, 148, UI_MUTED, "OK detiene  Solo lab");

  uiRefreshBeaconSpam();
}

void uiRefreshBeaconSpam() {
  const BeaconSpamStats &s = beaconSpamGetStats();
  char buf[40];

  // Contador grande dentro del card 1 (limpiar y reescribir)
  tft.fillRect(UI_PAD + 4, 38, tft.width() - UI_PAD * 2 - 8, 28, UI_PANEL);
  useDefaultBig();
  tft.setTextColor(UI_OK);
  tft.setCursor(UI_PAD + 8, 44);
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)s.totalTx);
  tft.print(buf);

  // Pulso animado al lado del contador para indicar TX activo
  if (s.active) {
    uint8_t pulse = (millis() / 120) & 1;
    tft.fillCircle(tft.width() - UI_PAD - 14, 52, 4, pulse ? UI_DANGER : UI_PANEL);
  }

  // Card 2: canal y SSID
  tft.fillRect(UI_PAD + 4, 90, tft.width() - UI_PAD * 2 - 8, 44, UI_PANEL);

  useDefault();
  tft.setTextColor(UI_TEXT);
  tft.setCursor(UI_PAD + 4, 92);
  snprintf(buf, sizeof(buf), "CH %u  /  %u de %u",
           s.currentChannel,
           s.currentSsidIdx + 1,
           beaconSpamSsidCount());
  tft.print(buf);

  // SSID actual truncado (puede tener emojis multibyte)
  const char *cur = beaconSpamCurrentSsid();
  // Truncado por bytes simple; los emojis pueden quedar cortados pero no es critico
  tft.setTextColor(UI_BRAND);
  tft.setCursor(UI_PAD + 4, 108);
  uint8_t shown = 0;
  while (cur[shown] && shown < 20) shown++;
  for (uint8_t i = 0; i < shown; i++) {
    tft.write(cur[i]);
  }

  // Segunda linea si el SSID es largo
  if (cur[shown] != '\0') {
    tft.setCursor(UI_PAD + 4, 120);
    uint8_t shown2 = 0;
    while (cur[shown + shown2] && shown2 < 20) shown2++;
    for (uint8_t i = 0; i < shown2; i++) {
      tft.write(cur[shown + i]);
    }
  }
}

// ===========================================================================
// BLE spam: advertiser con nombres rotativos
// ===========================================================================

void uiDrawBleSpam() {
  tft.fillScreen(UI_BG);
  const BleSpamStats &s = bleSpamGetStats();
  drawStatusBar("BLE spam");

  statusBarRight(s.active ? "ADV" : "OFF", s.active ? UI_INFO : UI_MUTED);

  // Etiqueta de contador
  textTiny(UI_PAD, 22, UI_MUTED, "CAMBIOS DE IDENTIDAD");

  // Card grande con contador
  drawCard(UI_PAD, 32, tft.width() - UI_PAD * 2, 38);

  // Card con device actual
  drawCard(UI_PAD, 76, tft.width() - UI_PAD * 2, 60);
  textTiny(UI_PAD + 4, 80, UI_MUTED, "DEVICE ANUNCIADO");

  textDefault(UI_PAD, 148, UI_MUTED, "OK detiene  Solo lab");

  uiRefreshBleSpam();
}

void uiRefreshBleSpam() {
  const BleSpamStats &s = bleSpamGetStats();
  char buf[40];

  // Contador grande (limpiar y reescribir)
  tft.fillRect(UI_PAD + 4, 38, tft.width() - UI_PAD * 2 - 8, 28, UI_PANEL);
  useDefaultBig();
  tft.setTextColor(UI_INFO);
  tft.setCursor(UI_PAD + 8, 44);
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)s.totalTx);
  tft.print(buf);

  // Pulso animado
  if (s.active) {
    uint8_t pulse = (millis() / 150) & 1;
    tft.fillCircle(tft.width() - UI_PAD - 14, 52, 4, pulse ? UI_BRAND : UI_PANEL);
  }

  // Card 2: index + nombre
  tft.fillRect(UI_PAD + 4, 90, tft.width() - UI_PAD * 2 - 8, 44, UI_PANEL);

  useDefault();
  tft.setTextColor(UI_TEXT);
  tft.setCursor(UI_PAD + 4, 92);
  snprintf(buf, sizeof(buf), "%u de %u",
           s.currentIdx + 1,
           bleSpamCount());
  tft.print(buf);

  // Nombre actual truncado (emojis multibyte se cortan en byte boundary)
  const char *cur = bleSpamCurrent();
  tft.setTextColor(UI_BRAND);
  tft.setCursor(UI_PAD + 4, 108);
  uint8_t shown = 0;
  while (cur[shown] && shown < 20) shown++;
  for (uint8_t i = 0; i < shown; i++) {
    tft.write(cur[i]);
  }

  // Segunda linea si el nombre es largo
  if (cur[shown] != '\0') {
    tft.setCursor(UI_PAD + 4, 120);
    uint8_t shown2 = 0;
    while (cur[shown + shown2] && shown2 < 20) shown2++;
    for (uint8_t i = 0; i < shown2; i++) {
      tft.write(cur[shown + i]);
    }
  }
}
