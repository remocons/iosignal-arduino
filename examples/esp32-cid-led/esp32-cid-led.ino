/*
  ESP32: device discovery (remocon.kr), direct CID commands and state publication.
  Wiring: GPIO 4 -> 330 ohm resistor -> LED anode; LED cathode -> GND.
  Uses an external active-high LED; no assumption about onboard LEDs.
  Wi-Fi: leave WIFI_SSID as it is and set it from https://arduino.iosignal.net/wifi/ over USB,
  or write your SSID and password below (then the sketch values are used at every boot).
  The same page can set another IOSignal server (host, port) and a device key (id.key);
  those stored values win over IO_HOST, IO_PORT below. Without a key the board is anonymous.
*/
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <string.h>
#include <IOSignalUi.h>

const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASS = "WIFI_PASS";
const char* IO_HOST = "io.iosignal.net";
const uint16_t IO_PORT = 55488;
const uint8_t LED_PIN = 4;

WiFiClient client;
IOSignal io;
IOSignalConfig config;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "ESP32-LED:HOME");
void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  iosignal::formatNameSetting(name, sizeof(name), nameSetting.value(), "ESP32-LED");
  io.signal("@$name", name);
  io.signal("@$ui", "on,off,toggle");
}
bool ledOn = false;

void publishState() { io.signal("@$state", ledOn ? "on=1" : "on=0"); }

void onReady() {
  Serial.print("READY CID: "); Serial.println(io.cid);
  io.subscribe("#search");
  sendInfo();
  publishState();
  io.signal("#notify", io.cid);
}

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (strcmp(tag, "#search") == 0) {
    io.signal("#notify", io.cid);
    return;
  }
  if (strcmp(tag, "@") != 0) return;
  // Only NUL-terminated TEXT is read as a command; anything else counts as unknown.
  const bool text = type == IOSignal::TEXT && payload && size > 0 && payload[size - 1] == 0;
  const char* command = text ? reinterpret_cast<const char*>(payload) : "";
  if (strcmp(command, "on") == 0) ledOn = true;
  else if (strcmp(command, "off") == 0) ledOn = false;
  else if (strcmp(command, "toggle") == 0) ledOn = !ledOn;
  digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
  publishState(); // Sent for every command, unknown ones too: the app waits for it.
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  config.begin(Serial, "ESP32-LED", WIFI_SSID, WIFI_PASS);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer(IO_HOST, IO_PORT);
  config.printServer();
  io.setRxBuffer(256);
  io.setLog([](const char* line) { Serial.println(line); }); // e.g. no reply from a wrong port
  io.onReady(onReady);
  io.onMessage(onMessage);
  if (config.key()) io.auth(config.key()); // device key set over USB, or anonymous
  io.begin(&client, config.host(), config.port());
}

void loop() {
  config.loop(); // browser Wi-Fi setup over USB, reconnect while offline
  if (config.connected()) io.loop();
}
