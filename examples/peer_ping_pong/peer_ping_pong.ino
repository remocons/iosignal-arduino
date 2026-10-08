/*
  ESP32 / ESP32-C3 / ESP8266 peer ping responder.
  Set WiFi credentials. From a CLI on the same server: ping <printed-cid>.
  One TEXT argument contains the sender CID; immediately reply with our CID.
*/
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <IOSignal.h>
#include <string.h>

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
WiFiClient client;
IOSignal io;

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t) {
  if (strcmp(tag, "@ping") == 0 && type == IOSignal::PAYLOAD_TYPE::TEXT) {
    io.signal2((const char*)payload, "@pong", io.cid);
  }
}

void onReady() {
  Serial.print("CID: ");
  Serial.println(io.cid);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(250);
  io.setRxBuffer(128);
  io.onReady(onReady);
  io.onMessage(onMessage);
  io.begin(&client, "io.remocon.kr", 55488);
}

void loop() {
  io.loop();
}
