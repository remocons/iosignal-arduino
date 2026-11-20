/*
 *  IOSignal device example: Remocon8266 scanner (finds the devices around it, list on the OLED)
 *
 *  The board sends #search, the way the web app does, and lists every device that answers
 *  with #notify (the devices on the same router, so the same public IP). For each it reads the
 *  kept $name (name:category) and $ui (official type or buttons) and shows them on the OLED.
 *    List:    SW1 (up) / SW3 (down) move, SW4 (right) opens the device, SW2 (left) searches again.
 *    Device:  CID, name, category, type or buttons, and its $state, which updates while open.
 *             SW1 / SW3 the previous or next device, SW2 (left) back to the list.
 *  A device that leaves (its $state "close") is marked with x in the list.
 *  The board is a card in the web app too: its "scan" button searches again.
 *  See the schematic PDF in this folder.
 *
 *  Wi-Fi: upload, then set it over USB on https://arduino.iosignal.net/wifi/ (the board keeps it),
 *  or write it in the sketch.
 *
 *  [kr] #search로 주변(같은 공유기) 장치를 찾아 OLED에 목록으로 보여 주고, 버튼으로 골라
 *       CID·이름·분류·타입·상태 같은 세부 정보를 봅니다. 왼쪽 버튼은 다시 찾기/뒤로입니다.
 *
 *  Taeo Lee <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *  MIT License
 */

#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <string.h>
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

#define SERVER_URL "io.iosignal.net"
#define SERVER_PORT 55488

#define MAX_DEVICES 16
#define LIST_ROWS 3      // rows of 16 px under the title line

IOSignalConfig config;
WiFiClient client;
IOSignal io;
// $name, the device name and its app tab ("name:category"): board setting 'name'.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "Remocon8266 Scanner:HOME");
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
Bounce2::Button upBtn, leftBtn, downBtn, rightBtn;

struct Device {
  char cid[MAX_CID_LEN + 1];
  char name[48];      // the whole $name, "name:category"
  char ui[48];        // $ui as sent: "t=switch3.1", "on,off", "p=1;b=..."
  char state[96];     // its $state, read while the device page is open
  bool gone;          // $state "close": the device left
};
Device devices[MAX_DEVICES];
uint8_t count = 0;
uint8_t selected = 0, top = 0;
bool detail = false;
unsigned long searchedAt = 0;

void sendInfo() {
  char name[64];
  iosignal::formatNameSetting(name, sizeof(name), nameSetting.value(), "Remocon8266 Scanner");
  io.signal("@$name", name);
  io.signal("@$ui", "scan");  // the card's button: search again
}

void publishState() {
  if (io.state != IO_READY) return;
  char text[48];
  snprintf(text, sizeof(text), "devices=%u;open=%s", count, detail && count ? devices[selected].cid : "-");
  io.signal("@$state", text);
}

int indexOf(const char* cid) {
  for (uint8_t i = 0; i < count; i++)
    if (strcmp(devices[i].cid, cid) == 0) return i;
  return -1;
}

void watch(const char* cid, const char* part, bool on) {
  char tag[MAX_CID_LEN + 10];
  snprintf(tag, sizeof(tag), "%s@$%s", cid, part);
  if (on) io.subscribe(tag);
  else io.unsubscribe(tag);
}

// Forget the list and ask again: every device on the router answers with #notify.
void search() {
  if (io.state != IO_READY) return;
  io.unsubscribe();  // all tags, then the ones this sketch always needs
  io.subscribe("#search");
  io.subscribe("#notify");
  count = selected = top = 0;
  detail = false;
  searchedAt = millis();
  io.signal("#search");
  Serial.println("SEARCH");
  publishState();
}

// ---------------------------------------------------------------- text helpers
// "name:category" -> name, or the category with category = true.
void namePart(const char* full, bool category, char* out, size_t size) {
  const char* colon = strchr(full, ':');
  const char* from = category ? (colon ? colon + 1 : "") : full;
  size_t n = category ? strlen(from) : (colon ? (size_t)(colon - full) : strlen(full));
  if (n >= size) n = size - 1;
  memcpy(out, from, n);
  out[n] = 0;
}

// What $ui says, short: the official type (t=), "panel" (p=1) or the button list.
void uiSummary(const char* ui, char* out, size_t size) {
  const char* t = strstr(ui, "t=");
  if ((ui[0] == 't' || ui[0] == 'p' || ui[0] == 'v') && ui[1] == '=') {
    if (t && (t == ui || t[-1] == ';')) {
      size_t n = 0;
      t += 2;
      while (t[n] && t[n] != ';' && n < size - 1) n++;
      memcpy(out, t, n);
      out[n] = 0;
      return;
    }
    snprintf(out, size, strstr(ui, "p=1") ? "own panel" : "extended");
    return;
  }
  snprintf(out, size, ui[0] ? "btn %s" : "-", ui);
}

// Draws text from x, cut where the next UTF-8 character would pass maxWidth. Each character is
// measured once: glyph lookups in the large Hangul font are slow.
void drawFit(uint8_t x, uint8_t y, const char* text, uint8_t maxWidth) {
  char buf[64];
  size_t n = 0;
  uint16_t width = 0;
  while (text[n] && n < sizeof(buf) - 5) {
    size_t len = 1;
    while ((text[n + len] & 0xC0) == 0x80) len++;  // one UTF-8 character
    char ch[5] = { 0 };
    memcpy(ch, text + n, len < 4 ? len : 4);
    width += u8g2.getUTF8Width(ch);
    if (width > maxWidth) break;
    n += len;
  }
  memcpy(buf, text, n);
  buf[n] = 0;
  u8g2.drawUTF8(x, y, buf);
}

// ---------------------------------------------------------------- OLED
void drawList() {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 0);
  if (!config.connected()) { u8g2.print("Wi-Fi..."); return; }
  if (io.state != IO_READY) { u8g2.print("Connecting..."); return; }
  u8g2.print("Devices ");
  u8g2.print(count);
  u8g2.setCursor(80, 0);
  u8g2.print(io.cid);
  u8g2.drawHLine(0, 11, 128);
  if (!count) {
    u8g2.drawStr(0, 20, millis() - searchedAt < 3000 ? "Searching..." : "No device answered");
    u8g2.drawStr(0, 36, "SW2: search again");
    return;
  }
  u8g2.setFont(u8g2_font_unifont_t_korean2);  // 16 px, Hangul included
  for (uint8_t row = 0; row < LIST_ROWS && top + row < count; row++) {
    const Device& d = devices[top + row];
    const uint8_t y = 13 + row * 17;
    char name[48];
    namePart(d.name, false, name, sizeof(name));
    if (top + row == selected) { u8g2.drawBox(0, y, 128, 17); u8g2.setDrawColor(0); }
    drawFit(2, y, name[0] ? name : d.cid, d.gone ? 112 : 124);
    if (d.gone) u8g2.drawUTF8(118, y, "x");
    u8g2.setDrawColor(1);
  }
}

void drawDetail() {
  const Device& d = devices[selected];
  char text[48];
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 0);
  u8g2.print(selected + 1);
  u8g2.print('/');
  u8g2.print(count);
  u8g2.print("  ");
  u8g2.print(d.cid);
  if (d.gone) u8g2.print(" (gone)");
  u8g2.setFont(u8g2_font_unifont_t_korean2);
  namePart(d.name, false, text, sizeof(text));
  drawFit(0, 11, text[0] ? text : "(no name)", 128);
  namePart(d.name, true, text, sizeof(text));
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 29, "cat");
  u8g2.setFont(u8g2_font_unifont_t_korean2);
  drawFit(22, 26, text[0] ? text : "-", 106);
  u8g2.setFont(u8g2_font_6x10_tf);
  uiSummary(d.ui, text, sizeof(text));
  u8g2.drawStr(0, 43, "ui");
  drawFit(22, 43, text, 106);
  u8g2.drawStr(0, 54, "st");
  drawFit(22, 54, d.state[0] ? d.state : "...", 106);
}

void show() {
  u8g2.clearBuffer();
  u8g2.setFontPosTop();
  if (detail && count && io.state == IO_READY) drawDetail();
  else drawList();
  u8g2.sendBuffer();
}

// ---------------------------------------------------------------- moving around
void openDetail(uint8_t index) {
  if (detail && count) watch(devices[selected].cid, "state", false);
  selected = index;
  detail = true;
  devices[selected].state[0] = 0;
  watch(devices[selected].cid, "state", true);  // the kept state arrives at once, then each change
  publishState();
}

void closeDetail() {
  if (detail && count) watch(devices[selected].cid, "state", false);
  detail = false;
  publishState();
}

void move(int step) {
  if (!count) return;
  const uint8_t next = (selected + count + step) % count;
  if (detail) { openDetail(next); return; }
  selected = next;
  if (selected < top) top = selected;
  if (selected >= top + LIST_ROWS) top = selected - LIST_ROWS + 1;
}

// ---------------------------------------------------------------- IOSignal
void onReady() {
  Serial.print("onReady cid: ");
  Serial.println(io.cid);
  sendInfo();
  search();
  io.signal("#notify", io.cid);
  show();
}

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  // A copy with an end: MJSON values (two or more) are a JSON array that is not always terminated.
  static char text[512];
  size_t n = payload && (type == IOSignal::TEXT || type == IOSignal::MJSON) ? (size < sizeof(text) - 1 ? size : sizeof(text) - 1) : 0;
  memcpy(text, payload, n);
  text[n] = 0;

  if (strcmp(tag, "#search") == 0) { io.signal("#notify", io.cid); return; }
  if (strcmp(tag, "#notify") == 0) {
    if (!text[0] || text[0] == '#' || strcmp(text, io.cid) == 0 || strlen(text) > MAX_CID_LEN) return;
    int i = indexOf(text);
    if (i >= 0) { devices[i].gone = false; show(); return; }
    if (count >= MAX_DEVICES) return;
    Device& d = devices[count++];
    memset(&d, 0, sizeof(d));
    strcpy(d.cid, text);
    watch(d.cid, "name", true);  // kept by the server: they arrive at once
    watch(d.cid, "ui", true);
    Serial.print("FOUND ");
    Serial.println(d.cid);
    publishState();
    show();
    return;
  }

  const char* at = strstr(tag, "@$");
  if (at) {
    char cid[MAX_CID_LEN + 1];
    const size_t len = (size_t)(at - tag) < sizeof(cid) ? (size_t)(at - tag) : sizeof(cid) - 1;
    memcpy(cid, tag, len);
    cid[len] = 0;
    int i = indexOf(cid);
    if (i < 0) return;
    Device& d = devices[i];
    if (strcmp(at, "@$name") == 0) strncpy(d.name, text, sizeof(d.name) - 1);
    else if (strcmp(at, "@$ui") == 0) strncpy(d.ui, text, sizeof(d.ui) - 1);
    else if (strcmp(at, "@$state") == 0) {
      if (strcmp(text, "close") == 0) d.gone = true;
      // Two or more values come as ["first",{...}]: show the first one.
      const char* s = text;
      if (type == IOSignal::MJSON && s[0] == '[' && s[1] == '"') {
        s += 2;
        const char* end = strchr(s, '"');
        size_t m = end ? (size_t)(end - s) : strlen(s);
        if (m >= sizeof(d.state)) m = sizeof(d.state) - 1;
        memcpy(d.state, s, m);
        d.state[m] = 0;
      } else strncpy(d.state, s, sizeof(d.state) - 1);
    }
    show();
    return;
  }

  if (strcmp(tag, "@") == 0) {  // the card's "scan" button; every command gets the state back
    if (strcmp(text, "scan") == 0) search();
    publishState();
    show();
  }
}

void setup() {
  upBtn.attach(BTN_UP, INPUT);  // external pull-up
  leftBtn.attach(BTN_LEFT, INPUT_PULLUP);
  downBtn.attach(BTN_DOWN, INPUT_PULLUP);
  rightBtn.attach(BTN_RIGHT, INPUT_PULLUP);
  for (Bounce2::Button* b : { &upBtn, &leftBtn, &downBtn, &rightBtn }) {
    b->interval(5);
    b->setPressedState(LOW);
  }
  Serial.begin(115200);
  Serial.println();
  Serial.println("Remocon8266 scanner");

  u8g2.begin();
  u8g2.enableUTF8Print();
  show();

  config.begin(Serial, "Remocon8266 Scanner", WIFI_ID, WIFI_KEY);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer(SERVER_URL, SERVER_PORT);
  config.printServer();
  io.setRxBuffer(1024);  // some devices keep long states (bayo.tv sends the video details)
  io.setLog([](const char* line) { Serial.println(line); });
  io.onReady(&onReady);
  io.onMessage(&onMessage);
  if (config.key()) io.auth(config.key());  // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}

bool wasConnected = false;
uint8_t lastIoState = 255;
bool searching = false;

void loop() {
  config.loop();  // USB setup requests, Wi-Fi reconnect
  const bool connected = config.connected();
  if (connected) io.loop();
  if (connected != wasConnected || io.state != lastIoState) {
    wasConnected = connected;
    lastIoState = io.state;
    show();
  }
  // "Searching..." turns into "No device answered" after 3 seconds.
  const bool nowSearching = millis() - searchedAt < 3000;
  if (searching != nowSearching) { searching = nowSearching; show(); }

  upBtn.update(); leftBtn.update(); downBtn.update(); rightBtn.update();
  if (io.state != IO_READY) return;
  bool changed = true;
  if (upBtn.pressed()) move(-1);
  else if (downBtn.pressed()) move(1);
  else if (rightBtn.pressed()) { if (count && !detail) openDetail(selected); }
  else if (leftBtn.pressed()) { if (detail) closeDetail(); else search(); }
  else changed = false;
  if (changed) show();
}
