// ESP32-CAM (AI Thinker) camera for https://remocon.kr with per-receiver session keys (type camera-session.1).
// Pictures go only encrypted, on <cid>@image, with a session key that one receiver (a remocon.kr card) and the
// camera agree on (X25519, see E2ESession.h). Only that receiver can open them. The board setting e2e_key proves
// both sides: a receiver without it cannot start a session.
// The session's secrets live in RAM only: a reboot, a new e2e_key or 10 minutes end the session and the
// receiver makes a new one. A key read from flash later does not open pictures recorded before.
// Board settings over USB on https://arduino.iosignal.net/wifi/: Wi-Fi, server, device key, and
//   name (the device name and app tab, "name:category"), e2e_key (secret, never shown again, required),
//   interval (ms between pictures).
// Commands on <cid>@: on, off, toggle, interval=<ms>. State: on=1;interval=1000;session=0
#include "esp_camera.h"
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <string.h>
#include <IOSignalUi.h>
#include "E2ESession.h"

const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASS = "WIFI_PASS";

WiFiClient client;
IOSignal io;
IOSignalConfig config;

// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "ESP32-CAM:HOME");
// Required: without it no session starts and no pictures are sent. Type the same key on the remocon.kr card.
IOSignalSetting e2eKey(config, "e2e_key", "E2E key (required)", "", IOSignalSetting::TEXT, IOSignalSetting::SECRET);
// Milliseconds between pictures. The app's interval command changes it too and the board keeps it.
IOSignalSetting interval(config, "interval", "Interval ms (100-60000)", "1000", IOSignalSetting::INT, IOSignalSetting::REMOTE);

#ifndef E2E_SESSION_MS
#define E2E_SESSION_MS (10UL * 60 * 1000)
#endif
const unsigned long SESSION_MS = E2E_SESSION_MS;  // a session's longest life, then the holder makes a new one
const unsigned long ROTATE_WAIT_MS = 10000;         // after a rotation, how long the holder has to answer
const unsigned long REQUEST_GAP_MS = 300;           // at most one challenge request per this time
const unsigned long HELLO_GAP_MS = 100;             // at most one signed hello per this time (slows key guessing)

bool sending = true;      // on/off from the app; every boot starts sending
bool cameraReady = false;
unsigned long lastShot = 0;

// The session, in RAM only.
bool sessionActive = false;
char sessionKey[65] = "";
uint8_t nonce[e2esession::NONCE];      // the challenge a hello must sign
uint8_t prevNonce[e2esession::NONCE];  // the one before, to tell a late hello from a wrong key
bool havePrevNonce = false;
uint8_t holder[e2esession::HOLDER] = {0};
unsigned long sessionStart = 0, rotatedAt = 0, lastRequest = 0, lastHello = 0;
bool requestSeen = false, helloSeen = false;

void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  remocon::formatNameSetting(name, sizeof(name), nameSetting.value(), "ESP32-CAM");
  io.signal("@$name", name);
  io.signal("@$ui", "t=camera-session.1");
}

void publishState() {
  char text[48];
  snprintf(text, sizeof(text), "on=%c;interval=%ld;session=%c", sending ? '1' : '0', interval.toInt(),
           sessionActive ? '1' : '0');
  io.signal("@$state", text);
}

// ---------------------------------------------------------------- session
void newNonce() {
  memcpy(prevNonce, nonce, sizeof(nonce));
  esp_fill_random(nonce, sizeof(nonce));  // hardware random numbers (true random while Wi-Fi is on)
}

// to: for REFUSED and BUSY, the holder id of the hello being answered; else the session's holder.
void sendChallenge(e2esession::Reason reason, const uint8_t* to = nullptr) {
  if (io.state != IO_READY || !e2eKey.value()[0]) return;
  uint8_t c[e2esession::CHALLENGE_SIZE];
  e2esession::challenge(c, reason, nonce, to ? to : holder);
  io.signal("@e2e-challenge", c, sizeof(c));
}

void endSession(const char* why) {
  if (sessionActive) {
    Serial.print("SESSION end: ");
    Serial.println(why);
  }
  sessionActive = false;
  e2esession::wipe(sessionKey, sizeof(sessionKey));
}

// Nobody has the session: a new nonce, no holder, and every open card may answer.
void restartSessions(const char* why) {
  endSession(why);
  memset(holder, 0, sizeof(holder));
  havePrevNonce = false;
  newNonce();
  rotatedAt = 0;
  sendChallenge(e2esession::START);
  publishState();
}

void onHello(const uint8_t* data, size_t size) {
  const char* key = e2eKey.value();
  if (!key[0] || !size || data[0] != e2esession::VERSION) return;

  if (size == e2esession::REQUEST_SIZE) {  // a receiver asks for the challenge
    if (requestSeen && millis() - lastRequest < REQUEST_GAP_MS) return;
    requestSeen = true;
    lastRequest = millis();
    sendChallenge(e2esession::REQUEST);
    return;
  }
  if (size != e2esession::HELLO_SIZE) return;
  if (helloSeen && millis() - lastHello < HELLO_GAP_MS) return;
  helloSeen = true;
  lastHello = millis();

  if (!e2esession::helloValid(key, nonce, data)) {
    // Signed for the previous nonce: another receiver was first. Otherwise the key does not match.
    const bool late = havePrevNonce && e2esession::helloValid(key, prevNonce, data);
    Serial.println(late ? "SESSION hello too late (another receiver has it)" : "SESSION hello refused: wrong key");
    uint8_t to[e2esession::HOLDER];
    e2esession::holderOf(to, data + 1);
    sendChallenge(late ? e2esession::BUSY : e2esession::REFUSED, to);
    return;
  }

  uint8_t secret[32], ready[e2esession::READY_SIZE];
  esp_fill_random(secret, sizeof(secret));
  char newKey[65];
  const unsigned long t = millis();
  if (!e2esession::accept(key, nonce, data + 1, secret, ready, newKey)) {
    uint8_t to[e2esession::HOLDER];
    e2esession::holderOf(to, data + 1);
    sendChallenge(e2esession::REFUSED, to);
    return;
  }
  memcpy(sessionKey, newKey, sizeof(sessionKey));
  e2esession::wipe(newKey, sizeof(newKey));
  e2esession::holderOf(holder, data + 1);
  sessionActive = true;
  sessionStart = millis();
  rotatedAt = 0;
  io.signal("@e2e-ready", ready, sizeof(ready));
  havePrevNonce = true;
  newNonce();  // a replayed hello no longer matches
  Serial.printf("SESSION new (holder %02x%02x%02x%02x, %lu ms)\n", holder[0], holder[1], holder[2], holder[3], millis() - t);
  publishState();
}

void sessionLoop() {
  if (sessionActive && millis() - sessionStart >= SESSION_MS) {
    endSession("expired");
    havePrevNonce = false;
    newNonce();
    rotatedAt = millis();
    sendChallenge(e2esession::ROTATE);  // the holder answers with a new hello
    publishState();
  } else if (!sessionActive && rotatedAt && millis() - rotatedAt >= ROTATE_WAIT_MS) {
    restartSessions("holder did not answer");  // anyone may take it now
  }
}

// ====== ESP32-CAM (AI Thinker) pins ======
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

bool cameraSetup() {
  camera_config_t cam = {};
  cam.ledc_channel = LEDC_CHANNEL_0;
  cam.ledc_timer   = LEDC_TIMER_0;
  cam.pin_d0       = Y2_GPIO_NUM;
  cam.pin_d1       = Y3_GPIO_NUM;
  cam.pin_d2       = Y4_GPIO_NUM;
  cam.pin_d3       = Y5_GPIO_NUM;
  cam.pin_d4       = Y6_GPIO_NUM;
  cam.pin_d5       = Y7_GPIO_NUM;
  cam.pin_d6       = Y8_GPIO_NUM;
  cam.pin_d7       = Y9_GPIO_NUM;
  cam.pin_xclk     = XCLK_GPIO_NUM;
  cam.pin_pclk     = PCLK_GPIO_NUM;
  cam.pin_vsync    = VSYNC_GPIO_NUM;
  cam.pin_href     = HREF_GPIO_NUM;
  cam.pin_sccb_sda = SIOD_GPIO_NUM;
  cam.pin_sccb_scl = SIOC_GPIO_NUM;
  cam.pin_pwdn     = PWDN_GPIO_NUM;
  cam.pin_reset    = RESET_GPIO_NUM;
  cam.xclk_freq_hz = 20000000;
  cam.pixel_format = PIXFORMAT_JPEG;
  cam.fb_location  = CAMERA_FB_IN_PSRAM;
  cam.frame_size   = FRAMESIZE_VGA;  // 640x480, usually 20-40 KB per JPEG
  cam.jpeg_quality = 12;             // lower is better quality and larger
  cam.fb_count     = 2;
  cam.grab_mode    = CAMERA_GRAB_LATEST;
  return esp_camera_init(&cam) == ESP_OK;
}

void sendPicture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  io.signal_e2e("@image", fb->buf, fb->len, sessionKey);  // never plain
  esp_camera_fb_return(fb);
}

void onReady() {
  Serial.print("READY CID: ");
  Serial.println(io.cid);
  io.subscribe("#search");
  sendInfo();
  // A new connection may have a new CID: receivers start again.
  restartSessions("connected again");
  io.signal("#notify", io.cid);
  if (!e2eKey.value()[0]) Serial.println("No e2e_key: set it on the board settings page. No pictures until then.");
}

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  if (strcmp(tag, "#search") == 0) {
    io.signal("#notify", io.cid);
    return;
  }
  if (strcmp(tag, "@e2e-hello") == 0) {
    if (type == IOSignal::BINARY && payload) onHello(payload, size);
    return;
  }
  if (strcmp(tag, "@") != 0 || type != IOSignal::TEXT || !payload || !size || !memchr(payload, 0, size)) return;
  const char* command = (const char*)payload;
  if (strcmp(command, "on") == 0) sending = true;
  else if (strcmp(command, "off") == 0) sending = false;
  else if (strcmp(command, "toggle") == 0) sending = !sending;
  else if (strncmp(command, "interval=", 9) == 0) interval.setFromRemote(command + 9);  // checked: 100-60000
  publishState();  // also for unknown or refused commands, so the app stops waiting
}

void setup() {
  Serial.begin(115200);
  // Wi-Fi, server and device key: set over USB, or the sketch values.
  config.begin(Serial, "ESP32-CAM", WIFI_SSID, WIFI_PASS);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  e2eKey.maxLength(64);
  e2eKey.onChange([](const char*) { restartSessions("new e2e_key"); });  // sessions made with the old key end
  interval.range(100, 60000);
  interval.onChange([](const char*) { if (io.state == IO_READY) publishState(); });
  config.setServer("io.remocon.kr", 55488);
  config.printServer();

  newNonce();
  cameraReady = cameraSetup();
  if (!cameraReady) Serial.println("Camera init failed: no pictures (board settings still work)");

  io.setRxBuffer(200);
  io.setLog([](const char* line) { Serial.println(line); });
  io.onReady(onReady);
  io.onMessage(onMessage);
  if (config.key()) io.auth(config.key());  // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}

void loop() {
  config.loop();  // USB setup requests, Wi-Fi reconnect
  if (!config.connected()) return;
  io.loop();
  if (io.state != IO_READY) return;
  sessionLoop();
  if (!cameraReady || !sending || !sessionActive) return;
  if (millis() - lastShot >= (unsigned long)interval.toInt()) {
    lastShot = millis();
    sendPicture();
  }
}
