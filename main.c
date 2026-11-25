/*
 * PIXEL RACER - pure C / Win32 GDI edition
 *
 * 160x240  32bpp DIB 
 *  StretchBlt(COLORONCOLOR) 
 *  5x7  GDI 
 * ClearType /
 *
 *  game-config.h game-config.json 
 *  index.html / Program.cs  sync-config.js 
 *
 *  build-c.bat Visual Studio  PixelRacerC.sln
 */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "game-config.h"

#define ST_TITLE 0
#define ST_PLAY  1
#define ST_PAUSE 2
#define ST_OVER  3

#define MAX_ENEMIES   16
#define MAX_COINS     32
#define MAX_PARTS     96
#define MAX_FLOATERS  16
#define MAX_WAVES     16
#define MAX_PATH_CH   512

#define TXT_LEFT   0
#define TXT_CENTER 1
#define TXT_RIGHT  2

/* ---------- 5x7  5 bit0  ---------- */
typedef struct { char ch; unsigned char col[5]; } GlyphDef;
static const GlyphDef GLYPHS[] = {
  { ' ', {0x00,0x00,0x00,0x00,0x00} },
  { 'A', {0x7E,0x11,0x11,0x11,0x7E} }, { 'B', {0x7F,0x49,0x49,0x49,0x36} },
  { 'C', {0x3E,0x41,0x41,0x41,0x22} }, { 'D', {0x7F,0x41,0x41,0x22,0x1C} },
  { 'E', {0x7F,0x49,0x49,0x49,0x41} }, { 'F', {0x7F,0x09,0x09,0x09,0x01} },
  { 'G', {0x3E,0x41,0x49,0x49,0x7A} }, { 'H', {0x7F,0x08,0x08,0x08,0x7F} },
  { 'I', {0x00,0x41,0x7F,0x41,0x00} }, { 'J', {0x20,0x40,0x41,0x3F,0x01} },
  { 'K', {0x7F,0x08,0x14,0x22,0x41} }, { 'L', {0x7F,0x40,0x40,0x40,0x40} },
  { 'M', {0x7F,0x02,0x0C,0x02,0x7F} }, { 'N', {0x7F,0x04,0x08,0x10,0x7F} },
  { 'O', {0x3E,0x41,0x41,0x41,0x3E} }, { 'P', {0x7F,0x09,0x09,0x09,0x06} },
  { 'Q', {0x3E,0x41,0x51,0x21,0x5E} }, { 'R', {0x7F,0x09,0x19,0x29,0x46} },
  { 'S', {0x46,0x49,0x49,0x49,0x31} }, { 'T', {0x01,0x01,0x7F,0x01,0x01} },
  { 'U', {0x3F,0x40,0x40,0x40,0x3F} }, { 'V', {0x1F,0x20,0x40,0x20,0x1F} },
  { 'W', {0x3F,0x40,0x38,0x40,0x3F} }, { 'X', {0x63,0x14,0x08,0x14,0x63} },
  { 'Y', {0x07,0x08,0x70,0x08,0x07} }, { 'Z', {0x61,0x51,0x49,0x45,0x43} },
  { '0', {0x3E,0x51,0x49,0x45,0x3E} }, { '1', {0x00,0x42,0x7F,0x40,0x00} },
  { '2', {0x42,0x61,0x51,0x49,0x46} }, { '3', {0x21,0x41,0x45,0x4B,0x31} },
  { '4', {0x18,0x14,0x12,0x7F,0x10} }, { '5', {0x27,0x45,0x45,0x45,0x39} },
  { '6', {0x3C,0x4A,0x49,0x49,0x30} }, { '7', {0x01,0x71,0x09,0x05,0x03} },
  { '8', {0x36,0x49,0x49,0x49,0x36} }, { '9', {0x06,0x49,0x49,0x29,0x1E} },
  { '+', {0x08,0x08,0x3E,0x08,0x08} }, { '-', {0x08,0x08,0x08,0x08,0x08} },
  { '/', {0x20,0x10,0x08,0x04,0x02} }, { '(', {0x00,0x1C,0x22,0x41,0x00} },
  { ')', {0x00,0x41,0x22,0x1C,0x00} }, { '.', {0x00,0x60,0x60,0x00,0x00} },
  { ':', {0x00,0x36,0x36,0x00,0x00} }, { '!', {0x00,0x00,0x5F,0x00,0x00} },
  { '?', {0x02,0x01,0x51,0x09,0x06} }, { 'x', {0x44,0x28,0x10,0x28,0x44} }
};
static const unsigned char* g_glyph[128];

/* ----------  ---------- */
static unsigned char* g_bits = NULL;
static HBITMAP g_dib = NULL;
static HDC g_memDC = NULL;
static HWND g_hwnd = NULL;
static int g_scale = 3;
static int g_dx = 0, g_dy = 0;   /*  */

/* ----------  ---------- */
typedef struct { int x; double y; unsigned int color; int passed; int nearMiss; } Enemy;
typedef struct { int x; double y; } Coin;
typedef struct { double x, y, vx, vy, life; unsigned int color; int s; } Part;
typedef struct { char text[24]; double x, y, life; unsigned int color; } Floater;

static Enemy   g_enemies[MAX_ENEMIES];   static int g_nEnemies;
static Coin    g_coins[MAX_COINS];       static int g_nCoins;
static Part    g_parts[MAX_PARTS];       static int g_nParts;
static Floater g_floaters[MAX_FLOATERS]; static int g_nFloaters;
static GCW     g_waveQ[MAX_WAVES];       static int g_waveQCount;
static GCW     g_warn;                   static int g_warnActive; static double g_warnT;

/* ----------  ---------- */
static int g_state = ST_TITLE, g_newRecord = 0, g_muted = 0, g_best = 0;
static double g_score, g_scroll, g_speed, g_spawnT, g_coinT, g_runTime, g_time;
static double g_shake, g_flash, g_hitStop;
static int g_lives, g_stage, g_laneIndex;
static double g_playerX, g_tilt, g_steerT, g_invuln, g_combo, g_comboT;
static double g_engineT, g_acc;
static int g_keysLeft, g_keysRight, g_ptrLeft, g_ptrRight;

/* ----------  ---------- */
static volatile LONG g_beepBusy = 0;
static int g_beepFreq = 440, g_beepMs = 30;
static DWORD WINAPI beepProc(LPVOID p)
{
  (void)p;
  Beep((DWORD)g_beepFreq, (DWORD)g_beepMs);
  InterlockedExchange(&g_beepBusy, 0);
  return 0;
}
static void beepAsync(int freq, int ms)
{
  HANDLE h;
  if (g_muted) return;
  if (InterlockedCompareExchange(&g_beepBusy, 1, 0) != 0) return;
  g_beepFreq = freq; g_beepMs = ms;
  h = CreateThread(NULL, 0, beepProc, NULL, 0, NULL);
  if (h) CloseHandle(h);
  else InterlockedExchange(&g_beepBusy, 0);
}

/* ----------  ---------- */
static double rnd(void) { return (double)rand() / ((double)RAND_MAX + 1.0); }
static int rndInt(int n) { return n > 0 ? (int)(rnd() * n) : 0; }
static int clampI(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static double clampD(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }
static double maxD(double a, double b) { return a > b ? a : b; }
static double minD(double a, double b) { return a < b ? a : b; }
static int overlap(double ax, double ay, double aw, double ah,
                   double bx, double by, double bw, double bh)
{
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}
static unsigned int shadeColor(unsigned int c, int amt)
{
  int r = (int)((c >> 16) & 0xFF) + amt;
  int g = (int)((c >> 8) & 0xFF) + amt;
  int b = (int)(c & 0xFF) + amt;
  r = clampI(r, 0, 255); g = clampI(g, 0, 255); b = clampI(b, 0, 255);
  return ((unsigned int)r << 16) | ((unsigned int)g << 8) | (unsigned int)b;
}

/*
 *  32bpp top-down DIB 0xRRGGBB
 * B,G,R,0 DIB 
 */
static void rect(int x, int y, int w, int h, unsigned int color)
{
  int x0 = x + g_dx, y0 = y + g_dy, x1 = x0 + w, y1 = y0 + h, xx, yy;
  if (!g_bits) return;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > GC_W) x1 = GC_W;
  if (y1 > GC_H) y1 = GC_H;
  for (yy = y0; yy < y1; yy++) {
    unsigned int* row = (unsigned int*)(g_bits + (size_t)yy * GC_W * 4);
    for (xx = x0; xx < x1; xx++) row[xx] = color;
  }
}
static void blendRect(int x, int y, int w, int h, unsigned int color, double a)
{
  int x0 = x + g_dx, y0 = y + g_dy, x1 = x0 + w, y1 = y0 + h, xx, yy;
  unsigned int sr = (color >> 16) & 0xFF, sg = (color >> 8) & 0xFF, sb = color & 0xFF;
  if (!g_bits) return;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > GC_W) x1 = GC_W;
  if (y1 > GC_H) y1 = GC_H;
  for (yy = y0; yy < y1; yy++) {
    unsigned int* row = (unsigned int*)(g_bits + (size_t)yy * GC_W * 4);
    for (xx = x0; xx < x1; xx++) {
      unsigned int d = row[xx];
      unsigned int dr = (d >> 16) & 0xFF, dg = (d >> 8) & 0xFF, db = d & 0xFF;
      unsigned int nr = (unsigned int)(sr * a + dr * (1 - a) + 0.5);
      unsigned int ng = (unsigned int)(sg * a + dg * (1 - a) + 0.5);
      unsigned int nb = (unsigned int)(sb * a + db * (1 - a) + 0.5);
      row[xx] = (nr << 16) | (ng << 8) | nb;
    }
  }
}
static void initFont(void)
{
  int i;
  for (i = 0; i < 128; i++) g_glyph[i] = NULL;
  for (i = 0; i < (int)(sizeof(GLYPHS) / sizeof(GLYPHS[0])); i++)
    g_glyph[(unsigned char)GLYPHS[i].ch] = GLYPHS[i].col;
  for (i = 0; i < 128; i++)
    if (!g_glyph[i]) g_glyph[i] = g_glyph[(unsigned char)'?'];
}
static void drawText(const char* s, int x, int y, unsigned int color, int scale, int align)
{
  int len = (int)strlen(s);
  int width = (len * 6 - 1) * scale;
  int cx = x, col, row;
  const unsigned char* g;
  if (align == TXT_CENTER) cx = x - width / 2;
  else if (align == TXT_RIGHT) cx = x - width;
  for (; *s; s++) {
    unsigned char ch = (unsigned char)*s;
    if (ch >= 128) ch = '?';
    g = g_glyph[ch];
    for (col = 0; col < 5; col++)
      for (row = 0; row < 7; row++)
        if (g[col] & (1u << row))
          rect(cx + col * scale, y + row * scale, scale, scale, color);
    cx += 6 * scale;
  }
}

/* ---------- %APPDATA%\PixelRacer\best.txt ---------- */
static void bestPath(char* out, int cap)
{
  const char* app = getenv("APPDATA");
  if (app) snprintf(out, cap, "%s\\PixelRacer\\best.txt", app);
  else snprintf(out, cap, "best.txt");
}
static void loadBest(void)
{
  char path[MAX_PATH_CH], dir[MAX_PATH_CH];
  const char* app = getenv("APPDATA");
  FILE* f;
  bestPath(path, sizeof(path));
  if (app) {
    snprintf(dir, sizeof(dir), "%s\\PixelRacer", app);
    CreateDirectoryA(dir, NULL);
  }
  f = fopen(path, "r");
  if (f) {
    if (fscanf(f, "%d", &g_best) != 1) g_best = 0;
    fclose(f);
  }
}
static void saveBest(void)
{
  char path[MAX_PATH_CH];
  FILE* f;
  bestPath(path, sizeof(path));
  f = fopen(path, "w");
  if (f) { fprintf(f, "%d", g_best); fclose(f); }
}

/* ----------  ---------- */
/* gameOver  */
static void burst(double x, double y, unsigned int color, int n);

static void reset(void)
{
  g_score = 0; g_scroll = 0; g_speed = GC_SPEED_BASE;
  g_spawnT = 0.7; g_coinT = 1.6; g_runTime = 0; g_time = 0;
  g_shake = 0; g_flash = 0; g_hitStop = 0; g_newRecord = 0;
  g_lives = GC_START_LIVES; g_invuln = 0; g_combo = 0; g_comboT = 0; g_stage = 0;
  g_laneIndex = 1; g_playerX = GC_LANES[1]; g_tilt = 0; g_steerT = 0;
  g_engineT = 0; g_acc = 0;
  g_nEnemies = 0; g_nCoins = 0; g_nParts = 0; g_nFloaters = 0;
  g_waveQCount = 0; g_warnActive = 0;
}
static void start(void)
{
  reset();
  g_state = ST_PLAY;
  beepAsync(520, 40);
}
static void gameOver(void)
{
  g_state = ST_OVER;
  burst(g_playerX, GC_PLAYER_Y, GC_PLAYER, 26);
  g_shake = 1;
  beepAsync(110, 80);
  if ((int)g_score > g_best) {
    g_best = (int)g_score;
    saveBest();
    g_newRecord = 1;
  }
}

/* ----------  ---------- */
static int g_carX, g_carY;
static double g_bandTilt;
static void carRect(int px, int py, int pw, int ph, unsigned int c)
{
  int band = (int)floor(g_bandTilt * (py / (double)GC_CAR_H - 0.45) + 0.5);
  rect(g_carX + px + band, g_carY + py, pw, ph, c);
}
static void drawCar(double fx, double fy, unsigned int body, int down, double bodyTilt)
{
  int w = GC_CAR_W, h = GC_CAR_H, headY, tailY;
  unsigned int hi, lo;
  g_carX = (int)fx; g_carY = (int)fy; g_bandTilt = bodyTilt;
  hi = shadeColor(body, 52); lo = shadeColor(body, -52);
  carRect(-2, 4, 2, 5, GC_BLACK);   carRect(w, 4, 2, 5, GC_BLACK);
  carRect(-2, 15, 2, 5, GC_BLACK); carRect(w, 15, 2, 5, GC_BLACK);
  carRect(0, 0, w, h, body);
  carRect(1, 1, w - 2, 2, hi);
  carRect(1, h - 2, w - 2, 2, lo);
  carRect(2, 6, w - 4, 5, GC_GLASS);
  carRect(2, 15, w - 4, 4, GC_GLASS);
  carRect(2, 7, w - 4, 1, GC_WHITE);
  carRect(3, 12, w - 6, 1, hi);
  headY = down ? h - 3 : 1;
  tailY = down ? 1 : h - 3;
  carRect(1, headY, 3, 2, GC_YELLOW);     carRect(w - 4, headY, 3, 2, GC_YELLOW);
  carRect(1, tailY, 3, 2, GC_RED);        carRect(w - 4, tailY, 3, 2, GC_RED);
}
static void tree(int x, int y)
{
  rect(x + 2, y + 7, 3, 5, GC_BLACK);
  rect(x, y + 1, 7, 7, GC_TREE);
  rect(x + 1, y + 2, 5, 3, GC_TREE_HI);
}
static void pole(int x, int y)
{
  rect(x + 1, y + 3, 2, 12, GC_BLACK);
  rect(x + 1, y + 3, 2, 10, GC_POLE);
  rect(x, y, 6, 3, GC_YELLOW);
  rect(x + 1, y, 4, 1, GC_WHITE);
}
static void drawSceneryFar(void)
{
  int step = 48, off = (int)(g_scroll * 0.35) % step, y;
  for (y = -step + off; y < GC_H; y += step) { tree(4, y); tree(146, y + 20); }
}
static void drawSceneryNear(void)
{
  int step = 80, off = (int)g_scroll % step, y;
  for (y = -step + off; y < GC_H; y += step) { pole(10, y); pole(146, y + 40); }
}
static void drawSpeedLines(void)
{
  int n, off, i, x, y;
  if (g_speed < 140) return;
  n = (int)minD(9, 2 + (g_speed - 140) / 24);
  off = (int)(g_scroll * 1.7);
  for (i = 0; i < n; i++) {
    x = GC_ROAD_L + 6 + ((i * 47) % 108);
    y = ((int)(i * 61 + off * (1 + (i % 3) * 0.35)) % (GC_H + 30)) - 20;
    rect(x, y, 1, 5 + (i % 3) * 3, GC_SPEED_LINE);
  }
}
static void drawRoad(void)
{
  int stripe = 16, off = ((int)g_scroll) % (stripe * 2), y;
  int rb = 8, ro = ((int)g_scroll) % (rb * 2);
  int to = ((int)g_scroll) % 24;
  int seg = 12, gap = 12, d = ((int)g_scroll) % (seg + gap);
  rect(0, 0, GC_W, GC_H, GC_GRASS);
  for (y = -stripe * 2 + off; y < GC_H; y += stripe * 2) rect(0, y, GC_W, stripe, GC_GRASS2);
  drawSceneryFar();
  for (y = -rb * 2 + ro; y < GC_H; y += rb * 2) {
    rect(GC_ROAD_L - 4, y, 4, rb, GC_RUMBLE1);
    rect(GC_ROAD_R, y, 4, rb, GC_RUMBLE1);
    rect(GC_ROAD_L - 4, y + rb, 4, rb, GC_RUMBLE2);
    rect(GC_ROAD_R, y + rb, 4, rb, GC_RUMBLE2);
  }
  rect(GC_ROAD_L, 0, GC_ROAD_R - GC_ROAD_L, GC_H, GC_ROAD);
  for (y = -24 + to; y < GC_H; y += 24) rect(GC_ROAD_L, y, GC_ROAD_R - GC_ROAD_L, 1, GC_ROAD2);
  for (y = -(seg + gap) + d; y < GC_H; y += seg + gap) {
    rect(59, y, 2, seg, GC_DASH);
    rect(99, y, 2, seg, GC_DASH);
  }
  drawSceneryNear();
  drawSpeedLines();
}
static void drawCoin(int x, double y)
{
  int w = (int)(3 + fabs(sin(g_time * 5)) * 5);
  rect((int)(x + (8 - w) / 2.0), (int)y, w, 8, GC_YELLOW);
  rect((int)(x + (8 - w) / 2.0), (int)y, w, 2, GC_WHITE);
}
static void drawWarn(void)
{
  int i, x;
  if (!g_warnActive || ((int)floor(g_time * 14) % 2) != 0) return;
  for (i = 0; i < g_warn.n; i++) {
    x = GC_LANES[g_warn.block[i]] + 4;
    rect(x, GC_HUD_H, 8, 3, GC_WARN);
    rect(x + 2, GC_HUD_H + 3, 4, 2, GC_WARN);
  }
}
static void drawFloaters(void)
{
  int i;
  for (i = 0; i < g_nFloaters; i++)
    drawText(g_floaters[i].text, (int)g_floaters[i].x, (int)g_floaters[i].y,
             g_floaters[i].color, 1, TXT_CENTER);
}
static void drawHUD(void)
{
  char b[32];
  int i, on;
  blendRect(0, 0, GC_W, GC_HUD_H, 0x0C0E14, 0.84);
  snprintf(b, sizeof(b), "SCORE %05d", (int)g_score);
  drawText(b, 4, 2, GC_WHITE, 1, TXT_LEFT);
  snprintf(b, sizeof(b), "BEST %05d", g_best);
  drawText(b, GC_W - 4, 2, GC_YELLOW, 1, TXT_RIGHT);
  for (i = 0; i < GC_MAX_LIVES; i++) {
    on = i < g_lives;
    rect(5 + i * 7, 12, 5, 5, on ? GC_PLAYER : GC_ROAD2);
    if (on) rect(6 + i * 7, 13, 3, 2, GC_WHITE);
  }
  snprintf(b, sizeof(b), "STG %d", g_stage + 1);
  drawText(b, 46, 11, GC_DASH, 1, TXT_LEFT);
  if (g_combo > 1) {
    snprintf(b, sizeof(b), "COMBO x%d", (int)g_combo);
    drawText(b, GC_W - 4, 11, g_comboT < 0.8 ? GC_DASH : GC_YELLOW, 1, TXT_RIGHT);
  }
}
/*  Courier  152  */
static void panel(int y, int h)
{
  blendRect(4, y, 152, h, 0x0A0C12, 0.88);
  rect(4, y, 152, 1, GC_WHITE);
  rect(4, y + h - 1, 152, 1, GC_WHITE);
}
static void drawTitle(void)
{
  char b[32];
  panel(52, 134);
  drawText("PIXEL RACER", 80, 60, GC_YELLOW, 2, TXT_CENTER);
  rect(30, 78, 100, 1, GC_ROAD2);
  drawText("DODGE THE TRAFFIC", 80, 84, GC_WHITE, 1, TXT_CENTER);
  drawText("NEAR MISS + GOLD COMBO", 80, 95, GC_WHITE, 1, TXT_CENTER);
  rect(30, 107, 100, 1, GC_ROAD2);
  drawText("ARROWS OR A / D", 80, 113, GC_DASH, 1, TXT_CENTER);
  drawText("3 LIVES  P PAUSE  M SOUND", 80, 124, GC_DASH, 1, TXT_CENTER);
  drawText("ANY KEY TO START", 80, 140, GC_YELLOW, 1, TXT_CENTER);
  snprintf(b, sizeof(b), "BEST %05d", g_best);
  drawText(b, 80, 156, GC_WHITE, 1, TXT_CENTER);
  drawText(g_muted ? "SOUND OFF (M)" : "SOUND ON (M)", 80, 169, GC_DASH, 1, TXT_CENTER);
}
static void drawPause(void)
{
  panel(96, 44);
  drawText("PAUSED", 80, 104, GC_YELLOW, 2, TXT_CENTER);
  drawText("PRESS P TO RESUME", 80, 124, GC_WHITE, 1, TXT_CENTER);
}
static void drawOver(void)
{
  char b[32];
  panel(70, 100);
  drawText("GAME OVER", 80, 78, GC_RED, 2, TXT_CENTER);
  rect(30, 97, 100, 1, GC_ROAD2);
  snprintf(b, sizeof(b), "SCORE %05d", (int)g_score);
  drawText(b, 80, 104, GC_WHITE, 1, TXT_CENTER);
  snprintf(b, sizeof(b), "BEST  %05d", g_best);
  drawText(b, 80, 117, GC_YELLOW, 1, TXT_CENTER);
  snprintf(b, sizeof(b), "REACHED STAGE %d", g_stage + 1);
  drawText(b, 80, 131, GC_DASH, 1, TXT_CENTER);
  drawText(g_newRecord ? "NEW RECORD!" : "ANY KEY TO RETRY", 80, 150,
           g_newRecord ? GC_YELLOW : GC_WHITE, 1, TXT_CENTER);
}

/* ----------  ---------- */
static void addEnemy(Enemy e)
{
  if (g_nEnemies < MAX_ENEMIES) g_enemies[g_nEnemies++] = e;
}
static void addCoin(int x, double y)
{
  if (g_nCoins < MAX_COINS) {
    g_coins[g_nCoins].x = x;
    g_coins[g_nCoins].y = y;
    g_nCoins++;
  }
}
static Enemy makeEnemy(int x)
{
  Enemy e;
  e.x = x; e.y = -GC_CAR_H - 2;
  e.color = GC_ENEMY_COLORS[rndInt(GC_ENEMY_COLOR_COUNT)];
  e.passed = 0; e.nearMiss = 0;
  return e;
}
static void spawnCoin(void)
{
  int freeLanes[GC_LANE_COUNT], nfree = 0, i, j, blocked;
  for (i = 0; i < GC_LANE_COUNT; i++) {
    blocked = 0;
    if (g_warnActive)
      for (j = 0; j < g_warn.n; j++) if (g_warn.block[j] == i) blocked = 1;
    if (blocked) continue;
    for (j = 0; j < g_nEnemies; j++)
      if (g_enemies[j].y < 50 && abs(g_enemies[j].x - GC_LANES[i]) < 4) { blocked = 1; break; }
    if (!blocked) freeLanes[nfree++] = GC_LANES[i];
  }
  if (nfree) addCoin(freeLanes[rndInt(nfree)] + 4, -12);
}
static void burst(double x, double y, unsigned int color, int n)
{
  int i;
  Part* p;
  for (i = 0; i < n && g_nParts < MAX_PARTS; i++) {
    p = &g_parts[g_nParts++];
    p->x = x + rnd() * GC_CAR_W;
    p->y = y + rnd() * GC_CAR_H;
    p->vx = (rnd() - 0.5) * 100;
    p->vy = (rnd() - 0.5) * 100 - 30;
    p->life = 0.5 + rnd() * 0.6;
    p->color = rnd() < 0.5 ? color : GC_WHITE;
    p->s = 1 + rndInt(2);
  }
}
static void addFloater(const char* s, double x, double y, unsigned int color)
{
  Floater* f;
  if (g_nFloaters >= MAX_FLOATERS) return;
  f = &g_floaters[g_nFloaters++];
  snprintf(f->text, sizeof(f->text), "%s", s);
  f->x = x; f->y = y; f->color = color; f->life = 0.9;
}

/* ----------  ---------- */
static double waveInterval(void)
{
  return maxD(GC_SPAWN_MIN, GC_SPAWN_BASE - g_stage * GC_SPAWN_STEP) * (0.9 + rnd() * 0.2);
}
/*  */
static void ensureReachable(GCW* w)
{
  double tArrive = (GC_PLAYER_Y + GC_CAR_H * 2) / (g_speed * GC_ENEMY_SPEED_FACTOR);
  double budget = tArrive * GC_REACH_MARGIN + GC_WARN_TIME;
  int gap = GC_LANES[1] - GC_LANES[0];
  int i, j, occupied, farIdx;
  for (i = 0; i < GC_LANE_COUNT; i++) {
    occupied = 0;
    for (j = 0; j < w->n; j++) if (w->block[j] == i) occupied = 1;
    if (!occupied && fabs(i - g_laneIndex) * gap / (double)GC_LANE_SPEED <= budget) return;
  }
  if (w->n == 0) return;
  farIdx = 0;
  for (i = 1; i < w->n; i++)
    if (abs(w->block[i] - g_laneIndex) > abs(w->block[farIdx] - g_laneIndex)) farIdx = i;
  for (i = farIdx; i < w->n - 1; i++) w->block[i] = w->block[i + 1];
  w->n--;
}
static void queueWave(void)
{
  const GCPOOL* pool = &GC_POOLS[GC_POOL_COUNT - 1];
  const GCP* def;
  const GCW* src;
  GCW w;
  int i, k, pi, count, startIdx;
  if (g_waveQCount == 0) {
    for (i = 0; i < GC_POOL_COUNT; i++)
      if (g_stage <= GC_POOLS[i].upTo) { pool = &GC_POOLS[i]; break; }
    pi = pool->pats[rndInt(pool->n)];
    def = &GC_PATTERNS[pi];
    count = def->sequence ? def->nw : 1;
    startIdx = def->sequence ? 0 : rndInt(def->nw);
    for (k = 0; k < count && g_waveQCount < MAX_WAVES; k++) {
      src = &def->w[startIdx + k];
      w.n = src->n; w.coin = src->coin;
      for (i = 0; i < GC_LANE_COUNT; i++)
        w.block[i] = (i < src->n) ? src->block[i] : 0;
      g_waveQ[g_waveQCount++] = w;
    }
  }
  if (g_waveQCount == 0) return;
  w = g_waveQ[0];
  for (i = 1; i < g_waveQCount; i++) g_waveQ[i - 1] = g_waveQ[i];
  g_waveQCount--;
  ensureReachable(&w);
  g_warn = w;
  g_warnT = GC_WARN_TIME;
  g_warnActive = 1;
}
static void emitWave(void)
{
  int i, cx;
  ensureReachable(&g_warn);
  for (i = 0; i < g_warn.n; i++) addEnemy(makeEnemy(GC_LANES[g_warn.block[i]]));
  if (g_warn.coin >= 0) {
    cx = GC_LANES[g_warn.coin] + 4;
    addCoin(cx, -12); addCoin(cx, -32); addCoin(cx, -52);
  }
  g_warnActive = 0;
}
/*  1  */
static int hitEnemy(Enemy* e)
{
  g_lives--;
  g_invuln = GC_INVULN_TIME; g_hitStop = GC_HIT_STOP;
  g_flash = GC_FLASH_TIME; g_shake = 1;
  burst(g_playerX, GC_PLAYER_Y, GC_PLAYER, 18);
  burst(e->x, e->y, GC_WHITE, 10);
  addFloater(g_lives > 0 ? "HIT! -1 LIFE" : "WRECKED",
             g_playerX, GC_PLAYER_Y - 14, GC_RED);
  beepAsync(140, 70);
  if (g_lives <= 0) { gameOver(); return 1; }
  return 0;
}

/* ---------- 1/120  ---------- */
static void update(double dt)
{
  int i, dir, clear, gain;
  double target, delta, move, dist;
  char b[32];
  g_time += dt;
  if (g_flash > 0) g_flash = maxD(0, g_flash - dt);
  if (g_hitStop > 0) { g_hitStop = maxD(0, g_hitStop - dt); return; }
  if (g_shake > 0) g_shake = maxD(0, g_shake - dt * 3);

  for (i = 0; i < g_nParts; ) {
    Part* p = &g_parts[i];
    p->life -= dt;
    if (p->life <= 0) { g_parts[i] = g_parts[--g_nParts]; continue; }
    p->vy += 180 * dt; p->x += p->vx * dt; p->y += p->vy * dt;
    i++;
  }
  for (i = 0; i < g_nFloaters; ) {
    Floater* f = &g_floaters[i];
    f->life -= dt; f->y -= 14 * dt;
    if (f->life <= 0) { g_floaters[i] = g_floaters[--g_nFloaters]; continue; }
    i++;
  }
  if (g_state == ST_TITLE) { g_scroll += 22 * dt; return; }
  if (g_state != ST_PLAY) return;

  g_runTime += dt;
  g_stage = (int)(g_runTime / GC_STAGE_SECONDS);
  g_score += GC_SCORE_PER_SECOND * dt;
  g_speed = minD(GC_SPEED_MAX, GC_SPEED_BASE + g_stage * GC_SPEED_STEP);
  g_scroll += g_speed * dt;
  if (g_invuln > 0) g_invuln = maxD(0, g_invuln - dt);
  if (g_comboT > 0) { g_comboT -= dt; if (g_comboT <= 0) g_combo = 0; }

  /*  */
  g_engineT -= dt;
  if (g_engineT <= 0) {
    g_engineT = maxD(0.16, 0.34 - g_speed / 2600.0);
    beepAsync((int)(48 + g_speed * 0.42), 22);
  }

  dir = (g_keysLeft || g_ptrLeft) ? -1 : ((g_keysRight || g_ptrRight) ? 1 : 0);
  if (dir != 0) {
    g_steerT -= dt;
    if (g_steerT <= 0) {
      g_laneIndex = clampI(g_laneIndex + dir, 0, GC_LANE_COUNT - 1);
      g_steerT = GC_LANE_REPEAT;
    }
  } else g_steerT = 0;
  target = GC_LANES[g_laneIndex];
  delta = target - g_playerX;
  move = GC_LANE_SPEED * dt;
  if (fabs(delta) <= move) { g_playerX = target; g_tilt *= 0.6; }
  else { g_playerX += (delta > 0 ? 1 : -1) * move; g_tilt = clampD(delta * 0.022, -2.6, 2.6); }

  if (!g_warnActive) {
    g_spawnT -= dt;
    if (g_spawnT <= 0) {
      clear = 1;
      for (i = 0; i < g_nEnemies; i++) if (g_enemies[i].y <= 40) { clear = 0; break; }
      if (clear) { queueWave(); g_spawnT = waveInterval(); }
      else g_spawnT = 0.15;
    }
  } else {
    g_warnT -= dt;
    if (g_warnT <= 0) emitWave();
  }
  g_coinT -= dt;
  if (g_coinT <= 0) {
    spawnCoin();
    g_coinT = maxD(1.2, GC_COIN_BASE - g_stage * 0.12) + rnd() * GC_COIN_JITTER;
  }

  for (i = 0; i < g_nEnemies; ) {
    Enemy* e = &g_enemies[i];
    e->y += g_speed * GC_ENEMY_SPEED_FACTOR * dt;
    if (!e->passed && e->y > GC_PLAYER_Y + GC_CAR_H) { e->passed = 1; g_score += GC_DODGE_SCORE; }
    if (e->y > GC_H + 4) { g_enemies[i] = g_enemies[--g_nEnemies]; continue; }
    if (overlap(g_playerX + 2, GC_PLAYER_Y + 2, GC_CAR_W - 4, GC_CAR_H - 4,
                e->x + 2, e->y + 2, GC_CAR_W - 4, GC_CAR_H - 4)) {
      Enemy hit = *e;
      g_enemies[i] = g_enemies[--g_nEnemies];
      if (g_invuln > 0) { burst(hit.x, hit.y, GC_WHITE, 6); continue; }
      if (hitEnemy(&hit)) return;
      continue;
    }
    dist = fabs(e->x - g_playerX);
    if (!e->nearMiss &&
        e->y < GC_PLAYER_Y + GC_CAR_H && e->y + GC_CAR_H > GC_PLAYER_Y &&
        dist > GC_CAR_W - 3 && dist < GC_CAR_W + GC_NEAR_MISS_GAP &&
        fabs(g_playerX - GC_LANES[g_laneIndex]) > 2) {
      e->nearMiss = 1;
      g_combo = minD(GC_COMBO_MAX, g_combo + 1);
      g_comboT = GC_COMBO_WINDOW;
      gain = (int)(GC_NEAR_MISS_SCORE * g_combo);
      g_score += gain;
      snprintf(b, sizeof(b), "+%d NEAR", gain);
      addFloater(b, e->x, e->y, GC_WHITE);
      beepAsync(1180, 25);
    }
    i++;
  }
  for (i = 0; i < g_nCoins; ) {
    Coin* c = &g_coins[i];
    c->y += g_speed * dt;
    if (c->y > GC_H + 8) { g_coins[i] = g_coins[--g_nCoins]; continue; }
    if (overlap(g_playerX, GC_PLAYER_Y, GC_CAR_W, GC_CAR_H,
                c->x - 2, c->y, 12, 12)) {
      int cx = c->x; double cy = c->y;
      g_coins[i] = g_coins[--g_nCoins];
      g_combo = minD(GC_COMBO_MAX, g_combo + 1);
      g_comboT = GC_COMBO_WINDOW;
      gain = (int)(GC_COIN_SCORE * g_combo);
      g_score += gain;
      if (g_combo > 1) snprintf(b, sizeof(b), "+%d x%d", gain, (int)g_combo);
      else snprintf(b, sizeof(b), "+%d", gain);
      addFloater(b, cx, cy, GC_YELLOW);
      burst(cx, cy, GC_YELLOW, 8);
      beepAsync(760 + (int)g_combo * 90, 30);
      continue;
    }
    i++;
  }
}

static void render(void)
{
  int i, blink;
  if (g_shake > 0) {
    g_dx = (int)((rnd() - 0.5) * 4 * g_shake);
    g_dy = (int)((rnd() - 0.5) * 4 * g_shake);
  } else g_dx = g_dy = 0;
  rect(-6, -6, GC_W + 12, GC_H + 12, GC_BLACK);
  drawRoad();
  drawWarn();
  for (i = 0; i < g_nCoins; i++) drawCoin(g_coins[i].x, g_coins[i].y);
  for (i = 0; i < g_nEnemies; i++) drawCar(g_enemies[i].x, g_enemies[i].y,
                                           g_enemies[i].color, 1, 0);
  blink = g_invuln > 0 && ((int)floor(g_time * 14) % 2) == 0;
  if (g_state != ST_OVER && !blink)
    drawCar(g_playerX, GC_PLAYER_Y, GC_PLAYER, 0, g_tilt);
  for (i = 0; i < g_nParts; i++)
    rect((int)g_parts[i].x, (int)g_parts[i].y, g_parts[i].s, g_parts[i].s, g_parts[i].color);
  drawFloaters();
  g_dx = g_dy = 0;
  drawHUD();
  if (g_flash > 0)
    blendRect(0, 0, GC_W, GC_H, 0xFFFFFF, g_flash / GC_FLASH_TIME * 0.5);
  if (g_state == ST_TITLE) drawTitle();
  else if (g_state == ST_PAUSE) drawPause();
  else if (g_state == ST_OVER) drawOver();
}

/* ----------  ---------- */
static void drawToWindow(HDC dc)
{
  if (!g_bits || !g_memDC) return;
  GdiFlush();
  SetStretchBltMode(dc, COLORONCOLOR);
  StretchBlt(dc, 0, 0, GC_W * g_scale, GC_H * g_scale,
             g_memDC, 0, 0, GC_W, GC_H, SRCCOPY);
}
static int isModifier(int vk)
{
  return vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU ||
         vk == VK_TAB || vk == VK_CAPITAL || vk == VK_LWIN || vk == VK_RWIN;
}
static void onKeyDown(int vk, int repeated)
{
  if (vk == VK_LEFT || vk == 'A') g_keysLeft = 1;
  if (vk == VK_RIGHT || vk == 'D') g_keysRight = 1;
  if (repeated) return;
  if ((g_state == ST_TITLE || g_state == ST_OVER) &&
      !isModifier(vk) && vk != 'M' && vk != 'P' && vk != VK_ESCAPE) {
    start();
    return;
  }
  if (vk == 'M') {
    g_muted = !g_muted;
    if (!g_muted) beepAsync(660, 60);
    return;
  }
  if (vk == 'P' || vk == VK_ESCAPE) {
    if (g_state == ST_PLAY) g_state = ST_PAUSE;
    else if (g_state == ST_PAUSE) g_state = ST_PLAY;
    return;
  }
  if ((vk == VK_SPACE || vk == VK_RETURN || vk == 'R') && g_state == ST_PAUSE)
    g_state = ST_PLAY;
}
static void onKeyUp(int vk)
{
  if (vk == VK_LEFT || vk == 'A') g_keysLeft = 0;
  if (vk == VK_RIGHT || vk == 'D') g_keysRight = 0;
}
static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
  switch (msg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      drawToWindow(dc);
      EndPaint(h, &ps);
      return 0;
    }
    case WM_KEYDOWN:
      onKeyDown((int)w, (l & 0x40000000) ? 1 : 0);
      return 0;
    case WM_KEYUP:
      onKeyUp((int)w);
      return 0;
    case WM_LBUTTONDOWN: {
      int lx = (int)(short)LOWORD(l) / g_scale;
      if (g_state == ST_TITLE || g_state == ST_OVER) { start(); return 0; }
      if (g_state == ST_PAUSE) { g_state = ST_PLAY; return 0; }
      g_ptrLeft = lx < GC_W / 2;
      g_ptrRight = !g_ptrLeft;
      SetCapture(h);
      return 0;
    }
    case WM_LBUTTONUP:
      g_ptrLeft = g_ptrRight = 0;
      ReleaseCapture();
      return 0;
    case WM_KILLFOCUS:
      g_keysLeft = g_keysRight = g_ptrLeft = g_ptrRight = 0;
      if (g_state == ST_PLAY) g_state = ST_PAUSE;
      return 0;
  }
  return DefWindowProcA(h, msg, w, l);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
  WNDCLASSA wc;
  BITMAPINFO bi;
  RECT r;
  MSG msg;
  HDC screen;
  LARGE_INTEGER freq, last, now;
  double dt;
  int n, screenH;
  (void)hPrev; (void)lpCmd;

  initFont();
  srand((unsigned int)GetTickCount());
  loadBest();
  reset();
  g_state = ST_TITLE;

  memset(&wc, 0, sizeof(wc));
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = wndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
  wc.lpszClassName = "PixelRacerC";
  if (!RegisterClassA(&wc)) return 1;

  screenH = GetSystemMetrics(SM_CYSCREEN) - 120;
  g_scale = screenH / GC_H;
  if (g_scale < 2) g_scale = 2;
  if (g_scale > 4) g_scale = 4;

  SetRect(&r, 0, 0, GC_W * g_scale, GC_H * g_scale);
  AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
  g_hwnd = CreateWindowA("PixelRacerC", "Pixel Racer - C / Win32 GDI",
                         WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         r.right - r.left, r.bottom - r.top,
                         NULL, NULL, hInst, NULL);
  if (!g_hwnd) return 1;

  memset(&bi, 0, sizeof(bi));
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = GC_W;
  bi.bmiHeader.biHeight = -GC_H;   /* top-downg_bits[0]  */
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  screen = GetDC(NULL);
  g_dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void**)&g_bits, NULL, 0);
  ReleaseDC(NULL, screen);
  if (!g_dib || !g_bits) return 1;
  g_memDC = CreateCompatibleDC(NULL);
  SelectObject(g_memDC, g_dib);

  ShowWindow(g_hwnd, nShow);
  UpdateWindow(g_hwnd);

  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&last);
  for (;;) {
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) goto cleanup;
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
    QueryPerformanceCounter(&now);
    dt = (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart;
    last = now;
    if (dt > 0.25) dt = 0.25;
    g_acc += dt;
    n = 0;
    while (g_acc >= GC_FIXED_STEP && n < GC_MAX_STEPS_PER_FRAME) {
      update(GC_FIXED_STEP);
      g_acc -= GC_FIXED_STEP;
      n++;
    }
    if (n == GC_MAX_STEPS_PER_FRAME) g_acc = 0;
    render();
    {
      HDC dc = GetDC(g_hwnd);
      drawToWindow(dc);
      ReleaseDC(g_hwnd, dc);
    }
    Sleep(1);
  }

cleanup:
  if (g_memDC) DeleteDC(g_memDC);
  if (g_dib) DeleteObject(g_dib);
  return 0;
}
