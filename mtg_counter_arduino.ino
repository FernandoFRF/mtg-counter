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

// --- Brilho máximo
#define TFT_BL 21

SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

// --- FRONTEIRAS DE TOQUE (ZONAS Y) ---
#define Z2_Y    90   // Onde termina a vida e começa o Swamp/Storm
#define Z3_Y    225   // Onde terminam os contadores e começam as Manas
#define Z4_Y    295   // Onde terminam as manas e começam os botões Reset/End

// ─── LAYOUT ──────────────────────────────────────────────────────────────────
#define SCR_W   240
#define SCR_H   320
#define MID_X   120

// --- VIDA ---
#define LIFE_Y        55  
#define CYCLER_X      65
#define OPPONENT_X    175
#define LIFE_W        100
#define LIFE_H        70

// --- CONTADORES (Swamp/Storm) ---
#define COUNTER_Y     150 
#define SWAMP_X       65  
#define STORM_X       175 
#define COUNTER_W     70  // Largura da área a ser restaurada
#define COUNTER_H     70  // Altura da área a ser restaurada

// --- MANAS ---
#define MANA_Y        275 // Posição vertical dos números de mana
#define MANA_W_X      40  // White
#define MANA_U_X      95  // Blue
#define MANA_R_X      145 // Red
#define MANA_G_X      200 // Green
#define MANA_PATCH_W  40  // Largura da limpeza para cada número
#define MANA_PATCH_H  30  // Altura da limpeza para cada número

// --- Arquivo do background ---

#define BG_FILE "/bg.bin"  // Mudamos de .bmp para .bin
#define BG_LINE_BYTES (SCR_W * 2) // 240 pixels * 2 bytes por pixel (RGB565)

// ─── CORES ───────────────────────────────────────────────────────────────────
#define COL_BG       TFT_BLACK
#define COL_PROGRESS 0x5ADF   // azul accent p/ barra de progresso
#define COL_OUTLINE  0x0000   // contorno preto dos números

// ─── ESTADO DO JOGO ──────────────────────────────────────────────────────────
int vCycler = 20, vOpponent = 20;
int cSwamp  = 0,  cStorm    = 0;
int mW = 0, mU = 0, mR = 0, mG = 0;

// ─── ESTADO DO TOQUE ─────────────────────────────────────────────────────────
bool          touching         = false;
unsigned long touchStartTime   = 0;
int           touchX = 0, touchY = 0;
bool          longPressHandled = false;
bool          progressDrawn    = false;

#define LONG_PRESS_MS 1000

// ─── HELPERS ─────────────────────────────────────────────────────────────────

String fmtNum(int v) { return String(v); }

// Texto com contorno preto ao redor (para sobrepor ícones sem obstruir)
void drawOutlinedString(const String& s, int x, int y, uint16_t fg) {
  tft.setTextColor(COL_OUTLINE);
  // 8 direções — contorno grosso de 2px
  for (int dx = -2; dx <= 2; dx += 2) {
    for (int dy = -2; dy <= 2; dy += 2) {
      if (dx == 0 && dy == 0) continue;
      tft.drawString(s, x + dx, y + dy);
    }
  }
  tft.setTextColor(fg);
  tft.drawString(s, x, y);
}

// ─── DESENHO DAS ZONAS ───────────────────────────────────────────────────────

void patchBackground(int x, int y, int w, int h) {
  fs::File file = LittleFS.open(BG_FILE, "r");
  if (!file) return;

  // Garante que não vamos ler fora da tela
  x = constrain(x, 0, SCR_W - w);
  y = constrain(y, 0, SCR_H - h);

  for (int16_t row = y; row < y + h; row++) {
    // Cálculo exato da posição da linha:
    // (Linha atual * total de bytes por linha) + (Coluna X inicial * 2 bytes por pixel)
    uint32_t pos = (row * (uint32_t)BG_LINE_BYTES) + (x * 2);
    
    file.seek(pos);
    
    uint16_t lineBuffer[w];
    // Lê apenas os bytes necessários para a largura do patch
    file.read((uint8_t*)lineBuffer, w * 2);
    
    // Restaura o fundo perfeito naquela linha
    tft.pushImage(x, row, w, 1, lineBuffer);
  }
  file.close();
}

void drawBackground() {
  fs::File file = LittleFS.open(BG_FILE, "r");
  if (!file) {
    Serial.println("Erro: Arquivo bg.bin nao encontrado!");
    return;
  }

  for (int16_t row = 0; row < SCR_H; row++) {
    uint16_t lineBuffer[SCR_W]; 
    
    // Lê uma linha inteira do arquivo (480 bytes) diretamente para o buffer
    file.read((uint8_t*)lineBuffer, BG_LINE_BYTES);
    
    // Envia a linha de pixels RGB565 prontos para a tela
    tft.pushImage(0, row, SCR_W, 1, lineBuffer);
  }
  file.close();
}

// ── Zona 1: Vidas ────────────────────────────────────────────────────────────
// Chame updateVidas(true) para Cycler ou updateVidas(false) para Opponent
void updateVidas(bool isCycler) {
  int targetX = isCycler ? CYCLER_X : OPPONENT_X;
  int valor   = isCycler ? vCycler : vOpponent;

  // 1. Restaura o fundo apenas do lado que mudou
  patchBackground(targetX - (LIFE_W/2), LIFE_Y - (LIFE_H/2), LIFE_W, LIFE_H);

  // 2. Desenha o novo número
  tft.setFreeFont(&FreeSans18pt7b);;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE); // Sem cor de fundo para ser transparente
  tft.drawString(fmtNum(valor), targetX, LIFE_Y);
}

// --- Atualiza Swamp ou Storm ---
void updateCounter(bool isSwamp) {
  int targetX = isSwamp ? SWAMP_X : STORM_X;
  int valor   = isSwamp ? cSwamp : cStorm;

  // Restaura apenas o fundo onde o ícone/número fica
  patchBackground(targetX - (COUNTER_W/2), COUNTER_Y - (COUNTER_H/2), COUNTER_W, COUNTER_H);

  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE); // Sem cor de fundo = Transparente
  tft.drawString(fmtNum(valor), targetX, COUNTER_Y + 5);
}

// --- Atualiza uma Mana específica (0=W, 1=U, 2=R, 3=G) ---
void updateMana(int index) {
  int targetX;
  int valor;
  uint16_t cor;

  // Define qual mana estamos mexendo
  switch(index) {
    case 0: targetX = MANA_W_X; valor = mW; cor = 0xFFFE; break; // Branco/Amarelado
    case 1: targetX = MANA_U_X; valor = mU; cor = 0x5DFF; break; // Azul
    case 2: targetX = MANA_R_X; valor = mR; cor = 0xFB8D; break; // Vermelho
    case 3: targetX = MANA_G_X; valor = mG; cor = 0x77EC; break; // Verde
    default: return;
  }

  // Limpa o fundo do número da mana (tira o quadrado preto)
  patchBackground(targetX - (MANA_PATCH_W/2), MANA_Y - (MANA_PATCH_H/2), MANA_PATCH_W, MANA_PATCH_H);

  tft.setTextFont(2); // Fonte menor para as manas
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(cor); 
  tft.drawString(fmtNum(valor), targetX, MANA_Y);
}

// ── Zona 4: Botões ────────────────────────────────────────────────────────────
void updateButtons() {
  // Não desenhamos mais os textos "END OF TURN" e "RESET"
  // Esta função agora serve apenas para limpar as barras de progresso se necessário
}

// Barra de progresso do long press (aparece embaixo do texto do botão)
void drawProgress(int btnCX, unsigned long elapsed) {
  int totalW = 80; // Reduzi um pouco para garantir que não bata nas bordas
  int barW   = map(constrain(elapsed, 0, LONG_PRESS_MS), 0, LONG_PRESS_MS, 0, totalW);
  int barX   = btnCX - totalW / 2;
  
  // Ajuste dinâmico: desenha a barra 5 pixels acima do final da tela
  // ou 2 pixels abaixo do limite do botão (Z4_Y)
  int barY   = SCR_H - 5; 

  // Desenha um fundo sutil para a barra (opcional, ajuda a ver o progresso)
  tft.fillRect(barX, barY, totalW, 3, 0x2104); // Cinza bem escuro
  
  // Desenha o progresso em si
  tft.fillRect(barX, barY, barW, 3, COL_PROGRESS);
}

// ─── TELA COMPLETA ───────────────────────────────────────────────────────────
void drawUI() {
  drawBackground(); // Desenha o bg.bmp inteiro
  updateVidas(true);
  updateVidas(false);
  updateCounter(true);
  updateCounter(false);
  for(int i=0; i<4; i++) updateMana(i);
  updateButtons();
}

// Versão da sua função de contorno para Sprites
void drawOutlinedStringSprite(TFT_eSprite *spr, const String& s, int x, int y, uint16_t fg) {
  spr->setTextColor(COL_OUTLINE);
  // Desenha o contorno com 2 pixels de espessura (dx/dy de -2 a 2)
  // Isso cria uma borda preta bem visível atrás do número branco
  for (int dx = -2; dx <= 2; dx++) {
    for (int dy = -2; dy <= 2; dy++) {
      if (dx == 0 && dy == 0) continue;
      spr->drawString(s, x + dx, y + dy);
    }
  }
  spr->setTextColor(fg);
  spr->drawString(s, x, y);
}

// ─── AÇÕES ───────────────────────────────────────────────────────────────────
void endOfTurn() {
  mW = 0; mU = 0; mR = 0; mG = 0;
  cStorm = 0; cSwamp = 0; 
  updateCounter(true);
  updateCounter(false);
  for(int i=0; i<4; i++) updateMana(i);
  updateButtons();
}

void resetAll() {
  vCycler = 20; vOpponent = 20;
  cSwamp  = 0;  cStorm    = 0;
  mW = 0; mU = 0; mR = 0; mG = 0;
  drawUI(); // Redesenha tudo, incluindo o fundo
}

// ─── TOQUE CURTO ─────────────────────────────────────────────────────────────
void handleTap(int x, int y) {
  // ZONA 1: VIDAS
  if (y < Z2_Y) {
    if (x < MID_X) { vCycler += (x > 60) ? 1 : -1; updateVidas(true); }
    else           { vOpponent += (x > 180) ? 1 : -1; updateVidas(false); }
  } 
  // ZONA 2: CONTADORES
  else if (y < Z3_Y) {
    if (x < MID_X) {
      if (y < Z2_Y + 35) cSwamp += 10;
      else               cSwamp = max(0, cSwamp + ((x > 60) ? 1 : -1));
      updateCounter(true);
    } else {
      cStorm = max(0, cStorm + ((x > 180) ? 1 : -1));
      updateCounter(false);
    }
  } 
  // ZONA 3: MANAS
  else if (y < Z4_Y) {
    int col = x / 60; // 0, 1, 2, 3
    int mod = (y < (Z3_Y + 25)) ? 1 : -1;
    
    if (col == 0) { mW = max(0, mW + mod); updateMana(0); }
    else if (col == 1) { mU = max(0, mU + mod); updateMana(1); }
    else if (col == 2) { mR = max(0, mR + mod); updateMana(2); }
    else if (col == 3) { mG = max(0, mG + mod); updateMana(3); }
  }
}

// ─── SETUP ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // HIGH é o brilho máximo
  WiFi.mode(WIFI_OFF);
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  if(!LittleFS.begin()){
    Serial.println("Erro ao montar LittleFS");
  }
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  tft.init();
  tft.setRotation(0);
  ts.setRotation(0);
  tft.setSwapBytes(true); // Ou false, teste os dois.
  drawUI();
}

// ─── LOOP ────────────────────────────────────────────────────────────────────
void loop() {
  bool nowTouching = ts.touched();

  if (nowTouching) {
    TS_Point p = ts.getPoint();
    // Calibração do toque (ajuste px/py se necessário conforme seu modelo)
    int px = map(p.x, 300, 3800, 0, SCR_W);
    int py = map(p.y, 200, 3700, 0, SCR_H);
    px = constrain(px, 0, SCR_W - 1);
    py = constrain(py, 0, SCR_H - 1);

    if (!touching) {
      // --- Início do Toque ---
      touchStartTime   = millis();
      touchX           = px;
      touchY           = py;
      longPressHandled = false;
    } else if (!longPressHandled) {
      // --- Toque em andamento (Hold) ---
      unsigned long elapsed = millis() - touchStartTime;

      if (touchY >= Z4_Y) {
        int btnCX = (touchX < MID_X) ? 60 : 180;
        
        // Verifica se o tempo de Long Press foi atingido
        if (elapsed >= LONG_PRESS_MS) {
          // 1. Apaga a barra IMEDIATAMENTE
          patchBackground(0, SCR_H - 8, SCR_W, 8); 
          
          // 2. Executa a ação
          if (touchX < MID_X) endOfTurn();
          else                resetAll();
          
          // 3. Marca como resolvido para não repetir e não entrar no handleTap
          longPressHandled = true; 
        } 
        else {
          // Enquanto não atinge o tempo, desenha o progresso
          drawProgress(btnCX, elapsed);
        }
      }
    }
  } else {
    // --- Soltou o dedo ---
    if (touching) {
      // Se não foi um clique longo, processa como tap normal
      if (!longPressHandled) {
        if ((millis() - touchStartTime) < 600) {
          handleTap(touchX, touchY);
        }
        
        // Limpa a barra se o usuário soltar antes de completar o tempo
        if (touchY >= Z4_Y) {
          patchBackground(0, SCR_H - 8, SCR_W, 8); 
        }
      }
      // O Reset do longPressHandled acontece naturalmente na próxima vez que tocar
    }
  }

  touching = nowTouching;
  delay(75);
}
