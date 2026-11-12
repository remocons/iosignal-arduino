/*
  ESP32 / ESP32-C3 / ESP8266 peer ping responder.
  Set Wi-Fi over USB on https://test.iosignal.net/wifi/ (the board keeps it; the page can also set
  another IOSignal server and a device key), or write it below. From a CLI on the same server:
  ping <printed-cid>.
  One TEXT argument contains the sender CID; immediately reply with our CID.
*/
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <string.h>

const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASSWORD = "WIFI_PASS";
WiFiClient client;
IOSignal io;
IOSignalConfig config;

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t) {
  Serial.print("[RX] ");
  Serial.println(tag);
  if (strcmp(tag, "@ping") == 0 && type == IOSignal::PAYLOAD_TYPE::TEXT) {
    io.signal2((const char*)payload, "@pong", io.cid);
    Serial.print("[TX] @pong -> ");
    Serial.println((const char*)payload);
  }
}

// Library messages, e.g. when a duplicate login stops this device.
void printLog(const char* message) {
  Serial.println(message);
}

void onReady() {
  Serial.print("CID: ");
  Serial.println(io.cid);
}

void setup() {
  Serial.begin(115200);
  // Wi-Fi, server and device key: set over USB, or the sketch values.
  config.begin(Serial, "Peer ping", WIFI_SSID, WIFI_PASSWORD);
  config.setServer("io.iosignal.net", 55488);
  config.printServer();
  io.setRxBuffer(128);
  io.onReady(onReady);
  io.onMessage(onMessage);
  io.setLog(printLog);
  if (config.key()) io.auth(config.key()); // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}

void loop() {
  config.loop(); // USB setup requests, Wi-Fi reconnect
  if (config.connected()) io.loop();
}
