// iris ESP32-C3 as a controller for another device (a 4-channel relay board, a switch, a light...).
// Its two buttons do what the board settings b1 and b2 say (IOSignalButtons.h):
//   empty  the default: all on and all off for 3 or more channels, channel 1 and 2 for a 2-channel target
//   "r1"   toggle relay 1    "r2=1"  relay 2 on    "all"  all off when any is on, else all on
//   "lamp1:toggle"  another device    "#lab:s2"  a group channel
// The target is the board setting target (a CID). Set them over USB on https://arduino.iosignal.net/wifi/,
// or remotely: remocon pub "<this cid>@" "b2=r3". The card's b1 and b2 buttons press the buttons.
// The four WS2812 LEDs (GPIO4) show the target's first four channels (r1-r4, s1-s4...), the status
// LED (GPIO8) is on while the target's state is known. Buttons GPIO1 and GPIO2. Pins follow iris board v1a.
#include <FastLED.h>
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <IOSignalButtons.h>
#include <string.h>
#include <IOSignalUi.h>

// USB serial blocks when the cable is in but no monitor reads it.
// Skip a log line when it does not fit, so buttons and commands stay responsive.
void logLine(const char* a, const char* b = "") {
  if (Serial.availableForWrite() < (int)(strlen(a) + strlen(b) + 2)) return;
  Serial.print(a); Serial.println(b);
}

const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASS = "WIFI_PASS";
const uint8_t DATA_PIN = 4, STATUS_LED = 8;

WiFiClient client;
IOSignal io;
IOSignalConfig config;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "Iris Button Control:HOME");
IOSignalSetting target(config, "target", "Target CID", "the-cid", IOSignalSetting::CID, IOSignalSetting::REMOTE);
IOSignalSetting b1(config, "b1", "Button 1 (empty: default)", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
IOSignalSetting b2(config, "b2", "Button 2 (empty: default)", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
const IOSignalButtons::Button BUTTONS[] = { { 1, &b1 }, { 2, &b2 } };
IOSignalButtons buttons(io, target, BUTTONS, 2);
CRGB leds[4];

void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  iosignal::formatNameSetting(name, sizeof(name), nameSetting.value(), "Iris Button Control");
  io.signal("@$name", name);
  io.signal("@$ui", "b1,b2");
}

// The target's first four channel keys on the four LEDs; all dark while its state is unknown.
void showTarget() {
  const char* state = buttons.state();
  char keys[4][iosignal::KEY_MAX + 1];
  size_t n = state ? iosignal::channelKeys(state, keys, 4) : 0;
  for (uint8_t i = 0; i < 4; i++) {
    char v[2] = "0";
    if (i < n) iosignal::stateValue(state, keys[i], v, sizeof(v));
    leds[i] = i < n && v[0] == '1' ? CRGB(100, 80, 50) : CRGB::Black;
  }
  FastLED.show();
  digitalWrite(STATUS_LED, state ? LOW : HIGH);  // active low
}

// target=?k3vd;link=1;b1=;b2=r3 (link: the target's state is known; a bad setting shows as ?).
void publishState() {
  if (io.state != IO_READY) return;
  char text[200];
  snprintf(text, sizeof(text), "target=%s;link=%c;b1=%s;b2=%s", buttons.target(), buttons.state() ? '1' : '0',
           buttons.valid(0) ? b1.value() : "?", buttons.valid(1) ? b2.value() : "?");
  io.signal("@$state", text);
}

void changed() {
  showTarget();
  publishState();
}

void onReady() {
  logLine("READY CID: ", io.cid);
  io.subscribe("#search");
  sendInfo();
  publishState();
  io.signal("#notify", io.cid);
  buttons.onReady();
}

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (strcmp(tag, "#search") == 0) { io.signal("#notify", io.cid); return; }
  if (buttons.onMessage(tag, type, payload, size)) return;  // a target's state (changed() follows)
  if (strcmp(tag, "@") != 0 || type != IOSignal::TEXT || !payload || !size || !memchr(payload, 0, size)) return;
  const char* text = (const char*)payload;
  logLine("CMD ", text);
  buttons.command(text);  // b1, b2 (press), b1=..., b2=..., target=...
  publishState();         // also for unknown commands, so the app stops waiting
}

void setup() {
  Serial.begin(115200);  // board settings over USB
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, HIGH);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, 4);
  FastLED.clear(true);

  // Wi-Fi, server and device key: set over USB, or the sketch values.
  config.begin(Serial, "Iris Button Control", WIFI_SSID, WIFI_PASS);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer("io.iosignal.net", 55488);
  config.printServer();
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // Small ESP32-C3 boards drop sent packets at full power (antenna).
  WiFi.setSleep(false);                // Power save delays incoming signals by 100-800 ms.

  buttons.onLog([](const char* line) { logLine(line); });
  buttons.onChange(changed);
  buttons.begin();
  logLine("TARGET ", buttons.target());

  io.setRxBuffer(512);  // the targets' states
  io.setLog([](const char* line) { logLine(line); });
  io.onReady(onReady);
  io.onMessage(onMessage);
  if (config.key()) io.auth(config.key());  // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}

void loop() {
  config.loop();  // USB setup requests, Wi-Fi reconnect
  if (config.connected()) {
    // Send each small signal at once instead of waiting for the previous ACK.
    if (client.connected() && !client.getNoDelay()) client.setNoDelay(true);
    io.loop();
  }
  buttons.loop();
}
