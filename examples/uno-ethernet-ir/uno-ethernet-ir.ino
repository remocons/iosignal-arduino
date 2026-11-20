/*
 *  IOSignal Remocon Example. 
 *  Arduino Uno + Ethernet shield W5100 + IR Receiver
 *  https://github.com/remocons/iosignal-arduino
 *
 
  An example of an Arduino detecting a remote control signal and forwarding it to another device, or an Arduino receiving a signal from a web app.
    - Connect the IR Receiver module to IR_RECEIVE_PIN.
    - iosignal server information is used without modification. If you use another server, change the server address and port number.
    - compile and upload to Arduino.
    - Access https://remocon.kr and https://bayo.tv with a modern web browser.
    - control youtube player on bayo.tv (right / left move by chapter in a video with chapters)
    - It works with NEC compatible remotes, for other remotes you can modify the library and signal code.

  [kr] 리모컨 신호를 아두이노에서 감지하고 다른 장치에 신호를 전달하거나 웹앱의 신호를 아두이노가 수신하는 예제입니다. 
    - IR수신모듈을 IR_RECEIVE_PIN에 연결하세요.
    - iosignal 서버 정보는 수정 없이 그대로 사용합니다. 다른 서버를 사용할 경우 서버 주소와 포트번호를 변경해줍니다.
    - 아두이노에 컴파일하고 업로딩합니다.
    - 모던 웹브라우저로  https://remocon.kr 와 https://bayo.tv 에 접속하세요.
    - bayo.tv 의 동영상을 제어하세요! (챕터가 있는 영상은 right / left 로 챕터 이동)
    - NEC 호환 리모컨으로 작동되며, 다른 리모컨의 경우 라이브러리와 신호 코드를 수정하시면됩니다.

 *
 *  I used a library for a low-capacity NEC-compatible remote.
 *  If you are using a different remote, change the library
 *
 */



#include <Arduino.h>
#define NO_LED_FEEDBACK_CODE
#define IR_RECEIVE_PIN 2
#include "TinyIRReceiver.hpp" 
#include <SPI.h>
#include <Ethernet.h>
#include <string.h>
#include <IOSignal.h>

// If you have multiple devices, you'll need to change the MAC address.
byte mac[]{0xDE, 0xAD, 0x1E, 0xEF, 0xFE, 0x50};
EthernetClient client;
IOSignal io;
const char *deviceName = "Uno-IR:HOME";  // $name "name:category" (device rules v2)
const char *deviceUi = "t=light.1";      // $ui: the official type

void publishState() { io.signal("@$state", digitalRead(5) ? "on=1" : "on=0"); }

void setup()
{
  pinMode(5, OUTPUT); // LED
  pinMode(6, OUTPUT); // Relay
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);

  Serial.begin(115200);
  initPCIInterruptForTinyReceiver();

  Ethernet.init(10);
  Ethernet.begin(mac); // DHCP
  Serial.println(F("Init.."));
  Serial.print(F("IP:"));
  Serial.println(Ethernet.localIP());

  io.setRxBuffer(80);
  io.begin(&client, "io.remocon.kr", 55488 );
  io.onReady(&onReady);
  io.onMessage(&onMessage);
  // io.auth("ID_KEY"); // Optional: use your own device key ("id.key"). The sketch still fits an Uno with it.
}

// LED and relay together, then the new state to the app. One function: an Uno with a device key
// (io.auth) has almost no flash left.
void setDevice(bool on)
{
  digitalWrite(5, on);
  digitalWrite(6, on);
  publishState();
}

void loop()
{
    io.loop();

    if (TinyIRReceiverData.justWritten)
    {
      TinyIRReceiverData.justWritten = false;

        // Serial.println( TinyIRReceiverData.Command );
      if ((TinyIRReceiverData.Flags & IRDATA_FLAGS_IS_REPEAT) != 0)
      {
        // Serial.println(F("Repeat"));
      }
      else
      {
        // Serial.print(F("A="));
        // Serial.print(TinyIRReceiverData.Address, HEX);
        Serial.print(F(" C="));
        Serial.println(TinyIRReceiverData.Command, HEX);

        // Change to the code value of your NEC compatible remote.
        // right / left: the next or previous chapter when the bayo.tv video has one, else the next or
        // previous video (next / prev always move videos).
        const char *tag = "#screen";
        const char *command = "playToggle";
        switch (TinyIRReceiverData.Command)
        {
        case 0x06 : command = "right"; break;
        case 0x07 : command = "left"; break;
        case 0x40 : command = "up"; break;
        case 0x41 : command = "down"; break;
        case 0x1E : tag = "cid1@"; command = "toggle"; break;
        case 0x1A : tag = "cid2@"; command = "toggle"; break;
        case 0x44 : tag = NULL; setDevice(!digitalRead(5)); break; // this board's LED and relay
        }
        if (tag) io.signal(tag, command);
      }
    }

}

void onReady()
{
  Serial.print(F("onReady cid: "));
  Serial.println(io.cid);
  io.subscribe("#search");
  io.signal("@$name", deviceName);
  io.signal("@$ui", deviceUi);
  publishState();
  io.signal("#notify", io.cid);
}

void onMessage(char *tag, uint8_t payloadType, uint8_t *payload, size_t payloadSize)
{
  if (strcmp(tag, "#search") == 0)
  {
    io.signal("#notify", io.cid);
  }

  // Ignore non-text or unterminated payloads before using string APIs.
  if (payloadType != IOSignal::PAYLOAD_TYPE::TEXT || !payload ||
      !payloadSize || !memchr(payload, 0, payloadSize)) return;

  Serial.print(tag);
  Serial.print(' ');
  Serial.println((char *)payload);

  if (strcmp(tag, "@") == 0)
  {
    if (strcmp((char *)payload, "on") == 0) setDevice(true);
    else if (strcmp((char *)payload, "off") == 0) setDevice(false);
    else if (strcmp((char *)payload, "toggle") == 0) setDevice(!digitalRead(5));
    else publishState(); // Unknown commands still get a reply.
  }
}
