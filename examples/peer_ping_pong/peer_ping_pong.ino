/*
  Peer ping/pong over the IOSignal server, ESP32 or ESP8266.
  Set WiFi credentials and upload. Serial Monitor: 115200 baud, newline.
  Type: ping <target-cid>, or send ping <this-cid> from io-client / web CLI.
  Both peers must connect to the SAME server. For local tests use a server
  with both WebSocket and CongSocket ports enabled.
  This uses ordinary @ping/@pong SIGNAL messages; it does not modify the core
  or replace io.ping()/io.pong() protocol heartbeats. No subscription needed.
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
const char* IO_HOST = "io.remocon.kr";
const uint16_t IO_PORT = 55488;
WiFiClient client;
IOSignal io;
char waitingCid[MAX_CID_LEN + 1] = {0};
unsigned long pingStarted = 0;
char serialLine[32] = {0};
size_t serialUsed = 0;
bool serialOverflow = false;

bool validPeerCid(const char* cid) {
  const size_t length = strlen(cid);
  if (!length || length > MAX_CID_LEN) return false;
  for (size_t i = 0; i < length; ++i) {
    const uint8_t c = (uint8_t)cid[i];
    if (c <= 32 || c == 127 || c == '@' || c == ',' || c == '#') return false;
  }
  return true;
}

bool readPeerCid(uint8_t type, const uint8_t* payload, size_t size, char* cid) {
  if (type != IOSignal::PAYLOAD_TYPE::TEXT || !payload || size < 2 ||
      size > MAX_CID_LEN + 1 || payload[size - 1] != 0) return false;
  // Never call strlen on a receive buffer before checking its bounds.
  if (memchr(payload, 0, size - 1)) return false;
  memcpy(cid, payload, size);
  return validPeerCid(cid);
}

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  char peer[MAX_CID_LEN + 1];
  if (!tag || !readPeerCid(type, payload, size, peer)) return;
  if (strcmp(tag, "@ping") == 0) {
    // Payload is the requesting CID; reply with our own CID as one TEXT value.
    io.signal2(peer, "@pong", io.cid);
    Serial.print("ping ("); Serial.print(peer); Serial.println(") -> pong");
  } else if (strcmp(tag, "@pong") == 0) {
    Serial.print("pong ("); Serial.print(peer); Serial.println(")");
    if (strcmp(waitingCid, peer) == 0) waitingCid[0] = 0;
  }
}

void sendPeerPing(const char* target) {
  if (io.state != IO_READY) { Serial.println("Not connected"); return; }
  if (!validPeerCid(target)) { Serial.println("Usage: ping <cid>"); return; }
  if (waitingCid[0]) { Serial.println("Waiting for previous peer pong"); return; }
  strcpy(waitingCid, target);
  pingStarted = millis();
  io.signal2(target, "@ping", io.cid);
}

void onReady() {
  waitingCid[0] = 0;
  Serial.print("CID: "); Serial.println(io.cid);
  Serial.println("Type ping <cid> (newline required)");
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(250);
  io.setRxBuffer(128);
  io.onReady(onReady);
  io.onMessage(onMessage);
  io.begin(&client, IO_HOST, IO_PORT);
}

void loop() {
  io.loop();
  if (waitingCid[0] && io.state != IO_READY) waitingCid[0] = 0;
  if (waitingCid[0] && (unsigned long)(millis() - pingStarted) >= 3000UL) {
    Serial.print("ping timeout ("); Serial.print(waitingCid); Serial.println(")");
    waitingCid[0] = 0;
  }
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      serialLine[serialUsed] = 0;
      if (!serialOverflow && strncmp(serialLine, "ping ", 5) == 0) sendPeerPing(serialLine + 5);
      else Serial.println("Usage: ping <cid>");
      serialUsed = 0; serialOverflow = false;
    } else if (serialUsed < sizeof(serialLine) - 1) serialLine[serialUsed++] = c;
    else serialOverflow = true;
  }
}
