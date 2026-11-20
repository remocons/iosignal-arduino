// The device rules v2 (_iosignal/agent-work/device-protocol.ko.md) for a board's own $name and $ui, and for
// reading another device's. No Arduino dependencies (tested on the host).
//
//   $name  "name" or "name:category", split at the first ':' (no category: the apps show 미분류).
//          64 bytes at most is recommended, so a board with the default 80-byte buffer can read it.
//   $ui    a button list "on,off,toggle", or the extended form when its first key is t, p or v:
//          "t=iris.2;p=1;v=1;b=on,off" (t official type, p=1 screen on $panel, v=1 picture on $view, b buttons)
//
//   char name[64];
//   iosignal::formatName(name, sizeof(name), "Iris Basic", "HOME");                 // "Iris Basic:HOME"
//   iosignal::formatNameSetting(name, sizeof(name), nameSetting.value(), "Iris Basic"); // the board setting "name"
//   io.signal("@$name", name);
//   io.signal("@$ui", "t=switch4.1");
#ifndef IOSignalUi_h
#define IOSignalUi_h

#include <stddef.h>
#include <string.h>

namespace iosignal {

static const size_t NAME_BYTES = 64;  // the recommended largest $name

// Whether a $ui value is the extended form: it starts (after spaces) with t=, p= or v=.
inline bool isExtendedUi(const char* ui) {
  if (!ui) return false;
  while (*ui == ' ') ui++;
  return (ui[0] == 't' || ui[0] == 'p' || ui[0] == 'v') && ui[1] == '=';
}

// Copies text into out (size n), each of the characters in bad replaced by a space, line breaks too.
inline void copyClean(char* out, size_t n, const char* text, const char* bad) {
  if (!out || !n) return;
  size_t i = 0;
  for (const char* c = text ? text : ""; *c && i + 1 < n; c++, i++) {
    out[i] = (*c == '\r' || *c == '\n' || (bad && strchr(bad, *c))) ? ' ' : *c;
  }
  out[i] = 0;
}

// The name of a person's choosing made safe for $name: ':' (it would start the category), ';' and line
// breaks become spaces.
inline void cleanName(char* name) {
  for (char* c = name; c && *c; c++) if (*c == ':' || *c == ';' || *c == '\r' || *c == '\n') *c = ' ';
}

// "name:category" into out (size n), or just the name when the category is empty. The name loses its ':',
// the category keeps it; both lose ';' and line breaks. A value that does not fit is cut.
inline void formatName(char* out, size_t n, const char* name, const char* category) {
  if (!out || !n) return;
  copyClean(out, n, name, ":;");
  size_t used = strlen(out);
  if (!category || !*category || used + 2 > n) return;
  out[used++] = ':';
  copyClean(out + used, n - used, category, ";");
}

// $name from a board setting that holds the device's "name" or "name:category" (the $name form itself).
// Empty value: defaultName with defaultCategory. Empty name part (":Kitchen"): defaultName. No ':': no category,
// so the apps show it under 미분류. Spaces around the name are dropped; the result is cleaned like formatName.
inline void formatNameSetting(char* out, size_t n, const char* value, const char* defaultName,
                              const char* defaultCategory = nullptr) {
  if (!out || !n) return;
  while (value && *value == ' ') value++;
  if (!value || !*value) { formatName(out, n, defaultName, defaultCategory); return; }
  const char* colon = strchr(value, ':');
  size_t len = colon ? (size_t)(colon - value) : strlen(value);
  while (len && value[len - 1] == ' ') len--;
  char name[NAME_BYTES + 1];
  if (len > NAME_BYTES) len = NAME_BYTES;
  memcpy(name, value, len);
  name[len] = 0;
  formatName(out, n, len ? name : defaultName, colon ? colon + 1 : "");
}

// len bytes of text into out (size n), line breaks as spaces and the spaces around it dropped, as the apps read
// a $name part (device rules: split at the first ':', both parts trimmed).
inline void copyTrimmed(char* out, size_t n, const char* text, size_t len) {
  if (!out || !n) return;
  while (len && (*text == ' ' || *text == '\r' || *text == '\n')) { text++; len--; }
  while (len && (text[len - 1] == ' ' || text[len - 1] == '\r' || text[len - 1] == '\n')) len--;
  if (len >= n) len = n - 1;
  for (size_t i = 0; i < len; i++) out[i] = (text[i] == '\r' || text[i] == '\n') ? ' ' : text[i];
  out[len] = 0;
}

// The name part of a $name (before the first ':') into out (size n), trimmed.
inline void nameOf(const char* text, char* out, size_t n) {
  if (!out || !n) return;
  const char* colon = text ? strchr(text, ':') : nullptr;
  copyTrimmed(out, n, text ? text : "", colon ? (size_t)(colon - text) : (text ? strlen(text) : 0));
}

// The category of a $name (after the first ':') into out (size n), trimmed; "" without one.
inline void categoryOf(const char* text, char* out, size_t n) {
  if (!out || !n) return;
  const char* colon = text ? strchr(text, ':') : nullptr;
  out[0] = 0;
  if (!colon) return;
  copyTrimmed(out, n, colon + 1, strlen(colon + 1));
}

// Whether a $name names the given device: its name part (before ':', spaces around it dropped) is exactly name.
inline bool nameIs(const char* text, const char* name) {
  if (!text || !name) return false;
  while (*text == ' ') text++;
  size_t len = strlen(name);
  if (strncmp(text, name, len) != 0) return false;
  const char* rest = text + len;
  while (*rest == ' ') rest++;
  return *rest == 0 || *rest == ':';
}

}  // namespace iosignal

// Up to 9.23.0 these helpers were in namespace remocon; the alias keeps those sketches compiling.
namespace remocon = iosignal;

#endif
