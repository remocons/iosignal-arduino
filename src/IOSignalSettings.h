// IOSignal server and device key set from a browser over USB (IOSignalConfig), without Arduino dependencies.
//
// Update data (IOSignal command 0xA1): [field][length][value]... in any order.
//   field 1 host  1-63 characters: letters, digits, '.', '-'
//   field 2 port  "1"-"65535"
//   field 3 key   "id.key": id 1-8 characters, a dot, then the key; printable ASCII without spaces, <= 64
// Length 0 clears the stored value (the sketch value is used again). Fields not sent stay as they are.
// The whole update is refused when any field is invalid, unknown or repeated.
//
// Sketch settings (IOSignalSetting handles, commands 0xA2 list / 0xA3 set) use the rules further down:
// names of 1-15 characters (a-z, 0-9, _), values up to 64 bytes checked by kind (text, cid, int, bool, choice).
#ifndef IOSignalSettings_h
#define IOSignalSettings_h

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

namespace iosignal_settings {

enum Field : uint8_t { FIELD_HOST = 1, FIELD_PORT = 2, FIELD_KEY = 3 };
static const size_t HOST_MAX = 63;
static const size_t KEY_MAX = 64;

struct Settings {
  char host[HOST_MAX + 1] = "";  // "" = not stored
  uint16_t port = 0;             // 0 = not stored
  char key[KEY_MAX + 1] = "";    // "" = not stored
};

inline bool validHost(const char* s, size_t n) {
  if (n == 0 || n > HOST_MAX || s[0] == '.' || s[0] == '-') return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
  }
  return true;
}

// Decimal 1-65535 without sign, spaces or leading zeros.
inline bool parsePort(const char* s, size_t n, uint16_t* out) {
  if (n == 0 || n > 5 || s[0] == '0') return false;
  uint32_t v = 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = v * 10 + (uint32_t)(s[i] - '0');
  }
  if (v < 1 || v > 65535) return false;
  *out = (uint16_t)v;
  return true;
}

// The same shape Boho::set_id_key accepts: id of 1-8 characters, '.', a non-empty key.
inline bool validKey(const char* s, size_t n) {
  if (n < 3 || n > KEY_MAX) return false;
  const char* dot = (const char*)memchr(s, '.', n);
  if (!dot || dot == s || dot - s > 8 || (size_t)(dot - s) + 1 >= n) return false;
  for (size_t i = 0; i < n; i++) if (s[i] <= ' ' || s[i] > '~') return false;
  return true;
}

// Writes the id part of "id.key" (at most 8 characters) into out[9].
inline void keyId(const char* key, char* out) {
  size_t i = 0;
  for (; key[i] && key[i] != '.' && i < 8; i++) out[i] = key[i];
  out[i] = 0;
}

// Applies an update to s. On any error s is left unchanged and false is returned.
// changed is set when a stored value differs afterwards.
inline bool applyUpdate(const uint8_t* data, size_t size, Settings* s, bool* changed) {
  Settings next = *s;
  bool seen[4] = {false, false, false, false};
  size_t at = 0;
  if (size == 0) return false;
  while (at < size) {
    if (at + 2 > size) return false;
    uint8_t field = data[at], len = data[at + 1];
    const char* value = (const char*)data + at + 2;
    if (at + 2 + len > size || field < FIELD_HOST || field > FIELD_KEY || seen[field]) return false;
    seen[field] = true;
    if (field == FIELD_HOST) {
      if (len && !validHost(value, len)) return false;
      memcpy(next.host, value, len);
      next.host[len] = 0;
    } else if (field == FIELD_PORT) {
      uint16_t port = 0;
      if (len && !parsePort(value, len, &port)) return false;
      next.port = port;
    } else {
      if (len && !validKey(value, len)) return false;
      memcpy(next.key, value, len);
      next.key[len] = 0;
    }
    at += 2 + len;
  }
  *changed = strcmp(next.host, s->host) != 0 || next.port != s->port || strcmp(next.key, s->key) != 0;
  *s = next;
  return true;
}

// Text form for a file: "host=...\nport=...\nkey=...\n", stored values only. Returns 0 if it does not fit.
inline size_t toText(const Settings& s, char* out, size_t cap) {
  size_t at = 0;
  auto put = [&](const char* name, const char* value) {
    size_t n = strlen(name), v = strlen(value);
    if (at + n + 1 + v + 1 >= cap) { at = cap; return; }
    memcpy(out + at, name, n); at += n; out[at++] = '=';
    memcpy(out + at, value, v); at += v; out[at++] = '\n';
  };
  char port[6] = "";
  if (s.port) {
    uint16_t p = s.port; char tmp[6]; size_t n = 0;
    while (p) { tmp[n++] = (char)('0' + p % 10); p /= 10; }
    for (size_t i = 0; i < n; i++) port[i] = tmp[n - 1 - i];
    port[n] = 0;
  }
  if (s.host[0]) put("host", s.host);
  if (s.port) put("port", port);
  if (s.key[0]) put("key", s.key);
  if (at >= cap) return 0;
  out[at] = 0;
  return at;
}

// Reads the text form. Invalid lines are skipped, so a damaged file never yields a bad value.
inline Settings fromText(const char* text, size_t size) {
  Settings s;
  size_t at = 0;
  while (at < size) {
    size_t end = at;
    while (end < size && text[end] != '\n') end++;
    const char* line = text + at;
    size_t n = end - at;
    if (n > 5 && memcmp(line, "host=", 5) == 0 && validHost(line + 5, n - 5)) { memcpy(s.host, line + 5, n - 5); s.host[n - 5] = 0; }
    else if (n > 5 && memcmp(line, "port=", 5) == 0) { uint16_t p; if (parsePort(line + 5, n - 5, &p)) s.port = p; }
    else if (n > 4 && memcmp(line, "key=", 4) == 0 && validKey(line + 4, n - 4)) { memcpy(s.key, line + 4, n - 4); s.key[n - 4] = 0; }
    at = end + 1;
  }
  return s;
}


// ---- Sketch settings ------------------------------------------------------------------------

enum Kind : uint8_t { KIND_TEXT, KIND_CID, KIND_INT, KIND_BOOL, KIND_CHOICE };
enum Flag : uint8_t {
  FLAG_REMOTE = 1,   // may also be changed by a network command (setFromRemote)
  FLAG_SECRET = 2,   // never sent back over USB
  FLAG_RESTART = 4   // a change restarts the board even when onChange is set
};
static const size_t SETTING_NAME_MAX = 15;   // NVS key limit on ESP32
static const size_t SETTING_VALUE_MAX = 64;
static const size_t CID_MAX = 12;

struct Spec {
  const char* name;
  const char* label;
  uint8_t kind;
  uint8_t flags;
  uint8_t maxLength;    // text: 1-64
  int32_t min, max;     // int
  const char* options;  // choice: "a|b|c"
};

inline bool validName(const char* s) {
  size_t n = s ? strlen(s) : 0;
  if (n == 0 || n > SETTING_NAME_MAX) return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

inline const char* kindName(uint8_t kind) {
  static const char* const names[] = {"text", "cid", "int", "bool", "choice"};
  return kind <= KIND_CHOICE ? names[kind] : "text";
}

// A CID: 1-12 printable characters without space or the tag characters @ # $ ; =.
inline bool validCid(const char* s, size_t n) {
  if (n == 0 || n > CID_MAX) return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (c <= ' ' || c > '~' || c == '@' || c == '#' || c == '$' || c == ';' || c == '=') return false;
  }
  return true;
}

inline bool parseInt(const char* s, size_t n, int32_t* out) {
  size_t i = 0;
  bool negative = n > 0 && s[0] == '-';
  if (negative) i = 1;
  if (i == n || n - i > 10) return false;
  int64_t v = 0;
  for (; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = v * 10 + (s[i] - '0');
  }
  if (negative) v = -v;
  if (v < INT32_MIN || v > INT32_MAX) return false;
  *out = (int32_t)v;
  return true;
}

// Non-empty values only: an empty value means "back to the sketch default" and is handled by the caller.
inline bool validValue(const Spec& spec, const char* v, size_t n) {
  if (n == 0 || n > SETTING_VALUE_MAX) return false;
  for (size_t i = 0; i < n; i++) if ((uint8_t)v[i] < 0x20 || v[i] == 0x7f) return false;
  switch (spec.kind) {
    case KIND_TEXT: return n <= (spec.maxLength ? spec.maxLength : SETTING_VALUE_MAX);
    case KIND_CID: return validCid(v, n);
    case KIND_BOOL: return n == 1 && (v[0] == '0' || v[0] == '1');
    case KIND_INT: {
      int32_t x;
      return parseInt(v, n, &x) && x >= spec.min && x <= spec.max;
    }
    case KIND_CHOICE: {
      const char* o = spec.options ? spec.options : "";
      while (*o) {
        const char* end = strchr(o, '|');
        size_t len = end ? (size_t)(end - o) : strlen(o);
        if (len == n && memcmp(o, v, n) == 0) return true;
        if (!end) break;
        o = end + 1;
      }
      return false;
    }
  }
  return false;
}

// The rule as text for the browser: "max=12", "min=0;max=100", "options=a|b", "" for bool.
inline void describe(const Spec& spec, char* out, size_t cap) {
  if (cap == 0) return;
  out[0] = 0;
  switch (spec.kind) {
    case KIND_TEXT: snprintf(out, cap, "max=%u", (unsigned)(spec.maxLength ? spec.maxLength : SETTING_VALUE_MAX)); break;
    case KIND_CID: snprintf(out, cap, "max=%u", (unsigned)CID_MAX); break;
    case KIND_INT: snprintf(out, cap, "min=%ld;max=%ld", (long)spec.min, (long)spec.max); break;
    case KIND_CHOICE: snprintf(out, cap, "options=%s", spec.options ? spec.options : ""); break;
  }
}

// "remote,secret,restart" (only those set).
inline void flagsText(uint8_t flags, char* out, size_t cap) {
  if (cap == 0) return;
  out[0] = 0;
  const char* names[] = {"remote", "secret", "restart"};
  size_t at = 0;
  for (uint8_t i = 0; i < 3; i++) {
    if (!(flags & (1 << i))) continue;
    int w = snprintf(out + at, cap - at, "%s%s", at ? "," : "", names[i]);
    if (w < 0 || (size_t)w >= cap - at) return;
    at += (size_t)w;
  }
}

// Set data (command 0xA3): [name length][name][value length][value]...
// Calls f(name, nameLength, value, valueLength) for each pair; false on a malformed stream or when f says so.
template <class F>
bool forEachPair(const uint8_t* data, size_t size, F f) {
  size_t at = 0;
  if (size == 0) return false;
  while (at < size) {
    if (at + 1 > size) return false;
    size_t nlen = data[at];
    if (at + 1 + nlen + 1 > size) return false;
    size_t vlen = data[at + 1 + nlen];
    if (at + 2 + nlen + vlen > size) return false;
    if (!f((const char*)data + at + 1, nlen, (const char*)data + at + 2 + nlen, vlen)) return false;
    at += 2 + nlen + vlen;
  }
  return true;
}

// File text form: one "name=value" per line. Calls f(name, nameLength, value, valueLength).
template <class F>
void forEachLine(const char* text, size_t size, F f) {
  size_t at = 0;
  while (at < size) {
    size_t end = at;
    while (end < size && text[end] != '\n') end++;
    const char* eq = (const char*)memchr(text + at, '=', end - at);
    if (eq) f(text + at, (size_t)(eq - (text + at)), eq + 1, (size_t)(text + end - (eq + 1)));
    at = end + 1;
  }
}

// Appends "name=value\n" at out[at]. Returns the new length, or cap when it does not fit.
inline size_t putLine(char* out, size_t at, size_t cap, const char* name, const char* value) {
  size_t n = strlen(name), v = strlen(value);
  if (at >= cap || at + n + 1 + v + 1 >= cap) return cap;
  memcpy(out + at, name, n); at += n; out[at++] = '=';
  memcpy(out + at, value, v); at += v; out[at++] = '\n';
  out[at] = 0;
  return at;
}

}  // namespace iosignal_settings

#endif
