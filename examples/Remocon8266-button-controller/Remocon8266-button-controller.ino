/*
 *  IOSignal Remocon example: Remocon8266 controller for a 4-channel relay board
 *
 *  Controls a board running esp8266-relay4ch by its CID (DEFAULT_TARGET, a fixed
 *  CID from that board's device key). The target can change without new firmware:
 *    - this board is a card in the web app, and its card sends "target=<cid>";
 *    - or from a CLI: remocon pub "<controller cid>@" "target=2001";
 *    - or over USB on the board settings page (https://arduino.iosignal.net/wifi/).
 *    It is the "target" setting (IOSignalSetting): kept in flash and used after a reboot.
 *    Clearing it on the settings page brings back DEFAULT_TARGET.
 *  Buttons SW1-SW4 toggle relays 1-4, and a button on D6 (GPIO12, to GND) turns
 *  all relays off, or all on when every relay is off. Each button is a board setting
 *  (b1-b5, IOSignalButtons.h): empty keeps that default, "r2=1" always turns relay 2 on,
 *  "lamp1:toggle" controls another device, "#lab:s1" a group. Set them on the board
 *  settings page or remotely ("b5=all=0"). The OLED shows the target CID, the four relay
 *  states and this board's CID.
 *
 *  Wi-Fi: leave WIFI_SSID as it is and set it from https://arduino.iosignal.net/wifi/ over USB
 *  (the board keeps it), or put it in _secrets.h / edit WIFI_SSID and WIFI_PASS below (then the
 *  sketch values are used at every boot). The same page can set another IOSignal server and a
 *  device key (id.key); those stored values win over the sketch ones. Use a device key different
 *  from the relay board's (set it over USB, or SECRET_ID_KEY in this folder's _secrets.h).
 *
 *  [kr] CID로 4채널 릴레이 보드를 제어합니다. 버튼 4개, D6 전체 버튼, OLED 상태 표시. 대상 CID는 앱이나 CLI로 바꿉니다.
 *  Wi-Fi는 USB로 연결해 https://arduino.iosignal.net/wifi/ 에서 넣을 수 있습니다.
 *
 *  Taeo Lee <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *  MIT License
 */

#include <EEPROM.h>   // only to move a target saved by an older version of this sketch
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <IOSignalButtons.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <string.h>
#include <IOSignalUi.h>

// Wi-Fi values (and an optional device key) can go in _secrets.h next to this sketch,
// which keeps them out of version control. Leave the placeholders to use the Wi-Fi set over USB.
#if __has_include("_secrets.h")
#include "_secrets.h"
#endif
#ifndef SECRET_WIFI_SSID
#define SECRET_WIFI_SSID "WIFI_SSID"
#define SECRET_WIFI_PASS "WIFI_PASS"
#endif
const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASS = SECRET_WIFI_PASS;

// Buttons, in relay order: SW1 -> relay 1 ... SW4 -> relay 4.
#define BTN_UP     16   // D0  SW1 *EXTERNAL HARDWARE PULLUP
#define BTN_LEFT    0   // D3  SW2
#define BTN_DOWN   14   // D5  SW3
#define BTN_RIGHT  13   // D7  SW4
// External button between D6 and GND: all relays on, or all off when any is on.
#define BTN_ALL    12   // D6  (OUT2 on the board), internal pull-up

// I2C OLED: SDA GPIO4 (D2), SCL GPIO5 (D1).

const char* DEFAULT_TARGET = "the-cid"; // CID of the relay board until the card sets another one.

// The card: a CID input and the relay states, kept in <cid>@$panel ("p=1" in the info).
// The "text" widget needs a web app that knows it. An app that does not falls back
// to the ui buttons, which toggle the relays through this board.
const char* devicePanel =
  "{\"schema\":1,\"title\":\"Relay Control\",\"controls\":["
  "{\"widget\":\"text\",\"label\":\"Target CID\",\"key\":\"target\",\"cmd\":\"target={v}\",\"maxLength\":12},"
  "{\"widget\":\"value\",\"label\":\"Relays 1-4\",\"key\":\"relays\"},"
  "{\"widget\":\"value\",\"label\":\"Connected\",\"key\":\"link\"},"
  "{\"widget\":\"buttons\",\"label\":\"Toggle\",\"options\":[\"r1\",\"r2\",\"r3\",\"r4\",\"all\"],\"cmd\":\"{v}\"}"
  "]}";

WiFiClient client;
IOSignal io;
IOSignalConfig config;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "Relay Control:HOME");
void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  remocon::formatNameSetting(name, sizeof(name), nameSetting.value(), "Relay Control");
  io.signal("@$name", name);
  io.signal("@$ui", "p=1;b=r1,r2,r3,r4,all");
}
// The relay board's CID: a setting the browser shows and changes, and a remote command may change.
IOSignalSetting targetSetting(config, "target", "Target CID", DEFAULT_TARGET, IOSignalSetting::CID, IOSignalSetting::REMOTE);
// What each button does (empty: relay 1-4 toggles, and "all" for the D6 button). See IOSignalBinding.h.
IOSignalSetting b1(config, "b1", "SW1 (empty: r1)", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
IOSignalSetting b2(config, "b2", "SW2 (empty: r2)", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
IOSignalSetting b3(config, "b3", "SW3 (empty: r3)", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
IOSignalSetting b4(config, "b4", "SW4 (empty: r4)", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
IOSignalSetting b5(config, "b5", "D6 button (empty: all)", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
// GPIO16 has an external pull-up and no internal one.
const IOSignalButtons::Button BUTTONS[] = {
  { BTN_UP, &b1, INPUT }, { BTN_LEFT, &b2 }, { BTN_DOWN, &b3 }, { BTN_RIGHT, &b4 }, { BTN_ALL, &b5 }
};
IOSignalButtons buttons(io, targetSetting, BUTTONS, 5);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// Relay states of the target from its last $state ("----" while unknown).
void relayStates(char states[5]) {
  strcpy(states, "----");
  const char* state = buttons.state();
  if (!state) return;
  for (uint8_t i = 0; i < 4; i++) {
    char key[3] = { 'r', char('1' + i), 0 }, v[2];
    states[i] = remocon::stateValue(state, key, v, sizeof(v)) && v[0] == '1' ? '1' : '0';
  }
}

// Older versions kept the target in EEPROM: a marker byte 0xA5, then the CID with its 0.
// Moved once into the target setting, then the marker is cleared so a later reset to the
// default is not undone. Remove this when no board runs the older version.
void moveOldTarget() {
  EEPROM.begin(32);
  if (EEPROM.read(0) != 0xA5) { EEPROM.end(); return; }
  char old[MAX_CID_LEN + 1];
  uint8_t i = 0;
  for (; i <= MAX_CID_LEN; i++) {
    old[i] = EEPROM.read(1 + i);
    if (!old[i]) break;
  }
  if (i <= MAX_CID_LEN && !targetSetting.isStored() && targetSetting.set(old)) {
    Serial.print("TARGET moved from EEPROM: ");
    Serial.println(old);
  }
  EEPROM.write(0, 0);
  EEPROM.end();  // commits
}

// OLED: target line, four boxes (a filled box is a relay that is on), then this board's CID.
void showState() {
  char states[5];
  relayStates(states);
  const bool stateKnown = buttons.state() != nullptr;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_t0_13_mr);
  u8g2.setFontPosTop();
  u8g2.setCursor(0, 0);
  if (!config.connected()) u8g2.print("Wi-Fi..");
  else if (io.state != IO_READY) u8g2.print("Connecting..");
  else {
    u8g2.print("Relay ");
    u8g2.print(buttons.target());
    if (!stateKnown) u8g2.print(" ?");
  }
  for (uint8_t i = 0; i < 4; i++) {
    const uint8_t x = 4 + i * 31, y = 20, w = 26, h = 30;
    char label[2] = { char('1' + i), 0 };
    if (stateKnown && states[i] == '1') {
      u8g2.drawBox(x, y, w, h);
      u8g2.setDrawColor(0);
      u8g2.drawStr(x + 10, y + 9, label);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(x, y, w, h);
      u8g2.drawStr(x + 10, y + 9, stateKnown ? label : "?");
    }
  }
  if (io.state == IO_READY) {
    u8g2.setCursor(0, 53);
    u8g2.print("CID ");
    u8g2.print(io.cid);
  }
  u8g2.sendBuffer();
}

// The controller's own state for its card: target=the-cid;relays=0100;link=1
void publishState() {
  if (io.state != IO_READY) return;
  char states[5], text[48];
  relayStates(states);
  snprintf(text, sizeof(text), "target=%s;relays=%s;link=%c", buttons.target(), states, buttons.state() ? '1' : '0');
  io.signal("@$state", text);
}

// A target state arrived or a setting changed (card, CLI or browser).
void changed() {
  showState();
  publishState();
}

void onReady() {
  Serial.print("onReady cid: ");
  Serial.println(io.cid);
  io.subscribe("#search");
  io.signal("@$panel", devicePanel); // Before the info, so it is kept when the app sees p=1.
  sendInfo();
  publishState();
  io.signal("#notify", io.cid);
  buttons.onReady(); // watches the target's $state: the server sends the kept one at once
  showState();
}

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (strcmp(tag, "#search") == 0) { io.signal("#notify", io.cid); return; }
  if (buttons.onMessage(tag, type, payload, size)) return;  // the target's state (changed() follows)
  if (type != IOSignal::TEXT || !payload || !size || !memchr(payload, 0, size)) return;
  const char* text = (const char*)payload;

  // Commands to this controller: target=<cid>, b1-b5 (press) or b1=... (a button's setting),
  // and the card's r1-r4 and all, which toggle on the target. Every command gets the state as a reply.
  if (strcmp(tag, "@") == 0) {
    Serial.print("CMD ");
    Serial.println(text);
    if (!buttons.command(text) && (strcmp(text, "all") == 0 || (text[0] == 'r' && text[1] >= '1' && text[1] <= '4' && !text[2]))) {
      buttons.run(text);
    }
    publishState();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Remocon8266 relay control start");

  u8g2.begin();
  showState();

  // Stored Wi-Fi (set over USB), or the sketch values when they are not the placeholders.
  config.begin(Serial, "Relay Control", WIFI_SSID, WIFI_PASS);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer("io.remocon.kr", 55488);
#ifdef SECRET_ID_KEY
  config.setKey(SECRET_ID_KEY);
#endif
  config.printServer();
  moveOldTarget();
  buttons.onLog([](const char* line) { Serial.println(line); });
  buttons.onChange(changed);
  buttons.begin();
  Serial.print("TARGET ");
  Serial.println(buttons.target());

  io.setRxBuffer(256); // the target's state
  io.setLog([](const char* line) { Serial.println(line); }); // e.g. no reply from a wrong port
  io.onReady(&onReady);
  io.onMessage(&onMessage);
  // Use a key of its own: the same key as the relay board would log one of them out.
  // Stored over USB first, then SECRET_ID_KEY, else anonymous.
  if (config.key()) io.auth(config.key());
  io.begin(&client, config.host(), config.port());
}

bool wasConnected = false;

void loop() {
  config.loop(); // Wi-Fi setup over USB, reconnect while offline
  const bool connected = config.connected();
  if (connected != wasConnected) {
    wasConnected = connected;
    if (connected) { Serial.print("IP: "); Serial.println(WiFi.localIP()); }
    showState();
  }
  if (connected) io.loop();
  buttons.loop(); // SW1-SW4 and D6: each runs its setting (b1-b5)
}
