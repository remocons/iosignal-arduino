/*
 *  IOSignal device example: ESP-01 relay and button
 *
 *  remocon.kr shows it as a 1-channel relay (relay1.1): r1 = the relay on GPIO0.
 *  The button on GPIO2 toggles the relay. Serial (TX GPIO1, RX GPIO3) stays free for the logs and
 *  the board settings page.
 *  GPIO0 and GPIO2 must be HIGH at boot: an active-low relay module keeps the relay off then.
 *  Written for the ESP-01. An ESP-01S has the same pins (its blue LED is on GPIO2) but was not tested.
 *
 *  Set Wi-Fi over the USB-TTL adapter on https://arduino.iosignal.net/wifi/ (the board keeps it);
 *  the page can also set another IOSignal server, a device key and the board setting 'name'.
 *  Or write the Wi-Fi in WIFI_SSID and WIFI_PASS below. Then open https://remocon.kr.
 *
 *  [kr] ESP-01의 릴레이(GPIO0)를 1채널 릴레이(relay1.1)로 제어하고, GPIO2 버튼으로 토글합니다.
 *       시리얼은 늘 켜져 있어 설정 페이지가 언제든 응답합니다.
 *
 *  Taeo Lee <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *  MIT License
 */

#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <string.h>
#include <Bounce2.h>
#include <IOSignalUi.h>

#define RELAY_PIN    0
#define BUTTON_PIN   2
// Most ESP-01 relay modules turn the relay on with LOW. Use false for a module that turns on with HIGH
// (the relay may then click on for a moment at boot, while GPIO0 is HIGH).
#define RELAY_ACTIVE_LOW true

#define WIFI_SSID    "WIFI_SSID"
#define WIFI_PASS    "WIFI_PASS"

IOSignalConfig config;
WiFiClient client;
IOSignal io;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "ESP01-Relay-Btn:HOME");

Bounce2::Button button = Bounce2::Button();
bool relayOn = false;

void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type and buttons for apps without it.
  char name[64];
  iosignal::formatNameSetting(name, sizeof(name), nameSetting.value(), "ESP01-Relay-Btn");
  io.signal("@$name", name);
  io.signal("@$ui", "t=relay1.1;b=r1=1,r1=0");
}

void publishState() {
  if (io.state != IO_READY) return;
  io.signal("@$state", relayOn ? "r1=1" : "r1=0");
}

void setRelay(bool on) {
  relayOn = on;
  digitalWrite(RELAY_PIN, on != RELAY_ACTIVE_LOW ? HIGH : LOW);
  publishState();
}

void onReady() {
  Serial.print("READY CID: ");
  Serial.println(io.cid);
  io.subscribe("#search");
  sendInfo();
  publishState();
  io.signal("#notify", io.cid);
}

void onMessage(char *tag, uint8_t payloadType, uint8_t *payload, size_t payloadSize) {
  if (strcmp(tag, "#search") == 0) { io.signal("#notify", io.cid); return; }

  // Ignore non-text or unterminated payloads before using string APIs.
  if (strcmp(tag, "@") != 0 || payloadType != IOSignal::PAYLOAD_TYPE::TEXT || !payload ||
      !payloadSize || !memchr(payload, 0, payloadSize)) return;

  // r1=1 / r1=0, and all=1 / all=0 (device rules: channel keys). Anything else, '?' too, gets the state.
  const char *c = (const char *)payload;
  Serial.print("CMD ");
  Serial.println(c);
  if (strcmp(c, "r1=1") == 0 || strcmp(c, "all=1") == 0) setRelay(true);
  else if (strcmp(c, "r1=0") == 0 || strcmp(c, "all=0") == 0) setRelay(false);
  else publishState();
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);  // off

  // GPIO2 must be HIGH at reset; the pull-up keeps it there and the button pulls it to GND.
  button.attach(BUTTON_PIN, INPUT_PULLUP);
  button.interval(5);  // debounce, ms
  button.setPressedState(LOW);

  Serial.begin(115200);  // logs and the board settings page
  // Wi-Fi, server and device key: stored over USB, or the sketch values.
  config.begin(Serial, "ESP01-Relay-Btn", WIFI_SSID, WIFI_PASS);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer("io.iosignal.net", 55488);
  config.printServer();

  io.setRxBuffer(200);
  io.onReady(onReady);
  io.onMessage(onMessage);
  if (config.key()) io.auth(config.key());  // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}

void loop() {
  config.loop();  // USB setup requests, Wi-Fi reconnect
  if (config.connected()) io.loop();
  button.update();
  if (button.pressed()) setRelay(!relayOn);
}
