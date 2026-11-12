// Board settings for ESP32 and ESP8266 sketches, set from a browser over USB (Improv Wi-Fi Serial):
// Wi-Fi, the IOSignal server and device key, and the sketch's own settings (IOSignalSetting, below).
//
//   IOSignalConfig config;
//   setup(): Serial.begin(115200); config.begin(Serial, "My Device", WIFI_SSID, WIFI_PASS);
//   loop():  config.loop(); if (config.connected()) io.loop();
//
// The chip keeps the last Wi-Fi it joined in flash. A browser page (Web Serial, Improv Wi-Fi)
// replaces it without rebuilding the sketch. SSID and password in the sketch are used only when
// they are not the placeholder "WIFI_SSID"; then they win over the stored ones at every boot.
//
// The same page can set the IOSignal server (host, port) and the device key ("id.key"):
//   config.setServer(IO_HOST, IO_PORT);            // sketch defaults
//   config.setKey(SECRET_ID_KEY);                  // optional sketch key
//   if (config.key()) io.auth(config.key());         // stored > sketch > anonymous
//   io.begin(&client, config.host(), config.port()); // stored > sketch
// Stored values win over the sketch ones. Clearing one in the browser brings back the sketch value.
// They are kept in Preferences (ESP32, namespace "iosignal") or LittleFS /iosignal.cfg (ESP8266, needs
// a flash layout with a file system). A change restarts the board. The key is never sent back or printed.
#ifndef IOSignalConfig_h
#define IOSignalConfig_h

#if defined(ESP32) || defined(ESP8266)

#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>
#else
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <flash_hal.h>
#endif
#include "IOSignalImprov.h"
#include "IOSignalSettings.h"

#define IOSIGNAL_VERSION "9.13.0"

class IOSignalConfig;

// A setting of the sketch, set from the browser (USB) and read in code. Declare it after the config,
// as a global:
//   IOSignalConfig config;
//   IOSignalSetting target(config, "target", "Target CID", "the-cid", IOSignalSetting::CID, IOSignalSetting::REMOTE);
//   IOSignalSetting level(config, "level", "Level", "50", IOSignalSetting::INT); // then level.range(0, 100) in setup()
// value() is the stored value, or the sketch default. set() checks, saves and calls onChange().
// The browser shows the settings in declaration order; up to 8 per sketch, values up to 64 bytes.
class IOSignalSetting {
 public:
  enum Kind : uint8_t {
    TEXT = iosignal_settings::KIND_TEXT, CID = iosignal_settings::KIND_CID, INT = iosignal_settings::KIND_INT,
    BOOL = iosignal_settings::KIND_BOOL, CHOICE = iosignal_settings::KIND_CHOICE
  };
  enum Flag : uint8_t {
    REMOTE = iosignal_settings::FLAG_REMOTE,    // setFromRemote() accepts it
    SECRET = iosignal_settings::FLAG_SECRET,    // never sent back over USB
    RESTART = iosignal_settings::FLAG_RESTART   // a change from the browser restarts the board
  };

  IOSignalSetting(IOSignalConfig& config, const char* name, const char* label, const char* fallback,
                  uint8_t kind = TEXT, uint8_t flags = 0);

  // Rules, for setup() or chained at declaration.
  IOSignalSetting& maxLength(uint8_t n) { _spec.maxLength = n; return *this; }
  IOSignalSetting& range(int32_t min, int32_t max) { _spec.min = min; _spec.max = max; return *this; }
  IOSignalSetting& options(const char* list) { _spec.options = list; return *this; }  // "low|mid|high"
  // Called with the new value after a change from set(), setFromRemote() or the browser.
  // Without it, a change from the browser restarts the board.
  void onChange(void (*callback)(const char* value)) { _onChange = callback; }

  const char* value() const { return _stored[0] ? _stored : (_fallback ? _fallback : ""); }
  long toInt() const { return atol(value()); }
  bool toBool() const { return value()[0] == '1'; }
  bool isStored() const { return _stored[0] != 0; }
  const char* name() const { return _spec.name; }

  // From code: checks the value, saves it, calls onChange. "" goes back to the sketch default.
  bool set(const char* value);
  // From a network command: only for settings declared REMOTE.
  bool setFromRemote(const char* value) { return (_spec.flags & REMOTE) && set(value); }

 private:
  friend class IOSignalConfig;
  bool accepts(const char* v, size_t n) const { return n == 0 || iosignal_settings::validValue(_spec, v, n); }
  iosignal_settings::Spec _spec;
  const char* _fallback;
  char _stored[iosignal_settings::SETTING_VALUE_MAX + 1] = "";
  void (*_onChange)(const char*) = nullptr;
  IOSignalConfig* _config;
  IOSignalSetting* _next = nullptr;
  bool _usable = true;
};

class IOSignalConfig {
 public:
  // name: shown in the browser as the device name. ssid/pass: optional values from the sketch.
  void begin(Stream& serial, const char* name, const char* ssid = nullptr, const char* pass = nullptr) {
    _serial = &serial;
    begin(name, ssid, pass);
  }

  // Without a serial port (a board whose TX pin is used for something else). attachSerial() later
  // turns on browser setup, for example in a setup mode chosen at boot.
  void begin(const char* name, const char* ssid = nullptr, const char* pass = nullptr) {
    _name = name;
    loadSettings();
    checkSettings();
    loadApp();
    WiFi.persistent(true);  // ESP8266 core 3.x defaults to false: nothing would survive a reboot
    WiFi.mode(WIFI_STA);
    if (ssid && ssid[0] && strcmp(ssid, "WIFI_SSID") != 0) {
      WiFi.begin(ssid, pass);
      out().println(F("Wi-Fi: using the SSID in the sketch"));
    } else if (hasStored()) {
      WiFi.begin();
      out().print(F("Wi-Fi: using the stored SSID "));
      out().println(storedSSID());
    } else {
      out().println(F("Wi-Fi: not set. Connect USB and open https://test.iosignal.net/wifi/"));
    }
  }

  void attachSerial(Stream& serial) { _serial = &serial; }

  // Bytes on the serial port that are not part of a browser request, for sketches that read
  // their own serial commands. Without it they are dropped.
  void onOtherInput(void (*callback)(uint8_t byte)) { _otherInput = callback; }

  // True when the chip keeps a Wi-Fi network (set over USB or by an earlier sketch).
  bool wifiStored() const { return hasStored(); }

  // Optional link the browser offers after Wi-Fi is set, for example a control page.
  void setUrl(const char* url) { _url = url; }

  // Optional firmware name and version shown in the browser (default IOSignal and its version).
  void setFirmware(const char* name, const char* version) { _firmware = name; _version = version; }

  bool connected() const { return WiFi.status() == WL_CONNECTED; }

  // Sketch defaults for the IOSignal server and an optional device key ("id.key").
  void setServer(const char* host, uint16_t port) { _sketchHost = host; _sketchPort = port; }
  void setKey(const char* idKey) { _sketchKey = idKey; }

  // Values to use: stored in the browser first, then the sketch ones.
  const char* host() const { return _stored.host[0] ? _stored.host : _sketchHost; }
  uint16_t port() const { return _stored.port ? _stored.port : _sketchPort; }
  // nullptr means anonymous.
  const char* key() const {
    if (_stored.key[0]) return _stored.key;
    return _sketchKey && iosignal_settings::validKey(_sketchKey, strlen(_sketchKey)) ? _sketchKey : nullptr;
  }

  // One line for the serial log, with the key id only: "IOSignal: io.example.net:55488, key abc (stored)".
  void printServer() {
    out().print(F("IOSignal: "));
    out().print(host());
    out().print(':');
    out().print(port());
    out().print(_stored.host[0] || _stored.port ? F(" (stored)") : F(" (sketch)"));
    const char* k = key();
    if (!k) { out().println(F(", anonymous")); return; }
    char id[9];
    iosignal_settings::keyId(k, id);
    out().print(F(", key "));
    out().print(id);
    out().println(_stored.key[0] ? F(" (stored)") : F(" (sketch)"));
  }

  // Reads browser requests from the serial port and retries Wi-Fi every 10 s while disconnected.
  void loop() {
    while (_serial && _serial->available()) {
      const uint8_t b = (uint8_t)_serial->read();
      const bool wasIdle = _parser.idle();
      int r = _parser.push(b);
      if (wasIdle && _parser.idle() && r == Parser::NONE) {  // not part of an Improv packet
        if (_otherInput) _otherInput(b);
        continue;
      }
      if (r == Parser::COMMAND) handle();
      else if (r == Parser::BAD_PACKET) sendError(improv::ERROR_INVALID_RPC);
    }
    if (!connected() && hasStored() && millis() - _lastRetry >= 10000) {
      _lastRetry = millis();
      out().println(F("Wi-Fi: waiting..."));
      WiFi.reconnect();
    }
  }

 private:
  void handle() {
    switch (_parser.command()) {
      case improv::CMD_WIFI_SETTINGS: setWiFi(); break;
      case improv::CMD_CURRENT_STATE:
        sendState(connected() ? improv::STATE_PROVISIONED : improv::STATE_READY);
        if (connected()) sendUrl(improv::CMD_CURRENT_STATE);
        break;
      case improv::CMD_DEVICE_INFO: {
        const char* info[] = {_firmware, _version, chip(), _name};
        sendResult(improv::CMD_DEVICE_INFO, info, 4);
        break;
      }
      case improv::CMD_SCAN: scan(); break;
      case CMD_GET_SETTINGS: sendSettings(CMD_GET_SETTINGS); break;
      case CMD_SET_SETTINGS: setSettings(); break;
      case CMD_LIST_APP: listApp(CMD_LIST_APP); break;
      case CMD_SET_APP: setApp(); break;
      default: sendError(improv::ERROR_UNKNOWN_RPC);
    }
  }

  void setWiFi() {
    char ssid[33], pass[65];
    if (!_parser.wifiSettings(ssid, pass)) { sendError(improv::ERROR_INVALID_RPC); return; }
    // Keep the old values: a failed attempt puts them back instead of leaving the new ones stored.
    String oldSsid = storedSSID(), oldPass = storedPass();
    sendState(improv::STATE_PROVISIONING);
    WiFi.disconnect();
    WiFi.begin(ssid, pass);
    unsigned long start = millis();
    while (!connected() && millis() - start < 20000) delay(100);
    memset(pass, 0, sizeof(pass));
    if (connected()) {
      sendError(improv::ERROR_NONE);
      sendState(improv::STATE_PROVISIONED);
      sendUrl(improv::CMD_WIFI_SETTINGS);
      return;
    }
    sendError(improv::ERROR_UNABLE_TO_CONNECT);
    sendState(improv::STATE_READY);
    WiFi.disconnect();
    if (oldSsid.length()) WiFi.begin(oldSsid.c_str(), oldPass.c_str());
  }

  // IOSignal commands outside the Improv range.
  static const uint8_t CMD_GET_SETTINGS = 0xA0;
  static const uint8_t CMD_SET_SETTINGS = 0xA1;
  static const uint8_t CMD_LIST_APP = 0xA2;
  static const uint8_t CMD_SET_APP = 0xA3;
  static const uint8_t MAX_APP = 8;

  // ---- Sketch settings ----
  friend class IOSignalSetting;

  void addSetting(IOSignalSetting* setting) {
    IOSignalSetting** at = &_first;
    while (*at) at = &(*at)->_next;
    *at = setting;
  }

  // A bad name, a repeated name or a 9th setting is left out, with a line in the log.
  void checkSettings() {
    uint8_t count = 0;
    for (IOSignalSetting* s = _first; s; s = s->_next) {
      bool repeated = false;
      for (IOSignalSetting* t = _first; t != s; t = t->_next) repeated |= t->_usable && strcmp(t->_spec.name, s->_spec.name) == 0;
      const char* fb = s->_fallback ? s->_fallback : "";
      s->_usable = iosignal_settings::validName(s->_spec.name) && !repeated && count < MAX_APP && s->accepts(fb, strlen(fb));
      if (s->_usable) count++;
      else { out().print(F("IOSignal: setting left out (name, repeat, default or more than 8): ")); out().println(s->_spec.name ? s->_spec.name : ""); }
    }
  }

  // One result per setting: name, label, kind, rule, value ("" when secret), source, flags. Then an empty one.
  void listApp(uint8_t command) {
    for (IOSignalSetting* s = _first; s; s = s->_next) {
      if (!s->_usable) continue;
      char rule[80], flags[24];
      iosignal_settings::describe(s->_spec, rule, sizeof(rule));
      iosignal_settings::flagsText(s->_spec.flags, flags, sizeof(flags));
      const bool secret = s->_spec.flags & IOSignalSetting::SECRET;
      const char* row[] = {s->_spec.name, s->_spec.label ? s->_spec.label : s->_spec.name,
                           iosignal_settings::kindName(s->_spec.kind), rule, secret ? "" : s->value(),
                           s->isStored() ? "stored" : "sketch", flags};
      sendResult(command, row, 7);
    }
    sendResult(command, nullptr, 0);
  }

  IOSignalSetting* findSetting(const char* name, size_t n) {
    for (IOSignalSetting* s = _first; s; s = s->_next)
      if (s->_usable && strlen(s->_spec.name) == n && memcmp(s->_spec.name, name, n) == 0) return s;
    return nullptr;
  }

  // All or nothing: every name must exist once and every value must pass its rule.
  void setApp() {
    char next[MAX_APP][iosignal_settings::SETTING_VALUE_MAX + 1];
    IOSignalSetting* list[MAX_APP];
    uint8_t count = 0;
    for (IOSignalSetting* s = _first; s && count < MAX_APP; s = s->_next) {
      if (!s->_usable) continue;
      list[count] = s;
      strcpy(next[count], s->_stored);
      count++;
    }
    bool seen[MAX_APP] = {false};
    bool ok = iosignal_settings::forEachPair(_parser.data(), _parser.size(),
        [&](const char* name, size_t n, const char* v, size_t vlen) {
          for (uint8_t i = 0; i < count; i++) {
            if (strlen(list[i]->_spec.name) != n || memcmp(list[i]->_spec.name, name, n) != 0) continue;
            if (seen[i] || !list[i]->accepts(v, vlen)) return false;
            seen[i] = true;
            memcpy(next[i], v, vlen);
            next[i][vlen] = 0;
            return true;
          }
          return false;  // unknown name
        });
    if (!ok) { memset(next, 0, sizeof(next)); sendError(improv::ERROR_INVALID_RPC); return; }
    bool changed[MAX_APP] = {false};
    bool any = false;
    for (uint8_t i = 0; i < count; i++) {
      changed[i] = strcmp(next[i], list[i]->_stored) != 0;
      any |= changed[i];
    }
    if (any) {
      char old[MAX_APP][iosignal_settings::SETTING_VALUE_MAX + 1];
      for (uint8_t i = 0; i < count; i++) { strcpy(old[i], list[i]->_stored); strcpy(list[i]->_stored, next[i]); }
      if (!saveApp()) {
        for (uint8_t i = 0; i < count; i++) strcpy(list[i]->_stored, old[i]);
        memset(next, 0, sizeof(next));
        memset(old, 0, sizeof(old));
        sendError(improv::ERROR_UNKNOWN);
        return;
      }
      memset(old, 0, sizeof(old));
    }
    memset(next, 0, sizeof(next));
    listApp(CMD_SET_APP);
    bool restart = false;
    for (uint8_t i = 0; i < count; i++) {
      if (!changed[i]) continue;
      if ((list[i]->_spec.flags & IOSignalSetting::RESTART) || !list[i]->_onChange) restart = true;
      else list[i]->_onChange(list[i]->value());
    }
    if (restart) {
      out().println(F("IOSignal: settings saved, restarting"));
      out().flush();
      delay(300);
      ESP.restart();
    }
  }

  // host, host source, port, port source, key state (stored:<id> | sketch:<id> | none), storage (ok | none).
  void sendSettings(uint8_t command) {
    char port[6], id[9], keyState[16];
    snprintf(port, sizeof(port), "%u", (unsigned)this->port());
    const char* k = key();
    if (k) {
      iosignal_settings::keyId(k, id);
      snprintf(keyState, sizeof(keyState), "%s:%s", _stored.key[0] ? "stored" : "sketch", id);
    } else {
      strcpy(keyState, "none");
    }
    const char* row[] = {host(), _stored.host[0] ? "stored" : "sketch", port, _stored.port ? "stored" : "sketch",
                         keyState, storageReady() ? "ok" : "none"};
    sendResult(command, row, 6);
  }

  void setSettings() {
    iosignal_settings::Settings next = _stored;
    bool changed = false;
    if (!iosignal_settings::applyUpdate(_parser.data(), _parser.size(), &next, &changed)) {
      sendError(improv::ERROR_INVALID_RPC);
      return;
    }
    if (!changed) { sendSettings(CMD_SET_SETTINGS); return; }
    if (!storageReady() || !saveSettings(next)) {
      memset(&next, 0, sizeof(next));
      sendError(improv::ERROR_UNKNOWN);
      return;
    }
    _stored = next;
    memset(&next, 0, sizeof(next));
    sendSettings(CMD_SET_SETTINGS);
    out().println(F("IOSignal: settings saved, restarting"));
    out().flush();
    delay(300);
    ESP.restart();
  }

#if defined(ESP32)
  static bool storageReady() { return true; }

  void loadSettings() {
    Preferences prefs;
    if (!prefs.begin("iosignal", true)) return;  // no namespace yet
    iosignal_settings::Settings s;
    String host = prefs.getString("host", ""), key = prefs.getString("key", "");
    if (iosignal_settings::validHost(host.c_str(), host.length())) strcpy(s.host, host.c_str());
    if (iosignal_settings::validKey(key.c_str(), key.length())) strcpy(s.key, key.c_str());
    s.port = prefs.getUShort("port", 0);
    prefs.end();
    _stored = s;
  }

  void loadApp() {
    Preferences prefs;
    if (!prefs.begin("app", true)) return;
    for (IOSignalSetting* s = _first; s; s = s->_next) {
      if (!s->_usable) continue;
      String v = prefs.getString(s->_spec.name, "");
      if (v.length() && s->accepts(v.c_str(), v.length())) strcpy(s->_stored, v.c_str());
    }
    prefs.end();
  }

  bool saveApp() {
    Preferences prefs;
    if (!prefs.begin("app", false)) return false;
    bool ok = true;
    for (IOSignalSetting* s = _first; s; s = s->_next) {
      if (!s->_usable) continue;
      if (s->_stored[0]) ok &= prefs.putString(s->_spec.name, s->_stored) > 0;
      else if (prefs.isKey(s->_spec.name)) prefs.remove(s->_spec.name);
    }
    prefs.end();
    return ok;
  }

  static bool saveSettings(const iosignal_settings::Settings& s) {
    Preferences prefs;
    if (!prefs.begin("iosignal", false)) return false;
    bool ok = true;
    if (s.host[0]) ok &= prefs.putString("host", s.host) > 0; else prefs.remove("host");
    if (s.port) ok &= prefs.putUShort("port", s.port) > 0; else prefs.remove("port");
    if (s.key[0]) ok &= prefs.putString("key", s.key) > 0; else prefs.remove("key");
    prefs.end();
    return ok;
  }
#else
  static constexpr const char* SETTINGS_FILE = "/iosignal.cfg";

  // A flash layout without a file system (FS: none) cannot keep these values.
  static bool storageReady() { return FS_PHYS_SIZE > 0; }

  // No automatic format: a sketch's own LittleFS data is never touched. An area that does not
  // mount holds no valid file system, so saving may format it.
  static bool mount(bool format) {
    if (!storageReady()) return false;
    LittleFSConfig config;
    config.setAutoFormat(false);
    LittleFS.setConfig(config);
    if (LittleFS.begin()) return true;
    return format && LittleFS.format() && LittleFS.begin();
  }

  void loadSettings() {
    if (!mount(false)) return;
    File f = LittleFS.open(SETTINGS_FILE, "r");
    if (!f) return;
    char text[160];
    size_t n = f.readBytes(text, sizeof(text));
    f.close();
    _stored = iosignal_settings::fromText(text, n);
    memset(text, 0, sizeof(text));
  }

  static constexpr const char* APP_FILE = "/app.cfg";

  void loadApp() {
    if (!mount(false)) return;
    File f = LittleFS.open(APP_FILE, "r");
    if (!f) return;
    char text[MAX_APP * (iosignal_settings::SETTING_NAME_MAX + iosignal_settings::SETTING_VALUE_MAX + 2) + 1];
    size_t n = f.readBytes(text, sizeof(text) - 1);
    f.close();
    iosignal_settings::forEachLine(text, n, [&](const char* name, size_t nlen, const char* v, size_t vlen) {
      IOSignalSetting* s = findSetting(name, nlen);
      if (s && vlen && s->accepts(v, vlen)) { memcpy(s->_stored, v, vlen); s->_stored[vlen] = 0; }
    });
    memset(text, 0, sizeof(text));
  }

  bool saveApp() {
    if (!mount(true)) return false;
    char text[MAX_APP * (iosignal_settings::SETTING_NAME_MAX + iosignal_settings::SETTING_VALUE_MAX + 2) + 1];
    size_t n = 0;
    for (IOSignalSetting* s = _first; s; s = s->_next)
      if (s->_usable && s->_stored[0]) n = iosignal_settings::putLine(text, n, sizeof(text), s->_spec.name, s->_stored);
    if (n >= sizeof(text)) return false;
    if (n == 0) return !LittleFS.exists(APP_FILE) || LittleFS.remove(APP_FILE);
    File f = LittleFS.open("/app.tmp", "w");
    bool ok = f && f.write((const uint8_t*)text, n) == n;
    if (f) f.close();
    memset(text, 0, sizeof(text));
    return ok && LittleFS.rename("/app.tmp", APP_FILE);
  }

  static bool saveSettings(const iosignal_settings::Settings& s) {
    if (!mount(true)) return false;
    char text[160];
    size_t n = iosignal_settings::toText(s, text, sizeof(text));
    if (n == 0 && (s.host[0] || s.port || s.key[0])) return false;
    if (n == 0) return !LittleFS.exists(SETTINGS_FILE) || LittleFS.remove(SETTINGS_FILE);
    // Write a new file, then rename it over the old one: a power cut never leaves half a file.
    File f = LittleFS.open("/iosignal.tmp", "w");
    bool ok = f && f.write((const uint8_t*)text, n) == n;
    if (f) f.close();
    memset(text, 0, sizeof(text));
    return ok && LittleFS.rename("/iosignal.tmp", SETTINGS_FILE);
  }
#endif

  void scan() {
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      String ssid = WiFi.SSID(i);
      if (!ssid.length()) continue;
      String rssi(WiFi.RSSI(i));
#if defined(ESP32)
      bool secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
#else
      bool secure = WiFi.encryptionType(i) != ENC_TYPE_NONE;
#endif
      const char* row[] = {ssid.c_str(), rssi.c_str(), secure ? "YES" : "NO"};
      sendResult(improv::CMD_SCAN, row, 3);
    }
    sendResult(improv::CMD_SCAN, nullptr, 0);  // end of list
    WiFi.scanDelete();
  }

  void sendUrl(uint8_t command) {
    const char* urls[] = {_url};
    sendResult(command, urls, _url ? 1 : 0);
  }

  void sendState(uint8_t state) { send(improv::TYPE_STATE, &state, 1); }
  void sendError(uint8_t error) { send(improv::TYPE_ERROR, &error, 1); }

  void sendResult(uint8_t command, const char* const* strings, size_t count) {
    uint8_t data[255];
    size_t size = improv::resultData(data, sizeof(data), command, strings, count);
    if (size) send(improv::TYPE_RESULT, data, (uint8_t)size);
  }

  void send(uint8_t type, const uint8_t* data, uint8_t size) {
    if (!_serial) return;
    uint8_t out[improv::MAX_PACKET];
    size_t n = improv::packet(out, type, data, size);
    _serial->write(out, n);
    _serial->write('\n');
  }

  static const char* chip() {
#if defined(ESP32)
    return ESP.getChipModel();
#else
    return "ESP8266";
#endif
  }

  static bool hasStored() { return storedSSID().length() > 0; }

#if defined(ESP32)
  static String storedSSID() {
    wifi_config_t conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) return String();
    char s[33] = {0};
    memcpy(s, conf.sta.ssid, 32);
    return String(s);
  }
  static String storedPass() {
    wifi_config_t conf;
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) return String();
    char s[65] = {0};
    memcpy(s, conf.sta.password, 64);
    return String(s);
  }
#else
  static String storedSSID() { return WiFi.SSID(); }
  static String storedPass() { return WiFi.psk(); }
#endif

  Stream* _serial = nullptr;
  void (*_otherInput)(uint8_t) = nullptr;
  struct NullPrint : public Print { size_t write(uint8_t) override { return 1; } };
  NullPrint _null;
  Print& out() { return _serial ? static_cast<Print&>(*_serial) : static_cast<Print&>(_null); }
  const char* _name = "IOSignal device";
  const char* _url = nullptr;
  const char* _firmware = "IOSignal";
  const char* _version = IOSIGNAL_VERSION;
  // Wi-Fi settings need about 100 bytes; server settings up to 140 (host 63, port 5, key 64).
  typedef improv::Parser<255> Parser;  // sketch settings may use a whole packet
  Parser _parser;
  iosignal_settings::Settings _stored;
  const char* _sketchHost = "";
  uint16_t _sketchPort = 0;
  const char* _sketchKey = nullptr;
  // No initializer on purpose: settings declared before the config still register (static zero-init).
  IOSignalSetting* _first;
  unsigned long _lastRetry = 0;
};

inline IOSignalSetting::IOSignalSetting(IOSignalConfig& config, const char* name, const char* label, const char* fallback,
                                        uint8_t kind, uint8_t flags)
    : _fallback(fallback), _config(&config) {
  _spec.name = name;
  _spec.label = label;
  _spec.kind = kind;
  _spec.flags = flags;
  _spec.maxLength = 0;
  _spec.min = INT32_MIN;
  _spec.max = INT32_MAX;
  _spec.options = nullptr;
  config.addSetting(this);
}

inline bool IOSignalSetting::set(const char* v) {
  size_t n = v ? strlen(v) : 0;
  if (!_usable || !accepts(v, n)) return false;
  if (strcmp(_stored, n ? v : "") == 0) return true;
  char old[sizeof(_stored)];
  strcpy(old, _stored);
  memcpy(_stored, v, n);
  _stored[n] = 0;
  if (!_config->saveApp()) { strcpy(_stored, old); return false; }
  if (_onChange) _onChange(value());
  return true;
}

#endif  // ESP32 || ESP8266
#endif
