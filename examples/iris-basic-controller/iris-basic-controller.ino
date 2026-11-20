// iris ESP32-C3 controller for an iris-basic board.
// It finds the Iris Basic like the web app does (#search, #notify, @$name),
// shows its state on its own four LEDs and sends commands from its buttons.
// Button 1 turns all LEDs on or off. Button 2 lights one more LED each press.
// The status LED (GPIO8) is on while a target is connected.
#include <FastLED.h>
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <Bounce2.h>
#include <string.h>
#include <IOSignalUi.h>
// USB serial blocks when the cable is in but no monitor reads it.
// Skip a log line when it does not fit, so buttons and commands stay responsive.
void logLine(const char* a, const char* b) {
  if (Serial.availableForWrite() < (int)(strlen(a) + strlen(b) + 2)) return;
  Serial.print(a); Serial.println(b);
}
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
WiFiClient client;
IOSignal io;
// Wi-Fi, server and device key: set over USB on https://arduino.iosignal.net/wifi/, or the sketch values.
IOSignalConfig config;
void connectNetwork() {
  config.begin(Serial, "Iris Control", WIFI_SSID, WIFI_PASS);
  config.setServer("io.iosignal.net", 55488);
  config.printServer();
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // Small ESP32-C3 boards drop sent packets at full power (antenna).
  WiFi.setSleep(false); // Power save delays incoming signals by 100-800 ms.
  io.setRxBuffer(512);
  io.onReady(onReady);
  io.onMessage(onMessage);
  io.setLog([](const char* m) { logLine(m, ""); }); // e.g. no reply from a wrong port
  if (config.key()) io.auth(config.key()); // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}
void pollNetwork() {
  config.loop(); // USB setup requests, Wi-Fi reconnect
  if (config.connected()) {
    // Send each small signal at once instead of waiting for the previous ACK.
    if (client.connected() && !client.getNoDelay()) client.setNoDelay(true);
    io.loop();
  }
}

const uint8_t DATA_PIN = 4, STATUS_LED = 8;
const uint8_t BUTTON_PINS[] = {1, 2};
const char* TARGET_NAME = "Iris Basic"; // The name part (before ':') of the target's $name.
CRGB leds[4];
Bounce2::Button buttons[2];
char target[MAX_CID_LEN + 1] = ""; // CID of the Iris Basic, empty while searching.
char states[5] = "0000";           // Last state reported by the target.
bool stateKnown = false;
unsigned long lastSearch = 0;

void showState() {
  for (uint8_t i = 0; i < 4; i++) leds[i] = stateKnown && states[i] == '1' ? CRGB(100, 80, 50) : CRGB::Black;
  FastLED.show();
  digitalWrite(STATUS_LED, target[0] ? LOW : HIGH);
}
void search() {
  lastSearch = millis();
  io.signal("#search");
}
void loseTarget() {
  logLine("TARGET lost", "");
  target[0] = 0; stateKnown = false;
  showState();
  search();
}
void onReady() {
  logLine("READY CID: ", io.cid);
  target[0] = 0; stateKnown = false; showState();
  io.subscribe("#notify");
  search();
}
// Tags arrive as <cid>@$name and <cid>@$state. Copies the <cid> part.
bool cidOf(const char* tag, const char* topic, char* cid) {
  const char* at = strstr(tag, topic);
  if (!at || strcmp(at, topic) != 0 || at == tag || at - tag > MAX_CID_LEN) return false;
  memcpy(cid, tag, at - tag); cid[at - tag] = 0;
  return true;
}
void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (type != IOSignal::TEXT || !payload || !size || !memchr(payload, 0, size)) return;
  const char* text = (const char*)payload;
  char cid[MAX_CID_LEN + 1], sub[MAX_CID_LEN + 8];
  if (strcmp(tag, "#notify") == 0) {
    // Ask every new device for its name until the target is found.
    if (target[0] || !text[0] || text[0] == '#' || strlen(text) > MAX_CID_LEN) return;
    snprintf(sub, sizeof(sub), "%s@$name", text);
    io.subscribe(sub);
  } else if (cidOf(tag, "@$name", cid)) {
    io.unsubscribe(tag); // The name is read once. Do not keep receiving it.
    // "Iris Basic" or "Iris Basic:<category>": the part before ':' must be the whole name.
    if (target[0] || !iosignal::nameIs(text, TARGET_NAME)) return;
    strcpy(target, cid);
    logLine("TARGET ", target);
    snprintf(sub, sizeof(sub), "%s@$state", target);
    io.subscribe(sub);
    showState();
  } else if (cidOf(tag, "@$state", cid) && strcmp(cid, target) == 0) {
    if (strcmp(text, "close") == 0) { loseTarget(); return; }
    for (uint8_t i = 0; i < 4; i++) {
      char key[4];
      snprintf(key, sizeof(key), "s%u=", i + 1);
      const char* value = strstr(text, key);
      if (value) states[i] = value[3] == '1' ? '1' : '0';
    }
    stateKnown = true;
    logLine("STATE ", text);
    showState();
  }
}
// Sends s1=1 style commands for every LED that differs from the target.
void request(const char* next) {
  for (uint8_t i = 0; i < 4; i++) {
    if (next[i] == states[i]) continue;
    char command[5] = {'s', char('1' + i), '=', next[i], 0};
    io.signal2(target, "@", command);
    logLine("COMMAND ", command);
  }
}
void setup() {
  Serial.begin(115200);
  logLine("iris basic control start", "");
  pinMode(STATUS_LED, OUTPUT); digitalWrite(STATUS_LED, HIGH);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, 4); FastLED.clear(true);
  for (uint8_t i = 0; i < 2; i++) {
    buttons[i].attach(BUTTON_PINS[i], INPUT_PULLUP);
    buttons[i].interval(10); buttons[i].setPressedState(LOW);
  }
  connectNetwork();
}
void loop() {
  pollNetwork();
  for (uint8_t i = 0; i < 2; i++) buttons[i].update();
  if (!target[0] && io.state == IO_READY && millis() - lastSearch >= 15000) search();
  if (!target[0] || !stateKnown) return; // Buttons wait for the target's state.
  char next[5];
  if (buttons[0].pressed()) {
    logLine("BUTTON 1: all on/off", "");
    memset(next, strcmp(states, "0000") == 0 ? '1' : '0', 4); next[4] = 0;
    request(next);
  }
  if (buttons[1].pressed()) {
    logLine("BUTTON 2: one more LED", "");
    // 0 -> 1 -> 2 -> 3 -> 4 -> 0 LEDs on, from the first LED.
    uint8_t count = 0;
    while (count < 4 && states[count] == '1') count++;
    count = count == 4 ? 0 : count + 1;
    for (uint8_t i = 0; i < 4; i++) next[i] = i < count ? '1' : '0';
    next[4] = 0;
    request(next);
  }
}
