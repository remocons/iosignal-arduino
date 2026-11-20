/*
  ESP32: Remocon group channel control. Two boards share the #lab group,
  which remocon.kr shows as one 2-way switch card (switch2.1).
  Wiring: GPIO 4 -> 330 ohm resistor -> LED anode; LED cathode -> GND.
  Upload the same sketch to two boards. Over USB on https://arduino.iosignal.net/wifi/ set Wi-Fi,
  and on the second board the setting "index" to 1 (the first keeps 0): s1 lights the first
  board, s2 the second. The same page can set another IOSignal server and a device key.
*/
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <string.h>
#include <stdio.h>
#include <IOSignalUi.h>

const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASS = "WIFI_PASS";
const char* IO_HOST = "io.remocon.kr";
const uint16_t IO_PORT = 55488;
const uint8_t LED_PIN = 4;

// Keep the channel identical on the two boards. The web app finds the group through #search
// and rewrites the shared state; each board reads its own key (s1 on the first, s2 on the second).
#define GROUP_CHANNEL "#lab"
const char* GROUP_NAME_TAG = GROUP_CHANNEL "$name";
const char* GROUP_UI_TAG = GROUP_CHANNEL "$ui";
const char* GROUP_STATE_TAG = GROUP_CHANNEL "$state";

WiFiClient client;
IOSignal io;
IOSignalConfig config;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "ESP32 LEDs:HOME");
void sendInfo() {
  // Both members send the same $name and $ui; a group's $ui is the extended form (t=…), device rules v2.
  char name[64];
  remocon::formatNameSetting(name, sizeof(name), nameSetting.value(), "ESP32 LEDs");
  io.signal(GROUP_NAME_TAG, name);
  io.signal(GROUP_UI_TAG, "t=switch2.1");
}
// Which key of the shared state this board follows: 0 -> s1, 1 -> s2. Set it over USB.
IOSignalSetting outputIndex(config, "index", "Output (0: s1, 1: s2)", "0", IOSignalSetting::INT);
char stateKey[4] = "s1=";
bool ledOn = false;

void onReady() {
  Serial.print("READY CID: "); Serial.println(io.cid);
  io.subscribe("#search");
  io.subscribe(GROUP_STATE_TAG);
  sendInfo();
  io.signal("#notify", GROUP_CHANNEL);
  // Do not overwrite the shared state on boot: the retained value arrives with the subscription.
}

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (strcmp(tag, "#search") == 0) {
    io.signal("#notify", GROUP_CHANNEL);
    return;
  }
  if (strcmp(tag, GROUP_STATE_TAG) != 0 || type != IOSignal::TEXT ||
      !payload || !size || !memchr(payload, 0, size)) return;
  const char* value = strstr((const char*)payload, stateKey);
  if (!value) return;
  ledOn = value[3] == '1';
  digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
}

// A new index applies at once: the LED follows the other key from the next state.
void useIndex(const char* value) {
  stateKey[1] = char('1' + atoi(value));
  Serial.print("Following "); Serial.println(stateKey);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  outputIndex.range(0, 1);
  config.begin(Serial, "ESP32 LEDs", WIFI_SSID, WIFI_PASS);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer(IO_HOST, IO_PORT);
  config.printServer();
  useIndex(outputIndex.value());
  outputIndex.onChange(useIndex);
  io.setRxBuffer(256);
  io.setLog([](const char* line) { Serial.println(line); });
  io.onReady(onReady);
  io.onMessage(onMessage);
  if (config.key()) io.auth(config.key()); // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}

void loop() {
  config.loop(); // USB setup requests, Wi-Fi reconnect
  if (config.connected()) io.loop();
}
