// iris ESP32-C3 basic: four onboard RGB LEDs as a 4-way switch.
// Pins: WS2812 data GPIO4, buttons GPIO1 and GPIO2 (iris board v1a). No DIP switches or groups.
// Button 1 turns all LEDs on or off. Button 2 lights one more LED each press.
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
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "Iris Basic:HOME");
void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  iosignal::formatNameSetting(name, sizeof(name), nameSetting.value(), "Iris Basic");
  io.signal("@$name", name);
  io.signal("@$ui", "t=switch4.1");
}
void connectNetwork() {
  config.begin(Serial, "Iris Basic", WIFI_SSID, WIFI_PASS);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer("io.iosignal.net", 55488);
#ifdef SECRET_ID_KEY
  config.setKey(SECRET_ID_KEY); // optional key (id.key) from _secrets.h; one set over USB wins
#endif
  config.printServer();
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // Small ESP32-C3 boards drop sent packets at full power (antenna).
  WiFi.setSleep(false); // Power save delays incoming signals by 100-800 ms.
  io.setRxBuffer(256);
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

const uint8_t DATA_PIN = 4;
const uint8_t BUTTON_PINS[] = {1, 2};
CRGB leds[4];
Bounce2::Button buttons[2];
char states[5] = "0000"; // One character per LED, '1' means on.

// Shows the states and reports them as s1=0;s2=1;s3=0;s4=0.
void publishState() {
  for (uint8_t i = 0; i < 4; i++) leds[i] = states[i] == '1' ? CRGB(100, 80, 50) : CRGB::Black;
  FastLED.show();
  char text[24];
  snprintf(text, sizeof(text), "s1=%c;s2=%c;s3=%c;s4=%c", states[0], states[1], states[2], states[3]);
  io.signal("@$state", text);
  logLine("STATE ", text);
}
void onReady() {
  logLine("READY CID: ", io.cid);
  io.subscribe("#search");
  sendInfo();
  publishState();
  io.signal("#notify", io.cid);
}
void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (strcmp(tag, "#search") == 0) { io.signal("#notify", io.cid); return; }
  if (strcmp(tag, "@") != 0 || type != IOSignal::TEXT || !payload || !size || !memchr(payload, 0, size)) return;
  // s1=1 sets one LED, all=1 / all=0 all four (device rules). Unknown commands still get the current state.
  const char* c = (const char*)payload;
  logLine("COMMAND ", c);
  if (c[0] == 's' && c[1] >= '1' && c[1] <= '4' && c[2] == '=' && (c[3] == '0' || c[3] == '1') && !c[4])
    states[c[1] - '1'] = c[3];
  else if (strcmp(c, "all=1") == 0 || strcmp(c, "all=0") == 0)
    memset(states, c[4], 4);
  publishState();
}
void setup() {
  Serial.begin(115200);
  logLine("iris basic start", "");
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
  if (buttons[0].pressed()) {
    logLine("BUTTON 1: all on/off", "");
    memset(states, strcmp(states, "0000") == 0 ? '1' : '0', 4);
    publishState();
  }
  if (buttons[1].pressed()) {
    logLine("BUTTON 2: one more LED", "");
    // 0 -> 1 -> 2 -> 3 -> 4 -> 0 LEDs on, from the first LED.
    uint8_t count = 0;
    while (count < 4 && states[count] == '1') count++;
    count = count == 4 ? 0 : count + 1;
    for (uint8_t i = 0; i < 4; i++) states[i] = i < count ? '1' : '0';
    publishState();
  }
}
