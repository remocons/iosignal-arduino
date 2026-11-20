// What a controller button does to another device: "target:action" text, kept in a board setting.
// No Arduino dependencies (tested on the host). IOSignalButtons.h uses it on ESP32 and ESP8266.
//
//   ?k3vd:r1      toggle key r1 of device ?k3vd (from its last $state)
//   ?k3vd:r2=1    set r2 to 1
//   ?k3vd:all     all channels off when any is on, else all on
//   ?k3vd:all=0   all channels to 0
//   lamp1:toggle  any other word is sent as it is (light.1: on, off, toggle)
//   #lab:s2       a group channel: the shared #lab$state is rewritten (groups take no commands)
//   r1            without "target:", the controller's target setting
//   (empty)       the default for this button (defaultAction)
//
// Channel keys are a lowercase name and a number (r1, s2, led1) with the value 0 or 1, in the order
// the target's $state lists them. A device gets one command: "r1=0", or "all=1" for all (a device
// with channel keys takes all=0 and all=1 by the device rules, and answers with one state). A group
// gets its whole state with the keys changed. Toggles need the target's state: until it is known,
// nothing is sent.
#ifndef IOSignalBinding_h
#define IOSignalBinding_h

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace iosignal {

static const size_t TARGET_MAX = 31;   // a CID (12 at most) or a #channel
static const size_t KEY_MAX = 15;
static const size_t VALUE_MAX = 23;
static const size_t STATE_MAX = 160;   // a group state written back, and the state kept per target
static const size_t CHANNELS_MAX = 8;

enum ActionKind : uint8_t {
  ACTION_NONE,        // empty: use the default
  ACTION_TOGGLE,      // channel key
  ACTION_SET,         // key=value
  ACTION_ALL_TOGGLE,  // all
  ACTION_ALL_SET,     // all=value
  ACTION_COMMAND      // a word sent as it is
};

struct Action {
  ActionKind kind = ACTION_NONE;
  char target[TARGET_MAX + 1] = "";  // empty: the controller's target
  char key[KEY_MAX + 1] = "";
  char value[VALUE_MAX + 1] = "";    // ACTION_SET, ACTION_ALL_SET, ACTION_COMMAND (the word)
};

struct Message {
  char tag[TARGET_MAX + 8];  // "<cid>@" for a command, "#ch$state" for a group
  char text[STATE_MAX + 1];
};

inline bool isGroup(const char* target) { return target && target[0] == '#'; }

// A CID (as the IOSignal settings check it) or #channel.
inline bool validTarget(const char* s, size_t n) {
  if (n == 0 || n > TARGET_MAX) return false;
  if (s[0] == '#') {
    if (n < 2) return false;
    for (size_t i = 1; i < n; i++) {
      char c = s[i];
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
  }
  if (n > 12) return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (c <= ' ' || c > '~' || c == '@' || c == '#' || c == '$' || c == ';' || c == '=' || c == ':') return false;
  }
  return true;
}

inline bool isWordChar(char c, bool first) {
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') return true;
  return !first && ((c >= '0' && c <= '9') || c == '-');
}

// r1, s2, led10: lowercase letters then digits.
inline bool isChannelKey(const char* s, size_t n) {
  size_t i = 0;
  while (i < n && s[i] >= 'a' && s[i] <= 'z') i++;
  if (i == 0 || i == n) return false;
  for (; i < n; i++) if (s[i] < '0' || s[i] > '9') return false;
  return n <= KEY_MAX;
}

inline bool validKey(const char* s, size_t n) {
  if (n == 0 || n > KEY_MAX || !isWordChar(s[0], true)) return false;
  for (size_t i = 1; i < n; i++) if (!isWordChar(s[i], false)) return false;
  return true;
}

inline bool validValue(const char* s, size_t n) {
  if (n == 0 || n > VALUE_MAX) return false;
  for (size_t i = 0; i < n; i++) if ((uint8_t)s[i] < 0x20 || s[i] == 0x7f || s[i] == ';') return false;
  return true;
}

// Parses a setting value. False when the text is not a valid action; out is then left as ACTION_NONE.
inline bool parseAction(const char* text, Action* out) {
  *out = Action();
  if (!text) return true;
  size_t n = strlen(text);
  while (n && text[n - 1] == ' ') n--;
  while (n && *text == ' ') { text++; n--; }
  if (n == 0) return true;
  Action a;
  const char* colon = (const char*)memchr(text, ':', n);
  const char* act = text;
  size_t an = n;
  if (colon) {
    size_t tn = colon - text;
    if (!validTarget(text, tn)) return false;
    memcpy(a.target, text, tn);
    a.target[tn] = 0;
    act = colon + 1;
    an = n - tn - 1;
  }
  if (an == 0) return false;
  const char* eq = (const char*)memchr(act, '=', an);
  if (eq) {
    size_t kn = eq - act, vn = an - kn - 1;
    if (!validKey(act, kn) || !validValue(eq + 1, vn)) return false;
    memcpy(a.key, act, kn); a.key[kn] = 0;
    memcpy(a.value, eq + 1, vn); a.value[vn] = 0;
    a.kind = (kn == 3 && memcmp(act, "all", 3) == 0) ? ACTION_ALL_SET : ACTION_SET;
  } else if (an == 3 && memcmp(act, "all", 3) == 0) {
    a.kind = ACTION_ALL_TOGGLE;
  } else if (isChannelKey(act, an)) {
    memcpy(a.key, act, an); a.key[an] = 0;
    a.kind = ACTION_TOGGLE;
  } else {
    if (!validKey(act, an) || an > VALUE_MAX) return false;
    memcpy(a.value, act, an); a.value[an] = 0;
    a.kind = ACTION_COMMAND;
  }
  if (a.kind == ACTION_COMMAND && isGroup(a.target)) return false;  // groups take no commands
  *out = a;
  return true;
}

// Calls fn(key, keyLen, value, valueLen) for each key=value of a state, in order.
template <typename Fn>
inline void forEachPair(const char* state, Fn fn) {
  const char* p = state ? state : "";
  while (*p) {
    const char* end = strchr(p, ';');
    size_t n = end ? (size_t)(end - p) : strlen(p);
    const char* eq = (const char*)memchr(p, '=', n);
    if (eq) fn(p, (size_t)(eq - p), eq + 1, (size_t)(n - (eq - p) - 1));
    if (!end) break;
    p = end + 1;
  }
}

// The value of key in a state ("r1=0;r2=1"), or false.
inline bool stateValue(const char* state, const char* key, char* out, size_t cap) {
  bool found = false;
  size_t kn = strlen(key);
  forEachPair(state, [&](const char* k, size_t klen, const char* v, size_t vlen) {
    if (found || klen != kn || memcmp(k, key, kn) != 0) return;
    size_t m = vlen < cap - 1 ? vlen : cap - 1;
    memcpy(out, v, m);
    out[m] = 0;
    found = true;
  });
  return found;
}

// Channel keys of a state with the value 0 or 1, in order. Returns how many (at most max).
inline size_t channelKeys(const char* state, char keys[][KEY_MAX + 1], size_t max) {
  size_t count = 0;
  forEachPair(state, [&](const char* k, size_t klen, const char* v, size_t vlen) {
    if (count >= max || !isChannelKey(k, klen) || vlen != 1 || (v[0] != '0' && v[0] != '1')) return;
    memcpy(keys[count], k, klen);
    keys[count][klen] = 0;
    count++;
  });
  return count;
}

// A button with no setting: with as many buttons as channels or more, button i is channel i and a
// button past the channels is "all"; a single button is "all"; two buttons are "all=1" and "all=0";
// otherwise button i is channel i. Needs the target's state for its keys; false while it is unknown.
inline bool defaultAction(uint8_t button, uint8_t buttons, const char* state, Action* out) {
  *out = Action();
  if (!state || !buttons || button >= buttons) return false;
  char keys[CHANNELS_MAX][KEY_MAX + 1];
  size_t m = channelKeys(state, keys, CHANNELS_MAX);
  if (m == 0) return false;
  if (buttons == 1) { out->kind = ACTION_ALL_TOGGLE; return true; }
  if (buttons == 2 && m > 2) {
    out->kind = ACTION_ALL_SET;
    strcpy(out->key, "all");
    strcpy(out->value, button == 0 ? "1" : "0");
    return true;
  }
  if (button < m) {
    out->kind = ACTION_TOGGLE;
    strcpy(out->key, keys[button]);
    return true;
  }
  if (buttons > m) { out->kind = ACTION_ALL_TOGGLE; return true; }
  return false;
}

// Messages for an action on target (the action's own, or the controller's when it names none).
// state is the target's last $state, or nullptr when unknown. Returns how many were written to out
// (0: nothing to send, e.g. a toggle before the state is known). A device always gets one message.
inline size_t plan(const Action& a, const char* target, const char* state, Message* out, size_t max) {
  if (a.kind == ACTION_NONE || !target || !*target || max == 0) return 0;
  const bool group = isGroup(target);
  auto command = [&](size_t i, const char* text) {
    snprintf(out[i].tag, sizeof(out[i].tag), "%s@", target);
    snprintf(out[i].text, sizeof(out[i].text), "%s", text);
  };
  // Keys to change and their new values.
  char keys[CHANNELS_MAX][KEY_MAX + 1];
  char values[CHANNELS_MAX][VALUE_MAX + 1];
  size_t n = 0;
  if (a.kind == ACTION_COMMAND) {
    command(0, a.value);
    return 1;
  } else if (a.kind == ACTION_SET) {
    strcpy(keys[0], a.key);
    strcpy(values[0], a.value);
    n = 1;
    if (!group) {
      char text[KEY_MAX + VALUE_MAX + 2];
      snprintf(text, sizeof(text), "%s=%s", a.key, a.value);
      command(0, text);
      return 1;
    }
  } else if (a.kind == ACTION_TOGGLE) {
    char v[VALUE_MAX + 1];
    if (!state || !stateValue(state, a.key, v, sizeof(v))) return 0;
    strcpy(keys[0], a.key);
    strcpy(values[0], strcmp(v, "1") == 0 ? "0" : "1");
    n = 1;
  } else {  // all, all=value
    // all=value needs no state on a device: it takes all=N itself.
    if (!group && a.kind == ACTION_ALL_SET) {
      char text[KEY_MAX + VALUE_MAX + 2];
      snprintf(text, sizeof(text), "all=%s", a.value);
      command(0, text);
      return 1;
    }
    if (!state) return 0;
    n = channelKeys(state, keys, CHANNELS_MAX);
    if (n == 0) return 0;
    const char* to = a.value;
    if (a.kind == ACTION_ALL_TOGGLE) {
      bool anyOn = false;
      for (size_t i = 0; i < n; i++) {
        char v[2];
        if (stateValue(state, keys[i], v, sizeof(v)) && v[0] == '1') anyOn = true;
      }
      to = anyOn ? "0" : "1";
    }
    for (size_t i = 0; i < n; i++) strcpy(values[i], to);
    if (!group) {
      char text[8];
      snprintf(text, sizeof(text), "all=%s", to);
      command(0, text);
      return 1;
    }
  }
  if (!group) {
    size_t count = 0;
    for (size_t i = 0; i < n && count < max; i++) {
      char text[KEY_MAX + VALUE_MAX + 2];
      snprintf(text, sizeof(text), "%s=%s", keys[i], values[i]);
      command(count++, text);
    }
    return count;
  }
  // A group: the whole state with the changed keys (and new keys at the end).
  if (!state && a.kind != ACTION_SET) return 0;
  char next[STATE_MAX + 1] = "";
  size_t len = 0;
  bool used[CHANNELS_MAX] = {false};
  bool fits = true;
  auto append = [&](const char* k, size_t kn, const char* v, size_t vn) {
    size_t need = (len ? 1 : 0) + kn + 1 + vn;
    if (len + need > STATE_MAX) { fits = false; return; }
    if (len) next[len++] = ';';
    memcpy(next + len, k, kn); len += kn;
    next[len++] = '=';
    memcpy(next + len, v, vn); len += vn;
    next[len] = 0;
  };
  forEachPair(state, [&](const char* k, size_t kn, const char* v, size_t vn) {
    for (size_t i = 0; i < n; i++) {
      if (strlen(keys[i]) == kn && memcmp(keys[i], k, kn) == 0) {
        used[i] = true;
        append(k, kn, values[i], strlen(values[i]));
        return;
      }
    }
    append(k, kn, v, vn);
  });
  for (size_t i = 0; i < n; i++) if (!used[i]) append(keys[i], strlen(keys[i]), values[i], strlen(values[i]));
  if (!fits) return 0;
  snprintf(out[0].tag, sizeof(out[0].tag), "%s$state", target);
  snprintf(out[0].text, sizeof(out[0].text), "%s", next);
  return 1;
}

}  // namespace iosignal

// Up to 9.23.0 these helpers were in namespace remocon; the alias keeps those sketches compiling.
namespace remocon = iosignal;

#endif
