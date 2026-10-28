/*
  Duplicate login test: ESP32-C3 Super Mini (also ESP32 / ESP8266 with LED_PIN changed)

  https://github.com/remocons/iosignal-arduino

  Shows what this board prints when another connection logs in with the same ID.
    1. Set WIFI_SSID / WIFI_PASSWORD, a registered ID_KEY ("did.key") and the server below.
    2. Arduino IDE: board "ESP32C3 Dev Module", "USB CDC On Boot: Enabled" so Serial
       prints over the USB port. Upload and open the Serial Monitor at 115200 baud.
    3. Wait for "READY cid: ...". The on-board LED (GPIO 8, active low) turns on.
    4. Log in elsewhere with the same ID_KEY: a second board running this sketch or the
       Uno example, or
         io-client --auth-idKey did.key   (iosignal-cli, pointed at the same server)
    5. keepNew server (IOSignal 7.3.0+ default): this board is replaced and prints
         [IOSignal] AUTH_CLEAR: duplicate login. connection stopped.
         ERROR: IO_AUTH_CLEAR reason: duplicate login.
       then stays stopped and the LED blinks. Press the BOOT button (GPIO 9), send 'r' in
       the Serial Monitor, or reset to log in again.
    6. keepOld server: the later login is refused. The server closes a CongSocket without
       a message, so this sketch reports repeated "closed before ready" attempts.

  [kr] 같은 ID로 다른 곳에서 로그인했을 때 이 보드에 출력되는 내용을 확인하는 예제입니다.
    1. WIFI_SSID / WIFI_PASSWORD, 등록한 ID_KEY("did.key"), 서버 주소를 설정합니다.
    2. Arduino IDE에서 보드 "ESP32C3 Dev Module", "USB CDC On Boot: Enabled"를 선택해야
       USB로 시리얼 출력이 보입니다. 업로드 후 시리얼 모니터(115200)를 엽니다.
    3. "READY cid: ..."가 나오면 보드 LED(GPIO 8, active low)가 켜집니다.
    4. 같은 ID_KEY로 다른 곳에서 로그인합니다. 이 예제나 Uno 예제를 올린 다른 보드 또는
         io-client --auth-idKey did.key   (같은 서버에 접속)
    5. keepNew 서버(IOSignal 7.3.0 이상 기본값): 이 보드가 교체되며 위 오류를 출력하고
       멈춘 채 LED가 깜빡입니다. BOOT 버튼(GPIO 9)을 누르거나 시리얼 모니터에서 'r'을 보내거나
       리셋하면 다시 로그인합니다.
    6. keepOld 서버: 나중 로그인이 거부됩니다. 서버가 메시지 없이 연결을 닫으므로
       "closed before ready" 반복 횟수로 확인합니다.

  Requires the IOSignal Arduino library release with setLog() and lastReason.
 */

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <IOSignal.h>

#define LED_PIN 8       // ESP32-C3 Super Mini on-board LED
#define LED_ON  LOW     // active low
#define BOOT_PIN 9      // ESP32-C3 Super Mini BOOT button (active low)

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char *ID_KEY = "did.key";             // registered device ID and key
const char *SERVER_HOST = "io.remocon.kr"; // ID_KEY from your remocon.kr account (device page)
const uint16_t SERVER_PORT = 55488;          // CongSocket port

WiFiClient client;
IOSignal io;

bool readySinceOpen = false;
uint8_t lastState = IO_CLOSED;
uint16_t failedAttempts = 0;

// Library log line, e.g. "[IOSignal] AUTH_CLEAR: duplicate login. connection stopped."
void printLog(const char *message)
{
  Serial.println(message);
}

void onError(uint8_t code)
{
  Serial.print(F("ERROR: "));
  if (code == IO_AUTH_CLEAR)
  {
    Serial.print(F("IO_AUTH_CLEAR reason: "));
    Serial.println(io.lastReason[0] ? io.lastReason : "-");
    Serial.println(F("Stopped. Press BOOT, send 'r' or reset to log in again."));
  }
  else if (code == IO_AUTH_FAIL)
  {
    Serial.println(F("IO_AUTH_FAIL (wrong ID_KEY, or the same ID is online on a keepOld server)"));
  }
  else
  {
    Serial.print(F("code "));
    Serial.println(code);
  }
}

void onReady()
{
  readySinceOpen = true;
  failedAttempts = 0;
  Serial.print(F("READY cid: "));
  Serial.println(io.cid);
}

// io.loop() reconnects inside one call, so a new attempt shows up as a change to IO_OPEN.
void watchConnection()
{
  if (io.state == lastState) return;
  if (io.state == IO_OPEN && lastState != IO_CLOSED)
  {
    if (readySinceOpen)
    {
      Serial.println(F("disconnected, reconnecting"));
    }
    else
    {
      failedAttempts++;
      Serial.print(F("closed before ready x"));
      Serial.println(failedAttempts);
      if (failedAttempts == 3)
        Serial.println(F("Login keeps failing: wrong ID_KEY, or the same ID is online on a keepOld server."));
    }
    readySinceOpen = false;
  }
  lastState = io.state;
}

void connectServer()
{
  readySinceOpen = false;
  failedAttempts = 0;
  lastState = IO_CLOSED;
  io.auth(ID_KEY);
  io.begin(&client, SERVER_HOST, SERVER_PORT);
}

// 'r' in the Serial Monitor or the BOOT button logs in again after the server stopped us.
void restartCommand()
{
  static unsigned long pressedAt = 0;
  bool request = Serial.available() && Serial.read() == 'r';
  if (digitalRead(BOOT_PIN) == LOW)
  {
    if (pressedAt == 0) pressedAt = millis();
  }
  else
  {
    if (pressedAt && millis() - pressedAt > 30) request = true; // debounced press
    pressedAt = 0;
  }
  if (request && io.state == IO_STOP)
  {
    Serial.println(F("log in again"));
    connectServer();
  }
}

// LED: on while ready, fast blink while stopped by the server, off otherwise.
void showState()
{
  if (io.state == IO_READY)
    digitalWrite(LED_PIN, LED_ON);
  else if (io.state == IO_STOP)
    digitalWrite(LED_PIN, (millis() / 200) % 2 ? LED_ON : !LED_ON);
  else
    digitalWrite(LED_PIN, !LED_ON);
}

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, !LED_ON);
  pinMode(BOOT_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  delay(1000); // USB CDC needs a moment after reset
  Serial.println(F("Duplicate login test"));

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(250);
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());

  io.setRxBuffer(200);
  io.onReady(onReady);
  io.onError(onError);
  io.setLog(printLog);
  connectServer();
}

void loop()
{
  static unsigned long wifiRetry = 0;
  if (WiFi.status() == WL_CONNECTED)
    io.loop();
  else if (millis() - wifiRetry >= 10000)
  {
    wifiRetry = millis();
    WiFi.reconnect();
  }
  watchConnection();
  restartCommand();
  showState();
}
