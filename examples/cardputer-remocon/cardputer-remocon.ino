/*
 *  IOSignal device example: Cardputer remocon (a pocket remote for the devices around it)
 *
 *  M5Stack Cardputer (and Cardputer ADV): ESP32-S3, 240x135 display, keyboard.
 *  The board sends #search, the way the web app does, and lists every device that answers with
 *  #notify (the devices on the same router). It reads each device's $name (name:category), $ui
 *  (official type, buttons) and $state, and controls it from the keyboard:
 *    List:    , / (left/right) category tab, ; . (up/down) move, Enter opens, ` searches again.
 *    Device:  ; . choose a control, Enter presses it (switch, button), , / change a value
 *             (slider, options), 1-9 press control 1-9, ` back to the list.
 *             The last control "> command" types any command (Enter sends, Tab cancels).
 *  Controls come from the official type (t=, a table in this sketch), else the b= buttons or the
 *  old button list. A device's $view (or its type's default picture) is drawn on the left.
 *  The board is a card in the web app too: its "scan" button searches again.
 *
 *  Board: M5Cardputer, USB CDC On Boot: Enabled, Partition Scheme: Minimal SPIFFS (the Hangul font
 *  makes the program larger than the default 1.2 MB). Library: M5Cardputer (with M5Unified, M5GFX).
 *
 *  Wi-Fi: upload, then set it over USB on https://arduino.iosignal.net/wifi/ (the board keeps it),
 *  or write it in the sketch.
 *
 *  [kr] #search로 같은 공유기의 장치를 찾아 분류 탭별 목록으로 보여 주고, 키보드로 골라 켜고 끄고
 *       값을 바꿉니다. 공식 타입은 그 타입의 조작(스위치, 슬라이더, 버튼)과 그림을, 예전 장치는
 *       버튼 목록을 보여 주고, 어떤 명령이든 직접 입력해 보낼 수 있습니다.
 *
 *  Taeo Lee <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *  MIT License
 */

#include <M5Cardputer.h>
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <IOSignalUi.h>
#include <IOSignalBinding.h>
#include <string.h>

struct Device;   // the Arduino builder puts function prototypes above the first function
struct Control;

#define WIFI_ID  "WIFI_SSID"
#define WIFI_KEY "WIFI_PASS"

#define SERVER_URL "io.iosignal.net"
#define SERVER_PORT 55488

#define MAX_DEVICES 24
#define MAX_TABS 8
#define MAX_CONTROLS 16
#define LIST_ROWS 6
#define CONTROL_ROWS 6

IOSignalConfig config;
WiFiClient client;
IOSignal io;
// $name, the device name and its app tab ("name:category"): board setting 'name'.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "Cardputer Remocon:HOME");
M5Canvas canvas(&M5Cardputer.Display);

// ---------------------------------------------------------------- colors
static const uint16_t C_BG = 0x0000, C_TEXT = 0xFFFF, C_DIM = 0x8410, C_LINE = 0x31A6,
                      C_ACCENT = 0x05D3, C_SEL = 0x1A4B, C_WARN = 0xFC60, C_OFF = 0x4208;

// ---------------------------------------------------------------- official types
// One control per line: kind|label|key|...
//   S|label|key|cmdOn|cmdOff      switch, on when the key's state is 1/on/true
//   L|label|key|cmd|min|max|step  slider ({v} in cmd is the value)
//   B|label|cmd                   button
//   O|label|key|cmd|label:value,… options (key may be empty)
//   V|label|key|unit              value, read only
// From _iosignal/devices/types (the web app's controls; colors are given as options).
struct TypeSpec { const char* name; const char* view; const char* controls; };
static const TypeSpec TYPES[] = {
  { "light.1", "bulb on=on",
    "S|전원|on|on|off\nB|토글|toggle" },
  { "dimmer.1", "bulb on=on level=level",
    "S|전원|on|on|off\nL|밝기|level|level={v}|0|100|5\nO|빠른 설정|level|level={v}|25%:25,50%:50,100%:100" },
  { "rgb-light.1", "bulb on=on level=level color=color",
    "S|전원|on|on|off\nL|밝기|level|level={v}|0|100|5\n"
    "O|색상|color|color={v}|빨강:#ff0000,주황:#ff6000,노랑:#ffc000,초록:#00ff00,하늘:#00c0ff,파랑:#0000ff,보라:#a000ff,흰색:#ffffff" },
  { "fan.1", "fan on=on speed=speed",
    "S|전원|on|on|off\nL|풍량|speed|speed={v}|1|10|1\nS|회전|swing|swing=1|swing=0" },
  { "relay4.1", "relays r1 r2 r3 r4",
    "S|1번|r1|r1=1|r1=0\nS|2번|r2|r2=1|r2=0\nS|3번|r3|r3=1|r3=0\nS|4번|r4|r4=1|r4=0\nB|모두 켜기|all=1\nB|모두 끄기|all=0" },
  { "switch2.1", "relays s1 s2",
    "S|1번|s1|s1=1|s1=0\nS|2번|s2|s2=1|s2=0" },
  { "switch3.1", "relays s1 s2 s3",
    "S|1번|s1|s1=1|s1=0\nS|2번|s2|s2=1|s2=0\nS|3번|s3|s3=1|s3=0" },
  { "switch4.1", "relays s1 s2 s3 s4",
    "S|1번|s1|s1=1|s1=0\nS|2번|s2|s2=1|s2=0\nS|3번|s3|s3=1|s3=0\nS|4번|s4|s4=1|s4=0" },
  { "switch8.1", "relays s1 s2 s3 s4 s5 s6 s7 s8",
    "S|1번|s1|s1=1|s1=0\nS|2번|s2|s2=1|s2=0\nS|3번|s3|s3=1|s3=0\nS|4번|s4|s4=1|s4=0\n"
    "S|5번|s5|s5=1|s5=0\nS|6번|s6|s6=1|s6=0\nS|7번|s7|s7=1|s7=0\nS|8번|s8|s8=1|s8=0" },
  { "thermo-hygrometer.1", "thermo temp=temp hum=hum",
    "V|온도|temp|°C\nV|습도|hum|%" },
  { "media-remote.1", "",
    "O|재생||{v}|이전:prev,재생/정지:playToggle,다음:next\nO|방향||{v}|위:up,왼쪽:left,확인:ok,오른쪽:right,아래:down\nV|현재 상태|_|" },
  { "iris.1", "",
    "S|전체|on|on|off\n"
    "O|빠른 색상|color|color={v}|빨강:#ff0000,주황:#ff6000,노랑:#ffc000,초록:#00ff00,하늘:#00c0ff,파랑:#0000ff,보라:#a000ff,흰색:#ffffff\n"
    "S|LED 1|s1|s1=1|s1=0\nL|LED 1 밝기|b1|b1={v}|0|100|5\nS|LED 2|s2|s2=1|s2=0\nL|LED 2 밝기|b2|b2={v}|0|100|5\n"
    "S|LED 3|s3|s3=1|s3=0\nL|LED 3 밝기|b3|b3={v}|0|100|5\nS|LED 4|s4|s4=1|s4=0\nL|LED 4 밝기|b4|b4={v}|0|100|5" },
  { "iris.2", "",
    "L|전체 색상|h|h={v}|0|359|15\nL|전체 밝기|b|b={v}|0|100|5\n"
    "L|LED 1 색상|h1|h1={v}|0|359|15\nL|LED 1 밝기|b1|b1={v}|0|100|5\nL|LED 2 색상|h2|h2={v}|0|359|15\nL|LED 2 밝기|b2|b2={v}|0|100|5\n"
    "L|LED 3 색상|h3|h3={v}|0|359|15\nL|LED 3 밝기|b3|b3={v}|0|100|5\nL|LED 4 색상|h4|h4={v}|0|359|15\nL|LED 4 밝기|b4|b4={v}|0|100|5" },
  { "camera.1", "",
    "S|전송|on|on|off\nO|간격|interval|interval={v}|0.1초:100,0.2초:200,0.5초:500,1초:1000,2초:2000,5초:5000" },
  { "camera-session.1", "",
    "S|전송|on|on|off\nO|간격|interval|interval={v}|0.1초:100,0.2초:200,0.5초:500,1초:1000,2초:2000,5초:5000" },
};

const TypeSpec* findType(const char* name) {
  for (const TypeSpec& t : TYPES)
    if (strcmp(t.name, name) == 0) return &t;
  return nullptr;
}

// ---------------------------------------------------------------- devices
struct Device {
  char cid[MAX_CID_LEN + 1];
  char name[72];      // the whole $name, "name:category"
  char ui[136];       // $ui as sent
  char state[200];    // its $state
  char view[160];     // its $view (v=1), JSON
  bool gone;          // $state "close": the device left
};
Device devices[MAX_DEVICES];
uint8_t count = 0;

char tabs[MAX_TABS][32];   // tab 0: all, then the categories in the order found ("" is 미분류)
uint8_t tabCount = 1, tab = 0;
uint8_t visible[MAX_DEVICES], visibleCount = 0;
uint8_t selected = 0, top = 0;      // in visible[]

enum Screen { LIST, DEVICE, TYPING };
Screen screen = LIST;
int8_t openIndex = -1;              // devices[] index of the open device

unsigned long searchedAt = 0;
unsigned long sentAt = 0;           // a command waits for the device's $state
bool dirty = true;

int indexOf(const char* cid) {
  for (uint8_t i = 0; i < count; i++)
    if (strcmp(devices[i].cid, cid) == 0) return i;
  return -1;
}

void categoryText(const Device& d, char* out, size_t n) { iosignal::categoryOf(d.name, out, n); }

void watch(const char* cid, const char* part, bool on) {
  char tag[MAX_CID_LEN + 10];
  snprintf(tag, sizeof(tag), "%s@$%s", cid, part);
  if (on) io.subscribe(tag);
  else io.unsubscribe(tag);
}

// One key of an extended $ui ("t=iris.2;p=1;v=1;b=on,off"), or "".
void uiValue(const char* ui, const char* key, char* out, size_t n) {
  out[0] = 0;
  if (!iosignal::isExtendedUi(ui)) return;
  while (*ui == ' ') ui++;
  iosignal::stateValue(ui, key, out, n);
}

void rebuildTabs() {
  tabCount = 1;
  for (uint8_t i = 0; i < count; i++) {
    char cat[32];
    categoryText(devices[i], cat, sizeof(cat));
    bool known = false;
    for (uint8_t t = 1; t < tabCount; t++)
      if (strcmp(tabs[t], cat) == 0) known = true;
    if (!known && tabCount < MAX_TABS) strcpy(tabs[tabCount++], cat);
  }
  if (tab >= tabCount) tab = 0;
  visibleCount = 0;
  for (uint8_t i = 0; i < count; i++) {
    char cat[32];
    categoryText(devices[i], cat, sizeof(cat));
    if (tab == 0 || strcmp(tabs[tab], cat) == 0) visible[visibleCount++] = i;
  }
  if (selected >= visibleCount) selected = visibleCount ? visibleCount - 1 : 0;
  if (selected < top) top = selected;
  if (selected >= top + LIST_ROWS) top = selected - LIST_ROWS + 1;
}

// ---------------------------------------------------------------- this board as a device
void sendInfo() {
  char name[64];
  iosignal::formatNameSetting(name, sizeof(name), nameSetting.value(), "Cardputer Remocon");
  io.signal("@$name", name);
  io.signal("@$ui", "scan");  // the card's button: search again
}

void publishState() {
  if (io.state != IO_READY) return;
  char text[48];
  snprintf(text, sizeof(text), "devices=%u;open=%s", count, openIndex >= 0 ? devices[openIndex].cid : "-");
  io.signal("@$state", text);
}

// Forget the list and ask again: every device on the router answers with #notify.
void search() {
  if (io.state != IO_READY) return;
  io.unsubscribe();  // all tags, then the ones this sketch always needs
  io.subscribe("#search");
  io.subscribe("#notify");
  count = selected = top = 0;
  tab = 0;
  openIndex = -1;
  screen = LIST;
  rebuildTabs();
  searchedAt = millis();
  io.signal("#search");
  Serial.println("SEARCH");
  publishState();
  dirty = true;
}

// ---------------------------------------------------------------- controls of the open device
enum Kind : uint8_t { K_SWITCH, K_SLIDER, K_BUTTON, K_OPTIONS, K_VALUE, K_TYPE };
struct Control {
  Kind kind;
  char label[40];
  char key[16];
  char cmd[40];       // switch: on command; slider, options: template; button: the command
  char cmdOff[24];
  int min, max, step;
  char options[200];  // label:value,…
  uint8_t option;     // chosen option
  int value;          // slider value while it is being changed
  bool changing;      // slider changed, not sent yet
  unsigned long changedAt;
};
Control controls[MAX_CONTROLS];
uint8_t controlCount = 0, controlSel = 0, controlTop = 0;
char typed[65] = "";

// Field i of a "|" line into out.
void field(const char* line, size_t lineLen, int i, char* out, size_t n) {
  const char* p = line;
  const char* end = line + lineLen;
  while (i-- > 0) {
    const char* bar = (const char*)memchr(p, '|', end - p);
    if (!bar) { out[0] = 0; return; }
    p = bar + 1;
  }
  const char* bar = (const char*)memchr(p, '|', end - p);
  size_t m = (bar ? bar : end) - p;
  if (m >= n) m = n - 1;
  memcpy(out, p, m);
  out[m] = 0;
}

void addButton(const char* cmd, size_t len) {
  while (len && *cmd == ' ') { cmd++; len--; }
  while (len && cmd[len - 1] == ' ') len--;
  if (!len || controlCount >= MAX_CONTROLS - 1) return;
  Control& c = controls[controlCount++];
  memset(&c, 0, sizeof(c));
  c.kind = K_BUTTON;
  size_t m = len < sizeof(c.cmd) - 1 ? len : sizeof(c.cmd) - 1;
  memcpy(c.cmd, cmd, m);
  strcpy(c.label, c.cmd);
}

void addButtons(const char* list) {
  const char* p = list;
  while (*p) {
    const char* comma = strchr(p, ',');
    addButton(p, comma ? (size_t)(comma - p) : strlen(p));
    if (!comma) break;
    p = comma + 1;
  }
}

void addTypeControls(const char* spec) {
  const char* p = spec;
  while (*p && controlCount < MAX_CONTROLS - 1) {
    const char* nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    Control& c = controls[controlCount++];
    memset(&c, 0, sizeof(c));
    char kind[4], num[8];
    field(p, len, 0, kind, sizeof(kind));
    field(p, len, 1, c.label, sizeof(c.label));
    switch (kind[0]) {
      case 'S':
        c.kind = K_SWITCH;
        field(p, len, 2, c.key, sizeof(c.key));
        field(p, len, 3, c.cmd, sizeof(c.cmd));
        field(p, len, 4, c.cmdOff, sizeof(c.cmdOff));
        break;
      case 'L':
        c.kind = K_SLIDER;
        field(p, len, 2, c.key, sizeof(c.key));
        field(p, len, 3, c.cmd, sizeof(c.cmd));
        field(p, len, 4, num, sizeof(num)); c.min = atoi(num);
        field(p, len, 5, num, sizeof(num)); c.max = atoi(num);
        field(p, len, 6, num, sizeof(num)); c.step = atoi(num) > 0 ? atoi(num) : 1;
        break;
      case 'B':
        c.kind = K_BUTTON;
        field(p, len, 2, c.cmd, sizeof(c.cmd));
        break;
      case 'O':
        c.kind = K_OPTIONS;
        field(p, len, 2, c.key, sizeof(c.key));
        field(p, len, 3, c.cmd, sizeof(c.cmd));
        field(p, len, 4, c.options, sizeof(c.options));
        break;
      default:
        c.kind = K_VALUE;
        field(p, len, 2, c.key, sizeof(c.key));
        field(p, len, 3, c.cmd, sizeof(c.cmd));  // the unit
    }
    if (!nl) break;
    p = nl + 1;
  }
}

// Screen choice (device rules): type → b buttons → old button list → none. Own panels (p=1)
// are not read here, so such a device shows its type or its b buttons.
void buildControls(const Device& d) {
  controlCount = controlSel = controlTop = 0;
  char type[32], buttons[136];
  if (iosignal::isExtendedUi(d.ui)) {
    uiValue(d.ui, "t", type, sizeof(type));
    uiValue(d.ui, "b", buttons, sizeof(buttons));
    const TypeSpec* spec = type[0] ? findType(type) : nullptr;
    if (spec) addTypeControls(spec->controls);
    else if (buttons[0]) addButtons(buttons);
  } else {
    addButtons(d.ui);
  }
  Control& c = controls[controlCount++];  // always last: type any command
  memset(&c, 0, sizeof(c));
  c.kind = K_TYPE;
  strcpy(c.label, "> 명령 입력");
}

bool isOn(const char* v) {
  return strcmp(v, "1") == 0 || strcasecmp(v, "on") == 0 || strcasecmp(v, "true") == 0;
}

bool stateOf(const char* key, char* out, size_t n) {
  out[0] = 0;
  if (openIndex < 0 || !key[0]) return false;
  return iosignal::stateValue(devices[openIndex].state, key, out, n);
}

// Option i of "label:value,…" into label and value. Returns how many options there are.
int option(const char* options, int i, char* label, size_t ln, char* value, size_t vn) {
  int n = 0;
  const char* p = options;
  label[0] = value[0] = 0;
  while (*p) {
    const char* comma = strchr(p, ',');
    size_t len = comma ? (size_t)(comma - p) : strlen(p);
    if (n == i) {
      const char* colon = (const char*)memchr(p, ':', len);
      size_t a = colon ? (size_t)(colon - p) : len;
      size_t b = colon ? len - a - 1 : len;
      memcpy(label, p, a < ln - 1 ? a : ln - 1); label[a < ln - 1 ? a : ln - 1] = 0;
      memcpy(value, colon ? colon + 1 : p, b < vn - 1 ? b : vn - 1); value[b < vn - 1 ? b : vn - 1] = 0;
    }
    n++;
    if (!comma) break;
    p = comma + 1;
  }
  return n;
}

void render(const char* tmpl, const char* value, char* out, size_t n) {
  size_t o = 0;
  for (const char* p = tmpl; *p && o < n - 1; p++) {
    if (p[0] == '{' && p[1] == 'v' && p[2] == '}') {
      for (const char* v = value; *v && o < n - 1; v++) out[o++] = *v;
      p += 2;
    } else out[o++] = *p;
  }
  out[o] = 0;
}

void sendCommand(const char* cmd) {
  if (openIndex < 0 || !cmd[0] || io.state != IO_READY) return;
  char tag[MAX_CID_LEN + 2];
  snprintf(tag, sizeof(tag), "%s@", devices[openIndex].cid);
  io.signal(tag, cmd);
  sentAt = millis();
  Serial.print("SEND ");
  Serial.print(tag);
  Serial.print(' ');
  Serial.println(cmd);
  dirty = true;
}

void sendSlider(Control& c) {
  char v[12], cmd[64];
  snprintf(v, sizeof(v), "%d", c.value);
  render(c.cmd, v, cmd, sizeof(cmd));
  c.changing = false;
  sendCommand(cmd);
}

void press(Control& c) {
  char v[24], label[24], cmd[64];
  switch (c.kind) {
    case K_SWITCH:
      stateOf(c.key, v, sizeof(v));
      sendCommand(isOn(v) ? c.cmdOff : c.cmd);
      break;
    case K_BUTTON:
      sendCommand(c.cmd);
      break;
    case K_OPTIONS:
      option(c.options, c.option, label, sizeof(label), v, sizeof(v));
      render(c.cmd, v, cmd, sizeof(cmd));
      sendCommand(cmd);
      break;
    case K_SLIDER:
      if (!c.changing) {  // Enter on a slider: send the value shown
        stateOf(c.key, v, sizeof(v));
        c.value = v[0] ? atoi(v) : c.min;
      }
      sendSlider(c);
      break;
    case K_TYPE:
      typed[0] = 0;
      screen = TYPING;
      break;
    default:
      break;
  }
  dirty = true;
}

// , and / on a control: change a slider or an option.
void adjust(Control& c, int dir) {
  if (c.kind == K_SLIDER) {
    if (!c.changing) {
      char v[12];
      stateOf(c.key, v, sizeof(v));
      c.value = v[0] ? atoi(v) : c.min;
    }
    c.value += dir * c.step;
    if (c.value < c.min) c.value = c.min;
    if (c.value > c.max) c.value = c.max;
    c.changing = true;
    c.changedAt = millis();  // sent when the key rests (loop)
  } else if (c.kind == K_OPTIONS) {
    char label[24], v[24];
    int n = option(c.options, 0, label, sizeof(label), v, sizeof(v));
    c.option = (c.option + n + dir) % n;
  } else return;
  dirty = true;
}

// ---------------------------------------------------------------- moving around
void openDevice(uint8_t index) {
  if (openIndex >= 0) {
    watch(devices[openIndex].cid, "view", false);
  }
  openIndex = index;
  Device& d = devices[index];
  d.view[0] = 0;
  char v[4];
  uiValue(d.ui, "v", v, sizeof(v));
  if (strcmp(v, "1") == 0) watch(d.cid, "view", true);  // kept by the server: arrives at once
  buildControls(d);
  sentAt = 0;
  screen = DEVICE;
  publishState();
  dirty = true;
}

void closeDevice() {
  if (openIndex >= 0) watch(devices[openIndex].cid, "view", false);
  openIndex = -1;
  screen = LIST;
  publishState();
  dirty = true;
}

void moveList(int step) {
  if (!visibleCount) return;
  selected = (selected + visibleCount + step) % visibleCount;
  if (selected < top) top = selected;
  if (selected >= top + LIST_ROWS) top = selected - LIST_ROWS + 1;
  dirty = true;
}

void moveControl(int step) {
  if (!controlCount) return;
  controlSel = (controlSel + controlCount + step) % controlCount;
  if (controlSel < controlTop) controlTop = controlSel;
  if (controlSel >= controlTop + CONTROL_ROWS) controlTop = controlSel - CONTROL_ROWS + 1;
  dirty = true;
}

// ---------------------------------------------------------------- drawing helpers
// Draws text from x, cut at the last whole UTF-8 character that fits maxWidth.
void drawFit(int x, int y, const char* text, int maxWidth) {
  char buf[96];
  size_t n = strlen(text);
  if (n >= sizeof(buf)) n = sizeof(buf) - 1;
  memcpy(buf, text, n);
  buf[n] = 0;
  while (n && canvas.textWidth(buf) > maxWidth) {
    do n--; while (n && (buf[n] & 0xC0) == 0x80);  // back to a character start
    buf[n] = 0;
  }
  canvas.drawString(buf, x, y);
}

uint16_t parseColor(const char* hex, uint16_t fallback) {
  if (!hex || hex[0] != '#' || strlen(hex) != 7) return fallback;
  for (int i = 1; i < 7; i++) if (!isxdigit((unsigned char)hex[i])) return fallback;
  long v = strtol(hex + 1, nullptr, 16);
  return canvas.color565((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

// A string field of a small JSON object ({"kind":"bulb","on":"s1"}), or "".
void jsonString(const char* json, const char* key, char* out, size_t n) {
  out[0] = 0;
  char pat[24];
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  const char* p = strstr(json, pat);
  if (!p) return;
  p = strchr(p + strlen(pat), ':');
  if (!p) return;
  while (*++p == ' ') {}
  if (*p != '"') return;
  const char* end = strchr(++p, '"');
  size_t m = end ? (size_t)(end - p) : 0;
  if (m >= n) m = n - 1;
  memcpy(out, p, m);
  out[m] = 0;
}

// The picture of the open device: its $view (v=1), else its type's default, as
// "kind key=name …" (type table) or JSON ($view).
void drawView(int x, int y, int w, int h) {
  const Device& d = devices[openIndex];
  char kind[12] = "", a[16], b[16], c[16], v[24];
  bool json = d.view[0] == '{';
  const TypeSpec* spec = nullptr;
  if (!json) {
    char type[32];
    uiValue(d.ui, "t", type, sizeof(type));
    spec = type[0] ? findType(type) : nullptr;
    if (!spec || !spec->view[0]) {  // no picture: the state as key=value lines
      canvas.setTextColor(C_DIM);
      int line = 0;
      if (!strchr(d.state, '=')) { drawFit(x, y, d.state[0] ? d.state : "...", w); return; }
      iosignal::forEachPair(d.state, [&](const char* k, size_t kl, const char* val, size_t vl) {
        if (line > 6) return;
        char row[48];
        snprintf(row, sizeof(row), "%.*s %.*s", (int)kl, k, (int)vl, val);
        drawFit(x, y + line++ * 15, row, w);
      });
      return;
    }
    sscanf(spec->view, "%11s", kind);
  } else {
    jsonString(d.view, "kind", kind, sizeof(kind));
  }
  // The state key a picture reads: from $view JSON, the type's "name=key", or the default.
  auto keyOf = [&](const char* name, const char* fallback, char* out, size_t n) {
    if (json) jsonString(d.view, name, out, n);
    else {
      char pat[16];
      snprintf(pat, sizeof(pat), " %s=", name);
      const char* p = strstr(spec->view, pat);
      out[0] = 0;
      if (p) sscanf(p + strlen(pat), "%15s", out);
    }
    if (!out[0]) snprintf(out, n, "%s", fallback);
  };
  const int cx = x + w / 2, cy = y + h / 2 - 8;
  if (strcmp(kind, "bulb") == 0) {
    keyOf("on", "on", a, sizeof(a)); keyOf("level", "level", b, sizeof(b)); keyOf("color", "color", c, sizeof(c));
    stateOf(a, v, sizeof(v)); bool on = isOn(v);
    stateOf(b, v, sizeof(v)); int level = v[0] ? constrain(atoi(v), 0, 100) : 100;
    stateOf(c, v, sizeof(v)); uint16_t col = parseColor(v, 0xFFE0);
    int r = 26;
    if (on) {
      for (int g = 3; g > 0; g--) canvas.fillCircle(cx, cy, r + g * 3 * level / 100, canvas.color565(30 * g, 30 * g, 0));
      canvas.fillCircle(cx, cy, r, col);
    } else canvas.drawCircle(cx, cy, r, C_DIM);
    canvas.fillRect(cx - 9, cy + r - 2, 18, 10, C_DIM);
    canvas.setTextColor(on ? C_TEXT : C_DIM);
    snprintf(v, sizeof(v), on ? "%d%%" : "OFF", level);
    canvas.setTextDatum(top_center); canvas.drawString(v, cx, y + h - 16); canvas.setTextDatum(top_left);
  } else if (strcmp(kind, "fan") == 0) {
    keyOf("on", "on", a, sizeof(a)); keyOf("speed", "speed", b, sizeof(b));
    stateOf(a, v, sizeof(v)); bool on = isOn(v);
    stateOf(b, v, sizeof(v)); int speed = v[0] ? constrain(atoi(v), 0, 10) : 0;
    float turn = on ? millis() / 1000.0f * (1 + speed) : 0;
    for (int i = 0; i < 3; i++) {
      float t = turn + i * 2.094f;
      canvas.fillTriangle(cx, cy, cx + cosf(t) * 28, cy + sinf(t) * 28, cx + cosf(t + 0.6f) * 26, cy + sinf(t + 0.6f) * 26,
                          on ? C_ACCENT : C_OFF);
    }
    canvas.fillCircle(cx, cy, 5, C_TEXT);
    canvas.setTextColor(on ? C_TEXT : C_DIM);
    snprintf(v, sizeof(v), on ? "%d단" : "OFF", speed);
    canvas.setTextDatum(top_center); canvas.drawString(v, cx, y + h - 16); canvas.setTextDatum(top_left);
  } else if (strcmp(kind, "relays") == 0) {
    char keys[16][16];
    int n = 0;
    if (json) {
      const char* p = strstr(d.view, "\"keys\"");
      const char* end = p ? strchr(p, ']') : nullptr;
      while (p && end && n < 16 && (p = strchr(p + 1, '"')) && p < end) {
        const char* q = strchr(++p, '"');
        if (!q || q > end) break;
        snprintf(keys[n++], 16, "%.*s", (int)(q - p), p);
        p = q;
      }
      // "keys" itself was matched first: drop it
      if (n && strcmp(keys[0], "keys") == 0) { for (int i = 1; i < n; i++) strcpy(keys[i - 1], keys[i]); n--; }
    } else {
      const char* p = spec->view + strlen(kind);
      while (n < 16 && sscanf(p, " %15s", keys[n]) == 1) { p = strstr(p, keys[n]) + strlen(keys[n]); n++; }
    }
    if (!n) iosignal::forEachPair(d.state, [&](const char* k, size_t kl, const char*, size_t) {
      if (n < 16) snprintf(keys[n++], 16, "%.*s", (int)kl, k);
    });
    const int cols = n > 4 ? 4 : (n ? n : 1), cell = w / cols;
    for (int i = 0; i < n; i++) {
      int px = x + (i % cols) * cell + cell / 2, py = y + 14 + (i / cols) * 34;
      stateOf(keys[i], v, sizeof(v));
      bool on = isOn(v);
      if (on) canvas.fillCircle(px, py, 9, C_ACCENT); else canvas.drawCircle(px, py, 9, C_DIM);
      canvas.setTextColor(C_DIM);
      canvas.setTextDatum(top_center); canvas.drawString(keys[i], px, py + 11); canvas.setTextDatum(top_left);
    }
  } else if (strcmp(kind, "thermo") == 0) {
    keyOf("temp", "temp", a, sizeof(a)); keyOf("hum", "hum", b, sizeof(b));
    canvas.setTextColor(C_TEXT);
    canvas.setTextSize(2);
    stateOf(a, v, sizeof(v)); strncat(v, v[0] ? "°" : "-", sizeof(v) - strlen(v) - 1);
    drawFit(x + 4, y + 10, v, w - 4);
    stateOf(b, v, sizeof(v)); strncat(v, v[0] ? "%" : "-", sizeof(v) - strlen(v) - 1);
    drawFit(x + 4, y + 46, v, w - 4);
    canvas.setTextSize(1);
  } else if (strcmp(kind, "blind") == 0) {
    keyOf("position", "position", a, sizeof(a));
    stateOf(a, v, sizeof(v));
    int pos = v[0] ? constrain(atoi(v), 0, 100) : 0;
    int bh = (h - 30) * (100 - pos) / 100;
    canvas.drawRect(x + 10, y + 4, w - 20, h - 30, C_DIM);
    for (int yy = 0; yy < bh; yy += 5) canvas.drawFastHLine(x + 11, y + 5 + yy, w - 22, C_TEXT);
    snprintf(v, sizeof(v), "%d%%", pos);
    canvas.setTextColor(C_TEXT);
    canvas.setTextDatum(top_center); canvas.drawString(v, cx, y + h - 16); canvas.setTextDatum(top_left);
  } else if (strcmp(kind, "text") == 0) {
    keyOf("key", "", a, sizeof(a));
    char t[64];
    if (a[0]) stateOf(a, t, sizeof(t)); else snprintf(t, sizeof(t), "%s", d.state);
    canvas.setTextColor(C_TEXT);
    drawFit(x, y + 20, t[0] ? t : "...", w);
  }
  // an unknown kind draws nothing (device rules)
}

// ---------------------------------------------------------------- screens
void drawHeader(const char* title) {
  canvas.fillRect(0, 0, 240, 16, C_SEL);
  canvas.setTextColor(C_TEXT);
  drawFit(3, 2, title, 150);
  canvas.setTextColor(C_DIM);
  char right[32];
  int bat = M5.Power.getBatteryLevel();
  if (bat >= 0) snprintf(right, sizeof(right), "%s %d%%", io.cid, bat);
  else snprintf(right, sizeof(right), "%s", io.cid);
  canvas.setTextDatum(top_right);
  canvas.drawString(right, 237, 2);
  canvas.setTextDatum(top_left);
}

void drawList() {
  char text[48];
  snprintf(text, sizeof(text), "장치 %u", count);
  drawHeader(text);
  // tabs
  int x = 2;
  for (uint8_t t = 0; t < tabCount && x < 236; t++) {
    const char* label = t == 0 ? "전체" : (tabs[t][0] ? tabs[t] : "미분류");
    int w = canvas.textWidth(label) + 8;
    if (w > 80) w = 80;
    if (t == tab) canvas.fillRoundRect(x, 18, w, 15, 4, C_ACCENT);
    canvas.setTextColor(t == tab ? C_BG : C_DIM);
    drawFit(x + 4, 20, label, w - 6);
    x += w + 3;
  }
  canvas.drawFastHLine(0, 35, 240, C_LINE);
  if (!visibleCount) {
    canvas.setTextColor(C_DIM);
    canvas.drawString(millis() - searchedAt < 3000 ? "찾는 중..." : "답한 장치가 없습니다", 8, 50);
    canvas.drawString("` 다시 찾기", 8, 70);
    return;
  }
  for (uint8_t row = 0; row < LIST_ROWS && top + row < visibleCount; row++) {
    const Device& d = devices[visible[top + row]];
    const int y = 37 + row * 16;
    const bool sel = top + row == selected;
    if (sel) canvas.fillRect(0, y, 240, 16, C_SEL);
    char name[48];
    iosignal::nameOf(d.name, name, sizeof(name));
    canvas.setTextColor(d.gone ? C_DIM : C_TEXT);
    drawFit(4, y + 2, name[0] ? name : d.cid, 128);
    canvas.setTextColor(d.gone ? C_WARN : C_DIM);
    drawFit(136, y + 2, d.gone ? "x 끊김" : (d.state[0] ? d.state : ""), 100);
  }
  if (visibleCount > LIST_ROWS) {  // scroll bar
    int h = 96 * LIST_ROWS / visibleCount;
    canvas.fillRect(238, 37 + 96 * top / visibleCount, 2, h, C_DIM);
  }
}

void drawControls(int x, int y, int w) {
  char v[24], label[24];
  for (uint8_t row = 0; row < CONTROL_ROWS && controlTop + row < controlCount; row++) {
    Control& c = controls[controlTop + row];
    const int yy = y + row * 17;
    const bool sel = controlTop + row == controlSel;
    if (sel) canvas.fillRoundRect(x, yy, w, 16, 3, C_SEL);
    canvas.setTextColor(c.kind == K_VALUE ? C_DIM : C_TEXT);
    char num[4] = "";
    if (controlTop + row < 9 && c.kind != K_VALUE) snprintf(num, sizeof(num), "%d", controlTop + row + 1);
    canvas.setTextColor(C_DIM);
    canvas.drawString(num, x + 2, yy + 2);
    canvas.setTextColor(C_TEXT);
    drawFit(x + 12, yy + 2, c.label, w / 2 - 4);
    const int vx = x + w / 2 + 10, vw = w / 2 - 14;
    switch (c.kind) {
      case K_SWITCH: {
        stateOf(c.key, v, sizeof(v));
        bool on = isOn(v);
        canvas.fillRoundRect(vx, yy + 3, 22, 11, 5, on ? C_ACCENT : C_OFF);
        canvas.fillCircle(on ? vx + 16 : vx + 5, yy + 8, 4, C_TEXT);
        break;
      }
      case K_SLIDER: {
        int val;
        if (c.changing) val = c.value;
        else { stateOf(c.key, v, sizeof(v)); val = v[0] ? atoi(v) : c.min; }
        int range = c.max > c.min ? c.max - c.min : 1;
        int bw = vw - 30;
        canvas.drawRoundRect(vx, yy + 5, bw, 6, 3, C_DIM);
        canvas.fillRoundRect(vx, yy + 5, bw * constrain(val - c.min, 0, range) / range, 6, 3, c.changing ? C_WARN : C_ACCENT);
        snprintf(v, sizeof(v), "%d", val);
        canvas.drawString(v, vx + bw + 4, yy + 2);
        break;
      }
      case K_OPTIONS:
        option(c.options, c.option, label, sizeof(label), v, sizeof(v));
        snprintf(v, sizeof(v), "< %s >", label);
        drawFit(vx, yy + 2, v, vw);
        break;
      case K_VALUE:
        stateOf(c.key, v, sizeof(v));
        strncat(v, c.cmd, sizeof(v) - strlen(v) - 1);
        drawFit(vx, yy + 2, v[0] ? v : "-", vw);
        break;
      case K_BUTTON:
        if (strcmp(c.label, c.cmd) != 0) { canvas.setTextColor(C_DIM); drawFit(vx, yy + 2, c.cmd, vw); }
        break;
      default:
        break;
    }
  }
  if (controlCount > CONTROL_ROWS) {
    int h = 102 * CONTROL_ROWS / controlCount;
    canvas.fillRect(238, y + 102 * controlTop / controlCount, 2, h, C_DIM);
  }
}

void drawDevice() {
  const Device& d = devices[openIndex];
  char name[48], cat[32], line[64];
  iosignal::nameOf(d.name, name, sizeof(name));
  iosignal::categoryOf(d.name, cat, sizeof(cat));
  drawHeader(name[0] ? name : d.cid);
  canvas.setTextColor(C_DIM);
  snprintf(line, sizeof(line), "%s · %s", d.cid, cat[0] ? cat : "미분류");
  drawFit(3, 18, line, 160);
  if (d.gone) { canvas.setTextColor(C_WARN); canvas.drawString("끊김", 200, 18); }
  else if (sentAt) {
    canvas.setTextColor(millis() - sentAt < 3000 ? C_WARN : C_DIM);
    canvas.drawString(millis() - sentAt < 3000 ? "보냄..." : "응답 없음", 180, 18);
  }
  canvas.drawFastHLine(0, 33, 240, C_LINE);
  drawView(2, 36, 78, 84);
  canvas.drawFastVLine(82, 36, 84, C_LINE);
  drawControls(85, 35, 153);
  canvas.drawFastHLine(0, 121, 240, C_LINE);
  canvas.setTextColor(C_DIM);
  drawFit(3, 123, d.state[0] ? d.state : "...", 234);
}

void drawTyping() {
  const Device& d = devices[openIndex];
  char title[64];
  snprintf(title, sizeof(title), "%s@ 명령", d.cid);
  drawHeader(title);
  canvas.setTextColor(C_DIM);
  canvas.drawString("Enter 보내기 · Tab 취소", 4, 22);
  canvas.drawRoundRect(2, 44, 236, 22, 4, C_ACCENT);
  canvas.setTextColor(C_TEXT);
  char shown[72];
  snprintf(shown, sizeof(shown), "%s_", typed);
  // keep the end of a long line visible
  const char* p = shown;
  while (canvas.textWidth(p) > 228 && *p) p++;
  canvas.drawString(p, 6, 48);
  canvas.setTextColor(C_DIM);
  drawFit(4, 76, "예: on, off, toggle, level=70", 232);
  canvas.drawFastHLine(0, 121, 240, C_LINE);
  drawFit(3, 123, d.state[0] ? d.state : "...", 234);
}

void show() {
  canvas.fillSprite(C_BG);
  canvas.setTextDatum(top_left);
  if (!config.connected()) {
    drawHeader("Cardputer Remocon");
    canvas.setTextColor(C_TEXT);
    canvas.drawString("Wi-Fi 연결 중...", 8, 40);
    canvas.setTextColor(C_DIM);
    canvas.drawString("설정: arduino.iosignal.net/wifi", 8, 64);
  } else if (io.state != IO_READY) {
    drawHeader("Cardputer Remocon");
    canvas.setTextColor(C_TEXT);
    canvas.drawString("서버 접속 중...", 8, 40);
    canvas.setTextColor(C_DIM);
    canvas.drawString(config.host(), 8, 64);
  } else if (screen == LIST || openIndex < 0) drawList();
  else if (screen == DEVICE) drawDevice();
  else drawTyping();
  canvas.pushSprite(0, 0);
  dirty = false;
}

// ---------------------------------------------------------------- keyboard
// Cardputer arrows are the keys ; . , / (printed under them), with or without Fn. While typing a
// command they are typed as characters (except ';', which a command may not hold).
void onKeys(const Keyboard_Class::KeysState& k) {
  if (io.state != IO_READY) return;
  if (screen == TYPING) {
    if (k.tab || (k.fn && !k.word.empty() && k.word[0] == '`')) { screen = DEVICE; dirty = true; return; }
    if (k.enter) {
      if (typed[0]) sendCommand(typed);
      screen = DEVICE;
      dirty = true;
      return;
    }
    if (k.del) { size_t n = strlen(typed); if (n) typed[n - 1] = 0; dirty = true; return; }
    for (char ch : k.word) {
      size_t n = strlen(typed);
      if (ch >= 32 && ch < 127 && ch != ';' && n < sizeof(typed) - 1) { typed[n] = ch; typed[n + 1] = 0; }
    }
    dirty = true;
    return;
  }
  char ch = k.word.empty() ? 0 : k.word[0];
  if (screen == LIST) {
    if (ch == ';') moveList(-1);
    else if (ch == '.') moveList(1);
    else if (ch == ',' || ch == '/') {
      tab = (tab + tabCount + (ch == ',' ? -1 : 1)) % tabCount;
      selected = top = 0;
      rebuildTabs();
      dirty = true;
    }
    else if (k.enter && visibleCount) openDevice(visible[selected]);
    else if (ch == '`' || k.del) search();
    return;
  }
  // DEVICE
  if (ch == '`' || k.del) { closeDevice(); return; }
  if (ch == ';') moveControl(-1);
  else if (ch == '.') moveControl(1);
  else if (ch == ',') adjust(controls[controlSel], -1);
  else if (ch == '/') adjust(controls[controlSel], 1);
  else if (k.enter || k.space) press(controls[controlSel]);
  else if (ch >= '1' && ch <= '9' && ch - '1' < controlCount) {
    controlSel = ch - '1';
    if (controlSel < controlTop) controlTop = controlSel;
    if (controlSel >= controlTop + CONTROL_ROWS) controlTop = controlSel - CONTROL_ROWS + 1;
    if (controls[controlSel].kind != K_SLIDER && controls[controlSel].kind != K_OPTIONS) press(controls[controlSel]);
    dirty = true;
  }
}

// ---------------------------------------------------------------- IOSignal
void onReady() {
  Serial.print("onReady cid: ");
  Serial.println(io.cid);
  sendInfo();
  search();
  io.signal("#notify", io.cid);
  dirty = true;
}

void onMessage(char* tag, uint8_t type, uint8_t* payload, size_t size) {
  // A copy with an end: MJSON values (two or more) are a JSON array that is not always terminated.
  static char text[1024];
  size_t n = payload && (type == IOSignal::TEXT || type == IOSignal::MJSON) ? (size < sizeof(text) - 1 ? size : sizeof(text) - 1) : 0;
  memcpy(text, payload, n);
  text[n] = 0;

  if (strcmp(tag, "#search") == 0) { io.signal("#notify", io.cid); return; }
  if (strcmp(tag, "#notify") == 0) {
    if (!text[0] || text[0] == '#' || strcmp(text, io.cid) == 0 || strlen(text) > MAX_CID_LEN) return;
    int i = indexOf(text);
    if (i >= 0) { devices[i].gone = false; dirty = true; return; }
    if (count >= MAX_DEVICES) return;
    Device& d = devices[count++];
    memset(&d, 0, sizeof(d));
    strcpy(d.cid, text);
    watch(d.cid, "name", true);  // kept by the server: they arrive at once
    watch(d.cid, "ui", true);
    watch(d.cid, "state", true);
    Serial.print("FOUND ");
    Serial.println(d.cid);
    rebuildTabs();
    publishState();
    dirty = true;
    return;
  }

  const char* at = strstr(tag, "@$");
  if (at) {
    char cid[MAX_CID_LEN + 1];
    const size_t len = (size_t)(at - tag) < sizeof(cid) ? (size_t)(at - tag) : sizeof(cid) - 1;
    memcpy(cid, tag, len);
    cid[len] = 0;
    int i = indexOf(cid);
    if (i < 0) return;
    Device& d = devices[i];
    if (strcmp(at, "@$name") == 0) {
      strlcpy(d.name, text, sizeof(d.name));
      rebuildTabs();
    } else if (strcmp(at, "@$ui") == 0) {
      strlcpy(d.ui, text, sizeof(d.ui));
      if (i == openIndex && screen != TYPING) buildControls(d);
    } else if (strcmp(at, "@$view") == 0) {
      strlcpy(d.view, text, sizeof(d.view));
    } else if (strcmp(at, "@$state") == 0) {
      if (strcmp(text, "close") == 0) { d.gone = true; dirty = true; return; }
      d.gone = false;
      // Two or more values come as ["first",{...}]: keep the first one.
      const char* s = text;
      if (type == IOSignal::MJSON && s[0] == '[' && s[1] == '"') {
        s += 2;
        const char* end = strchr(s, '"');
        size_t m = end ? (size_t)(end - s) : strlen(s);
        if (m >= sizeof(d.state)) m = sizeof(d.state) - 1;
        memcpy(d.state, s, m);
        d.state[m] = 0;
      } else strlcpy(d.state, s, sizeof(d.state));
      if (i == openIndex) sentAt = 0;  // the device answered
    }
    dirty = true;
    return;
  }

  if (strcmp(tag, "@") == 0) {  // the card's "scan" button; every command gets the state back
    if (strcmp(text, "scan") == 0) search();
    publishState();
    dirty = true;
  }
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  // Landscape, keyboard below: rotation 1 on the Cardputer. Any rotation that is not wider than
  // tall is skipped, in case a board starts the panel the other way.
  for (uint8_t r : { 1, 3, 0, 2 }) {
    M5Cardputer.Display.setRotation(r);
    if (M5Cardputer.Display.width() > M5Cardputer.Display.height()) break;
  }
  M5Cardputer.Display.setBrightness(80);
  canvas.setColorDepth(16);
  canvas.createSprite(240, 135);
  canvas.setFont(&fonts::efontKR_12);

  Serial.begin(115200);
  Serial.println();
  Serial.println("Cardputer remocon");
  Serial.printf("board %d, display %dx%d, rotation %d\n", (int)M5.getBoard(), (int)M5Cardputer.Display.width(),
                (int)M5Cardputer.Display.height(), (int)M5Cardputer.Display.getRotation());
  show();

  config.begin(Serial, "Cardputer Remocon", WIFI_ID, WIFI_KEY);
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer(SERVER_URL, SERVER_PORT);
  config.printServer();
  io.setRxBuffer(1024);  // panels, views and long states (bayo.tv sends the video details)
  io.setLog([](const char* line) { Serial.println(line); });
  io.onReady(&onReady);
  io.onMessage(&onMessage);
  if (config.key()) io.auth(config.key());  // a device key set over USB, else anonymous
  io.begin(&client, config.host(), config.port());
}

bool wasConnected = false;
uint8_t lastIoState = 255;
bool searching = false;
unsigned long drawnAt = 0;

void loop() {
  M5Cardputer.update();
  config.loop();  // USB setup requests, Wi-Fi reconnect
  const bool connected = config.connected();
  if (connected) io.loop();
  if (connected != wasConnected || io.state != lastIoState) {
    wasConnected = connected;
    lastIoState = io.state;
    dirty = true;
  }
  const bool nowSearching = millis() - searchedAt < 3000;  // "찾는 중" turns into "답한 장치가 없습니다"
  if (searching != nowSearching) { searching = nowSearching; dirty = true; }

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    onKeys(M5Cardputer.Keyboard.keysState());
  }
  if (M5Cardputer.BtnA.wasPressed() && io.state == IO_READY) search();  // G0 button

  // A slider is sent when its key rests for 300 ms.
  if (screen == DEVICE)
    for (uint8_t i = 0; i < controlCount; i++)
      if (controls[i].changing && millis() - controls[i].changedAt > 300) sendSlider(controls[i]);

  // The device page redraws now and then: the fan turns, "보냄..." turns into "응답 없음".
  const unsigned long period = screen == DEVICE ? 100 : 1000;
  if (dirty || millis() - drawnAt > period) {
    drawnAt = millis();
    show();
  }
}
