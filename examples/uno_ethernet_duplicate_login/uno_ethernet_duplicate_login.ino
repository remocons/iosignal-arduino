/*
  Duplicate login test: Arduino Uno R3 + Ethernet Shield W5100

  https://github.com/remocons/iosignal-arduino

  Shows what this board prints when another connection logs in with the same ID.
    1. Register a device on your server and set ID_KEY ("did.key") and the server below.
    2. Upload and open the Serial Monitor at 115200 baud. Wait for "READY cid: ...".
    3. Log in elsewhere with the same ID_KEY: a second board running this sketch, or
         io-client --auth-idKey did.key   (iosignal-cli, pointed at the same server)
    4. keepNew server (IOSignal 7.3.0+ default): this board is replaced and prints
         [IOSignal] AUTH_CLEAR: duplicate login. connection stopped.
         ERROR: IO_AUTH_CLEAR reason: duplicate login.
       then stays stopped. Send 'r' in the Serial Monitor (or press reset) to log in again.
    5. keepOld server: the later login is refused. The server closes a CongSocket without
       a message, so this sketch reports repeated "closed before ready" attempts.

  [kr] 같은 ID로 다른 곳에서 로그인했을 때 이 보드에 출력되는 내용을 확인하는 예제입니다.
    1. 서버에 장치를 등록하고 ID_KEY("did.key")와 서버 주소를 설정합니다.
    2. 업로드 후 시리얼 모니터(115200)에서 "READY cid: ..."를 확인합니다.
    3. 같은 ID_KEY로 다른 곳에서 로그인합니다. 이 예제를 올린 다른 보드 또는
         io-client --auth-idKey did.key   (같은 서버에 접속)
    4. keepNew 서버(IOSignal 7.3.0 이상 기본값): 이 보드가 교체되며 위 오류를 출력하고 멈춥니다.
       시리얼 모니터에서 'r'을 보내거나 리셋하면 다시 로그인합니다.
    5. keepOld 서버: 나중 로그인이 거부됩니다. 서버가 메시지 없이 연결을 닫으므로
       "closed before ready" 반복 횟수로 확인합니다.

  Pins 10-13 are used by the W5100 SPI bus, so this sketch uses no LED.
  Requires the IOSignal Arduino library release with setLog() and lastReason.
 */

#include <SPI.h>
#include <Ethernet.h>
#include <IOSignal.h>

const char *ID_KEY = "did.key";             // registered device ID and key
const char *SERVER_HOST = "io.remocon.kr"; // ID_KEY from your remocon.kr account (device page)
const uint16_t SERVER_PORT = 55488;          // CongSocket port

// If you have multiple boards on one network, give each a different MAC address.
byte mac[]{0, 0, 0, 0, 0, 0x82};
EthernetClient client;
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
    Serial.println(F("Stopped. Send 'r' or reset to log in again."));
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

void serialCommand()
{
  if (!Serial.available()) return;
  if (Serial.read() == 'r' && io.state == IO_STOP)
  {
    Serial.println(F("log in again"));
    connectServer();
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println(F("Duplicate login test"));

  Ethernet.init(10);
  Ethernet.begin(mac); // DHCP
  Serial.print(F("IP: "));
  Serial.println(Ethernet.localIP());

  io.setRxBuffer(80);
  io.onReady(onReady);
  io.onError(onError);
  io.setLog(printLog);
  connectServer();
}

void loop()
{
  io.loop();
  watchConnection();
  serialCommand();
}
