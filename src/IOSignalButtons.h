// Controller buttons that control other devices, each set by a board setting (ESP32, ESP8266).
// What a setting means is in IOSignalBinding.h: "?k3vd:r1", "r2=1", "all", "lamp1:toggle", "#lab:s2", or
// empty for the default. The sketch declares the settings, so the board settings page and the example
// page list them:
//
//   IOSignalSetting target(config, "target", "Target CID", "the-cid", IOSignalSetting::CID, IOSignalSetting::REMOTE);
//   IOSignalSetting b1(config, "b1", "Button 1", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
//   IOSignalSetting b2(config, "b2", "Button 2", "", IOSignalSetting::TEXT, IOSignalSetting::REMOTE);
//   const IOSignalButtons::Button BUTTONS[] = { { 14, &b1 }, { 12, &b2 } };
//   IOSignalButtons buttons(io, target, BUTTONS, 2);
//
//   setup():      config.begin(...); buttons.begin();
//   onReady():    buttons.onReady();
//   onMessage():  if (buttons.onMessage(tag, type, payload, size)) return;
//                 for "@" commands: if (buttons.command(text)) { publishState(); return; }
//   loop():       buttons.loop();
//
// The controller watches <target>@$state of every device its buttons name (a group: #ch$state), so a
// toggle sends the opposite of the last state. Until a target's state arrives, its toggles do nothing.
// Use io.setRxBuffer() large enough for those states (256 or more).
#ifndef IOSignalButtons_h
#define IOSignalButtons_h

#if defined(ESP32) || defined(ESP8266)

#include <Arduino.h>
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <Bounce2.h>
#include "IOSignalBinding.h"

class IOSignalButtons {
 public:
  static const uint8_t MAX_BUTTONS = 7;  // board settings hold 8 values; one is the target
  struct Button {
    uint8_t pin;
    IOSignalSetting* setting;
    uint8_t mode = INPUT_PULLUP;  // INPUT for a pin with an external pull-up (GPIO16 on a D1 mini)
  };

  IOSignalButtons(IOSignal& io, IOSignalSetting& target, const Button* buttons, uint8_t count)
      : _io(io), _target(target), _buttons(buttons), _count(count > MAX_BUTTONS ? MAX_BUTTONS : count) {}

  // Calls fn after a watched state or a setting changed (to redraw a display or publish a state).
  void onChange(void (*fn)()) { _changed = fn; }
  // Lines such as "BUTTON 2: ?k3vd@ r1=1" or "b1: bad setting".
  void onLog(void (*fn)(const char*)) { _log = fn; }

  // In setup(), after config.begin(): the pins and the settings.
  void begin() {
    _self = this;
    for (uint8_t i = 0; i < _count; i++) {
      _bounce[i].attach(_buttons[i].pin, _buttons[i].mode);
      _bounce[i].interval(10);
      _bounce[i].setPressedState(LOW);
      if (_buttons[i].setting) _buttons[i].setting->onChange(settingChanged);
    }
    _target.onChange(settingChanged);
    readSettings();
  }

  // In onReady(): subscribes to the targets' states (the server sends the kept ones at once).
  void onReady() {
    for (uint8_t i = 0; i < _watching; i++) {
      _watch[i].known = false;
      subscribe(_watch[i].target, true);
    }
  }

  // In onMessage(): true when it was a watched target's state (nothing else to do with it).
  bool onMessage(const char* tag, uint8_t type, const uint8_t* payload, size_t size) {
    Watch* w = watchOfTag(tag);
    if (!w) return false;
    if (type != IOSignal::TEXT || !payload || !size || !memchr(payload, 0, size)) return true;
    const char* text = (const char*)payload;
    if (!iosignal::isGroup(w->target) && strcmp(text, "close") == 0) w->known = false;
    else {
      snprintf(w->state, sizeof(w->state), "%s", text);
      w->known = true;
    }
    if (_changed) _changed();
    return true;
  }

  // A command to the controller: "b1=<setting>" or "target=<cid>" changes a setting (checked first),
  // "b1" presses button 1. True when it was one of these; the sketch then publishes its state.
  bool command(const char* text) {
    const char* eq = strchr(text, '=');
    size_t n = eq ? (size_t)(eq - text) : strlen(text);
    if (n == 6 && strncmp(text, "target", 6) == 0 && eq) {
      _target.setFromRemote(eq + 1);
      return true;
    }
    for (uint8_t i = 0; i < _count; i++) {
      IOSignalSetting* s = _buttons[i].setting;
      if (!s || strlen(s->name()) != n || strncmp(s->name(), text, n) != 0) continue;
      if (!eq) { press(i); return true; }
      iosignal::Action a;
      if (iosignal::parseAction(eq + 1, &a)) s->setFromRemote(eq + 1);
      else log(s->name(), ": bad setting");
      return true;
    }
    return false;
  }

  void loop() {
    for (uint8_t i = 0; i < _count; i++) {
      _bounce[i].update();
      if (_bounce[i].pressed()) press(i);
    }
  }

  // Runs button i. False when nothing was sent: a bad setting, or the target's state is not known yet.
  bool press(uint8_t i) {
    if (i >= _count) return false;
    iosignal::Action a;
    const char* text = _buttons[i].setting ? _buttons[i].setting->value() : "";
    if (!iosignal::parseAction(text, &a)) { log(_buttons[i].setting->name(), ": bad setting"); return false; }
    if (a.kind == iosignal::ACTION_NONE) {
      Watch* w = watchOf(_target.value());
      if (!iosignal::defaultAction(i, _count, w && w->known ? w->state : nullptr, &a)) {
        log("BUTTON ", i + 1, ": target state not known yet");
        return false;
      }
    }
    return execute(a, "BUTTON ", i + 1);
  }

  // Runs an action given as text (same form as a setting; a target it names must be watched already,
  // or its toggles wait for a state): for a card's own buttons such as "r1" or "all".
  bool run(const char* text) {
    iosignal::Action a;
    if (!iosignal::parseAction(text, &a) || a.kind == iosignal::ACTION_NONE) return false;
    return execute(a, "RUN ", 0);
  }

 private:
  bool execute(const iosignal::Action& a, const char* label, int number) {
    const char* target = a.target[0] ? a.target : _target.value();
    Watch* w = watchOf(target);
    iosignal::Message* out = _out;
    size_t n = iosignal::plan(a, target, w && w->known ? w->state : nullptr, out, iosignal::CHANNELS_MAX);
    if (!n) { log(label, number, ": target state not known yet"); return false; }
    for (size_t m = 0; m < n; m++) {
      size_t len = strlen(out[m].tag);
      if (out[m].tag[len - 1] == '@') {
        out[m].tag[len - 1] = 0;
        _io.signal2(out[m].tag, "@", out[m].text);
        out[m].tag[len - 1] = '@';
      } else {
        _io.signal(out[m].tag, out[m].text);
        // A group's state is ours too now; members follow it.
        if (w) { snprintf(w->state, sizeof(w->state), "%s", out[m].text); w->known = true; }
      }
      log(label, number, ": ", out[m].tag, " ", out[m].text);
    }
    if (_changed) _changed();
    return true;
  }

 public:
  // The controller's target and its last state (nullptr while unknown).
  const char* target() const { return _target.value(); }
  const char* state(const char* target = nullptr) const {
    const Watch* w = const_cast<IOSignalButtons*>(this)->watchOf(target ? target : _target.value());
    return w && w->known ? w->state : nullptr;
  }
  // Whether button i's setting is valid (an empty one is).
  bool valid(uint8_t i) const {
    iosignal::Action a;
    return i < _count && (!_buttons[i].setting || iosignal::parseAction(_buttons[i].setting->value(), &a));
  }

 private:
  struct Watch {
    char target[iosignal::TARGET_MAX + 1];
    char state[iosignal::STATE_MAX + 1];
    bool known;
  };

  static void settingChanged(const char*) {
    if (!_self) return;
    _self->readSettings();
    if (_self->_changed) _self->_changed();
  }

  // The targets to watch: the controller's target, then each one a setting names.
  void readSettings() {
    Watch* next = _next;
    uint8_t count = 0;
    auto add = [&](const char* t) {
      if (!t || !*t) return;
      for (uint8_t i = 0; i < count; i++) if (strcmp(next[i].target, t) == 0) return;
      Watch* old = watchOf(t);
      if (old) next[count] = *old;
      else {
        snprintf(next[count].target, sizeof(next[count].target), "%s", t);
        next[count].state[0] = 0;
        next[count].known = false;
      }
      count++;
    };
    add(_target.value());
    for (uint8_t i = 0; i < _count; i++) {
      iosignal::Action a;
      if (!_buttons[i].setting) continue;
      if (iosignal::parseAction(_buttons[i].setting->value(), &a)) add(a.target);
      else log(_buttons[i].setting->name(), ": bad setting");
    }
    // Stop watching targets no longer named, start the new ones.
    for (uint8_t i = 0; i < _watching; i++) {
      bool kept = false;
      for (uint8_t j = 0; j < count; j++) if (strcmp(next[j].target, _watch[i].target) == 0) kept = true;
      if (!kept) subscribe(_watch[i].target, false);
    }
    for (uint8_t j = 0; j < count; j++) if (!watchOf(next[j].target)) subscribe(next[j].target, true);
    memcpy(_watch, next, sizeof(Watch) * count);
    _watching = count;
  }

  // A device: <cid>@$state. A group: its shared #ch$state.
  void subscribe(const char* target, bool on) {
    if (_io.state != IO_READY) return;
    char tag[iosignal::TARGET_MAX + 8];
    snprintf(tag, sizeof(tag), iosignal::isGroup(target) ? "%s$state" : "%s@$state", target);
    if (on) _io.subscribe(tag);
    else _io.unsubscribe(tag);
  }

  Watch* watchOf(const char* target) {
    for (uint8_t i = 0; i < _watching; i++) if (strcmp(_watch[i].target, target) == 0) return &_watch[i];
    return nullptr;
  }

  Watch* watchOfTag(const char* tag) {
    for (uint8_t i = 0; i < _watching; i++) {
      size_t n = strlen(_watch[i].target);
      const char* rest = iosignal::isGroup(_watch[i].target) ? "$state" : "@$state";
      if (strncmp(tag, _watch[i].target, n) == 0 && strcmp(tag + n, rest) == 0) return &_watch[i];
    }
    return nullptr;
  }

  template <typename... Parts>
  void log(Parts... parts) {
    if (!_log) return;
    char line[iosignal::STATE_MAX + 48] = "";
    append(line, sizeof(line), parts...);
    _log(line);
  }
  static void append(char*, size_t) {}
  template <typename... Rest>
  static void append(char* line, size_t cap, const char* part, Rest... rest) {
    size_t len = strlen(line);
    snprintf(line + len, cap - len, "%s", part ? part : "");
    append(line, cap, rest...);
  }
  template <typename... Rest>
  static void append(char* line, size_t cap, int part, Rest... rest) {
    size_t len = strlen(line);
    snprintf(line + len, cap - len, "%d", part);
    append(line, cap, rest...);
  }

  IOSignal& _io;
  IOSignalSetting& _target;
  const Button* _buttons;
  uint8_t _count;
  Bounce2::Button _bounce[MAX_BUTTONS];
  Watch _watch[MAX_BUTTONS + 1];
  uint8_t _watching = 0;
  void (*_changed)() = nullptr;
  void (*_log)(const char*) = nullptr;
  // Scratch space kept off the stack (the ESP8266 loop stack is 4 KB).
  Watch _next[MAX_BUTTONS + 1];
  iosignal::Message _out[iosignal::CHANNELS_MAX];
  // One controller per sketch: the settings' change callbacks have no context.
  static inline IOSignalButtons* _self = nullptr;
};

#endif  // ESP32 || ESP8266
#endif
