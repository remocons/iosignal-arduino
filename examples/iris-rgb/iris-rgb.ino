// iris ESP32-C3: four RGB LEDs, each with its own hue and brightness (0 = off).
// The web app finds it without a key and shows the iris.2 screen.
// DIP switches (read at boot, LOW = 1):
//   GPIO21, 20, 10: group id 0-7    GPIO7, 6, 5: device id 0-7 -> name "iris<device id>"
// Buttons: GPIO1 turns the whole group on or off, GPIO2 turns this board on or off.
// The status LED (GPIO8) is on while the server connection is ready.
#include <FastLED.h>
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <Bounce2.h>
#include <string.h>
#include <IOSignalUi.h>
#include <stdlib.h>
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
  config.begin(Serial, "Iris RGB", WIFI_SSID, WIFI_PASS);
  config.setServer("io.remocon.kr", 55488);
  config.printServer();
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // Small ESP32-C3 boards drop sent packets at full power (antenna).
  WiFi.setSleep(false); // Power save delays incoming signals by 100-800 ms.
  io.setRxBuffer(256);
  io.onReady(onReady);
  io.onMessage(onMessage);
  io.setLog([](const char* m) { logLine(m, ""); }); // e.g. a send that failed and reconnects
  if (config.key()) io.auth(config.key()); // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
  // No key: the server gives a new CID on each connection and the app finds it by #search.
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
const uint8_t GROUP_BUTTON = 1, SELF_BUTTON = 2;
const uint8_t DIP_PINS[] = {21, 20, 10, 7, 6, 5};
CRGB leds[4];
Bounce2::Button groupButton, selfButton;
uint8_t groupId = 0, deviceId = 0;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty: "iris<device id>:iris G<group id>"
// from the DIP switches; an empty name part (":Kitchen") keeps the DIP name.
IOSignalSetting nameSetting(config, "name", "Name & category (empty: from DIP)", "");
// $name "iris<device id>:<category>" and $ui "t=iris.2" (device rules v2).
char deviceName[64];
void makeInfo() {
  char name[12], category[16];
  snprintf(name, sizeof(name), "iris%u", deviceId);
  snprintf(category, sizeof(category), "iris G%u", groupId);
  remocon::formatNameSetting(deviceName, sizeof(deviceName), nameSetting.value(), name, category);
}
void sendInfo() {
  io.signal("@$name", deviceName);
  io.signal("@$ui", "t=iris.2");
}
char groupChannel[16]; // #iris-g<group id>: members only, carries "on" and "off".

// What the app sees (iris.2): each LED has a hue 0-359 and a brightness 0-100.
// Brightness 0 is off; anything else is on. There is no separate on/off per LED.
uint16_t hue[4] = {30, 30, 30, 30};
uint8_t level[4] = {0, 0, 0, 0};
uint8_t lastLevel[4] = {50, 50, 50, 50}; // Restored by "on" after "off".

bool anyOn() { return level[0] || level[1] || level[2] || level[3]; }
void setLevel(uint8_t i, uint8_t value) {
  level[i] = value;
  if (value) lastLevel[i] = value;
}
// on brings back each LED's last brightness, off sets them all to 0.
void setAll(bool on) {
  for (uint8_t i = 0; i < 4; i++) setLevel(i, on ? (level[i] ? level[i] : lastLevel[i]) : 0);
}

// Full-saturation color of a hue, the same wheel the app draws on the slider.
CRGB hueColor(uint16_t h) {
  const uint8_t up = (h % 60) * 255 / 60, down = 255 - up;
  switch (h / 60) {
    case 0: return CRGB(255, up, 0);
    case 1: return CRGB(down, 255, 0);
    case 2: return CRGB(0, 255, up);
    case 3: return CRGB(0, down, 255);
    case 4: return CRGB(up, 0, 255);
    default: return CRGB(255, 0, down);
  }
}

// Shows the LEDs and reports on=1;h=30;b=50;h1=30;b1=50;h2=...
// h is the shared hue of all four LEDs, left out when they differ.
// b is the brightest LED, so the all-brightness slider never reads 0 while a LED is on.
void publishState() {
  for (uint8_t i = 0; i < 4; i++) {
    leds[i] = hueColor(hue[i]);
    leds[i].nscale8_video(level[i] * 255 / 100);
  }
  FastLED.show();
  char text[96];
  int n = snprintf(text, sizeof(text), "on=%u", anyOn());
  if (hue[0] == hue[1] && hue[0] == hue[2] && hue[0] == hue[3])
    n += snprintf(text + n, sizeof(text) - n, ";h=%u", hue[0]);
  n += snprintf(text + n, sizeof(text) - n, ";b=%u", max(max(level[0], level[1]), max(level[2], level[3])));
  for (uint8_t i = 0; i < 4; i++)
    n += snprintf(text + n, sizeof(text) - n, ";h%u=%u;b%u=%u", i + 1, hue[i], i + 1, level[i]);
  io.signal("@$state", text);
  logLine("STATE ", text);
}

// "70" -> 70. Accepts 0 to max only.
bool parseNumber(const char* text, long max, long& out) {
  if (!*text) return false;
  char* end;
  out = strtol(text, &end, 10);
  return !*end && out >= 0 && out <= max;
}
// on, off, h=120 and b=70 (all LEDs), h1=120, b1=70 (LED 1-4). b1=0 turns LED 1 off.
void applyCommand(const char* c) {
  long value;
  if (strcmp(c, "on") == 0) setAll(true);
  else if (strcmp(c, "off") == 0) setAll(false);
  else if (strncmp(c, "h=", 2) == 0) {
    if (parseNumber(c + 2, 359, value)) for (uint8_t i = 0; i < 4; i++) hue[i] = value;
  }
  else if (strncmp(c, "b=", 2) == 0) {
    if (parseNumber(c + 2, 100, value)) for (uint8_t i = 0; i < 4; i++) setLevel(i, value);
  }
  else if ((c[0] == 'h' || c[0] == 'b') && c[1] >= '1' && c[1] <= '4' && c[2] == '=') {
    const uint8_t i = c[1] - '1';
    if (c[0] == 'h' && parseNumber(c + 3, 359, value)) hue[i] = value;
    else if (c[0] == 'b' && parseNumber(c + 3, 100, value)) setLevel(i, value);
  }
}

void onReady() {
  logLine("READY CID: ", io.cid);
  char rssi[8];
  snprintf(rssi, sizeof(rssi), "%d", WiFi.RSSI());
  logLine("RSSI dBm: ", rssi); // Below about -75 the link may stall.
  io.subscribe("#search");
  io.subscribe(groupChannel);
  sendInfo();
  publishState();
  io.signal("#notify", io.cid);
}
void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (strcmp(tag, "#search") == 0) { io.signal("#notify", io.cid); return; }
  if (type != IOSignal::TEXT || !payload || !size || !memchr(payload, 0, size)) return;
  const char* text = (const char*)payload;
  if (strcmp(tag, "@") == 0) {
    logLine("COMMAND ", text);
    applyCommand(text);
    publishState(); // Also for unknown commands, so the app stops waiting.
  } else if (strcmp(tag, groupChannel) == 0 && (strcmp(text, "on") == 0 || strcmp(text, "off") == 0)) {
    logLine("GROUP ", text);
    setAll(text[1] == 'n');
    publishState();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT); digitalWrite(STATUS_LED, HIGH);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, 4); FastLED.clear(true);
  for (uint8_t i = 0; i < 6; i++) pinMode(DIP_PINS[i], INPUT_PULLUP);
  delay(10);
  for (uint8_t i = 0; i < 3; i++) {
    groupId = (groupId << 1) | (digitalRead(DIP_PINS[i]) == LOW);
    deviceId = (deviceId << 1) | (digitalRead(DIP_PINS[i + 3]) == LOW);
  }
  snprintf(groupChannel, sizeof(groupChannel), "#iris-g%u", groupId);
  makeInfo();
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { makeInfo(); if (io.state == IO_READY) sendInfo(); });
  logLine("iris rgb start: ", deviceName);
  groupButton.attach(GROUP_BUTTON, INPUT_PULLUP); groupButton.interval(10); groupButton.setPressedState(LOW);
  selfButton.attach(SELF_BUTTON, INPUT_PULLUP); selfButton.interval(10); selfButton.setPressedState(LOW);
  connectNetwork();
}
void loop() {
  pollNetwork();
  digitalWrite(STATUS_LED, io.state == IO_READY ? LOW : HIGH);
  groupButton.update(); selfButton.update();
  if (selfButton.pressed()) {
    logLine("BUTTON self", "");
    setAll(!anyOn());
    publishState();
  }
  if (groupButton.pressed()) {
    // The group follows this board: any LED on -> all off, else all on.
    const char* next = anyOn() ? "off" : "on";
    logLine("BUTTON group ", next);
    setAll(next[1] == 'n');
    publishState();
    io.signal(groupChannel, next);
  }
}
