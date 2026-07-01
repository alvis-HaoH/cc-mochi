/*
 * ╔══════════════════════════════════════════════════════════════╗
 *   CC MOCHI — ESP32-C3 Super Mini + ST7789 1.54" 240×240
 *
 *   Wiring:
 *     SDA → GPIO 10  (hardware SPI MOSI)
 *     SCL → GPIO 8   (hardware SPI SCK)
 *     RST → GPIO 2
 *     DC  → GPIO 1
 *     CS  → GPIO 4
 *     BL  → GPIO 3
 *     VCC → 3V3
 *     GND → GND
 *
 *   WiFi: "cc-mochi"  pw: ccmochi1234  → http://192.168.4.1
 * ╚══════════════════════════════════════════════════════════════╝
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <math.h>
#include <WiFi.h>
#include <WebServer.h>

// ── Pins ──────────────────────────────────────────────────────
#define TFT_CS  4
#define TFT_DC  1
#define TFT_RST 2
#define TFT_BLK 3

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ── WiFi ──────────────────────────────────────────────────────
const char* AP_SSID = "cc-mochi";
const char* AP_PASS = "ccmochi1234";
WebServer server(80);

// ── Display ───────────────────────────────────────────────────
#define DISP_W 240
#define DISP_H 240

// ── Eye constants (shared by both eye views) ──────────────────
#define EYE_W   30
#define EYE_H   60
#define EYE_GAP 120
#define EYE_OX  0     // horizontal offset
#define EYE_OY  40    // vertical offset upward (subtracted from centre)

// ── Colours ───────────────────────────────────────────────────
uint16_t C_ORANGE, C_DARKBG, C_MUTED, C_GREEN, C_BLUE, C_YELLOW, C_RED, C_CARD, C_SOFT;
#define C_WHITE ST77XX_WHITE
#define C_BLACK ST77XX_BLACK

// ── State ─────────────────────────────────────────────────────
#define VIEW_EYES_NORMAL 0
#define VIEW_EYES_SQUISH 1
#define VIEW_CODE        2
#define VIEW_DRAW        3
#define VIEW_CC_STATUS   4
#define VIEW_CC_USAGE    5

uint8_t  currentView  = VIEW_EYES_NORMAL;
bool     busy         = false;
bool     backlightOn  = true;
uint8_t  animSpeed    = 1;   // 1=slow(default) 2=normal 3=fast

uint16_t animBgColor  = 0;   // background for eye/logo animations
uint16_t drawBgColor  = 0;   // background for canvas

String   ccSource      = "cc";
String   ccState       = "sleeping";
String   ccLabel       = "idle";
uint8_t  ccActive      = 0;
String   usageProvider = "codex";
int      usagePrimary  = 0;
int      usageSecondary = 0;
String   usageReset    = "";
String   usageBadge    = "";
bool     usageUnavailable = false;
bool     usageStale       = false;
String   serialLine;

struct AgentTheme {
  uint16_t bg;
  uint16_t face;
  uint16_t accent;
  uint16_t soft;
  uint16_t card;
  bool codex;
  bool claude;
};

// ── Terminal ──────────────────────────────────────────────────
#define TERM_COLS      15
#define TERM_ROWS       8
#define TERM_CHAR_W    12
#define TERM_CHAR_H    20
#define TERM_PAD_X      8
#define TERM_PAD_Y     18

bool    termMode    = false;
String  termLines[TERM_ROWS];
uint8_t termRow     = 0;
uint8_t termCol     = 0;

// ── Logo data ─────────────────────────────────────────────────
#define LOGO_CX 120
#define LOGO_CY 105

#define LOGO_TRI_COUNT 162
static const int16_t LOGO_TRIS[][6] PROGMEM = {
  {120,105,65,134,100,114},{120,105,100,114,101,113},{120,105,101,113,100,112},
  {120,105,100,112,99,112},{120,105,99,112,93,111},{120,105,93,111,73,111},
  {120,105,73,111,55,110},{120,105,55,110,38,109},{120,105,38,109,34,108},
  {120,105,34,108,30,103},{120,105,30,103,30,100},{120,105,30,100,34,98},
  {120,105,34,98,39,98},{120,105,39,98,50,99},{120,105,50,99,67,100},
  {120,105,67,100,80,101},{120,105,80,101,98,103},{120,105,98,103,101,103},
  {120,105,101,103,101,102},{120,105,101,102,100,101},{120,105,100,101,100,100},
  {120,105,100,100,82,88},{120,105,82,88,63,76},{120,105,63,76,53,69},
  {120,105,53,69,48,65},{120,105,48,65,45,61},{120,105,45,61,44,54},
  {120,105,44,54,49,49},{120,105,49,49,55,49},{120,105,55,49,57,49},
  {120,105,57,49,64,55},{120,105,64,55,78,66},{120,105,78,66,96,79},
  {120,105,96,79,99,81},{120,105,99,81,100,81},{120,105,100,81,100,80},
  {120,105,100,80,99,78},{120,105,99,78,89,60},{120,105,89,60,78,41},
  {120,105,78,41,73,34},{120,105,73,34,72,29},{120,105,72,29,72,28},
  {120,105,72,28,72,27},{120,105,72,27,71,26},{120,105,71,26,71,25},
  {120,105,71,25,71,24},{120,105,71,24,77,16},{120,105,77,16,80,15},
  {120,105,80,15,87,16},{120,105,87,16,91,19},{120,105,91,19,95,29},
  {120,105,95,29,103,46},{120,105,103,46,114,68},{120,105,114,68,118,75},
  {120,105,118,75,119,81},{120,105,119,81,120,83},{120,105,120,83,121,83},
  {120,105,121,83,121,82},{120,105,121,82,122,69},{120,105,122,69,124,54},
  {120,105,124,54,126,34},{120,105,126,34,126,28},{120,105,126,28,129,21},
  {120,105,129,21,135,18},{120,105,135,18,139,20},{120,105,139,20,143,25},
  {120,105,143,25,142,28},{120,105,142,28,140,42},{120,105,140,42,136,64},
  {120,105,136,64,133,78},{120,105,133,78,135,78},{120,105,135,78,136,76},
  {120,105,136,76,144,67},{120,105,144,67,156,51},{120,105,156,51,162,45},
  {120,105,162,45,168,38},{120,105,168,38,172,35},{120,105,172,35,180,35},
  {120,105,180,35,185,43},{120,105,185,43,183,52},{120,105,183,52,175,62},
  {120,105,175,62,168,71},{120,105,168,71,159,83},{120,105,159,83,153,94},
  {120,105,153,94,154,94},{120,105,154,94,155,94},{120,105,155,94,176,90},
  {120,105,176,90,188,88},{120,105,188,88,201,85},{120,105,201,85,208,88},
  {120,105,208,88,208,91},{120,105,208,91,206,97},{120,105,206,97,191,101},
  {120,105,191,101,174,104},{120,105,174,104,148,110},{120,105,148,110,148,111},
  {120,105,148,111,148,111},{120,105,148,111,160,112},{120,105,160,112,165,112},
  {120,105,165,112,177,112},{120,105,177,112,200,114},{120,105,200,114,205,118},
  {120,105,205,118,209,123},{120,105,209,123,208,126},{120,105,208,126,199,131},
  {120,105,199,131,187,128},{120,105,187,128,159,121},{120,105,159,121,149,119},
  {120,105,149,119,147,119},{120,105,147,119,147,120},{120,105,147,120,156,128},
  {120,105,156,128,170,141},{120,105,170,141,189,158},{120,105,189,158,190,163},
  {120,105,190,163,188,166},{120,105,188,166,185,166},{120,105,185,166,169,153},
  {120,105,169,153,162,148},{120,105,162,148,148,136},{120,105,148,136,147,136},
  {120,105,147,136,147,137},{120,105,147,137,150,142},{120,105,150,142,168,168},
  {120,105,168,168,169,176},{120,105,169,176,168,179},{120,105,168,179,163,180},
  {120,105,163,180,158,179},{120,105,158,179,148,165},{120,105,148,165,137,149},
  {120,105,137,149,129,134},{120,105,129,134,128,135},{120,105,128,135,123,189},
  {120,105,123,189,120,192},{120,105,120,192,115,194},{120,105,115,194,110,191},
  {120,105,110,191,108,185},{120,105,108,185,110,174},{120,105,110,174,113,160},
  {120,105,113,160,116,148},{120,105,116,148,118,134},{120,105,118,134,119,129},
  {120,105,119,129,119,129},{120,105,119,129,118,129},{120,105,118,129,107,144},
  {120,105,107,144,91,166},{120,105,91,166,78,180},{120,105,78,180,75,181},
  {120,105,75,181,70,178},{120,105,70,178,70,173},{120,105,70,173,73,169},
  {120,105,73,169,91,146},{120,105,91,146,102,132},{120,105,102,132,109,124},
  {120,105,109,124,109,123},{120,105,109,123,108,123},{120,105,108,123,61,153},
  {120,105,61,153,52,155},{120,105,52,155,49,151},{120,105,49,151,49,146},
  {120,105,49,146,51,144},{120,105,51,144,65,134},{120,105,65,134,65,134},
};

#define LOGO_SEG_COUNT 162
static const int16_t LOGO_SEGS[][4] PROGMEM = {
  {65,134,100,114},{100,114,101,113},{101,113,100,112},{100,112,99,112},
  {99,112,93,111},{93,111,73,111},{73,111,55,110},{55,110,38,109},
  {38,109,34,108},{34,108,30,103},{30,103,30,100},{30,100,34,98},
  {34,98,39,98},{39,98,50,99},{50,99,67,100},{67,100,80,101},
  {80,101,98,103},{98,103,101,103},{101,103,101,102},{101,102,100,101},
  {100,101,100,100},{100,100,82,88},{82,88,63,76},{63,76,53,69},
  {53,69,48,65},{48,65,45,61},{45,61,44,54},{44,54,49,49},
  {49,49,55,49},{55,49,57,49},{57,49,64,55},{64,55,78,66},
  {78,66,96,79},{96,79,99,81},{99,81,100,81},{100,81,100,80},
  {100,80,99,78},{99,78,89,60},{89,60,78,41},{78,41,73,34},
  {73,34,72,29},{72,29,72,28},{72,28,72,27},{72,27,71,26},
  {71,26,71,25},{71,25,71,24},{71,24,77,16},{77,16,80,15},
  {80,15,87,16},{87,16,91,19},{91,19,95,29},{95,29,103,46},
  {103,46,114,68},{114,68,118,75},{118,75,119,81},{119,81,120,83},
  {120,83,121,83},{121,83,121,82},{121,82,122,69},{122,69,124,54},
  {124,54,126,34},{126,34,126,28},{126,28,129,21},{129,21,135,18},
  {135,18,139,20},{139,20,143,25},{143,25,142,28},{142,28,140,42},
  {140,42,136,64},{136,64,133,78},{133,78,135,78},{135,78,136,76},
  {136,76,144,67},{144,67,156,51},{156,51,162,45},{162,45,168,38},
  {168,38,172,35},{172,35,180,35},{180,35,185,43},{185,43,183,52},
  {183,52,175,62},{175,62,168,71},{168,71,159,83},{159,83,153,94},
  {153,94,154,94},{154,94,155,94},{155,94,176,90},{176,90,188,88},
  {188,88,201,85},{201,85,208,88},{208,88,208,91},{208,91,206,97},
  {206,97,191,101},{191,101,174,104},{174,104,148,110},{148,110,148,111},
  {148,111,148,111},{148,111,160,112},{160,112,165,112},{165,112,177,112},
  {177,112,200,114},{200,114,205,118},{205,118,209,123},{209,123,208,126},
  {208,126,199,131},{199,131,187,128},{187,128,159,121},{159,121,149,119},
  {149,119,147,119},{147,119,147,120},{147,120,156,128},{156,128,170,141},
  {170,141,189,158},{189,158,190,163},{190,163,188,166},{188,166,185,166},
  {185,166,169,153},{169,153,162,148},{162,148,148,136},{148,136,147,136},
  {147,136,147,137},{147,137,150,142},{150,142,168,168},{168,168,169,176},
  {169,176,168,179},{168,179,163,180},{163,180,158,179},{158,179,148,165},
  {148,165,137,149},{137,149,129,134},{129,134,128,135},{128,135,123,189},
  {123,189,120,192},{120,192,115,194},{115,194,110,191},{110,191,108,185},
  {108,185,110,174},{110,174,113,160},{113,160,116,148},{116,148,118,134},
  {118,134,119,129},{119,129,119,129},{119,129,118,129},{118,129,107,144},
  {107,144,91,166},{91,166,78,180},{78,180,75,181},{75,181,70,178},
  {70,178,70,173},{70,173,73,169},{73,169,91,146},{91,146,102,132},
  {102,132,109,124},{109,124,109,123},{109,123,108,123},{108,123,61,153},
  {61,153,52,155},{52,155,49,151},{49,151,49,146},{49,146,51,144},
  {51,144,65,134},{65,134,65,134},
};

// ═════════════════════════════════════════════════════════════
//  HELPERS
// ═════════════════════════════════════════════════════════════

int speedMs(int ms) {
  if (animSpeed == 3) return ms / 2;
  if (animSpeed == 1) return ms * 2;
  return ms;
}

uint16_t hexToRgb565(String hex) {
  hex.replace("#", "");
  if (hex.length() != 6) return C_WHITE;
  long v = strtol(hex.c_str(), nullptr, 16);
  return tft.color565((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

void setBacklight(bool on) {
  backlightOn = on;
  digitalWrite(TFT_BLK, on ? HIGH : LOW);
}

void initColours() {
  // C_ORANGE = tft.color565(170, 72, 28);
  C_ORANGE = tft.color565(218, 17, 0);
  C_DARKBG = tft.color565(10,  12,  16);
  C_MUTED  = tft.color565(90,  88,  86);
  C_GREEN  = tft.color565(80, 220, 130);
  C_BLUE   = tft.color565(72, 150, 230);
  C_YELLOW = tft.color565(245, 185, 70);
  C_RED    = tft.color565(230, 70, 64);
  C_CARD   = tft.color565(28, 30, 38);
  C_SOFT   = tft.color565(238, 224, 204);
  animBgColor = C_ORANGE;
  drawBgColor = C_ORANGE;
}

String jsonStringValue(const String& json, const char* key, const String& fallback = "") {
  String needle = "\""; needle += key; needle += "\"";
  int searchFrom = 0;
  int pos = -1;
  while (true) {
    pos = json.indexOf(needle, searchFrom);
    if (pos < 0) return fallback;
    int after = pos + needle.length();
    while (after < (int)json.length() && (json[after] == ' ' || json[after] == '\t')) after++;
    if (after < (int)json.length() && json[after] == ':') {
      pos = after;
      break;
    }
    searchFrom = pos + needle.length();
  }
  pos++;
  while (pos < (int)json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  if (pos >= (int)json.length()) return fallback;
  if (json[pos] != '"') {
    int end = pos;
    while (end < (int)json.length() && json[end] != ',' && json[end] != '}') end++;
    return json.substring(pos, end);
  }
  pos++;
  String out = "";
  while (pos < (int)json.length() && json[pos] != '"') {
    if (json[pos] == '\\' && pos + 1 < (int)json.length()) pos++;
    out += json[pos++];
  }
  return out.length() ? out : fallback;
}

int jsonIntValue(const String& json, const char* key, int fallback = 0) {
  String v = jsonStringValue(json, key, "");
  if (!v.length()) return fallback;
  return v.toInt();
}

bool jsonBoolValue(const String& json, const char* key, bool fallback = false) {
  String v = jsonStringValue(json, key, "");
  v.toLowerCase();
  if (v == "true" || v == "1") return true;
  if (v == "false" || v == "0") return false;
  return fallback;
}

String shortText(String text, uint8_t maxLen) {
  text.replace("\n", " ");
  text.replace("\r", " ");
  text.trim();
  if (text.length() > maxLen) text = text.substring(0, maxLen);
  return text;
}

// ═════════════════════════════════════════════════════════════
//  LOGO
// ═════════════════════════════════════════════════════════════

void drawLogoFilled(uint16_t bg, uint16_t fg) {
  tft.fillScreen(bg);
  for (uint16_t i = 0; i < LOGO_TRI_COUNT; i++) {
    tft.fillTriangle(
      pgm_read_word(&LOGO_TRIS[i][0]), pgm_read_word(&LOGO_TRIS[i][1]),
      pgm_read_word(&LOGO_TRIS[i][2]), pgm_read_word(&LOGO_TRIS[i][3]),
      pgm_read_word(&LOGO_TRIS[i][4]), pgm_read_word(&LOGO_TRIS[i][5]),
      fg);
  }
  tft.setTextColor(fg); tft.setTextSize(2);
  tft.setCursor(LOGO_CX - 42, 210); tft.print("cc-mochi");
  tft.setCursor(LOGO_CX - 41, 210); tft.print("cc-mochi");
}

void drawMochiPixel(int16_t cx, int16_t cy, int16_t scale, uint16_t body, uint16_t hi, uint16_t eye, uint16_t shadow, bool blink, int8_t wave, int8_t squash) {
  int16_t u = scale;
  int16_t x = cx - 9 * u;
  int16_t y = cy - (6 * u) + squash;

  tft.fillRect(cx - 9 * u, cy + 7 * u, 18 * u, 2 * u, shadow);

  tft.fillRect(x + 3 * u, y - u, 12 * u, u, shadow);
  tft.fillRect(x + 2 * u, y, 14 * u, 10 * u, shadow);
  tft.fillRect(x + u, y + 2 * u, 16 * u, 6 * u, shadow);
  tft.fillRect(x - u, y + (4 - wave) * u, 4 * u, 2 * u, shadow);
  tft.fillRect(x - 2 * u, y + (3 - wave) * u, 2 * u, 4 * u, shadow);
  tft.fillRect(x + 16 * u, y + (4 + wave) * u, 4 * u, 2 * u, shadow);
  tft.fillRect(x + 19 * u, y + (3 + wave) * u, 2 * u, 4 * u, shadow);
  tft.fillRect(x + 3 * u, y + 10 * u, 12 * u, 4 * u, shadow);

  tft.fillRect(x + 3 * u, y, 12 * u, 2 * u, body);
  tft.fillRect(x + 2 * u, y + 2 * u, 14 * u, 6 * u, body);
  tft.fillRect(x + 3 * u, y + 8 * u, 12 * u, 2 * u, body);

  tft.fillRect(x + 4 * u, y + u, 10 * u, u, hi);

  int16_t ly = y + (4 - wave) * u;
  int16_t ry = y + (4 + wave) * u;
  tft.fillRect(x, ly, 3 * u, 2 * u, body);
  tft.fillRect(x - 2 * u, ly - u, 2 * u, 3 * u, body);
  tft.fillRect(x + 16 * u, ry, 3 * u, 2 * u, body);
  tft.fillRect(x + 19 * u, ry - u, 2 * u, 3 * u, body);

  tft.fillRect(x + 3 * u, y + 10 * u, 2 * u, 3 * u, body);
  tft.fillRect(x + 6 * u, y + 10 * u, 2 * u, 3 * u, body);
  tft.fillRect(x + 10 * u, y + 10 * u, 2 * u, 3 * u, body);
  tft.fillRect(x + 13 * u, y + 10 * u, 2 * u, 3 * u, body);

  if (blink) {
    tft.fillRect(x + 6 * u, y + 5 * u, 2 * u, u, eye);
    tft.fillRect(x + 11 * u, y + 5 * u, 2 * u, u, eye);
  } else {
    tft.fillRect(x + 6 * u, y + 4 * u, 2 * u, 2 * u, eye);
    tft.fillRect(x + 11 * u, y + 4 * u, 2 * u, 2 * u, eye);
  }
}

void drawMochiSleepingSprite(int16_t cx, int16_t cy, int16_t scale, uint16_t body, uint16_t hi, uint16_t eye, uint16_t shadow) {
  int16_t u = scale;
  int16_t x = cx - 10 * u;
  int16_t y = cy - 3 * u;
  tft.fillRect(cx - 10 * u, cy + 4 * u, 20 * u, 2 * u, shadow);
  tft.fillRect(x + 2 * u, y - u, 16 * u, u, shadow);
  tft.fillRect(x + u, y, 18 * u, 6 * u, shadow);
  tft.fillRect(x - u, y + 3 * u, 3 * u, 2 * u, shadow);
  tft.fillRect(x + 18 * u, y + 3 * u, 3 * u, 2 * u, shadow);
  tft.fillRect(x + 4 * u, y + 5 * u, 14 * u, 3 * u, shadow);
  tft.fillRect(x + 2 * u, y, 16 * u, 5 * u, body);
  tft.fillRect(x + 3 * u, y, 14 * u, u, hi);
  tft.fillRect(x, y + 3 * u, 2 * u, 2 * u, body);
  tft.fillRect(x + 18 * u, y + 3 * u, 2 * u, 2 * u, body);
  tft.fillRect(x + 4 * u, y + 5 * u, 2 * u, 2 * u, body);
  tft.fillRect(x + 8 * u, y + 5 * u, 2 * u, 2 * u, body);
  tft.fillRect(x + 12 * u, y + 5 * u, 2 * u, 2 * u, body);
  tft.fillRect(x + 16 * u, y + 5 * u, 2 * u, 2 * u, body);
  tft.fillRect(x + 6 * u, y + 3 * u, 2 * u, u, eye);
  tft.fillRect(x + 12 * u, y + 3 * u, 2 * u, u, eye);
}

void drawMochiBootBubble(int16_t x, int16_t y, uint16_t fill, uint16_t dot, uint8_t step) {
  if (step == 0) return;
  tft.fillCircle(x - 12, y + 34, 4, fill);
  if (step < 2) return;
  tft.fillCircle(x - 2, y + 22, 6, fill);
  if (step < 3) return;
  tft.fillRoundRect(x, y, 72, 42, 9, fill);
  tft.fillCircle(x + 12, y + 8, 10, fill);
  tft.fillCircle(x + 58, y + 9, 10, fill);
  tft.fillRect(x + 12, y + 2, 48, 38, fill);
  tft.fillRect(x + 18, y + 18, 7, 7, dot);
  tft.fillRect(x + 32, y + 18, 7, 7, dot);
  tft.fillRect(x + 46, y + 18, 7, 7, dot);
}

void drawBootTerminalBase(int16_t ty) {
  uint16_t term = tft.color565(30, 30, 46);
  uint16_t termTop = tft.color565(40, 40, 58);
  uint16_t termEdge = tft.color565(18, 18, 28);
  uint16_t red = tft.color565(245, 88, 78);
  uint16_t yellow = tft.color565(245, 190, 78);
  uint16_t green = tft.color565(55, 205, 100);

  tft.fillRect(76, ty, 88, 72, termEdge);
  tft.fillRect(78, ty + 2, 84, 68, term);
  tft.fillRect(78, ty + 2, 84, 7, termTop);
  tft.fillRect(82, ty + 4, 3, 3, red);
  tft.fillRect(88, ty + 4, 3, 3, yellow);
  tft.fillRect(94, ty + 4, 3, 3, green);
  tft.fillRect(101, ty + 5, 54, 1, tft.color565(58, 58, 80));
}

void drawBootTerminalCode(int16_t ty, uint8_t frame) {
  uint16_t term = tft.color565(30, 30, 46);
  uint16_t green = tft.color565(55, 205, 100);
  uint16_t yellow = tft.color565(245, 190, 78);
  uint16_t blue = tft.color565(86, 150, 235);
  uint16_t gray = tft.color565(112, 118, 138);

  tft.fillRect(84, ty + 16, 58, 30, term);
  uint8_t codeW = frame < 8 ? 6 + frame * 4 : 38;
  tft.fillRect(86, ty + 22, codeW, 4, green);
  if (frame >= 8) {
    tft.fillRect(86, ty + 30, 24, 3, blue);
    tft.fillRect(114, ty + 30, 18, 3, gray);
  }
  if (frame >= 11) {
    tft.fillRect(86, ty + 38, 12, 3, yellow);
    tft.fillRect(102, ty + 38, 34, 3, green);
  }
}

void drawBootMochiBody(int16_t y, bool openEyes) {
  uint16_t crab = tft.color565(232, 124, 82);
  uint16_t crabHi = tft.color565(255, 158, 106);
  uint16_t eye = tft.color565(8, 10, 12);

  int16_t x = 51;
  tft.fillRect(x + 30, y - 7, 78, 38, crab);
  tft.fillRect(x + 30, y - 9, 78, 4, crabHi);
  tft.fillRect(x + 18, y + 13, 16, 13, crab);
  tft.fillRect(x + 12, y + 10, 14, 8, crab);
  tft.fillRect(x + 106, y + 13, 16, 13, crab);
  tft.fillRect(x + 114, y + 10, 14, 8, crab);
  if (openEyes) {
    tft.fillRect(x + 47, y + 15, 8, 8, eye);
    tft.fillRect(x + 88, y + 15, 8, 8, eye);
  } else {
    tft.fillRect(x + 47, y + 18, 8, 3, eye);
    tft.fillRect(x + 88, y + 18, 8, 3, eye);
  }
}

void drawBootKeyboard(int16_t ky, uint8_t frame) {
  uint16_t green = tft.color565(55, 205, 100);
  uint16_t kb = tft.color565(82, 118, 132);
  uint16_t kbDark = tft.color565(42, 64, 72);
  uint16_t kbHi = tft.color565(124, 158, 168);

  tft.fillRect(68, ky, 104, 22, kbDark);
  tft.fillRect(68, ky + 2, 104, 16, kb);
  for (uint8_t row = 0; row < 3; row++) {
    for (uint8_t col = 0; col < 11; col++) {
      int16_t kx = 72 + col * 9 + (row == 1 ? 3 : 0);
      int16_t yy = ky + 4 + row * 5;
      uint16_t c = ((frame + row + col) % 7 == 0) ? green : kbHi;
      tft.fillRect(kx, yy, 6, 3, c);
    }
  }
}

void drawMochiBootTypingFrame(uint8_t frame) {
  tft.fillScreen(C_BLACK);
  int16_t ty = 44 + (frame < 5 ? (5 - frame) * 4 : 0);
  drawBootTerminalBase(ty);
  drawBootTerminalCode(ty, frame);

  int8_t bob = (frame % 4 == 1 || frame % 4 == 2) ? -2 : 0;
  drawBootMochiBody(138 + bob, true);
  drawBootKeyboard(164, frame);
}

void updateMochiBootTypingFrame(uint8_t frame) {
  int16_t ty = 44;
  drawBootTerminalCode(ty, frame);

  tft.fillRect(56, 124, 128, 48, C_BLACK);
  int8_t bob = (frame % 4 == 1 || frame % 4 == 2) ? -2 : 0;
  drawBootMochiBody(138 + bob, true);
  drawBootKeyboard(164, frame);
}

void drawMochiBootSleepFrame(uint8_t frame) {
  uint16_t bg = C_BLACK;
  uint16_t crab = tft.color565(232, 124, 82);
  uint16_t crabHi = tft.color565(255, 158, 106);
  uint16_t eye = tft.color565(8, 10, 12);
  uint16_t shadow = tft.color565(18, 18, 18);
  uint16_t zCol = tft.color565(148, 178, 188);

  tft.fillScreen(bg);
  int16_t y = 136 + (frame == 1 ? 1 : 0);
  tft.fillRect(57, y + 42, 126, 5, shadow);
  tft.fillRect(70, y + 8, 100, 34, crab);
  tft.fillRect(78, y + 4, 84, 6, crabHi);
  tft.fillRect(62, y + 24, 16, 12, crab);
  tft.fillRect(162, y + 24, 16, 12, crab);
  for (uint8_t i = 0; i < 6; i++) tft.fillRect(78 + i * 15, y + 38, 8, 13, crab);
  tft.fillRect(92, y + 25, 28, 4, eye);
  tft.fillRect(132, y + 25, 28, 4, eye);

  if (frame < 5) {
    int16_t zy = 92 - frame * 5;
    tft.fillRect(123, zy, 16, 4, zCol);
    tft.fillRect(135, zy + 4, 4, 4, zCol);
    tft.fillRect(131, zy + 8, 4, 4, zCol);
    tft.fillRect(123, zy + 12, 16, 4, zCol);
  }
}

void drawMochiBootWakeToTypingFrame(uint8_t frame) {
  uint16_t bg = C_BLACK;
  uint16_t term = tft.color565(30, 30, 46);
  uint16_t termTop = tft.color565(40, 40, 58);
  uint16_t termEdge = tft.color565(18, 18, 28);
  uint16_t green = tft.color565(55, 205, 100);
  uint16_t red = tft.color565(245, 88, 78);
  uint16_t yellow = tft.color565(245, 190, 78);
  uint16_t blue = tft.color565(86, 150, 235);
  uint16_t gray = tft.color565(112, 118, 138);
  uint16_t crab = tft.color565(232, 124, 82);
  uint16_t crabHi = tft.color565(255, 158, 106);
  uint16_t eye = tft.color565(8, 10, 12);
  uint16_t kb = tft.color565(82, 118, 132);
  uint16_t kbDark = tft.color565(42, 64, 72);
  uint16_t kbHi = tft.color565(124, 158, 168);

  tft.fillScreen(bg);

  int16_t termY = 44 - min((int)frame, 4) * 2;
  tft.fillRect(76, termY, 88, 72, termEdge);
  tft.fillRect(78, termY + 2, 84, 68, term);
  tft.fillRect(78, termY + 2, 84, 7, termTop);
  tft.fillRect(82, termY + 4, 3, 3, red);
  tft.fillRect(88, termY + 4, 3, 3, yellow);
  tft.fillRect(94, termY + 4, 3, 3, green);
  tft.fillRect(101, termY + 5, 54, 1, tft.color565(58, 58, 80));
  if (frame > 4) tft.fillRect(86, termY + 22, min(38, (int)(frame - 4) * 7), 4, green);
  if (frame > 5) tft.fillRect(86, termY + 30, min(28, (int)(frame - 5) * 7), 3, blue);
  if (frame > 6) {
    tft.fillRect(118, termY + 30, 16, 3, gray);
    tft.fillRect(86, termY + 38, 12, 3, yellow);
  }

  int16_t baseY = 184 - min((int)frame, 6) * 8;
  int16_t x = 51;
  tft.fillRect(x + 30, baseY - 7, 78, 38, crab);
  tft.fillRect(x + 30, baseY - 9, 78, 4, crabHi);
  tft.fillRect(x + 18, baseY + 13, 16, 13, crab);
  tft.fillRect(x + 12, baseY + 10, 14, 8, crab);
  tft.fillRect(x + 106, baseY + 13, 16, 13, crab);
  tft.fillRect(x + 114, baseY + 10, 14, 8, crab);
  if (frame < 3) {
    tft.fillRect(x + 47, baseY + 18, 8, 3, eye);
    tft.fillRect(x + 88, baseY + 18, 8, 3, eye);
  } else {
    tft.fillRect(x + 47, baseY + 15, 8, 8, eye);
    tft.fillRect(x + 88, baseY + 15, 8, 8, eye);
  }

  int16_t ky = 204 - min((int)frame, 7) * 6;
  tft.fillRect(68, ky, 104, 22, kbDark);
  tft.fillRect(68, ky + 2, 104, 16, kb);
  for (uint8_t row = 0; row < 3; row++) {
    for (uint8_t col = 0; col < 11; col++) {
      int16_t kx = 72 + col * 9 + (row == 1 ? 3 : 0);
      int16_t yy = ky + 4 + row * 5;
      tft.fillRect(kx, yy, 6, 3, kbHi);
    }
  }
}

// ═════════════════════════════════════════════════════════════
//  VIEWS
// ═════════════════════════════════════════════════════════════

// Eye helpers — shared constants via #define EYE_*
inline int16_t eyeLX(int16_t ox) {
  return (DISP_W - (EYE_W * 2 + EYE_GAP)) / 2 + EYE_OX + ox;
}
inline int16_t eyeRX(int16_t ox) { return eyeLX(ox) + EYE_W + EYE_GAP; }
inline int16_t eyeY()            { return (DISP_H - EYE_H) / 2 - EYE_OY; }
inline int16_t eyeCY()           { return eyeY() + EYE_H / 2; }

void drawNormalEyes(int16_t ox = 0, bool blink = false) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(ox), rx = eyeRX(ox), ey = eyeY();
  if (!blink) {
    tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
    tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  } else {
    tft.fillRect(lx, ey + EYE_H / 2 - 3, EYE_W, 6, C_BLACK);
    tft.fillRect(rx, ey + EYE_H / 2 - 3, EYE_W, 6, C_BLACK);
  }
}

void drawChevron(int16_t cx, int16_t cy, int16_t arm, int16_t reach,
                 uint8_t thk, bool rightFacing, uint16_t col) {
  for (int8_t t = -(int8_t)thk; t <= (int8_t)thk; t++) {
    if (rightFacing) {
      tft.drawLine(cx - reach/2, cy - arm + t, cx + reach/2, cy + t,      col);
      tft.drawLine(cx + reach/2, cy + t,       cx - reach/2, cy + arm + t, col);
    } else {
      tft.drawLine(cx + reach/2, cy - arm + t, cx - reach/2, cy + t,      col);
      tft.drawLine(cx - reach/2, cy + t,       cx + reach/2, cy + arm + t, col);
    }
  }
}

void drawSquishEyes(bool closed = false) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), cy = eyeCY();
  const int16_t arm   = EYE_H / 2;
  const int16_t reach = EYE_W / 2;
  const int16_t lcx   = lx + EYE_W / 2;
  const int16_t rcx   = rx + EYE_W / 2;
  if (!closed) {
    drawChevron(lcx, cy, arm, reach, 10, true,  C_BLACK);
    drawChevron(rcx, cy, arm, reach, 10, false, C_BLACK);
  } else {
    tft.fillRect(lx, cy - 5, EYE_W, 10, C_BLACK);
    tft.fillRect(rx, cy - 5, EYE_W, 10, C_BLACK);
  }
}

void drawCodeView() {
  termMode = false;
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0,          DISP_W, 4, C_ORANGE);
  tft.fillRect(0, DISP_H - 4, DISP_W, 4, C_ORANGE);
  tft.setTextColor(C_ORANGE); tft.setTextSize(4);
  tft.setCursor((DISP_W - 48) / 2, DISP_H / 2 - 52); tft.print("CC");
  tft.setTextColor(C_WHITE);  tft.setTextSize(4);
  tft.setCursor((DISP_W - 120) / 2,  DISP_H / 2 + 8);  tft.print("Mochi");
  tft.fillRect((DISP_W - 96) / 2, DISP_H / 2 + 52, 96, 3, C_ORANGE);
}

bool isCodexSource(String source) {
  source.toLowerCase();
  return source.indexOf("codex") >= 0;
}

bool isClaudeSource(String source) {
  source.toLowerCase();
  return source.indexOf("claude") >= 0;
}

uint16_t stateAccent(const String& state, const AgentTheme& theme) {
  if (state == "error" || state == "blocked" || state == "usage_critical") return C_RED;
  if (state == "permission" || state == "compact" || state == "usage_low") return C_YELLOW;
  if (state == "success") return C_GREEN;
  if (state == "shell" || state == "reading" || state == "writing") return C_BLUE;
  return theme.accent;
}

AgentTheme agentTheme(String source) {
  AgentTheme theme;
  theme.codex = isCodexSource(source);
  theme.claude = isClaudeSource(source);
  if (theme.codex) {
    theme.bg = C_BLACK;
    theme.face = C_GREEN;
    theme.accent = C_GREEN;
    theme.soft = tft.color565(120, 235, 165);
    theme.card = tft.color565(10, 16, 12);
  } else if (theme.claude) {
    theme.bg = C_ORANGE;
    theme.face = tft.color565(28, 24, 20);
    theme.accent = tft.color565(36, 26, 18);
    theme.soft = tft.color565(255, 222, 188);
    theme.card = C_ORANGE;
  } else {
    theme.bg = C_ORANGE;
    theme.face = C_BLACK;
    theme.accent = C_BLACK;
    theme.soft = C_SOFT;
    theme.card = C_CARD;
  }
  return theme;
}

uint16_t statusBgForState(const String& state, const AgentTheme& theme) {
  if (state == "sleeping" || state == "idle") {
    if (theme.codex) return tft.color565(4, 7, 5);
    if (theme.claude) return C_ORANGE;
    return tft.color565(52, 35, 24);
  }
  if (state == "error" || state == "usage_critical") {
    return theme.codex ? tft.color565(18, 4, 5) : C_ORANGE;
  }
  if (state == "blocked") {
    return theme.codex ? tft.color565(18, 14, 4) : C_ORANGE;
  }
  return theme.bg;
}

void drawThickLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t thk, uint16_t col) {
  for (int8_t i = -(int8_t)thk; i <= (int8_t)thk; i++) {
    tft.drawLine(x0 + i, y0, x1 + i, y1, col);
    tft.drawLine(x0, y0 + i, x1, y1 + i, col);
  }
}

static const uint8_t OPENAI_MARK_32[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x80, 0x00, 0x00, 0x7F, 0xC0, 0x00,
  0x00, 0xF0, 0xFF, 0x80, 0x01, 0xC0, 0xFF, 0xC0, 0x01, 0x83, 0xE0, 0xE0,
  0x03, 0x87, 0x80, 0x70, 0x07, 0x1E, 0x08, 0x30, 0x1F, 0x18, 0x3E, 0x38,
  0x1F, 0x18, 0xFF, 0x98, 0x3B, 0x19, 0xE3, 0xF8, 0x73, 0x1F, 0xE0, 0xF8,
  0x63, 0x1E, 0x78, 0x78, 0x63, 0x1C, 0x3C, 0x1C, 0x63, 0x18, 0x1F, 0x0C,
  0x63, 0x98, 0x1F, 0xCE, 0x73, 0xF8, 0x19, 0xC6, 0x30, 0xF8, 0x18, 0xC6,
  0x38, 0x7C, 0x38, 0xC6, 0x1E, 0x1E, 0x78, 0xC6, 0x1F, 0x07, 0xF8, 0xCE,
  0x1F, 0xC7, 0x98, 0xDC, 0x19, 0xFF, 0x18, 0xF8, 0x1C, 0x7C, 0x18, 0xF0,
  0x0C, 0x10, 0x78, 0xE0, 0x0E, 0x01, 0xE1, 0x80, 0x07, 0x07, 0xC1, 0x80,
  0x03, 0xFF, 0x03, 0x80, 0x01, 0xFF, 0x0F, 0x00, 0x00, 0x03, 0xFE, 0x00,
  0x00, 0x01, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t OPENAI_MARK_44[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0x00, 0x00, 0x00,
  0x00, 0x0F, 0xFF, 0xDE, 0x00, 0x00, 0x00, 0x1F, 0x03, 0xFF, 0xC0, 0x00,
  0x00, 0x1C, 0x03, 0xFF, 0xE0, 0x00, 0x00, 0x38, 0x07, 0xE1, 0xF0, 0x00,
  0x00, 0x38, 0x1F, 0x00, 0x78, 0x00, 0x00, 0x70, 0x7E, 0x00, 0x3C, 0x00,
  0x00, 0xF0, 0xF8, 0x10, 0x1C, 0x00, 0x03, 0xF1, 0xE0, 0x3C, 0x0E, 0x00,
  0x07, 0xF1, 0xC0, 0xFF, 0x0E, 0x00, 0x0F, 0x71, 0xC3, 0xFF, 0xCE, 0x00,
  0x1E, 0x71, 0xCF, 0xC3, 0xEE, 0x00, 0x1C, 0x71, 0xDF, 0x81, 0xFE, 0x00,
  0x38, 0x71, 0xFF, 0xE0, 0x7E, 0x00, 0x38, 0x71, 0xF9, 0xF8, 0x1E, 0x00,
  0x38, 0x71, 0xE0, 0x7C, 0x0F, 0x00, 0x38, 0x71, 0xC0, 0x3F, 0x07, 0x80,
  0x38, 0x71, 0xC0, 0x3F, 0xC3, 0x80, 0x38, 0x71, 0xC0, 0x3B, 0xE1, 0xC0,
  0x38, 0x7D, 0xC0, 0x38, 0xE1, 0xC0, 0x1C, 0x3F, 0xC0, 0x38, 0xE1, 0xC0,
  0x1C, 0x0F, 0xC0, 0x38, 0xE1, 0xC0, 0x0F, 0x03, 0xE0, 0x78, 0xE1, 0xC0,
  0x07, 0x81, 0xF9, 0xF8, 0xE1, 0xC0, 0x07, 0xE0, 0x7F, 0xF8, 0xE3, 0xC0,
  0x07, 0xF0, 0x1F, 0xB8, 0xE3, 0x80, 0x07, 0x7C, 0x3E, 0x38, 0xE7, 0x80,
  0x07, 0x3F, 0xFC, 0x38, 0xEF, 0x00, 0x07, 0x0F, 0xF0, 0x38, 0xFE, 0x00,
  0x07, 0x03, 0xC0, 0x78, 0xFC, 0x00, 0x03, 0x80, 0x01, 0xF0, 0xF0, 0x00,
  0x03, 0xC0, 0x07, 0xE0, 0xE0, 0x00, 0x01, 0xE0, 0x1F, 0x81, 0xC0, 0x00,
  0x00, 0xF8, 0x7E, 0x01, 0xC0, 0x00, 0x00, 0x7F, 0xFC, 0x03, 0x80, 0x00,
  0x00, 0x3F, 0xFC, 0x0F, 0x80, 0x00, 0x00, 0x07, 0xBF, 0xFF, 0x00, 0x00,
  0x00, 0x00, 0x0F, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x03, 0xF0, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void drawOpenAIMark(int16_t x, int16_t y, int16_t size, uint16_t col) {
  if (size >= 40) tft.drawBitmap(x, y, OPENAI_MARK_44, 44, 44, col);
  else tft.drawBitmap(x, y, OPENAI_MARK_32, 32, 32, col);
}

void drawClaudeMark(int16_t x, int16_t y, int16_t size, uint16_t col) {
  for (uint16_t i = 0; i < LOGO_SEG_COUNT; i++) {
    int16_t x1 = pgm_read_word(&LOGO_SEGS[i][0]);
    int16_t y1 = pgm_read_word(&LOGO_SEGS[i][1]);
    int16_t x2 = pgm_read_word(&LOGO_SEGS[i][2]);
    int16_t y2 = pgm_read_word(&LOGO_SEGS[i][3]);
    int16_t sx1 = x + ((x1 - 30) * size) / 180;
    int16_t sy1 = y + ((y1 - 15) * size) / 180;
    int16_t sx2 = x + ((x2 - 30) * size) / 180;
    int16_t sy2 = y + ((y2 - 15) * size) / 180;
    drawThickLine(sx1, sy1, sx2, sy2, size >= 40 ? 1 : 0, col);
  }
}

void drawTinyCodexLogo(int16_t x, int16_t y, uint16_t col) {
  drawOpenAIMark(x, y, 32, col);
}

void drawTinyClaudeLogo(int16_t x, int16_t y, uint16_t col) {
  drawClaudeMark(x, y, 32, col);
}

void drawEyePair(int16_t lx, int16_t rx, int16_t y, int16_t w, int16_t h, uint16_t col) {
  tft.fillRoundRect(lx, y, w, h, 10, col);
  tft.fillRoundRect(rx, y, w, h, 10, col);
}

void drawXEye(int16_t x, int16_t y, uint16_t col) {
  drawThickLine(x, y, x + 38, y + 44, 3, col);
  drawThickLine(x + 38, y, x, y + 44, 3, col);
}

void drawArcLine(int16_t cx, int16_t cy, int16_t r, int startDeg, int endDeg, uint8_t thk, uint16_t col) {
  int step = startDeg <= endDeg ? 8 : -8;
  int16_t px = cx + cos(startDeg * 0.0174533) * r;
  int16_t py = cy + sin(startDeg * 0.0174533) * r;
  for (int deg = startDeg + step; (step > 0 ? deg <= endDeg : deg >= endDeg); deg += step) {
    float rad = deg * 0.0174533;
    int16_t nx = cx + cos(rad) * r;
    int16_t ny = cy + sin(rad) * r;
    drawThickLine(px, py, nx, ny, thk, col);
    px = nx; py = ny;
  }
}

String visualState(String state) {
  if (state == "multi-active") return "multi";
  if (state == "subagent") return "thinking";
  return state;
}

void drawCodexExpression(const String& state, const AgentTheme& theme) {
  const int16_t lx = 54, rx = 148, y = 92;
  uint16_t face = theme.face;
  uint16_t cut = statusBgForState(state, theme);

  if (state == "sleeping" || state == "idle") {
    drawThickLine(lx, y + 28, lx + 42, y + 28, 3, face);
    drawThickLine(rx, y + 28, rx + 42, y + 28, 3, face);
    tft.setTextColor(face); tft.setTextSize(2);
    tft.setCursor(174, 76); tft.print("z");
    tft.setCursor(194, 58); tft.print("Z");
    return;
  }
  if (state == "thinking") {
    tft.fillRoundRect(lx, y, 40, 64, 4, face);
    tft.fillRoundRect(rx, y, 40, 64, 4, face);
    tft.fillRect(lx + 20, y + 14, 12, 12, cut);
    tft.fillRect(rx + 20, y + 14, 12, 12, cut);
    tft.fillRect(112, 166, 28, 6, face);
    return;
  }
  if (state == "multi") {
    tft.fillRoundRect(lx, y, 40, 62, 4, face);
    tft.fillRoundRect(rx, y, 40, 62, 4, face);
    tft.fillRect(lx + 14, y + 10, 12, 12, cut);
    tft.fillRect(rx + 14, y + 10, 12, 12, cut);
    tft.fillRect(108, 166, 24, 5, face);
    return;
  }
  if (state == "reading") {
    tft.fillRoundRect(lx - 2, y + 18, 48, 20, 4, face);
    tft.fillRoundRect(rx - 2, y + 18, 48, 20, 4, face);
    tft.fillRect(lx + 6, y + 24, 30, 6, cut);
    tft.fillRect(rx + 6, y + 24, 30, 6, cut);
    tft.drawFastHLine(82, 152, 78, face);
    return;
  }
  if (state == "writing") {
    drawThickLine(lx - 2, y + 18, lx + 44, y + 34, 5, face);
    drawThickLine(rx - 2, y + 34, rx + 44, y + 18, 5, face);
    drawThickLine(88, 164, 152, 152, 3, face);
    return;
  }
  if (state == "shell") {
    tft.fillRoundRect(lx - 2, y + 4, 52, 54, 4, face);
    tft.fillRoundRect(rx - 2, y + 4, 52, 54, 4, face);
    tft.fillRect(lx + 10, y + 18, 10, 7, cut);
    tft.fillRect(lx + 22, y + 27, 18, 6, cut);
    tft.fillRect(rx + 11, y + 17, 24, 7, cut);
    tft.fillRect(rx + 36, y + 17, 6, 26, cut);
    return;
  }
  if (state == "permission") {
    tft.drawRoundRect(lx - 4, y + 2, 52, 60, 6, face);
    tft.drawRoundRect(rx - 4, y + 2, 52, 60, 6, face);
    tft.fillRect(lx + 26, y + 18, 8, 18, face);
    tft.fillRect(rx + 10, y + 18, 8, 18, face);
    tft.fillCircle(120, 166, 5, face);
    return;
  }
  if (state == "usage_low") {
    tft.fillRoundRect(lx - 2, y + 28, 48, 12, 4, face);
    tft.fillRoundRect(rx - 2, y + 28, 48, 12, 4, face);
    drawThickLine(102, 164, 138, 164, 2, face);
    return;
  }
  if (state == "blocked") {
    drawThickLine(lx, y + 26, lx + 46, y + 20, 5, face);
    drawThickLine(rx, y + 20, rx + 46, y + 26, 5, face);
    tft.fillRoundRect(86, 160, 68, 8, 3, face);
    return;
  }
  if (state == "error" || state == "usage_critical") {
    drawXEye(lx + 2, y + 8, face);
    drawXEye(rx + 2, y + 8, face);
    tft.fillRoundRect(104, 162, 34, 7, 2, face);
    return;
  }
  if (state == "success") {
    drawThickLine(lx + 2, y + 32, lx + 20, y + 18, 4, face);
    drawThickLine(lx + 20, y + 18, lx + 42, y + 32, 4, face);
    drawThickLine(rx + 2, y + 32, rx + 20, y + 18, 4, face);
    drawThickLine(rx + 20, y + 18, rx + 42, y + 32, 4, face);
    drawThickLine(106, 164, 118, 176, 3, face);
    drawThickLine(118, 176, 140, 150, 3, face);
    return;
  }
  if (state == "compact") {
    tft.fillRoundRect(lx - 6, y + 28, 54, 12, 4, face);
    tft.fillRoundRect(rx - 6, y + 28, 54, 12, 4, face);
    drawThickLine(100, 164, 114, 164, 2, face);
    drawThickLine(140, 164, 126, 164, 2, face);
    return;
  }
  drawEyePair(lx, rx, y, 40, 60, face);
}

void drawClaudeExpression(const String& state, const AgentTheme& theme) {
  const int16_t lx = 70, rx = 150, y = 100;
  uint16_t face = theme.face;

  if (state == "sleeping" || state == "idle") {
    drawArcLine(lx + 18, y + 24, 18, 205, 335, 3, face);
    drawArcLine(rx + 18, y + 24, 18, 205, 335, 3, face);
    tft.setTextColor(tft.color565(255, 210, 160)); tft.setTextSize(2);
    tft.setCursor(176, 76); tft.print("z");
    tft.setCursor(196, 58); tft.print("Z");
    return;
  }
  if (state == "thinking") {
    tft.fillRoundRect(lx, y, 34, 52, 12, face);
    tft.fillRoundRect(rx, y, 34, 52, 12, face);
    tft.fillCircle(lx + 23, y + 14, 6, statusBgForState(state, theme));
    tft.fillCircle(rx + 23, y + 14, 6, statusBgForState(state, theme));
    drawArcLine(120, 166, 12, 205, 335, 2, face);
    return;
  }
  if (state == "multi") {
    tft.fillRoundRect(lx, y, 34, 52, 12, face);
    tft.fillRoundRect(rx, y, 34, 52, 12, face);
    tft.fillCircle(lx + 17, y + 12, 6, statusBgForState(state, theme));
    tft.fillCircle(rx + 17, y + 12, 6, statusBgForState(state, theme));
    tft.fillRoundRect(108, 166, 24, 5, 2, face);
    return;
  }
  if (state == "reading") {
    drawArcLine(lx + 17, y + 28, 20, 195, 345, 3, face);
    drawArcLine(rx + 17, y + 28, 20, 195, 345, 3, face);
    tft.fillRoundRect(88, 154, 64, 10, 4, face);
    return;
  }
  if (state == "writing") {
    drawArcLine(lx + 17, y + 28, 20, 210, 335, 3, face);
    drawArcLine(rx + 17, y + 22, 20, 200, 325, 3, face);
    drawThickLine(96, 164, 144, 158, 2, face);
    return;
  }
  if (state == "shell") {
    tft.drawRoundRect(lx - 4, y + 2, 44, 46, 8, face);
    tft.drawRoundRect(rx - 4, y + 2, 44, 46, 8, face);
    drawThickLine(lx + 8, y + 18, lx + 17, y + 24, 2, face);
    drawThickLine(lx + 17, y + 24, lx + 8, y + 30, 2, face);
    drawThickLine(rx + 8, y + 28, rx + 26, y + 28, 2, face);
    return;
  }
  if (state == "permission") {
    tft.drawCircle(lx + 17, y + 26, 22, face);
    tft.drawCircle(rx + 17, y + 26, 22, face);
    tft.fillCircle(lx + 23, y + 20, 6, face);
    tft.fillCircle(rx + 11, y + 20, 6, face);
    tft.fillCircle(120, 166, 4, face);
    return;
  }
  if (state == "usage_low") {
    drawArcLine(lx + 17, y + 25, 19, 195, 345, 4, face);
    drawArcLine(rx + 17, y + 25, 19, 195, 345, 4, face);
    tft.fillRoundRect(104, 164, 32, 5, 2, face);
    return;
  }
  if (state == "blocked") {
    drawArcLine(lx + 17, y + 20, 20, 205, 335, 4, face);
    drawArcLine(rx + 17, y + 20, 20, 205, 335, 4, face);
    tft.fillRoundRect(92, 160, 56, 7, 3, face);
    return;
  }
  if (state == "error" || state == "usage_critical") {
    drawXEye(lx - 1, y + 2, face);
    drawXEye(rx - 1, y + 2, face);
    return;
  }
  if (state == "success") {
    drawArcLine(lx + 17, y + 24, 20, 205, 335, 4, face);
    drawArcLine(rx + 17, y + 24, 20, 205, 335, 4, face);
    drawArcLine(120, 158, 20, 25, 155, 2, face);
    return;
  }
  if (state == "compact") {
    tft.fillRoundRect(lx - 8, y + 26, 48, 10, 4, face);
    tft.fillRoundRect(rx - 8, y + 26, 48, 10, 4, face);
    return;
  }
  drawEyePair(lx, rx, y, 34, 52, face);
}

void drawStatusExpression(const String& state, const AgentTheme& theme) {
  String s = visualState(state);
  if (theme.claude) drawClaudeExpression(s, theme);
  else drawCodexExpression(s, theme);
}

void drawPixelStar(int16_t x, int16_t y, uint16_t col) {
  tft.fillRect(x + 4, y, 5, 13, col);
  tft.fillRect(x, y + 4, 13, 5, col);
}

void drawMiniTerminal(int16_t x, int16_t y, uint16_t frame, uint16_t line1, uint16_t line2) {
  tft.fillRect(x, y, 58, 40, frame);
  tft.fillRect(x + 2, y + 2, 54, 36, tft.color565(28, 29, 44));
  tft.fillRect(x + 2, y + 2, 54, 6, tft.color565(42, 42, 60));
  tft.fillRect(x + 6, y + 4, 3, 3, C_RED);
  tft.fillRect(x + 12, y + 4, 3, 3, C_YELLOW);
  tft.fillRect(x + 18, y + 4, 3, 3, C_GREEN);
  tft.fillRect(x + 8, y + 17, 28, 4, line1);
  tft.fillRect(x + 8, y + 26, 38, 3, line2);
}

void drawMiniKeyboard(int16_t x, int16_t y, uint16_t base, uint16_t key, uint16_t hot) {
  tft.fillRect(x, y, 74, 18, tft.color565(38, 58, 66));
  tft.fillRect(x, y + 2, 74, 13, base);
  for (uint8_t row = 0; row < 3; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      uint16_t c = ((row + col) % 6 == 0) ? hot : key;
      tft.fillRect(x + 5 + col * 8 + (row == 1 ? 3 : 0), y + 4 + row * 4, 5, 2, c);
    }
  }
}

void drawHelmet(int16_t x, int16_t y, uint16_t col, uint16_t dark) {
  tft.fillRect(x + 8, y + 10, 44, 14, col);
  tft.fillRect(x + 13, y + 4, 34, 12, col);
  tft.fillRect(x + 25, y + 2, 10, 22, dark);
  tft.fillRect(x + 4, y + 22, 52, 6, dark);
  tft.fillRect(x + 10, y + 24, 40, 4, col);
}

void drawOpenBook(int16_t x, int16_t y, uint16_t cover, uint16_t page, uint16_t ink) {
  tft.fillRoundRect(x, y, 78, 34, 5, cover);
  tft.fillRoundRect(x + 4, y + 4, 32, 26, 3, page);
  tft.fillRoundRect(x + 42, y + 4, 32, 26, 3, page);
  tft.fillRect(x + 38, y + 3, 2, 29, ink);
  tft.drawFastHLine(x + 10, y + 12, 20, ink);
  tft.drawFastHLine(x + 10, y + 19, 16, ink);
  tft.drawFastHLine(x + 48, y + 12, 18, ink);
  tft.drawFastHLine(x + 48, y + 19, 22, ink);
}

void drawTinyBattery(int16_t x, int16_t y, uint16_t frame, uint16_t fill, bool critical) {
  tft.drawRoundRect(x, y, 42, 22, 4, frame);
  tft.fillRect(x + 42, y + 7, 5, 8, frame);
  tft.fillRect(x + 5, y + 5, critical ? 8 : 18, 12, fill);
  if (critical) {
    tft.fillRect(x + 20, y + 4, 5, 14, frame);
    tft.fillRect(x + 20, y + 4, 15, 5, frame);
  }
}

void drawBugProbe(int16_t x, int16_t y, uint16_t col, uint16_t bg) {
  tft.fillRoundRect(x + 12, y + 10, 28, 22, 6, col);
  tft.fillRect(x + 22, y + 4, 8, 8, col);
  tft.fillRect(x + 18, y + 16, 5, 5, bg);
  tft.fillRect(x + 30, y + 16, 5, 5, bg);
  tft.drawLine(x + 8, y + 14, x, y + 8, col);
  tft.drawLine(x + 44, y + 14, x + 52, y + 8, col);
  tft.drawLine(x + 8, y + 24, x, y + 30, col);
  tft.drawLine(x + 44, y + 24, x + 52, y + 30, col);
}

void drawStatusProp(const String& state, const AgentTheme& theme) {
  uint16_t col = stateAccent(state, theme);
  uint16_t face = theme.face;
  uint16_t bg = statusBgForState(state, theme);

  if (state == "thinking") {
    uint16_t bubble = theme.codex ? tft.color565(210, 255, 226) : tft.color565(255, 238, 198);
    tft.fillCircle(166, 58, 5, bubble);
    tft.fillCircle(178, 44, 8, bubble);
    tft.fillRoundRect(188, 22, 34, 28, 7, bubble);
    tft.fillRect(198, 34, 4, 4, face);
    tft.fillRect(208, 34, 4, 4, face);
    return;
  }

  if (state == "shell") {
    drawMiniTerminal(152, 28, face, C_GREEN, C_BLUE);
    return;
  }

  if (state == "writing") {
    drawMiniKeyboard(83, 182, tft.color565(82, 118, 132), tft.color565(124, 158, 168), C_GREEN);
    tft.fillRect(158, 172, 9, 32, col);
    tft.fillTriangle(158, 204, 167, 204, 162, 214, col);
    return;
  }

  if (state == "reading") {
    tft.fillRoundRect(76, 170, 36, 20, 6, col);
    tft.fillRoundRect(128, 170, 36, 20, 6, col);
    tft.fillRect(112, 178, 16, 4, col);
    tft.fillRect(82, 176, 24, 4, bg);
    tft.fillRect(134, 176, 24, 4, bg);
    drawOpenBook(81, 196, col, theme.codex ? tft.color565(214, 244, 224) : tft.color565(255, 224, 184), face);
    tft.fillRect(54, 64, 4, 4, C_BLUE);
    tft.fillRect(184, 58, 5, 5, C_GREEN);
    return;
  }

  if (state == "permission") {
    drawBugProbe(28, 34, col, bg);
    tft.drawCircle(177, 55, 18, col);
    drawThickLine(190, 68, 207, 85, 2, col);
    tft.fillRect(172, 51, 10, 12, col);
    tft.fillRect(174, 47, 6, 8, bg);
    return;
  }

  if (state == "blocked") {
    uint16_t stripe = theme.codex ? C_YELLOW : tft.color565(255, 224, 184);
    tft.fillRoundRect(64, 36, 112, 18, 3, face);
    for (uint8_t i = 0; i < 5; i++) {
      int16_t x = 70 + i * 21;
      tft.fillTriangle(x, 37, x + 12, 37, x, 53, stripe);
    }
    tft.fillRect(82, 54, 8, 18, face);
    tft.fillRect(150, 54, 8, 18, face);
    return;
  }

  if (state == "error" || state == "usage_critical") {
    uint16_t err = theme.codex ? C_RED : C_WHITE;
    if (state == "usage_critical") {
      drawTinyBattery(170, 38, err, C_RED, true);
      tft.fillTriangle(100, 26, 78, 66, 122, 66, err);
      tft.fillTriangle(100, 34, 86, 62, 114, 62, bg);
      tft.fillRect(97, 43, 6, 12, err);
      tft.fillRect(97, 58, 6, 5, err);
    } else {
      tft.fillTriangle(120, 24, 92, 72, 148, 72, err);
      tft.fillTriangle(120, 32, 100, 68, 140, 68, bg);
      tft.fillRect(117, 42, 6, 16, err);
      tft.fillRect(117, 62, 6, 5, err);
    }
    tft.fillRect(82, 178, 76, 6, face);
    return;
  }

  if (state == "success") {
    drawPixelStar(48, 56, C_YELLOW);
    drawPixelStar(180, 48, C_YELLOW);
    drawPixelStar(196, 78, C_YELLOW);
    return;
  }

  if (state == "compact") {
    tft.fillRoundRect(48, 42, 42, 18, 3, col);
    tft.fillRoundRect(150, 42, 42, 18, 3, col);
    drawThickLine(90, 51, 112, 51, 2, col);
    drawThickLine(150, 51, 128, 51, 2, col);
    tft.fillTriangle(112, 43, 112, 59, 124, 51, col);
    tft.fillTriangle(128, 43, 128, 59, 116, 51, col);
    return;
  }

  if (state == "subagent") {
    drawArcLine(120, 118, 76, 205, 335, 3, col);
    tft.fillRoundRect(44, 94, 15, 34, 5, col);
    tft.fillRoundRect(181, 94, 15, 34, 5, col);
    drawPixelStar(70, 50, col);
    drawPixelStar(160, 46, col);
    return;
  }

  if (state == "multi" || state == "multi-active") {
    tft.fillCircle(83, 50, 7, C_YELLOW);
    tft.fillCircle(120, 38, 7, C_GREEN);
    tft.fillCircle(157, 50, 7, C_RED);
    return;
  }

  if (state == "usage_low") {
    drawTinyBattery(172, 40, C_YELLOW, C_RED, false);
    tft.fillRect(60, 60, 9, 3, C_YELLOW);
    tft.fillRect(184, 80, 7, 3, C_YELLOW);
    return;
  }
}

void drawStateIcon(const String& state, const AgentTheme& theme) {
  if (state == "multi" || state == "multi-active" || state == "subagent") return;
  uint16_t col = stateAccent(state, theme);
  uint16_t badge = theme.codex ? tft.color565(8, 24, 13) : tft.color565(255, 176, 104);
  if (state == "error" || state == "blocked" || state == "usage_critical") badge = tft.color565(72, 12, 12);
  if (state == "permission" || state == "compact" || state == "usage_low") badge = tft.color565(72, 54, 8);
  if (state == "success") badge = tft.color565(6, 52, 24);
  const int16_t bx = 12, by = 12;
  const int16_t x = bx + 7, y = by + 7;
  if (state == "sleeping" || state == "idle") return;
  tft.fillRoundRect(bx, by, 50, 50, 8, badge);
  tft.drawRoundRect(bx, by, 50, 50, 8, col);
  if (state == "thinking") {
    tft.drawCircle(x + 18, y + 14, 12, col);
    tft.drawCircle(x + 18, y + 14, 11, col);
    tft.fillRect(x + 12, y + 27, 13, 6, col);
    tft.drawFastHLine(x + 13, y + 37, 11, col);
    tft.drawFastVLine(x + 18, y - 2, 7, col);
  } else if (state == "reading") {
    tft.drawRoundRect(x, y + 8, 36, 27, 4, col);
    tft.drawRoundRect(x + 1, y + 9, 34, 25, 4, col);
    tft.drawFastVLine(x + 18, y + 9, 26, col);
    tft.drawLine(x + 1, y + 14, x + 16, y + 18, col);
    tft.drawLine(x + 35, y + 14, x + 20, y + 18, col);
  } else if (state == "writing") {
    drawThickLine(x + 4, y + 34, x + 32, y + 6, 3, col);
    tft.fillTriangle(x + 32, y + 6, x + 38, y + 14, x + 25, y + 12, col);
    drawThickLine(x + 2, y + 40, x + 30, y + 40, 1, col);
  } else if (state == "shell") {
    tft.drawRoundRect(x, y + 9, 38, 29, 5, col);
    drawThickLine(x + 7, y + 18, x + 14, y + 24, 2, col);
    drawThickLine(x + 14, y + 24, x + 7, y + 30, 2, col);
    drawThickLine(x + 21, y + 30, x + 34, y + 30, 2, col);
  } else if (state == "permission") {
    tft.drawRoundRect(x + 6, y + 21, 27, 20, 4, col);
    tft.drawCircle(x + 19, y + 21, 10, col);
    tft.fillRect(x + 9, y + 21, 20, 7, badge);
    tft.fillCircle(x + 19, y + 31, 3, col);
  } else if (state == "blocked") {
    tft.drawCircle(x + 19, y + 21, 18, col);
    drawThickLine(x + 7, y + 34, x + 31, y + 10, 3, col);
  } else if (state == "error" || state == "usage_critical") {
    tft.drawTriangle(x + 19, y + 4, x + 1, y + 39, x + 37, y + 39, col);
    tft.drawTriangle(x + 19, y + 7, x + 5, y + 37, x + 33, y + 37, col);
    tft.fillRect(x + 17, y + 16, 5, 14, col);
    tft.fillCircle(x + 19, y + 34, 3, col);
  } else if (state == "success") {
    drawThickLine(x + 4, y + 24, x + 15, y + 35, 3, col);
    drawThickLine(x + 15, y + 35, x + 37, y + 9, 3, col);
  } else if (state == "compact") {
    drawThickLine(x + 3, y + 22, x + 17, y + 22, 2, col);
    drawThickLine(x + 3, y + 22, x + 10, y + 15, 2, col);
    drawThickLine(x + 3, y + 22, x + 10, y + 29, 2, col);
    drawThickLine(x + 36, y + 22, x + 22, y + 22, 2, col);
    drawThickLine(x + 36, y + 22, x + 29, y + 15, 2, col);
    drawThickLine(x + 36, y + 22, x + 29, y + 29, 2, col);
  } else if (state == "usage_low") {
    tft.drawCircle(x + 19, y + 21, 18, col);
    tft.fillRect(x + 17, y + 10, 5, 19, col);
    tft.fillCircle(x + 19, y + 34, 3, col);
  }
}

void drawActivityCorner(const String& state, uint8_t active, const AgentTheme& theme) {
  bool hasSubagent = (state == "subagent");
  bool hasMulti = (state == "multi" || state == "multi-active" || active > 1);
  if (!hasSubagent && !hasMulti) return;
  uint16_t bg = theme.codex ? tft.color565(8, 18, 11) : tft.color565(245, 126, 70);
  uint16_t col = theme.codex ? C_GREEN : tft.color565(36, 26, 18);
  tft.fillRoundRect(174, 12, 54, 28, 7, bg);
  tft.drawRoundRect(174, 12, 54, 28, 7, col);
  if (hasSubagent) {
    tft.drawCircle(188, 26, 5, col);
    tft.drawCircle(206, 26, 5, col);
    tft.drawLine(193, 26, 201, 26, col);
  } else {
    uint8_t dots = constrain(active, 2, 4);
    for (uint8_t i = 0; i < dots; i++) tft.fillCircle(188 + i * 10, 26, 3, col);
  }
}

void drawStatusView() {
  currentView = VIEW_CC_STATUS;
  termMode = false;
  AgentTheme theme = agentTheme(ccSource);
  tft.fillScreen(statusBgForState(ccState, theme));
  drawStateIcon(ccState, theme);
  drawActivityCorner(ccState, ccActive, theme);
  drawStatusExpression(ccState, theme);
  drawStatusProp(ccState, theme);
}

void drawSegmentedRing(int16_t cx, int16_t cy, int16_t r, int pct, uint16_t col, uint16_t muted) {
  pct = constrain(pct, 0, 100);
  for (int deg = -90; deg < 270; deg += 18) {
    float a0 = deg * 0.0174533;
    float a1 = (deg + 11) * 0.0174533;
    uint16_t segCol = (deg + 90) < map(pct, 0, 100, 0, 360) ? col : muted;
    drawThickLine(cx + cos(a0) * r, cy + sin(a0) * r,
                  cx + cos(a1) * r, cy + sin(a1) * r, 2, segCol);
  }
  tft.setTextColor(C_WHITE); tft.setTextSize(3);
  int16_t tx = pct >= 100 ? cx - 27 : (pct >= 10 ? cx - 18 : cx - 9);
  tft.setCursor(tx, cy - 15); tft.print(pct);
  tft.setTextSize(1);
  tft.setCursor(cx + 24, cy + 3); tft.print("%");
}

void drawBar(int16_t x, int16_t y, int16_t w, int pct, uint16_t col) {
  pct = constrain(pct, 0, 100);
  tft.fillRoundRect(x, y, w, 9, 4, tft.color565(52, 54, 58));
  tft.fillRoundRect(x, y, map(pct, 0, 100, 0, w), 9, 4, col);
}

void drawUsageLogo(String provider, uint16_t col) {
  if (isCodexSource(provider)) drawTinyCodexLogo(24, 21, col);
  else drawTinyClaudeLogo(24, 21, C_WHITE);
}

void drawUsageView() {
  currentView = VIEW_CC_USAGE;
  termMode = false;
  bool codex = isCodexSource(usageProvider);
  uint16_t accent = usagePrimary >= 95 ? C_RED : (usagePrimary >= 80 ? C_YELLOW : (codex ? C_GREEN : C_ORANGE));
  uint16_t bg = codex ? C_BLACK : tft.color565(34, 26, 22);
  uint16_t card = codex ? tft.color565(9, 14, 11) : tft.color565(48, 34, 28);
  uint16_t muted = usageUnavailable ? tft.color565(52, 52, 52) : tft.color565(78, 74, 70);
  if (usageUnavailable) accent = muted;

  tft.fillScreen(bg);
  tft.fillRoundRect(12, 12, 216, 216, 8, card);
  tft.drawRoundRect(12, 12, 216, 216, 8, accent);
  drawUsageLogo(usageProvider, accent);

  tft.setTextColor(accent); tft.setTextSize(2);
  tft.setCursor(70, 30);
  tft.print(codex ? "Codex" : "Claude");
  if (usageStale) {
    tft.fillRoundRect(170, 28, 38, 17, 4, muted);
    tft.setTextColor(C_WHITE); tft.setTextSize(1);
    tft.setCursor(179, 33); tft.print("stale");
  }

  drawSegmentedRing(76, 126, 44, usageUnavailable ? 0 : usagePrimary, accent, muted);
  tft.setTextColor(tft.color565(180, 176, 168)); tft.setTextSize(1);
  tft.setCursor(61, 180); tft.print(codex ? "5h" : "session");

  tft.setTextColor(tft.color565(205, 198, 188)); tft.setTextSize(1);
  tft.setCursor(136, 92); tft.print("weekly");
  drawBar(136, 108, 66, usageUnavailable ? 0 : usageSecondary, accent);
  tft.setCursor(136, 136); tft.print("reset");
  tft.setTextColor(C_WHITE);
  tft.setCursor(136, 151); tft.print(shortText(usageReset.length() ? usageReset : "--", 9));

  tft.fillRoundRect(132, 178, 76, 22, 5, bg);
  tft.setTextColor(accent); tft.setTextSize(1);
  tft.setCursor(142, 185);
  if (usageUnavailable) tft.print("local");
  else tft.print(shortText(usageBadge.length() ? usageBadge : (codex ? "plus" : "plan"), 8));
}

// ═════════════════════════════════════════════════════════════
//  TERMINAL
// ═════════════════════════════════════════════════════════════

void termClear() {
  for (uint8_t i = 0; i < TERM_ROWS; i++) termLines[i] = "";
  termRow = 0; termCol = 0;
}

void termDrawHeader() {
  tft.fillRect(0, 0, DISP_W, TERM_PAD_Y + 1, C_DARKBG);
  tft.setTextColor(C_ORANGE); tft.setTextSize(1);
  tft.setCursor(TERM_PAD_X, 4); tft.print("cc@mochi terminal");
  tft.drawFastHLine(0, TERM_PAD_Y, DISP_W, C_ORANGE);
}

// Prefix "cc:~$ " in green, drawn only when the row has content
void termDrawPrefix(int16_t yy) {
  tft.setTextColor(C_GREEN); tft.setTextSize(1);
  tft.setCursor(TERM_PAD_X, yy + 6);
  tft.print("cc:~$ ");
}

#define PREFIX_PX 36   // 6 chars x 6px at textSize 1

void termDrawLine(uint8_t r) {
  const int16_t yy = TERM_PAD_Y + 4 + r * TERM_CHAR_H;
  tft.fillRect(0, yy, DISP_W, TERM_CHAR_H, C_DARKBG);
  // show prefix only on the currently active (cursor) line
  if (r == termRow) termDrawPrefix(yy);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  tft.setCursor(TERM_PAD_X + PREFIX_PX, yy + 1);
  tft.print(termLines[r]);
  if (r == termRow) {
    const int16_t cx = TERM_PAD_X + PREFIX_PX + termCol * TERM_CHAR_W;
    tft.fillRect(cx, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, C_GREEN);
  }
}

void termDrawLastChar() {
  if (termCol == 0) return;
  const int16_t yy    = TERM_PAD_Y + 4 + termRow * TERM_CHAR_H;
  const int16_t baseX = TERM_PAD_X + PREFIX_PX;
  const uint8_t prev  = termCol - 1;
  // erase prev cell (had cursor block)
  tft.fillRect(baseX + prev * TERM_CHAR_W, yy + 1, TERM_CHAR_W, TERM_CHAR_H - 1, C_DARKBG);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  tft.setCursor(baseX + prev * TERM_CHAR_W, yy + 1);
  tft.print(termLines[termRow][prev]);
  // new cursor
  tft.fillRect(baseX + termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, C_GREEN);
}

void termDrawBackspace() {
  const int16_t yy    = TERM_PAD_Y + 4 + termRow * TERM_CHAR_H;
  const int16_t baseX = TERM_PAD_X + PREFIX_PX;
  // erase deleted char + old cursor
  tft.fillRect(baseX + termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W * 2, TERM_CHAR_H - 1, C_DARKBG);
  // new cursor
  tft.fillRect(baseX + termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, C_GREEN);
  // if line now empty, erase the prefix too
  if (termLines[termRow].length() == 0) {
    tft.fillRect(0, yy, TERM_PAD_X + PREFIX_PX, TERM_CHAR_H, C_DARKBG);
  }
}

void termFullRedraw() {
  tft.fillScreen(C_DARKBG);
  termDrawHeader();
  for (uint8_t r = 0; r < TERM_ROWS; r++) termDrawLine(r);
}

void termScroll() {
  for (uint8_t i = 0; i < TERM_ROWS - 1; i++) termLines[i] = termLines[i + 1];
  termLines[TERM_ROWS - 1] = "";
  termRow = TERM_ROWS - 1;
  termFullRedraw();
}

void termAddChar(char c) {
  if (c == '\n' || c == '\r') {
    const int16_t yy = TERM_PAD_Y + 4 + termRow * TERM_CHAR_H;
    // erase cursor on current row
    tft.fillRect(TERM_PAD_X + PREFIX_PX + termCol * TERM_CHAR_W,
                 yy + 1, TERM_CHAR_W, TERM_CHAR_H - 1, C_DARKBG);
    termRow++; termCol = 0;
    if (termRow >= TERM_ROWS) { termScroll(); return; }
    termDrawLine(termRow);  // draws prefix on new line
  } else if (c == '\b' || c == 127) {
    if (termCol > 0) {
      termCol--;
      termLines[termRow].remove(termLines[termRow].length() - 1);
      termDrawBackspace();
    }
  } else if (c >= 32 && c < 127) {
    if (termCol >= TERM_COLS) {
      termRow++; termCol = 0;
      if (termRow >= TERM_ROWS) { termScroll(); return; }
    }
    // draw prefix on first char of this line
    if (termCol == 0) termDrawPrefix(TERM_PAD_Y + 4 + termRow * TERM_CHAR_H);
    termLines[termRow] += c;
    termCol++;
    termDrawLastChar();
  }
}

// ═════════════════════════════════════════════════════════════
//  ANIMATIONS
// ═════════════════════════════════════════════════════════════

void animNormalEyes() {
  busy = true;
  const int16_t offs[] = {-16, 16, -16, 16, 0};
  for (uint8_t i = 0; i < 5; i++) { drawNormalEyes(offs[i]); delay(speedMs(80)); }
  drawNormalEyes(0, true);  delay(speedMs(100));
  drawNormalEyes(0, false); delay(speedMs(70));
  drawNormalEyes(0, true);  delay(speedMs(70));
  drawNormalEyes(0, false);
  busy = false;
}

void animSquishEyes() {
  busy = true;
  for (uint8_t i = 0; i < 3; i++) {
    drawSquishEyes(false); delay(speedMs(160));
    drawSquishEyes(true);  delay(speedMs(100));
  }
  drawSquishEyes(false);
  busy = false;
}

void animLogoReveal() {
  busy = true;
  for (uint8_t i = 0; i < 6; i++) {
    drawMochiBootSleepFrame(i);
    server.handleClient();
    delay(speedMs(150));
  }
  for (uint8_t i = 0; i < 8; i++) {
    drawMochiBootWakeToTypingFrame(i);
    server.handleClient();
    delay(speedMs(115));
  }
  for (uint8_t i = 8; i < 18; i++) {
    if (i == 8) drawMochiBootTypingFrame(i);
    else updateMochiBootTypingFrame(i);
    server.handleClient();
    delay(speedMs(110));
  }
  tft.setTextColor(tft.color565(232, 124, 82)); tft.setTextSize(2);
  tft.setCursor(70, 204); tft.print("cc-mochi");
  delay(900);
  busy = false;
}

// ═════════════════════════════════════════════════════════════
//  WEB PAGE
// ═════════════════════════════════════════════════════════════
const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>cc-mochi</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
body{background:#1c1c20;font-family:'Courier New',monospace;color:#e8e4dc;
  display:flex;flex-direction:column;align-items:center;
  padding:20px 14px 52px;gap:14px;min-height:100vh}

.hdr{text-align:center;padding:2px 0 4px}
.mascot{font-size:15px;color:#c96a3e;line-height:1.3;font-weight:bold;
  font-family:'Courier New',monospace;display:block;letter-spacing:1px}
.sitename{font-size:10px;color:#5a5048;margin-top:8px;letter-spacing:3px}

.sec{width:100%;max-width:390px;font-size:10px;color:#8a8278;
  letter-spacing:2px;font-weight:bold;padding:0 2px}

/* Busy bar */
.busy{width:100%;max-width:390px;height:2px;background:#2e2a28;
  border-radius:1px;overflow:hidden;opacity:0;transition:opacity .2s}
.busy.show{opacity:1}
.busy-i{height:100%;width:30%;background:#c96a3e;border-radius:1px;
  animation:sl 1s linear infinite}
@keyframes sl{0%{margin-left:-30%}100%{margin-left:100%}}

/* Controls */
.ctrl{display:flex;gap:8px;width:100%;max-width:390px}
.cbtn{flex:1;background:#252428;border:1.5px solid #38343a;border-radius:10px;
  color:#b8b4ac;font-family:'Courier New',monospace;font-size:11px;font-weight:bold;
  padding:12px 4px;cursor:pointer;text-align:center;transition:all .12s}
.cbtn:active:not(:disabled){transform:scale(.94)}
.cbtn:disabled{opacity:.3;cursor:default}
.cbtn.on{border-color:#c96a3e;color:#c96a3e;background:#201408}
.cbtn.dim{border-color:#2e2a28;color:#4a4540}

/* View grid */
.vgrid{display:grid;grid-template-columns:1fr 1fr;gap:8px;width:100%;max-width:390px}
.vbtn{background:#252428;border:1.5px solid #38343a;border-radius:12px;
  color:#d8d4cc;font-family:'Courier New',monospace;
  padding:14px 6px 10px;cursor:pointer;text-align:center;
  transition:all .12s;user-select:none}
.vbtn:active:not(:disabled){transform:scale(.94)}
.vbtn:disabled{opacity:.3;cursor:default}
.vbtn .ic{font-size:20px;display:block;margin-bottom:4px;line-height:1;color:#c96a3e}
.vbtn .nm{font-size:12px;font-weight:bold;color:#e8e4dc}
.vbtn .ht{font-size:9px;color:#8a8278;margin-top:3px}
.vbtn.active{border-color:#c96a3e;background:#201408}
.vbtn[data-v="1"].active{border-color:#c96a3e;background:#201408}
.vbtn[data-v="2"].active{border-color:#4a8acd;background:#0c1628}
.vbtn[data-v="3"].active{border-color:#38343a;background:#201c18}

/* Speed slider */
.speed-row{width:100%;max-width:390px;display:flex;align-items:center;gap:10px}
.sl{font-size:10px;color:#6a6058;white-space:nowrap;min-width:36px}
input[type=range]{flex:1;accent-color:#c96a3e;cursor:pointer;height:20px}
.sv{font-size:11px;color:#c96a3e;min-width:44px;text-align:right;font-weight:bold}

/* Terminal */
.twrap{width:100%;max-width:390px;display:none;flex-direction:column;gap:8px}
.twrap.open{display:flex}
.thdr{display:flex;justify-content:space-between;align-items:center}
.tttl{font-size:11px;color:#28b878;letter-spacing:1px;font-weight:bold}
.tx{background:#0c1e12;border:2px solid #1a4828;border-radius:9px;
  color:#28b878;font-family:'Courier New',monospace;font-size:13px;
  font-weight:bold;padding:10px 18px;cursor:pointer}
.tx:active{background:#081410}
.trow{display:flex;gap:6px}
.tin{flex:1;background:#0c1018;border:1.5px solid #1a2820;border-radius:9px;
  color:#40d880;font-family:'Courier New',monospace;font-size:15px;
  padding:11px;outline:none}
.tin::placeholder{color:#2a3828}
.tgo{background:#1a9060;border:none;border-radius:9px;color:#fff;
  font-family:'Courier New',monospace;font-size:22px;font-weight:bold;
  padding:11px 16px;cursor:pointer;min-width:52px}
.tgo:active{background:#0f6040}

/* Canvas */
.cwrap{width:100%;max-width:390px;background:#222028;border:1.5px solid #38343a;
  border-radius:12px;padding:12px;flex-direction:column;gap:10px;display:none}
.cwrap.open{display:flex}
.crow{display:flex;gap:8px}
.ci{display:flex;flex-direction:column;align-items:center;gap:4px;flex:1}
.cl{font-size:10px;color:#7a7068;letter-spacing:1px;font-weight:bold}
.cs{width:100%;height:38px;border-radius:7px;border:1.5px solid #38343a;cursor:pointer;padding:0}
.dacts{display:flex;gap:7px}
.db{flex:1;background:#1c1820;border:1.5px solid #38343a;border-radius:9px;
  color:#c0bab8;font-family:'Courier New',monospace;font-size:11px;
  font-weight:bold;padding:11px 4px;cursor:pointer;transition:all .12s}
.db:active{transform:scale(.95);background:#281838}
.db.hi{border-color:#c96a3e;color:#c96a3e}
canvas{width:100%;border-radius:8px;border:1.5px solid #38343a;
  touch-action:none;cursor:crosshair;display:block}

/* Toast */
.toast{position:fixed;bottom:18px;left:50%;transform:translateX(-50%);
  background:#252428;border:1.5px solid #38343a;border-radius:9px;
  font-size:12px;color:#d8d4cc;padding:7px 16px;opacity:0;
  transition:opacity .18s;pointer-events:none;white-space:nowrap;z-index:99}
.toast.show{opacity:1}
</style>
</head>
<body>

<div class="hdr">
  <span class="mascot">&#x2590;&#x259B;&#x2588;&#x2588;&#x2588;&#x259C;&#x258C;<br>&#x259C;&#x2588;&#x2588;&#x2588;&#x2588;&#x2588;&#x259B;<br>&#x2598;&#x2598;&nbsp;&#x259D;&#x259D;</span>
  <div class="sitename">CC &middot; MOCHI &middot; CONTROLLER</div>
</div>

<div class="busy" id="busy"><div class="busy-i"></div></div>

<div class="sec">// controls</div>
<div class="ctrl">
  <button class="cbtn on" id="blBtn" onclick="toggleBL()">&#9728; display on</button>
</div>

<div class="sec">// views</div>
<div class="vgrid">
  <button class="vbtn active" data-v="0" onclick="setView(0)">
    <span class="ic">&#9632; &#9632;</span>
    <span class="nm">Normal eyes</span>
    <span class="ht">wiggle + blink</span>
  </button>
  <button class="vbtn" data-v="1" onclick="setView(1)">
    <span class="ic">&gt; &lt;</span>
    <span class="nm">Squish eyes</span>
    <span class="ht">open / close</span>
  </button>
  <button class="vbtn" data-v="2" onclick="setView(2)">
    <span class="ic">{ }</span>
    <span class="nm">CC status</span>
    <span class="ht">hooks + terminal</span>
  </button>
  <button class="vbtn" data-v="3" onclick="toggleCanvas()">
    <span class="ic">&#11035;</span>
    <span class="nm">Canvas</span>
    <span class="ht">draw on display</span>
  </button>
</div>

<div class="sec">// speed</div>
<div class="speed-row">
  <span class="sl">slow</span>
  <input type="range" id="spd" min="1" max="3" value="1" step="1" oninput="setSpeed(this.value)">
  <span class="sv" id="spdV">slow</span>
</div>

<div class="ctrl">
  <div class="ci" style="flex:1;display:flex;flex-direction:column;gap:4px;align-items:stretch">
    <span class="cl" style="font-size:10px;color:#8a8278;letter-spacing:1px;font-weight:bold;text-align:center">BACKGROUND</span>
    <input type="color" class="cs" id="bgCol" value="#aa4818" oninput="onBgChange(this.value)">
  </div>
  <div class="ci" style="flex:1;display:flex;flex-direction:column;gap:4px;align-items:stretch">
    <span class="cl" style="font-size:10px;color:#8a8278;letter-spacing:1px;font-weight:bold;text-align:center">PEN COLOR</span>
    <input type="color" class="cs" id="penCol" value="#000000">
  </div>
</div>

<div class="sec">// terminal</div>
<div class="twrap" id="twrap">
  <div class="thdr">
    <span class="tttl">&#9658; cc:~$</span>
    <button class="tx" onclick="closeTerm()">&#x2715; exit terminal</button>
  </div>
  <div class="trow">
    <input class="tin" id="tin" type="text" placeholder="type here..."
           autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false">
    <button class="tgo" onclick="termEnter()">&#8629;</button>
  </div>
</div>

<div class="cwrap" id="cwrap">
  <div class="dacts">
    <button class="db hi" onclick="clearAll()">&#11035; clear</button>
    <button class="db" style="border-color:#28b878;color:#28b878" onclick="toggleCanvas()">&#10003; done</button>
  </div>
  <canvas id="cvs" width="240" height="240"></canvas>
</div>

<div class="toast" id="toast"></div>

<script>
let activeView  = 0;
let termOpen    = false;
let canvasOpen  = false;
let blOn        = true;
let isBusy      = false;
let drawing     = false;
let lastX = 0, lastY = 0;
let tt;

const spdLabels = ['','slow','normal','fast'];

// ── Toast ──────────────────────────────────────────────────────
function toast(msg, ok=true) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.style.borderColor = ok ? '#28b878' : '#c96a3e';
  el.classList.add('show');
  clearTimeout(tt);
  tt = setTimeout(() => el.classList.remove('show'), 1300);
}

// ── Busy ────────────────────────────────────────────────────────
function setBusy(b) {
  isBusy = b;
  document.getElementById('busy').classList.toggle('show', b);
  const locked = b || termOpen;
  document.querySelectorAll('.vbtn').forEach(el => {
    // when canvas open, keep canvas btn (data-v=3) active so user can exit
    el.disabled = canvasOpen ? parseInt(el.dataset.v) !== 3 : locked;
  });
  document.querySelectorAll('.lbtn').forEach(el => el.disabled = locked || canvasOpen);
  document.querySelectorAll('.cbtn').forEach(el => {
    if (el.id !== 'blBtn') el.disabled = locked;
  });
}

// ── HTTP ────────────────────────────────────────────────────────
async function req(path) {
  try { const r = await fetch(path); return r.ok; }
  catch(e) { toast('no connection', false); return false; }
}

async function waitNotBusy() {
  for (let i = 0; i < 100; i++) {
    try {
      const r = await fetch('/state');
      const j = await r.json();
      if (!j.busy) return;
    } catch(e) {}
    await new Promise(r => setTimeout(r, 150));
  }
}

// ── Background colour ───────────────────────────────────────────
async function onBgChange(hex) {
  if (canvasOpen) {
    await req('/draw/clear?bg=' + encodeURIComponent(hex));
  } else {
    await req('/redraw?bg=' + encodeURIComponent(hex));
  }
  redrawCanvas(hex);
}

// ── Speed ───────────────────────────────────────────────────────
async function setSpeed(v) {
  document.getElementById('spdV').textContent = spdLabels[v];
  await req('/speed?v=' + v);
}

// ── Views ───────────────────────────────────────────────────────
async function setView(v) {
  if (isBusy || termOpen || canvasOpen) return;
  if (v === 3) { toggleCanvas(); return; }  // canvas button in grid
  const keys = ['w','s','d'];
  if (!await req('/cmd?k=' + keys[v])) return;
  activeView = v;
  document.querySelectorAll('.vbtn').forEach(b =>
    b.classList.toggle('active', parseInt(b.dataset.v) === v));
  if (v === 2) {
    termOpen = true;
    document.getElementById('twrap').classList.add('open');
    setBusy(false);   // re-run to apply termOpen lock
    setBusy(false);
    document.querySelectorAll('.vbtn,.lbtn').forEach(b => b.disabled = true);
    const cvb = document.getElementById('cvBtn'); if (cvb) cvb.disabled = true;
    document.getElementById('tin').focus();
    toast('terminal open');
    return;
  }
  setBusy(true);
  await waitNotBusy();
  setBusy(false);
}

// ── Logo animations (kept for startup, not exposed in UI) ──────

// ── Backlight ───────────────────────────────────────────────────
async function toggleBL() {
  blOn = !blOn;
  await req('/backlight?on=' + (blOn ? 1 : 0));
  const b = document.getElementById('blBtn');
  b.textContent = blOn ? '\u2600 display on' : '\u25cb display off';
  b.classList.toggle('on', blOn);
  b.classList.toggle('dim', !blOn);
}

// ── Canvas toggle ───────────────────────────────────────────────
async function toggleCanvas() {
  canvasOpen = !canvasOpen;
  document.getElementById('cwrap').classList.toggle('open', canvasOpen);
  const b = document.getElementById('cvBtn');
  if (b) { b.classList.toggle('on', canvasOpen); b.textContent = canvasOpen ? '\u2b1b canvas on' : '\u2b1b canvas'; }
  // highlight the canvas vbtn (data-v=3) in the grid
  document.querySelectorAll('.vbtn').forEach(btn =>
    btn.classList.toggle('active', canvasOpen && parseInt(btn.dataset.v) === 3));
  await req('/canvas?on=' + (canvasOpen ? 1 : 0));
  if (canvasOpen) {
    const bg = document.getElementById('bgCol').value;
    redrawCanvas(bg);
    await req('/draw/clear?bg=' + encodeURIComponent(bg));
    // lock all other buttons
    document.querySelectorAll('.vbtn,.lbtn').forEach(b => b.disabled = true);
    toast('canvas active');
  } else {
    setBusy(false);   // re-evaluate locks
    toast('canvas off');
  }
}

// ── Terminal ────────────────────────────────────────────────────
const tin = document.getElementById('tin');
let lastVal = '';
tin.addEventListener('input', async () => {
  const cur = tin.value, prev = lastVal;
  if (cur.length > prev.length) {
    await req('/char?c=' + encodeURIComponent(cur[cur.length - 1]));
  } else if (cur.length < prev.length) {
    await req('/char?c=%08');
  }
  lastVal = cur;
});
async function termEnter() {
  await req('/char?c=%0A');
  tin.value = ''; lastVal = ''; tin.focus();
}
tin.addEventListener('keydown', e => {
  if (e.key === 'Enter') { e.preventDefault(); termEnter(); }
});
async function closeTerm() {
  await req('/cmd?k=q');
  termOpen = false;
  document.getElementById('twrap').classList.remove('open');
  setBusy(false);
  toast('terminal closed');
}

// ── Canvas drawing — send full stroke on finger lift ────────────
const cvs = document.getElementById('cvs');
const ctx = cvs.getContext('2d');
let strokePts = [];

function getPos(e) {
  const r = cvs.getBoundingClientRect();
  const sx = cvs.width / r.width, sy = cvs.height / r.height;
  const s = e.touches ? e.touches[0] : e;
  return { x: (s.clientX - r.left) * sx, y: (s.clientY - r.top) * sy };
}

function redrawCanvas(hex) {
  ctx.fillStyle = hex;
  ctx.fillRect(0, 0, cvs.width, cvs.height);
}

function startDraw(e) {
  e.preventDefault();
  drawing = true;
  strokePts = [];
  const p = getPos(e); lastX = p.x; lastY = p.y;
  strokePts.push({ x: Math.round(p.x), y: Math.round(p.y) });
  // draw dot on canvas preview only — no display send yet
  ctx.beginPath(); ctx.arc(p.x, p.y, 2, 0, Math.PI * 2);
  ctx.fillStyle = document.getElementById('penCol').value; ctx.fill();
}
function moveDraw(e) {
  if (!drawing) return; e.preventDefault();
  const p = getPos(e);
  ctx.beginPath(); ctx.moveTo(lastX, lastY); ctx.lineTo(p.x, p.y);
  ctx.strokeStyle = document.getElementById('penCol').value;
  ctx.lineWidth = 4; ctx.lineCap = 'round'; ctx.stroke();
  strokePts.push({ x: Math.round(p.x), y: Math.round(p.y) });
  lastX = p.x; lastY = p.y;
}
async function endDraw(e) {
  if (!drawing) return; drawing = false;
  if (!canvasOpen || strokePts.length < 1) return;
  const pen = document.getElementById('penCol').value.replace('#', '');
  const pts = strokePts.map(p => p.x + ',' + p.y).join(';');
  await req('/draw/stroke?pen=' + pen + '&pts=' + encodeURIComponent(pts));
  strokePts = [];
}

cvs.addEventListener('mousedown',  startDraw);
cvs.addEventListener('mousemove',  moveDraw);
cvs.addEventListener('mouseup',    endDraw);
cvs.addEventListener('mouseleave', endDraw);
cvs.addEventListener('touchstart', startDraw, {passive:false});
cvs.addEventListener('touchmove',  moveDraw,  {passive:false});
cvs.addEventListener('touchend',   endDraw);

// Clear = clear both web canvas and display
async function clearAll() {
  const bg = document.getElementById('bgCol').value;
  redrawCanvas(bg);
  await req('/draw/clear?bg=' + encodeURIComponent(bg));
  toast('cleared');
}

// Init: sync speed and backlight from ESP32, reset bg to default
(async () => {
  try {
    const r = await fetch('/state');
    const j = await r.json();
    // Sync speed
    const spd = j.speed || 1;
    document.getElementById('spd').value = spd;
    document.getElementById('spdV').textContent = spdLabels[spd];
    // Sync backlight
    if (j.bl === false) {
      blOn = false;
      const b = document.getElementById('blBtn');
      b.textContent = '\u25cb display off';
      b.classList.remove('on'); b.classList.add('dim');
    }
  } catch(e) {}
  // Always reset bg picker to default orange on page load
  document.getElementById('bgCol').value = '#aa4818';
  redrawCanvas('#aa4818');
})();
</script>
</body>
</html>
)rawhtml";

// ═════════════════════════════════════════════════════════════
//  WEB ROUTES
// ═════════════════════════════════════════════════════════════

void routeRoot() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.send_P(200, "text/html", INDEX_HTML);
}

void routeCmd() {
  if (!server.hasArg("k") || server.arg("k").isEmpty()) {
    server.send(400, "application/json", "{\"e\":1}"); return;
  }
  const char c = server.arg("k")[0];

  if (termMode) {
    if (c == 'q') { termMode = false; drawCodeView(); }
    server.send(200, "application/json", "{\"ok\":1}"); return;
  }

  server.send(200, "application/json", "{\"ok\":1}");
  switch (c) {
    case 'w': currentView = VIEW_EYES_NORMAL; animNormalEyes(); break;
    case 's': currentView = VIEW_EYES_SQUISH; animSquishEyes(); break;
    case 'd':
      currentView = VIEW_CODE; drawCodeView();
      termMode = true; termClear(); termFullRedraw(); break;
    case 'a':
      currentView = VIEW_EYES_NORMAL;
      animLogoReveal();
      break;
  }
}

void routeChar() {
  if (!termMode) { server.send(200, "application/json", "{\"ok\":1}"); return; }
  const String val = server.arg("c");
  if (val.length() > 0) termAddChar(val[0]);
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeSpeed() {
  if (server.hasArg("v")) animSpeed = constrain(server.arg("v").toInt(), 1, 3);
  server.send(200, "application/json", "{\"ok\":1}");
}

void applyCcState(String source, String state, String label, int active) {
  source.toLowerCase();
  state.toLowerCase();
  state.replace("-", "_");
  ccSource = shortText(source, 10);
  ccState = shortText(state, 20);
  ccLabel = shortText(label, 20);
  ccActive = constrain(active, 0, 9);
  drawStatusView();
}

void applyUsage(String provider, int primary, int secondary, String reset, String badge, bool unavailable, bool stale) {
  provider.toLowerCase();
  usageProvider = shortText(provider, 10);
  usagePrimary = constrain(primary, 0, 100);
  usageSecondary = constrain(secondary, 0, 100);
  usageReset = shortText(reset, 12);
  usageBadge = shortText(badge, 10);
  usageUnavailable = unavailable;
  usageStale = stale;
  drawUsageView();
}

void routeCcState() {
  applyCcState(
    server.hasArg("source") ? server.arg("source") : "cc",
    server.hasArg("state") ? server.arg("state") : "thinking",
    server.hasArg("label") ? server.arg("label") : "",
    server.hasArg("active") ? server.arg("active").toInt() : 1);
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeCcUsage() {
  applyUsage(
    server.hasArg("provider") ? server.arg("provider") : "codex",
    server.hasArg("primary") ? server.arg("primary").toInt() : 0,
    server.hasArg("secondary") ? server.arg("secondary").toInt() : 0,
    server.hasArg("reset") ? server.arg("reset") : "",
    server.hasArg("badge") ? server.arg("badge") : "",
    server.hasArg("unavailable") && server.arg("unavailable") == "1",
    server.hasArg("stale") && server.arg("stale") == "1");
  server.send(200, "application/json", "{\"ok\":1}");
}

// /redraw?bg=hex — set animBg and immediately redraw current view
void routeRedraw() {
  if (server.hasArg("bg")) {
    animBgColor = hexToRgb565(server.arg("bg"));
    drawBgColor = animBgColor;
  }
  switch (currentView) {
    case VIEW_EYES_NORMAL: drawNormalEyes(); break;
    case VIEW_EYES_SQUISH: drawSquishEyes(); break;
    case VIEW_CODE:        drawCodeView();   break;
    case VIEW_DRAW:        tft.fillScreen(drawBgColor); break;
    case VIEW_CC_STATUS:   drawStatusView(); break;
    case VIEW_CC_USAGE:    drawUsageView();  break;
  }
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeCanvas() {
  const bool on = server.hasArg("on") && server.arg("on") == "1";
  if (on) { currentView = VIEW_DRAW; tft.fillScreen(drawBgColor); }
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeDrawClear() {
  const String bg = server.hasArg("bg") ? server.arg("bg") : "#aa4818";
  drawBgColor = hexToRgb565(bg);
  animBgColor = drawBgColor;  // keep in sync
  currentView = VIEW_DRAW; termMode = false;
  tft.fillScreen(drawBgColor);
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeDrawStroke() {
  if (!server.hasArg("pts") || !server.hasArg("pen")) {
    server.send(200, "application/json", "{\"ok\":1}"); return;
  }
  const uint16_t color = hexToRgb565(server.arg("pen"));
  const String   data  = server.arg("pts");
  currentView = VIEW_DRAW;

  struct Pt { int16_t x, y; };
  Pt prev = {-1, -1};
  int start = 0;
  while (start < (int)data.length()) {
    int semi = data.indexOf(';', start);
    if (semi == -1) semi = data.length();
    String entry = data.substring(start, semi);
    const int comma = entry.indexOf(',');
    if (comma > 0) {
      const int16_t x = entry.substring(0, comma).toInt();
      const int16_t y = entry.substring(comma + 1).toInt();
      if (prev.x >= 0) {
        tft.drawLine(prev.x, prev.y, x, y, color);
        tft.drawLine(prev.x + 1, prev.y, x + 1, y, color);
        tft.drawLine(prev.x, prev.y + 1, x, y + 1, color);
      } else {
        tft.fillCircle(x, y, 2, color);
      }
      prev = {x, y};
    }
    start = semi + 1;
  }
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeBacklight() {
  setBacklight(server.hasArg("on") && server.arg("on") == "1");
  server.send(200, "application/json", "{\"ok\":1}");
}

// Convert RGB565 back to #RRGGBB for state endpoint
String rgb565ToHex(uint16_t c) {
  uint8_t r = ((c >> 11) & 0x1F) << 3;
  uint8_t g = ((c >> 5)  & 0x3F) << 2;
  uint8_t b = (c & 0x1F) << 3;
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
  return String(buf);
}

void routeState() {
  String j = "{\"view\":"; j += currentView;
  j += ",\"busy\":";   j += busy        ? "true" : "false";
  j += ",\"term\":";   j += termMode    ? "true" : "false";
  j += ",\"bl\":";     j += backlightOn ? "true" : "false";
  j += ",\"speed\":";  j += animSpeed;
  j += ",\"cc_state\":\""; j += ccState; j += "\"";
  j += "}";
  server.send(200, "application/json", j);
}

void routeNotFound() { server.send(404, "text/plain", "not found"); }

void handleJsonLine(String line) {
  line.trim();
  if (!line.length()) return;
  String type = jsonStringValue(line, "type", "");
  if (type == "state") {
    applyCcState(
      jsonStringValue(line, "source", "cc"),
      jsonStringValue(line, "state", "thinking"),
      jsonStringValue(line, "label", ""),
      jsonIntValue(line, "active", 1));
    Serial.print("{\"ok\":true,\"type\":\"state\",\"source\":\"");
    Serial.print(ccSource);
    Serial.print("\",\"state\":\"");
    Serial.print(ccState);
    Serial.println("\",\"view\":4}");
    return;
  }
  if (type == "usage") {
    applyUsage(
      jsonStringValue(line, "provider", "codex"),
      jsonIntValue(line, "primary", 0),
      jsonIntValue(line, "secondary", 0),
      jsonStringValue(line, "reset", ""),
      jsonStringValue(line, "badge", ""),
      jsonBoolValue(line, "unavailable", false),
      jsonBoolValue(line, "stale", false));
    Serial.println("{\"ok\":true,\"type\":\"usage\"}");
    return;
  }
  if (type == "boot" || type == "splash") {
    animLogoReveal();
    Serial.println("{\"ok\":true,\"type\":\"boot\"}");
    return;
  }
  if (type == "ping") {
    Serial.println("{\"ok\":true,\"name\":\"cc-mochi\"}");
    return;
  }
  Serial.println("{\"ok\":false,\"error\":\"unknown_type\"}");
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      handleJsonLine(serialLine);
      serialLine = "";
    } else if (c != '\r') {
      if (serialLine.length() < 512) serialLine += c;
      else serialLine = "";
    }
  }
}

// ═════════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BLK, OUTPUT);
  setBacklight(true);

  SPI.begin(8, -1, 10, TFT_CS);   // SCK=8, MOSI=10
  tft.init(240, 240);
  tft.setSPISpeed(40000000);
  tft.setRotation(1);
  initColours();

  // ── Boot splash ────────────────────────────────────────────
  tft.fillScreen(animBgColor);
  tft.setTextColor(C_WHITE); tft.setTextSize(3);
  tft.setCursor(DISP_W / 2 - 36, DISP_H / 2 - 22); tft.print("cc");
  tft.setCursor(DISP_W / 2 - 54, DISP_H / 2 + 14); tft.print("Mochi");
  delay(1200);

  // ── Logo shown once at startup ─────────────────────────────
  animLogoReveal();

  // ── Start WiFi ─────────────────────────────────────────────
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  // ── WiFi info screen (stays until first web request) ───────
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 4, C_ORANGE);
  tft.setTextColor(C_WHITE);  tft.setTextSize(2);
  tft.setCursor(12, 16);  tft.print("WiFi: cc-mochi");
  tft.setTextColor(C_MUTED);  tft.setTextSize(1);
  tft.setCursor(12, 44);  tft.print("password: ccmochi1234");
  tft.setTextColor(C_WHITE);  tft.setTextSize(2);
  tft.setCursor(12, 68);  tft.print("Open browser:");
  tft.setTextColor(C_ORANGE); tft.setTextSize(2);
  tft.setCursor(12, 94);  tft.print("192.168.4.1");
  tft.setTextColor(C_MUTED);  tft.setTextSize(1);
  tft.setCursor(12, 124); tft.print("press any button to start");

  // ── Register routes ────────────────────────────────────────
  server.on("/",            HTTP_GET, routeRoot);
  server.on("/cmd",         HTTP_GET, routeCmd);
  server.on("/char",        HTTP_GET, routeChar);
  server.on("/speed",       HTTP_GET, routeSpeed);
  server.on("/redraw",      HTTP_GET, routeRedraw);
  server.on("/canvas",      HTTP_GET, routeCanvas);
  server.on("/draw/clear",  HTTP_GET, routeDrawClear);
  server.on("/draw/stroke", HTTP_GET, routeDrawStroke);
  server.on("/backlight",   HTTP_GET, routeBacklight);
  server.on("/cc/state",    HTTP_GET, routeCcState);
  server.on("/cc/usage",    HTTP_GET, routeCcUsage);
  server.on("/state",       HTTP_GET, routeState);
  server.onNotFound(routeNotFound);
  server.begin();

  // WiFi info stays on screen — first button press triggers setView/cmd
  // which will replace it with the correct view
}

// ═════════════════════════════════════════════════════════════
//  LOOP
// ═════════════════════════════════════════════════════════════

void loop() {
  server.handleClient();
  handleSerialInput();
}
