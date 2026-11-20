/*
 *  IOSignal Remocon example: Remocon8266 basic (controlled from the web app, state on the OLED)
 *
 *  remocon.kr shows the board as a 3-way switch (switch3.1):
 *    s1 = built-in LED (active low), s2 = OUT1 (GPIO15, D8), s3 = OUT2 (GPIO12, D6).
 *  Every change, from the app, a CLI or a button, is published and drawn on the OLED:
 *  three boxes (filled = on) and the last change with where it came from.
 *  Buttons: SW1-SW3 toggle s1-s3 on the board, SW4 shows this board's CID, IP and server.
 *  Text sent to <cid>@msg is shown on the OLED for a few seconds (Korean too).
 *  See the schematic PDF in this folder.
 *
 *  Wi-Fi: upload, then set it over USB on https://arduino.iosignal.net/wifi/ (the board keeps it),
 *  or write it in the sketch (then the sketch values are used at every boot). The same page can
 *  set another IOSignal server, a device key and the board setting 'name'.
 *  Then open https://remocon.kr in a browser on the same router.
 *
 *  [kr] 웹앱(remocon.kr)에서 Remocon8266 보드의 출력 3개를 3구 스위치로 제어하고,
 *       바뀐 상태와 누가 바꿨는지를 OLED에 보여 줍니다. SW1~SW3은 출력 토글, SW4는 접속 정보입니다.
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

// Outputs
#define OUT1       15   // D8  LED, NeoPixel, Servo or else.
#define OUT2       12   // D6

#define WIFI_ID  "WIFI_SSID"
#define WIFI_KEY "WIFI_PASS"

#define SERVER_URL "io.remocon.kr"
#define SERVER_PORT 55488

IOSignalConfig config;
WiFiClient client;
IOSignal io;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "Remocon8266:HOME");
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// 1 means on, whatever the pin polarity.
char states[] = "000";
const bool activeLow[] = { true, false, false };
const uint8_t pinMap[] = { LED_BUILTIN, OUT1, OUT2 };
const char* const LABELS[] = { "LED", "OUT1", "OUT2" };

Bounce2::Button buttons[4];
const uint8_t BUTTON_PINS[] = { BTN_UP, BTN_LEFT, BTN_DOWN, BTN_RIGHT };

char lastChange[32] = "";        // "app s2=1", "SW1 s1=0", shown under the boxes
char message[96] = "";           // the last @msg, shown until messageUntil
unsigned long messageUntil = 0;
bool infoPage = false;           // SW4: CID, IP and server instead of the states

void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  remocon::formatNameSetting(name, sizeof(name), nameSetting.value(), "Remocon8266");
  io.signal("@$name", name);
  io.signal("@$ui", "t=switch3.1");
}

void publishState() {
  if (io.state != IO_READY) return;
  char text[30];
  snprintf(text, sizeof(text), "s1=%c;s2=%c;s3=%c", states[0], states[1], states[2]);
  io.signal("@$state", text);
}

// ---------------------------------------------------------------- OLED
void drawStates() {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 0);
  if (!config.connected()) u8g2.print("Wi-Fi...");
  else if (io.state != IO_READY) u8g2.print("Connecting...");
  else { u8g2.print("CID "); u8g2.print(io.cid); }
  for (uint8_t i = 0; i < 3; i++) {
    const uint8_t x = 2 + i * 42, y = 13, w = 38, h = 30;
    const uint8_t tx = x + (w - u8g2.getStrWidth(LABELS[i])) / 2;
    if (states[i] == '1') {
      u8g2.drawRBox(x, y, w, h, 3);
      u8g2.setDrawColor(0);
      u8g2.drawStr(tx, y + 6, LABELS[i]);
      u8g2.drawStr(x + 13, y + 18, "ON");
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawRFrame(x, y, w, h, 3);
      u8g2.drawStr(tx, y + 6, LABELS[i]);
      u8g2.drawStr(x + 10, y + 18, "off");
    }
  }
  u8g2.setCursor(0, 53);
  u8g2.print(lastChange[0] ? lastChange : "remocon.kr");
}

void drawInfo() {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 0);
  u8g2.print("CID  "); u8g2.print(io.state == IO_READY ? io.cid : "-");
  u8g2.setCursor(0, 13);
  u8g2.print("IP   "); u8g2.print(config.connected() ? WiFi.localIP().toString() : "-");
  u8g2.setCursor(0, 26);
  u8g2.print("SRV  "); u8g2.print(config.host());
  u8g2.setCursor(0, 39);
  u8g2.print("PORT "); u8g2.print(config.port());
  u8g2.setCursor(0, 53);
  u8g2.print("SW4: back");
}

void drawMessage() {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 0, "msg");
  u8g2.setFont(u8g2_font_unifont_t_korean2);  // 16 px, Hangul included
  // Up to three lines, broken where the next character would not fit. Each character is measured
  // once: glyph lookups in the large Hangul font are slow.
  const char* p = message;
  for (uint8_t line = 0; line < 3 && *p; line++) {
    char buf[64];
    size_t n = 0;
    uint16_t width = 0;
    while (*p && n < sizeof(buf) - 5) {
      size_t len = 1;
      while ((p[len] & 0xC0) == 0x80) len++;  // one UTF-8 character
      char ch[5] = { 0 };
      memcpy(ch, p, len < 4 ? len : 4);
      width += u8g2.getUTF8Width(ch);
      if (n && width > 128) break;
      memcpy(buf + n, p, len);
      n += len;
      p += len;
    }
    buf[n] = 0;
    u8g2.drawUTF8(0, 12 + line * 17, buf);
  }
}

void show() {
  u8g2.clearBuffer();
  u8g2.setFontPosTop();
  if (messageUntil) drawMessage();
  else if (infoPage) drawInfo();
  else drawStates();
  u8g2.sendBuffer();
}

// ---------------------------------------------------------------- outputs
void setOutput(uint8_t i, char value, const char* from) {
  states[i] = value;
  digitalWrite(pinMap[i], (value == '1') != activeLow[i]);
  snprintf(lastChange, sizeof(lastChange), "%s s%u=%c", from, i + 1, value);
  Serial.println(lastChange);
}

void onReady() {
  Serial.print("onReady cid: ");
  Serial.println(io.cid);
  io.subscribe("#search");
  sendInfo();
  publishState();
  io.signal("#notify", io.cid);
  show();
}

void onMessage(char* tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize) {
  if (strcmp(tag, "#search") == 0) { io.signal("#notify", io.cid); return; }

  // Ignore non-text or unterminated payloads before using string APIs.
  if (payloadType != IOSignal::PAYLOAD_TYPE::TEXT || !payload ||
      !payloadSize || !memchr(payload, 0, payloadSize)) return;
  const char* c = (const char*)payload;

  if (strcmp(tag, "@msg") == 0) {
    strncpy(message, c, sizeof(message) - 1);
    message[sizeof(message) - 1] = 0;
    messageUntil = millis() + 6000;
    show();
    return;
  }
  if (strcmp(tag, "@") != 0) return;

  // s1=1 sets an output, all=1 / all=0 sets all three (device rules). Unknown commands still get a reply.
  Serial.print("CMD ");
  Serial.println(c);
  int i = c[0] == 's' ? c[1] - '1' : -1;
  if (i >= 0 && i < 3 && c[2] == '=' && (c[3] == '0' || c[3] == '1') && c[4] == 0) {
    setOutput(i, c[3], "app");
  } else if (strcmp(c, "all=1") == 0 || strcmp(c, "all=0") == 0) {
    for (uint8_t k = 0; k < 3; k++) setOutput(k, c[4], "app");
    snprintf(lastChange, sizeof(lastChange), "app all=%c", c[4]);
  }
  publishState();
  infoPage = false;
  show();
}

void setup() {
  for (uint8_t i = 0; i < 3; i++) {
    pinMode(pinMap[i], OUTPUT);
    digitalWrite(pinMap[i], activeLow[i] ? HIGH : LOW);  // off
  }
  for (uint8_t i = 0; i < 4; i++) {
    buttons[i].attach(BUTTON_PINS[i], BUTTON_PINS[i] == BTN_UP ? INPUT : INPUT_PULLUP);  // GPIO16: external pull-up
    buttons[i].interval(5);
    buttons[i].setPressedState(LOW);
  }

  Serial.begin(115200);
  Serial.println();
  Serial.println("Remocon8266 basic");

  u8g2.begin();
  u8g2.enableUTF8Print();
  show();

  // Wi-Fi, server and device key: set over USB, or the sketch values.
  config.begin(Serial, "Remocon8266", WIFI_ID, WIFI_KEY);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer(SERVER_URL, SERVER_PORT);
  config.printServer();
  io.setRxBuffer(200);
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
  if (connected != wasConnected || io.state != lastIoState) {  // the first line follows the connection
    wasConnected = connected;
    lastIoState = io.state;
    show();
  }
  if (messageUntil && (long)(millis() - messageUntil) >= 0) { messageUntil = 0; show(); }

  for (uint8_t i = 0; i < 4; i++) {
    buttons[i].update();
    if (!buttons[i].pressed()) continue;
    messageUntil = 0;
    if (i == 3) {
      infoPage = !infoPage;
    } else {
      char label[4] = { 'S', 'W', char('1' + i), 0 };
      setOutput(i, states[i] == '1' ? '0' : '1', label);
      infoPage = false;
      publishState();
    }
    show();
  }
}
