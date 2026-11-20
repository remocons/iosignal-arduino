// ESP32-CAM (AI Thinker) camera for https://remocon.kr: the device card shows its pictures (type camera.1).
// Pictures go as binary JPEG on <cid>@image (not kept by the server). With an E2E key they are encrypted
// end to end (signal_e2e) and the card needs the same key; without one they are plain JPEG.
// Board settings over USB on https://arduino.iosignal.net/wifi/: Wi-Fi, server, device key, and
//   name (the device name and app tab, "name:category"), e2e_key (secret, never shown again), interval (ms between pictures).
// Commands on <cid>@: on, off, toggle, interval=<ms>. State: on=1;interval=1000
#include "esp_camera.h"
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <string.h>
#include <IOSignalUi.h>

const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASS = "WIFI_PASS";

WiFiClient client;
IOSignal io;
IOSignalConfig config;

// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "ESP32-CAM:HOME");
// Empty: plain JPEG. Set it on the board settings page and type the same key on the remocon.kr card.
IOSignalSetting e2eKey(config, "e2e_key", "E2E key (empty: plain)", "", IOSignalSetting::TEXT, IOSignalSetting::SECRET);
// Milliseconds between pictures. The app's interval command changes it too and the board keeps it.
IOSignalSetting interval(config, "interval", "Interval ms (100-60000)", "1000", IOSignalSetting::INT, IOSignalSetting::REMOTE);

bool sending = true;      // on/off from the app; every boot starts sending
bool cameraReady = false;
unsigned long lastShot = 0;

void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  remocon::formatNameSetting(name, sizeof(name), nameSetting.value(), "ESP32-CAM");
  io.signal("@$name", name);
  io.signal("@$ui", "t=camera.1");
}

void publishState() {
  char text[40];
  snprintf(text, sizeof(text), "on=%c;interval=%ld", sending ? '1' : '0', interval.toInt());
  io.signal("@$state", text);
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
  if (e2eKey.value()[0]) io.signal_e2e("@image", fb->buf, fb->len, e2eKey.value());
  else io.signal("@image", fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void onReady() {
  Serial.print("READY CID: ");
  Serial.println(io.cid);
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
  e2eKey.onChange([](const char*) {});  // read for each picture: no restart needed
  interval.range(100, 60000);
  interval.onChange([](const char*) { if (io.state == IO_READY) publishState(); });
  config.setServer("io.remocon.kr", 55488);
  config.printServer();

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
  if (!cameraReady || !sending || io.state != IO_READY) return;
  if (millis() - lastShot >= (unsigned long)interval.toInt()) {
    lastShot = millis();
    sendPicture();
  }
}
