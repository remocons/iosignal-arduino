/*
 *  IOSignal Remocon example: Remocon8266 TV remote (controls a bayo.tv screen, state on the OLED)
 *
 *  The four buttons send commands to a bayo.tv screen (https://bayo.tv) by its CID, and the
 *  screen's state (playing or paused, volume, mute, shuffle, title) is drawn on the OLED.
 *    SW1 (up)    playToggle     SW2 (left)  left (previous chapter, or video)
 *    SW3 (down)  down (next category)        SW4 (right) right (next chapter, or video)
 *  left and right move by chapter when the video has one next to it, else to the previous or next video
 *  (bayo.tv decides; prev and next always move videos, chapterPrev and chapterNext only chapters).
 *  Each button is a board setting (b1-b4) holding a bayo.tv command, e.g. "vol+", "mute", "1".
 *  In a video with chapters the OLED shows the chapter playing (its title and number, like 3/41).
 *  The screen: board setting 'target' (its CID). Empty finds one: #search, and the first device
 *  whose name starts with "bayo.tv" (the same router, so the same public IP). Until one is known,
 *  the buttons send to the #screen home channel, which every bayo.tv screen on the router follows.
 *  A found screen that leaves (a reload gives it a new CID) is searched for again, every 5 seconds.
 *  The board is a card in the web app too: its buttons go through it to the screen,
 *  and "target=<cid>" picks another screen (remocon pub "<this cid>@" "target=?3vtg").
 *  See the schematic PDF in this folder.
 *
 *  Wi-Fi: upload, then set it over USB on https://arduino.iosignal.net/wifi/ (the board keeps it,
 *  with target and b1-b4), or write it in the sketch.
 *
 *  [kr] Remocon8266 보드의 버튼 4개로 bayo.tv 화면을 제어하고, 재생 상태·음량·제목을 OLED에 보여 줍니다.
 *       대상 화면(target)과 버튼 명령(b1~b4)은 보드 설정입니다. target이 비면 같은 공유기의 bayo.tv를 찾습니다.
 *
 *  Taeo Lee <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *  MIT License
 */

#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <string.h>
#include <stdlib.h>
#include <Bounce2.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <IOSignalUi.h>

// Buttons
#define BTN_UP     16   // D0  SW1 *EXTERNAL HARDWARE PULLUP
#define BTN_LEFT    0   // D3  SW2
#define BTN_DOWN   14   // D5  SW3
#define BTN_RIGHT  13   // D7  SW4

// I2C OLED: SDA GPIO4 (D2), SCL GPIO5 (D1).

#define WIFI_ID  "WIFI_SSID"
#define WIFI_KEY "WIFI_PASS"

#define SERVER_URL "io.remocon.kr"
#define SERVER_PORT 55488

IOSignalConfig config;
WiFiClient client;
IOSignal io;
// $name, the device name and its app tab ("name:category"): board setting 'name'.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "Remocon8266 TV:HOME");
// The bayo.tv screen's CID; empty: the first bayo.tv found on this router.
IOSignalSetting target(config, "target", "bayo.tv CID (empty: find)", "", IOSignalSetting::CID, IOSignalSetting::REMOTE);
// The command each button sends (bayo.tv commands: play, pause, playToggle, left, right, prev, next, chapterPrev,
// chapterNext, up, down, vol+, vol-, mute, shuffle, 1-3).
IOSignalSetting b1(config, "b1", "SW1 command", "playToggle", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
IOSignalSetting b2(config, "b2", "SW2 command", "left", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
IOSignalSetting b3(config, "b3", "SW3 command", "down", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
IOSignalSetting b4(config, "b4", "SW4 command", "right", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
IOSignalSetting* const COMMANDS[] = { &b1, &b2, &b3, &b4 };

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
Bounce2::Button buttons[4];
const uint8_t BUTTON_PINS[] = { BTN_UP, BTN_LEFT, BTN_DOWN, BTN_RIGHT };

// The screen in use and what its last $state said.
char tv[MAX_CID_LEN + 1] = "";
struct {
  bool known = false;     // a $state arrived
  char state[12] = "";    // playing, paused, ...
  char title[160] = "";   // the chapter playing (chapterTitle), else the video's title
  int volume = -1;
  int chapter = 0, chapters = 0;  // 1-based chapter number and how many, 0 without chapters
  bool muted = false, shuffle = false;
} screen;
// The title's lines (breakTitle): where each starts in screen.title, and how many.
#define TITLE_LINES 12
uint8_t lineStart[TITLE_LINES];
uint8_t lineEnd[TITLE_LINES];  // without the space a line was broken at
uint8_t lineCount = 0;
char lastSent[24] = "";   // the last command, shown until the state answers
unsigned long titlePageAt = 0;
uint8_t titlePage = 0;

void sendInfo() {
  char name[64];
  remocon::formatNameSetting(name, sizeof(name), nameSetting.value(), "Remocon8266 TV");
  io.signal("@$name", name);
  // The card's buttons are bayo.tv commands: this board sends them on to the screen.
  io.signal("@$ui", "playToggle,left,right,down,vol+,vol-,mute");
}

// This board's own state for its card: tv=?3vtg;link=1;state=playing;vol=70
void publishState() {
  if (io.state != IO_READY) return;
  char text[80];
  snprintf(text, sizeof(text), "tv=%s;link=%c;state=%s;vol=%d", tv[0] ? tv : "-", screen.known ? '1' : '0',
           screen.known ? screen.state : "-", screen.volume);
  io.signal("@$state", text);
}

// ---------------------------------------------------------------- the screen's $state
// bayo.tv sends two values: "state=playing" and an object with title, volume, muted, shuffle, ...
// They arrive as one JSON array (MJSON) or as text. Only a few fields are read, without a JSON library.
const char* findKey(const char* json, const char* key) {
  char pattern[24];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char* p = strstr(json, pattern);
  return p ? p + strlen(pattern) : nullptr;
}

void readString(const char* json, const char* key, char* out, size_t outSize) {
  out[0] = 0;
  const char* p = findKey(json, key);
  if (!p || *p != '"') return;
  size_t n = 0;
  for (p++; *p && *p != '"' && n < outSize - 1; p++) {
    if (*p == '\\' && p[1]) {  // \" \\ \/ kept as the character; \n and other escapes become a space
      p++;
      out[n++] = (*p == '"' || *p == '\\' || *p == '/') ? *p : ' ';
      if (*p == 'u') p += 4;
    } else out[n++] = *p;
  }
  // Cut short: never end inside a UTF-8 character.
  if (*p && *p != '"') {
    size_t k = n;
    while (k && (out[k - 1] & 0xC0) == 0x80) k--;
    if (k && (out[k - 1] & 0x80)) {
      const uint8_t lead = out[k - 1];
      const size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
      if (n - (k - 1) < need) n = k - 1;
    }
  }
  out[n] = 0;
}

bool readBool(const char* json, const char* key) {
  const char* p = findKey(json, key);
  return p && strncmp(p, "true", 4) == 0;
}

void readScreenState(const char* text) {
  if (strcmp(text, "close") == 0) {  // the server says the screen left
    screen.known = false;
    strcpy(screen.state, "offline");
    return;
  }
  screen.known = true;
  const char* s = strstr(text, "state=");
  screen.state[0] = 0;
  if (s) {
    s += 6;
    size_t n = 0;
    while (s[n] && s[n] != ';' && s[n] != '"' && n < sizeof(screen.state) - 1) n++;
    memcpy(screen.state, s, n);
    screen.state[n] = 0;
  }
  const char* v = findKey(text, "volume");
  screen.volume = v ? atoi(v) : -1;
  screen.muted = readBool(text, "muted");
  screen.shuffle = readBool(text, "shuffle");
  lastSent[0] = 0;
  // A new title starts again on its first page; a volume or play change keeps the page shown.
  char title[sizeof(screen.title)];
  // A video with chapters: the chapter playing now (bayo.tv sends chapterTitle and chapterCount).
  readString(text, "chapterTitle", title, sizeof(title));
  const char* count = findKey(text, "chapterCount");
  const char* index = findKey(text, "index");
  screen.chapters = title[0] && count ? atoi(count) : 0;
  screen.chapter = screen.chapters && index ? atoi(index) + 1 : 0;
  if (!title[0]) readString(text, "title", title, sizeof(title));
  if (strcmp(title, screen.title) != 0 || !lineCount) {
    strcpy(screen.title, title);
    titlePage = 0;
    titlePageAt = millis();
    breakTitle();
  }
}

// ---------------------------------------------------------------- choosing the screen
void useScreen(const char* cid) {
  if (strcmp(tv, cid) == 0) return;
  if (tv[0]) {
    char tag[MAX_CID_LEN + 10];
    snprintf(tag, sizeof(tag), "%s@$state", tv);
    io.unsubscribe(tag);
  }
  strncpy(tv, cid, sizeof(tv) - 1);
  tv[sizeof(tv) - 1] = 0;
  screen.known = false;
  screen.title[0] = 0;
  lineCount = 0;
  if (tv[0] && io.state == IO_READY) {
    char tag[MAX_CID_LEN + 10];
    snprintf(tag, sizeof(tag), "%s@$state", tv);
    io.subscribe(tag);  // the server sends the kept state at once
  }
  Serial.print("TV ");
  Serial.println(tv[0] ? tv : "(searching)");
  publishState();
}

unsigned long searchedAt = 0;

void findScreen() {
  if (target.value()[0] || io.state != IO_READY) return;
  searchedAt = millis();
  io.subscribe("#notify");
  io.signal("#search");
}

void sendCommand(const char* command) {
  if (!command[0]) return;
  char tag[MAX_CID_LEN + 2];
  if (tv[0]) snprintf(tag, sizeof(tag), "%s@", tv);
  else strcpy(tag, "#screen");  // no screen known yet: every bayo.tv screen on this router
  io.signal(tag, command);
  snprintf(lastSent, sizeof(lastSent), "> %s", command);
  Serial.print(tag);
  Serial.print(' ');
  Serial.println(command);
}

// ---------------------------------------------------------------- OLED
// The title's lines: where each starts in screen.title, worked out once per state (Hangul glyph lookups
// in the large font are slow, so each character is measured once, never the whole line again).

void breakTitle() {
  u8g2.setFont(u8g2_font_unifont_t_korean2);
  lineCount = 0;
  uint16_t width = 0;
  bool open = false;  // a line is started (a character missing from the font has width 0, so not width)
  size_t i = 0;
  while (screen.title[i]) {
    if (!open) {
      if (screen.title[i] == ' ') { i++; continue; }  // no space at the start of a line
      if (lineCount == TITLE_LINES) break;
      lineStart[lineCount++] = i;
      lineEnd[lineCount - 1] = i;
      open = true;
      width = 0;
    }
    size_t len = 1;
    while ((screen.title[i + len] & 0xC0) == 0x80) len++;  // one UTF-8 character
    char ch[5] = { 0 };
    memcpy(ch, screen.title + i, len < 4 ? len : 4);
    const uint16_t w = u8g2.getUTF8Width(ch);
    if (width && width + w > 128) { open = false; continue; }  // the character starts the next line
    width += w;
    i += len;
    if (screen.title[i - len] != ' ') lineEnd[lineCount - 1] = i;
    yield();
  }
  Serial.print("TITLE ");
  Serial.print(lineCount);
  Serial.println(" lines");
  for (uint8_t k = 0; k < lineCount; k++) {
    Serial.print("  ");
    Serial.write((const uint8_t*)screen.title + lineStart[k], lineEnd[k] - lineStart[k]);
    Serial.println('|');
  }
}

void drawTitle() {
  u8g2.setFont(u8g2_font_unifont_t_korean2);  // 16 px, Hangul included
  for (uint8_t k = 0; k < 2; k++) {
    const uint8_t line = titlePage * 2 + k;
    if (line >= lineCount) break;
    char buf[64];
    size_t n = lineEnd[line] - lineStart[line];
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, screen.title + lineStart[line], n);
    buf[n] = 0;
    u8g2.drawUTF8(0, 14 + k * 17, buf);
  }
}

void show() {
  u8g2.clearBuffer();
  u8g2.setFontPosTop();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 0);
  if (!config.connected()) u8g2.print("Wi-Fi...");
  else if (io.state != IO_READY) u8g2.print("Connecting...");
  else if (!tv[0]) u8g2.print("Finding bayo.tv...");
  else if (!screen.known) { u8g2.print("TV "); u8g2.print(tv); u8g2.print(screen.state[0] ? " offline" : " ..."); }
  else {
    // Play or pause sign, then volume and the M (mute) and S (shuffle) marks.
    if (strcmp(screen.state, "playing") == 0) u8g2.drawTriangle(1, 0, 1, 9, 8, 4);
    else { u8g2.drawBox(1, 0, 3, 9); u8g2.drawBox(6, 0, 3, 9); }
    u8g2.setCursor(13, 0);
    if (screen.chapters) {  // in a chapter: its number instead of the word (the sign shows play or pause)
      u8g2.print(screen.chapter);
      u8g2.print('/');
      u8g2.print(screen.chapters);
    } else u8g2.print(screen.state);
    u8g2.setCursor(74, 0);
    if (screen.volume >= 0) { u8g2.print("v"); u8g2.print(screen.volume); }
    if (screen.muted) u8g2.drawStr(104, 0, "M");
    if (screen.shuffle) u8g2.drawStr(116, 0, "S");
  }
  u8g2.drawHLine(0, 11, 128);

  // The title, two lines at a time, in pages that change every 3 seconds.
  if (titlePage * 2 >= lineCount) titlePage = 0;
  drawTitle();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 53);
  if (lastSent[0]) u8g2.print(lastSent);
  else if (tv[0]) { u8g2.print("TV "); u8g2.print(tv); }
  if (io.state == IO_READY) { u8g2.setCursor(80, 53); u8g2.print(io.cid); }
  u8g2.sendBuffer();
}

// ---------------------------------------------------------------- IOSignal
void onReady() {
  Serial.print("onReady cid: ");
  Serial.println(io.cid);
  io.subscribe("#search");
  sendInfo();
  io.signal("#notify", io.cid);
  const char* chosen = target.value();
  tv[0] = 0;
  if (chosen[0]) useScreen(chosen);
  else findScreen();
  publishState();
  show();
}

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (strcmp(tag, "#search") == 0) { io.signal("#notify", io.cid); return; }
  if (!payload || !size || (type != IOSignal::TEXT && type != IOSignal::MJSON)) return;

  // A copy with an end, for the string functions (MJSON is not always terminated).
  static char text[1024];
  const size_t n = size < sizeof(text) - 1 ? size : sizeof(text) - 1;
  memcpy(text, payload, n);
  text[n] = 0;

  if (strcmp(tag, "#notify") == 0) {  // a device on this router: ask its name
    if (!tv[0] && !target.value()[0] && strcmp(text, io.cid) != 0 && text[0] != '#') {
      char nameTag[MAX_CID_LEN + 10];
      snprintf(nameTag, sizeof(nameTag), "%s@$name", text);
      io.subscribe(nameTag);
    }
    return;
  }
  const char* at = strstr(tag, "@$");
  if (at && strcmp(at, "@$name") == 0) {  // the first bayo.tv becomes the screen
    char cid[MAX_CID_LEN + 1];
    const size_t len = at - tag < (int)sizeof(cid) - 1 ? at - tag : sizeof(cid) - 1;
    memcpy(cid, tag, len);
    cid[len] = 0;
    io.unsubscribe(tag);
    if (!tv[0] && !target.value()[0] && strncmp(text, "bayo.tv", 7) == 0) {
      useScreen(cid);
      io.unsubscribe("#notify");
      show();
    }
    return;
  }
  if (at && strcmp(at, "@$state") == 0) {
    if (strncmp(tag, tv, at - tag) != 0 || tv[at - tag] != 0) return;
    if (strcmp(text, "close") == 0 && !target.value()[0]) {
      // A found screen left (a reload gives it a new CID): find one again.
      useScreen("");
      findScreen();
      show();
      return;
    }
    readScreenState(text);
    publishState();
    show();
    return;
  }

  // Commands to this board: target=<cid>, b1=<command>, b1-b4 (press), or a bayo.tv command to pass on.
  if (strcmp(tag, "@") == 0 && type == IOSignal::TEXT) {
    Serial.print("CMD ");
    Serial.println(text);
    const char* eq = strchr(text, '=');
    if (eq) {
      const size_t len = eq - text;
      bool ok = false;
      if (len == 6 && strncmp(text, "target", 6) == 0) ok = target.setFromRemote(eq + 1);
      for (uint8_t i = 0; i < 4 && !ok; i++)
        if (len == 2 && text[0] == 'b' && text[1] == char('1' + i)) ok = COMMANDS[i]->setFromRemote(eq + 1);
    } else if (text[0] == 'b' && text[1] >= '1' && text[1] <= '4' && !text[2]) {
      sendCommand(COMMANDS[text[1] - '1']->value());
    } else {
      sendCommand(text);
    }
    publishState();
    show();
  }
}

void setup() {
  for (uint8_t i = 0; i < 4; i++) {
    buttons[i].attach(BUTTON_PINS[i], BUTTON_PINS[i] == BTN_UP ? INPUT : INPUT_PULLUP);  // GPIO16: external pull-up
    buttons[i].interval(5);
    buttons[i].setPressedState(LOW);
  }
  Serial.begin(115200);
  Serial.println();
  Serial.println("Remocon8266 TV remote");

  u8g2.begin();
  u8g2.enableUTF8Print();
  show();

  config.begin(Serial, "Remocon8266 TV", WIFI_ID, WIFI_KEY);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  // A new target: use it, or find a screen again when it was cleared.
  target.onChange([](const char* cid) {
    if (io.state != IO_READY) return;
    if (cid[0]) useScreen(cid);
    else { useScreen(""); findScreen(); }
    show();
  });
  config.setServer(SERVER_URL, SERVER_PORT);
  config.printServer();
  Serial.print("TARGET ");
  Serial.println(target.value()[0] ? target.value() : "(find bayo.tv)");

  io.setRxBuffer(1024);  // bayo.tv's state carries the video title and more
  io.setLog([](const char* line) { Serial.println(line); });
  io.onReady(&onReady);
  io.onMessage(&onMessage);
  if (config.key()) io.auth(config.key());  // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}

bool wasConnected = false;
uint8_t lastIoState = 255;

void loop() {
  config.loop();  // USB setup requests, Wi-Fi reconnect
  const bool connected = config.connected();
  if (connected) io.loop();
  if (connected != wasConnected || io.state != lastIoState) {
    wasConnected = connected;
    lastIoState = io.state;
    show();
  }
  // No screen yet: search again every 5 seconds (one may open later, or still be reloading).
  if (!tv[0] && !target.value()[0] && io.state == IO_READY && millis() - searchedAt > 5000) findScreen();
  if (lineCount > 2 && millis() - titlePageAt > 3000) {  // the next two lines of a long title
    titlePageAt = millis();
    titlePage++;
    show();
  }
  for (uint8_t i = 0; i < 4; i++) {
    buttons[i].update();
    if (buttons[i].pressed() && io.state == IO_READY) {
      sendCommand(COMMANDS[i]->value());
      show();
    }
  }
}
