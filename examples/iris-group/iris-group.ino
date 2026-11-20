// iris ESP32-C3 group lights: boards in one group share an 8-way switch (switch8.1).
// DIP switches (read at boot, LOW = 1): GPIO21, 20, 10 group id 0-7; GPIO7, 6, 5 device id 0-7.
// Each board lights its four WS2812 LEDs (GPIO4) from switch s<device id + 1>.
// Button GPIO1 turns the whole group on or off, GPIO2 toggles this board. Status LED: GPIO8.
// Pins follow iris board v1a.
#include <FastLED.h>
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <Bounce2.h>
#include <string.h>
#include <IOSignalUi.h>
const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASS = "WIFI_PASS";
WiFiClient client;
IOSignal io;
// Wi-Fi, server and device key: set over USB on https://arduino.iosignal.net/wifi/, or the sketch values.
IOSignalConfig config;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty: "Iris Lights <group id>:iris G<group id>"
// from the DIP switches; an empty name part (":Kitchen") keeps the DIP name.
IOSignalSetting nameSetting(config, "name", "Name & category (empty: from DIP)", "");
void connectNetwork() {
  config.begin(Serial, "Iris Group", WIFI_SSID, WIFI_PASS);
  config.setServer("io.remocon.kr", 55488);
  config.printServer();
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // Small ESP32-C3 boards drop sent packets at full power (antenna).
  io.setRxBuffer(256);
  io.onReady(onReady);
  io.onMessage(onMessage);
  if (config.key()) io.auth(config.key()); // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}
void pollNetwork() {
  config.loop(); // USB setup requests, Wi-Fi reconnect
  if (config.connected()) io.loop();
}

const uint8_t DATA_PIN = 4, STATUS_LED = 8;
const uint8_t BUTTON_PINS[] = {1, 2};
const uint8_t DIP_PINS[] = {21, 20, 10, 7, 6, 5};
CRGB leds[4];
Bounce2::Button buttons[2];
uint8_t groupId = 0, deviceId = 0;
char channel[16], stateTag[24], nameTag[24], uiTag[24], groupName[64];
char states[9] = "00000000";
// Buttons write the whole state, so wait for the stored value. A new group has none.
bool stateKnown = false;
unsigned long readyAt = 0;
// The group's $name "Iris Lights <group id>:<category>". Every member sends the same $name and $ui (t=switch8.1).
void makeInfo() {
  char name[24], category[16];
  snprintf(name, sizeof(name), "Iris Lights %u", groupId);
  snprintf(category, sizeof(category), "iris G%u", groupId);
  remocon::formatNameSetting(groupName, sizeof(groupName), nameSetting.value(), name, category);
}
void sendInfo() {
  io.signal(nameTag, groupName);
  io.signal(uiTag, "t=switch8.1");
}
void applyState() {
  const bool on = states[deviceId] == '1';
  fill_solid(leds, 4, on ? CRGB(100,80,50) : CRGB::Black);
  FastLED.show();
  digitalWrite(STATUS_LED, on ? LOW : HIGH);
}
// The eight lights share one state: s1=0;s2=1;...
void publishState() {
  applyState();
  char text[40] = "";
  for (uint8_t i = 0; i < 8; i++) {
    char item[6];
    snprintf(item, sizeof(item), "s%u=%c", i + 1, states[i]);
    if (i) strcat(text, ";");
    strcat(text, item);
  }
  io.signal(stateTag, text);
}
void onReady() {
  readyAt = millis();
  io.subscribe(stateTag); io.subscribe("#search");
  sendInfo();
  io.signal("#notify", channel);
  // Do not overwrite the shared state on reconnect.
}
void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (strcmp(tag, "#search") == 0) { io.signal("#notify", channel); return; }
  if (strcmp(tag, stateTag) != 0 || type != IOSignal::TEXT || !payload || !size || !memchr(payload, 0, size)) return;
  for (uint8_t i = 0; i < 8; i++) {
    char key[4];
    snprintf(key, sizeof(key), "s%u=", i + 1);
    const char* value = strstr((const char*)payload, key);
    if (value) states[i] = value[3] == '1' ? '1' : '0';
  }
  stateKnown = true;
  applyState();
}
void setup() {
  Serial.begin(115200); // board settings over USB
  pinMode(STATUS_LED, OUTPUT); digitalWrite(STATUS_LED, HIGH);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, 4); FastLED.clear(true);
  for (uint8_t i = 0; i < 6; i++) pinMode(DIP_PINS[i], INPUT_PULLUP);
  for (uint8_t i = 0; i < 3; i++) {
    groupId = (groupId << 1) | (digitalRead(DIP_PINS[i]) == LOW);
    deviceId = (deviceId << 1) | (digitalRead(DIP_PINS[i+3]) == LOW);
  }
  snprintf(channel, sizeof(channel), "#Iris-g%u", groupId);
  snprintf(stateTag, sizeof(stateTag), "%s$state", channel);
  snprintf(nameTag, sizeof(nameTag), "%s$name", channel);
  snprintf(uiTag, sizeof(uiTag), "%s$ui", channel);
  makeInfo();
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { makeInfo(); if (io.state == IO_READY) sendInfo(); });
  for (uint8_t i = 0; i < 2; i++) {
    buttons[i].attach(BUTTON_PINS[i], INPUT_PULLUP);
    buttons[i].interval(10); buttons[i].setPressedState(LOW);
  }
  connectNetwork();
}
void loop() {
  pollNetwork();
  for (uint8_t i = 0; i < 2; i++) buttons[i].update();
  if (!stateKnown && (!readyAt || millis() - readyAt < 2000)) return;
  if (buttons[0].pressed()) {
    const char next = strcmp(states, "00000000") == 0 ? '1' : '0';
    memset(states, next, 8); publishState();
  }
  if (buttons[1].pressed()) {
    states[deviceId] = states[deviceId] == '1' ? '0' : '1'; publishState();
  }
}
