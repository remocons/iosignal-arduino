/*
 *  IOSignal device example: ESP-01 one button on the worynim carrier board
 *
 *  A carrier board for the ESP-01 with USB-C (USB serial), a button "SW IO3" and OLED, NEO and G4 pins.
 *  This sketch uses the button on GPIO3 (RX) only, and no other pin, to control another device by its
 *  CID: a 4-channel relay board (esp8266-relay4ch), an ESP-01 relay (esp01-relay-button), an iris board
 *  (iris-basic), a Remocon8266 board, a light or a group channel.
 *  Board settings (IOSignalButtons.h, IOSignalBinding.h):
 *    target  the device to control (CID)
 *    b1      empty: all channels off when any is on, else all on
 *            "s1"  toggle channel 1      "r2=1"  relay 2 on      "all=0"  all off
 *            "lamp1:toggle"  another device      "#lab:s2"  a group channel
 *  Remote changes: remocon pub "<this cid>@" "target=?k3vd"      remocon pub "<this cid>@" "b1=s2"
 *
 *  GPIO3 is the RX pin, so this sketch normally does not use serial.
 *  Setup mode: when the board keeps no Wi-Fi yet, or the button is held during the first 3 seconds
 *  after power on, serial is turned on and the button is not used until the next reset.
 *  Then set Wi-Fi, target and b1 over the USB-C port on https://arduino.iosignal.net/wifi/
 *  (the board keeps them); the page can also set another IOSignal server and a device key.
 *  Do not press the button in setup mode: it would pull the USB serial chip's TX line low.
 *
 *  [kr] ESP-01 캐리어 보드(worynim, USB-C, 버튼 SW IO3)의 GPIO3 버튼 하나로 다른 장치를 제어합니다.
 *       다른 핀은 쓰지 않습니다. 대상과 버튼 동작은 보드 설정(target, b1)입니다.
 *       Wi-Fi가 없거나 켠 뒤 3초 안에 버튼을 누르고 있으면 설정 모드(시리얼 켜짐, 버튼 꺼짐)로 시작합니다.
 *
 *  Taeo Lee <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *  MIT License
 */

#if !defined(ESP8266)
 #error "Select an ESP8266 board (Generic ESP8266 Module for the ESP-01)."
#endif

#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <IOSignalButtons.h>
#include <IOSignalUi.h>
#include <string.h>

#define BUTTON_PIN   3   // RX; the board's button "SW IO3" connects it to GND

#define WIFI_SSID    "WIFI_SSID"
#define WIFI_PASS    "WIFI_PASS"

WiFiClient client;
IOSignal io;
IOSignalConfig config;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "ESP01 Worynim:HOME");
// The device to control, and what the button does to it (see the top of this file).
IOSignalSetting target(config, "target", "Target CID", "the-cid", IOSignalSetting::CID, IOSignalSetting::REMOTE);
IOSignalSetting b1(config, "b1", "Button (empty: all on/off)", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
const IOSignalButtons::Button BUTTONS[] = { { BUTTON_PIN, &b1 } };
IOSignalButtons buttons(io, target, BUTTONS, 1);

bool setupMode = false;  // serial on GPIO1/GPIO3 instead of the button until the next reset

void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  iosignal::formatNameSetting(name, sizeof(name), nameSetting.value(), "ESP01 Worynim");
  io.signal("@$name", name);
  io.signal("@$ui", "b1");
}

// Whether the target is "on": the channel b1 names, or any channel for the default and "all".
bool targetOn() {
  iosignal::Action a;
  if (!iosignal::parseAction(b1.value(), &a)) return false;
  const char* state = buttons.state(a.target[0] ? a.target : buttons.target());
  if (!state) return false;
  char value[8];
  if ((a.kind == iosignal::ACTION_TOGGLE || a.kind == iosignal::ACTION_SET) && iosignal::stateValue(state, a.key, value, sizeof(value)))
    return strcmp(value, "1") == 0;
  char keys[iosignal::CHANNELS_MAX][iosignal::KEY_MAX + 1];
  size_t n = iosignal::channelKeys(state, keys, iosignal::CHANNELS_MAX);
  for (size_t i = 0; i < n; i++)
    if (iosignal::stateValue(state, keys[i], value, sizeof(value)) && strcmp(value, "1") == 0) return true;
  return false;
}

// The controller's own state: target=?k3vd;link=1;on=1;b1=s1 (link: the target's state is known).
void publishState() {
  if (io.state != IO_READY) return;
  char text[160];
  snprintf(text, sizeof(text), "target=%s;link=%c;on=%c;b1=%s", buttons.target(), buttons.state() ? '1' : '0',
           targetOn() ? '1' : '0', buttons.valid(0) ? b1.value() : "?");
  io.signal("@$state", text);
}

void onReady() {
  if (setupMode) {
    Serial.print("READY CID: ");
    Serial.println(io.cid);
  }
  io.subscribe("#search");
  sendInfo();
  publishState();
  io.signal("#notify", io.cid);
  buttons.onReady();
}

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (strcmp(tag, "#search") == 0) { io.signal("#notify", io.cid); return; }
  if (buttons.onMessage(tag, type, payload, size)) return;  // a target's state (onChange publishes)
  if (strcmp(tag, "@") != 0 || type != IOSignal::TEXT || !payload || !size || !memchr(payload, 0, size)) return;
  buttons.command((const char*)payload);  // b1 (press), b1=..., target=...
  publishState();                          // also for unknown commands, so the app stops waiting
}

void setup() {
  // Wi-Fi, server and device key: stored over USB, or the sketch values. No serial yet.
  config.begin("ESP01 Worynim", WIFI_SSID, WIFI_PASS);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer("io.iosignal.net", 55488);

  buttons.onChange(publishState);
  buttons.begin();  // GPIO3 becomes an input with a pull-up

  setupMode = !config.wifiStored() && strcmp(WIFI_SSID, "WIFI_SSID") == 0;
  for (unsigned long start = millis(); !setupMode && millis() - start < 3000; delay(10)) {
    if (digitalRead(BUTTON_PIN) == LOW) setupMode = true;
  }
  if (setupMode) {
    Serial.begin(115200);  // takes GPIO1 and GPIO3 back for serial
    config.attachSerial(Serial);
    Serial.println();
    Serial.println("ESP01 setup mode: set Wi-Fi, target and b1 on https://arduino.iosignal.net/wifi/");
    Serial.println("The button works after a reset (without holding the button).");
    config.printServer();
    Serial.print("TARGET ");
    Serial.println(buttons.target());
    buttons.onLog([](const char* line) { Serial.println(line); });
    io.setLog([](const char* line) { Serial.println(line); });
  }

  io.setRxBuffer(256);  // the targets' states
  io.onReady(onReady);
  io.onMessage(onMessage);
  if (config.key()) io.auth(config.key());  // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}

void loop() {
  config.loop();  // USB setup requests (setup mode), Wi-Fi reconnect
  if (config.connected()) io.loop();
  if (!setupMode) buttons.loop();  // in setup mode GPIO3 carries serial data, not presses
}
