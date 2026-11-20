// A controller with two buttons for another device: a 4-channel relay board, a light, a group...
// What each button does is a board setting (IOSignalButtons.h, IOSignalBinding.h):
//   target  the device to control (CID), e.g. the relay board's
//   b1, b2  empty: the default (two buttons: all on, all off; or channel 1 and 2 of a 2-channel target)
//           "r1"  toggle relay 1      "r2=1"  relay 2 on      "all"  all off when any is on, else all on
//           "lamp1:toggle"  another device      "#lab:s2"  a group channel
// Set them over USB on https://arduino.iosignal.net/wifi/ (with Wi-Fi), or remotely:
//   remocon pub "<this cid>@" "b2=r3"      remocon pub "<this cid>@" "target=?k3vd"
// The card in the web app shows the target and its state, and its b1 and b2 buttons press the buttons.
// Buttons connect a pin to GND (internal pull-up).
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <IOSignalButtons.h>
#include <string.h>
#include <IOSignalUi.h>

#if defined(ESP8266)
const uint8_t PIN_B1 = 14, PIN_B2 = 12;   // D1 mini D5, D6
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
const uint8_t PIN_B1 = 9, PIN_B2 = 10;    // GPIO 9 is the BOOT button on most C3 boards
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
const uint8_t PIN_B1 = 0, PIN_B2 = 4;     // GPIO 0 is the BOOT button
#else
const uint8_t PIN_B1 = 13, PIN_B2 = 14;
#endif

const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASS = "WIFI_PASS";

WiFiClient client;
IOSignal io;
IOSignalConfig config;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "Button Control:HOME");
// The device to control, and what each button does to it (see the top of this file).
IOSignalSetting target(config, "target", "Target CID", "the-cid", IOSignalSetting::CID, IOSignalSetting::REMOTE);
IOSignalSetting b1(config, "b1", "Button 1 (empty: default)", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
IOSignalSetting b2(config, "b2", "Button 2 (empty: default)", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
const IOSignalButtons::Button BUTTONS[] = { { PIN_B1, &b1 }, { PIN_B2, &b2 } };
IOSignalButtons buttons(io, target, BUTTONS, 2);

void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  iosignal::formatNameSetting(name, sizeof(name), nameSetting.value(), "Button Control");
  io.signal("@$name", name);
  io.signal("@$ui", "b1,b2");
}

// The controller's own state: target=?k3vd;link=1;b1=r1;b2= (link: the target's state is known).
void publishState() {
  if (io.state != IO_READY) return;
  char text[200];
  snprintf(text, sizeof(text), "target=%s;link=%c;b1=%s;b2=%s", buttons.target(), buttons.state() ? '1' : '0',
           buttons.valid(0) ? b1.value() : "?", buttons.valid(1) ? b2.value() : "?");
  io.signal("@$state", text);
}

void onReady() {
  Serial.print("READY CID: ");
  Serial.println(io.cid);
  io.subscribe("#search");
  sendInfo();
  publishState();
  io.signal("#notify", io.cid);
  buttons.onReady();
}

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (strcmp(tag, "#search") == 0) { io.signal("#notify", io.cid); return; }
  if (buttons.onMessage(tag, type, payload, size)) return;  // a target's state
  if (strcmp(tag, "@") != 0 || type != IOSignal::TEXT || !payload || !size || !memchr(payload, 0, size)) return;
  const char* text = (const char*)payload;
  Serial.print("CMD ");
  Serial.println(text);
  buttons.command(text);  // b1, b2 (press), b1=..., b2=..., target=...
  publishState();         // also for unknown commands, so the app stops waiting
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  // Wi-Fi, server and device key: set over USB, or the sketch values.
  config.begin(Serial, "Button Control", WIFI_SSID, WIFI_PASS);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer("io.iosignal.net", 55488);
  config.printServer();

  buttons.onLog([](const char* line) { Serial.println(line); });
  buttons.onChange(publishState);
  buttons.begin();
  Serial.print("TARGET ");
  Serial.println(buttons.target());

  io.setRxBuffer(256);  // the targets' states
  io.setLog([](const char* line) { Serial.println(line); });
  io.onReady(onReady);
  io.onMessage(onMessage);
  if (config.key()) io.auth(config.key());  // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}

void loop() {
  config.loop();  // USB setup requests, Wi-Fi reconnect
  if (config.connected()) io.loop();
  buttons.loop();
}
