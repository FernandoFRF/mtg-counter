/*
 * Magic: The Gathering — Life / Storm / Mana Counter  (v2)
 * Hardware: ESP32 CYD — tela touch 240x320
 *
 * Para trocar a imagem, utilize um PNG > RGB565, como esse https://longfangsong.github.io/en/image-to-rgb565/
 */

#include <WiFi.h>
#include "esp_bt.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>
#include <FS.h>
#include <LittleFS.h>

// ─── PINAGEM CYD ────────────────────────────────────────────────────────────
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33
#define TFT_BL       21

SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

// ─── DISPLAY ─────────────────────────────────────────────────────────────────
#define SCR_W   240
#define SCR_H   320
#define MID_X   120

// ─── ZONAS Y ─────────────────────────────────────────────────────────────────
#define Z2_Y     90   // fim da vida / início swamp+storm
#define Z3_Y    225   // fim dos contadores / início manas
#define Z4_Y    295   // fim das manas / início botões

// ─── BACKGROUND ──────────────────────────────────────────────────────────────
#define BG_FILE       "/bg.bin"
#define BG_LINE_BYTES (SCR_W * 2)

// ─── PARÂMETROS DE INTERAÇÃO ─────────────────────────────────────────────────
#define LONG_PRESS_MS       1000
#define SWAMP_LONG_PRESS_MS  300
#define TAP_MAX_MS           600
#define NO_MIN             -9999  // sentinela: sem limite inferior

// ─── CORES ───────────────────────────────────────────────────────────────────
#define COL_PROGRESS 0x5ADF
#define COL_OUTLINE  0x0000

// ═══════════════════════════════════════════════════════════════════════════════
// COUNTERS — um struct por elemento de UI numérico
// ═══════════════════════════════════════════════════════════════════════════════

struct Counter {
  int*           value;
  int            minValue;         // NO_MIN = sem piso
  int            resetValue;       // valor no resetAll()
  bool           resetOnEndOfTurn; // zerar no endOfTurn()
  int            cx, cy;           // centro do número na tela
  int            patchW, patchH;   // área de restauração do background
  uint16_t       color;
  const GFXfont* font;             // nullptr → setTextFont(2)
  int            textYOffset;      // deslocamento vertical do drawString
};

int vCycler, vOpponent;
int cSwamp, cStorm;
int mW, mU, mR, mG;

Counter counters[] = {
//  value         min    reset   eot    cx    cy   pW   pH    color       font                  yOff
  { &vCycler,  NO_MIN,   20,  false,   65,  55,  100, 70,  TFT_WHITE, &FreeSans18pt7b,           0 },
  { &vOpponent,NO_MIN,   20,  false,  175,  55,  100, 70,  TFT_WHITE, &FreeSans18pt7b,           0 },
  { &cSwamp,      0,      0,   true,   65, 150,   70, 70,  TFT_WHITE, &FreeSansBold24pt7b,       5 },
  { &cStorm,      0,      0,   true,  175, 150,   70, 70,  TFT_WHITE, &FreeSansBold24pt7b,       5 },
  { &mW,          0,      0,   true,   40, 275,   40, 30,  0xFFFE,    nullptr,                   0 },
  { &mU,          0,      0,   true,   95, 275,   40, 30,  0x5DFF,    nullptr,                   0 },
  { &mR,          0,      0,   true,  145, 275,   40, 30,  0xFB8D,    nullptr,                   0 },
  { &mG,          0,      0,   true,  200, 275,   40, 30,  0x77EC,    nullptr,                   0 },
};
const int NUM_COUNTERS = sizeof(counters) / sizeof(counters[0]);

// ═══════════════════════════════════════════════════════════════════════════════
// TOUCH REGIONS — mapeamento declarativo de área → ação
// ═══════════════════════════════════════════════════════════════════════════════

struct TouchRegion {
  int           x1, y1, x2, y2;
  int           counterIdx;      // índice em counters[], -1 = ação global
  int           delta;
  unsigned long holdMs;          // 0 = tap; >0 = long press
  int           progressY;
  void          (*globalAction)();
};

// Declarações antecipadas para uso nos ponteiros de função abaixo
void endOfTurn();
void resetAll();

TouchRegion regions[] = {
//   x1    y1                         x2             y2                 cIdx  delta  holdMs              pY          action
  // ── Zona 1: Vida ──────────────────────────────────────────────────────────────────────────────────────────────
  {    0,   0,                         60,            89,                 0,   -1,    0,                   0,          nullptr   },  // vCycler  −1
  {   61,   0,                        119,            89,                 0,   +1,    0,                   0,          nullptr   },  // vCycler  +1
  {  120,   0,                        180,            89,                 1,   -1,    0,                   0,          nullptr   },  // vOpponent −1
  {  181,   0,                        239,            89,                 1,   +1,    0,                   0,          nullptr   },  // vOpponent +1
  // ── Zona 2: Swamp +10 — long press, 1/3 superior ─────────────────────────────────────────────────────────────
  {    0,   Z2_Y,                     119,  Z2_Y+(Z3_Y-Z2_Y)/3-1,        2,  +10,  SWAMP_LONG_PRESS_MS,  Z2_Y+1,    nullptr   },  // swamp +10
  // ── Zona 2: Swamp ±1 — 2/3 inferiores ────────────────────────────────────────────────────────────────────────
  {    0,   Z2_Y+(Z3_Y-Z2_Y)/3,       60,  Z3_Y-1,                       2,   -1,    0,                   0,          nullptr   },  // swamp −1
  {   61,   Z2_Y+(Z3_Y-Z2_Y)/3,      119,  Z3_Y-1,                       2,   +1,    0,                   0,          nullptr   },  // swamp +1
  // ── Zona 2: Storm ±1 ──────────────────────────────────────────────────────────────────────────────────────────
  {  120,   Z2_Y,                     180,  Z3_Y-1,                       3,   -1,    0,                   0,          nullptr   },  // storm −1
  {  181,   Z2_Y,                     239,  Z3_Y-1,                       3,   +1,    0,                   0,          nullptr   },  // storm +1
  // ── Zona 3: Manas — linha superior +1, inferior −1 ───────────────────────────────────────────────────────────
  {    0,   Z3_Y,                      59,  Z3_Y+24,                      4,   +1,    0,                   0,          nullptr   },  // mW +1
  {    0,   Z3_Y+25,                   59,  Z4_Y-1,                       4,   -1,    0,                   0,          nullptr   },  // mW −1
  {   60,   Z3_Y,                     119,  Z3_Y+24,                      5,   +1,    0,                   0,          nullptr   },  // mU +1
  {   60,   Z3_Y+25,                  119,  Z4_Y-1,                       5,   -1,    0,                   0,          nullptr   },  // mU −1
  {  120,   Z3_Y,                     179,  Z3_Y+24,                      6,   +1,    0,                   0,          nullptr   },  // mR +1
  {  120,   Z3_Y+25,                  179,  Z4_Y-1,                       6,   -1,    0,                   0,          nullptr   },  // mR −1
  {  180,   Z3_Y,                     239,  Z3_Y+24,                      7,   +1,    0,                   0,          nullptr   },  // mG +1
  {  180,   Z3_Y+25,                  239,  Z4_Y-1,                       7,   -1,    0,                   0,          nullptr   },  // mG −1
  // ── Zona 4: Botões — long press ───────────────────────────────────────────────────────────────────────────────
  {    0,   Z4_Y,                     119,  SCR_H-1,                     -1,    0,  LONG_PRESS_MS,         SCR_H-5,   endOfTurn },  // end of turn
  {  120,   Z4_Y,                     239,  SCR_H-1,                     -1,    0,  LONG_PRESS_MS,         SCR_H-5,   resetAll  },  // reset all
};
const int NUM_REGIONS = sizeof(regions) / sizeof(regions[0]);

// ─── ESTADO DO TOQUE ─────────────────────────────────────────────────────────
bool          touching         = false;
unsigned long touchStartTime   = 0;
int           touchX = 0, touchY = 0;
bool          longPressHandled = false;

// ═══════════════════════════════════════════════════════════════════════════════
// BACKGROUND
// ═══════════════════════════════════════════════════════════════════════════════

void patchBackground(int x, int y, int w, int h) {
  fs::File file = LittleFS.open(BG_FILE, "r");
  if (!file) return;
  x = constrain(x, 0, SCR_W - w);
  y = constrain(y, 0, SCR_H - h);
  for (int16_t row = y; row < y + h; row++) {
    uint32_t pos = (row * (uint32_t)BG_LINE_BYTES) + (x * 2);
    file.seek(pos);
    uint16_t lineBuffer[w];
    file.read((uint8_t*)lineBuffer, w * 2);
    tft.pushImage(x, row, w, 1, lineBuffer);
  }
  file.close();
}

void drawBackground() {
  fs::File file = LittleFS.open(BG_FILE, "r");
  if (!file) { Serial.println("Erro: bg.bin nao encontrado!"); return; }
  for (int16_t row = 0; row < SCR_H; row++) {
    uint16_t lineBuffer[SCR_W];
    file.read((uint8_t*)lineBuffer, BG_LINE_BYTES);
    tft.pushImage(0, row, SCR_W, 1, lineBuffer);
  }
  file.close();
}

// ═══════════════════════════════════════════════════════════════════════════════
// DISPLAY DE COUNTER
// ═══════════════════════════════════════════════════════════════════════════════

void updateCounterDisplay(int idx) {
  const Counter& c = counters[idx];
  patchBackground(c.cx - c.patchW / 2, c.cy - c.patchH / 2, c.patchW, c.patchH);
  if (c.font) tft.setFreeFont(c.font);
  else        tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(c.color);
  tft.drawString(String(*c.value), c.cx, c.cy + c.textYOffset);
}

void drawUI() {
  drawBackground();
  for (int i = 0; i < NUM_COUNTERS; i++) updateCounterDisplay(i);
}

// ═══════════════════════════════════════════════════════════════════════════════
// DISPATCH DE TOQUE
// ═══════════════════════════════════════════════════════════════════════════════

void applyDelta(int idx, int delta) {
  Counter& c = counters[idx];
  int next = *c.value + delta;
  if (c.minValue != NO_MIN) next = max(next, c.minValue);
  *c.value = next;
}

const TouchRegion* findRegion(int x, int y) {
  for (int i = 0; i < NUM_REGIONS; i++) {
    const TouchRegion& r = regions[i];
    if (x >= r.x1 && x <= r.x2 && y >= r.y1 && y <= r.y2) return &r;
  }
  return nullptr;
}

void fireAction(const TouchRegion& r) {
  if (r.counterIdx >= 0) {
    applyDelta(r.counterIdx, r.delta);
    updateCounterDisplay(r.counterIdx);
  } else if (r.globalAction) {
    r.globalAction();
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// BARRA DE PROGRESSO
// ═══════════════════════════════════════════════════════════════════════════════

void drawProgress(const TouchRegion& r, unsigned long elapsed) {
  int totalW = r.x2 - r.x1 + 1;
  int barW   = map(constrain(elapsed, 0, r.holdMs), 0, r.holdMs, 0, totalW);
  tft.fillRect(r.x1, r.progressY, totalW, 3, 0x2104);
  tft.fillRect(r.x1, r.progressY, barW,   3, COL_PROGRESS);
}

void clearProgress(const TouchRegion& r) {
  patchBackground(r.x1, r.progressY - 1, r.x2 - r.x1 + 1, 5);
}

// ═══════════════════════════════════════════════════════════════════════════════
// AÇÕES GLOBAIS
// ═══════════════════════════════════════════════════════════════════════════════

void endOfTurn() {
  for (int i = 0; i < NUM_COUNTERS; i++) {
    if (counters[i].resetOnEndOfTurn) {
      *counters[i].value = 0;
      updateCounterDisplay(i);
    }
  }
}

void resetAll() {
  for (int i = 0; i < NUM_COUNTERS; i++) {
    *counters[i].value = counters[i].resetValue;
    updateCounterDisplay(i);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  WiFi.mode(WIFI_OFF);
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  if (!LittleFS.begin()) Serial.println("Erro ao montar LittleFS");
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  tft.init();
  tft.setRotation(0);
  ts.setRotation(0);
  tft.setSwapBytes(true);
  resetAll();
}

// ═══════════════════════════════════════════════════════════════════════════════
// LOOP
// ═══════════════════════════════════════════════════════════════════════════════

void loop() {
  bool nowTouching = ts.touched();

  if (nowTouching) {
    TS_Point p = ts.getPoint();
    int px = constrain(map(p.x, 300, 3800, 0, SCR_W), 0, SCR_W - 1);
    int py = constrain(map(p.y, 200, 3700, 0, SCR_H), 0, SCR_H - 1);

    if (!touching) {
      touchStartTime   = millis();
      touchX           = px;
      touchY           = py;
      longPressHandled = false;
    } else if (!longPressHandled) {
      unsigned long      elapsed = millis() - touchStartTime;
      const TouchRegion* r       = findRegion(touchX, touchY);

      if (r && r->holdMs > 0) {
        if (elapsed >= r->holdMs) {
          clearProgress(*r);
          fireAction(*r);
          longPressHandled = true;
        } else {
          drawProgress(*r, elapsed);
        }
      }
    }
  } else {
    if (touching && !longPressHandled) {
      const TouchRegion* r = findRegion(touchX, touchY);
      if (r) {
        if (r->holdMs == 0 && (millis() - touchStartTime) < TAP_MAX_MS)
          fireAction(*r);
        else if (r->holdMs > 0)
          clearProgress(*r);
      }
    }
  }

  touching = nowTouching;
  delay(10);
}
