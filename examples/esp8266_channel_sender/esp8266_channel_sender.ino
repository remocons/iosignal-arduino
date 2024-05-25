/*
  ESP8266 General or D1 mini + Button

  https://github.com/remocons/iosignal-arduino
  
  An example of communicating between Arduinos and controlling them with a webapp.
    1. set up AP and KEY when using wifi
    2. iosignal server information is used without modification.
       If you use another server, change the server address and port number.
    3. compile and upload to Arduino.
    4. access http://test.iosignal.net in a web browser

  [kr] 아두이노들간의 통신과 웹앱으로 제어하는 예제입니다.
    1. wifi 사용시 AP와 KEY 를 설정하세요
    2. iosignal 서버 정보는 수정 없이 그대로 사용합니다. 
       다른 서버를 사용할 경우 서버 주소와 포트번호를 변경해줍니다.
    3. 아두이노에 컴파일하고 업로딩합니다.
    4. 웹브라우저에서  http://test.iosignal.net 에 접속하세요

 */

#include <ESP8266WiFi.h>
#include <IOSignal.h>
#include <Bounce2.h>

#define BUTTON_PIN   14 // D5 (D1 mini )

WiFiClient client;
IOSignal io;
Bounce2::Button downBtn = Bounce2::Button();

void setup() {
  downBtn.attach(BUTTON_PIN, INPUT_PULLUP);
  downBtn.interval(5);           
  downBtn.setPressedState(LOW);  

  Serial.begin(115200);
  Serial.println();
  Serial.print("Connecting... ");

  WiFi.mode(WIFI_STA);
  WiFi.begin("WIFI_SSID", "WIFI_PASS");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  io.setRxBuffer( 200 );
  io.begin( &client , "io.iosignal.net", 55488);  
  // io.begin( &client , "192.168.0.204", 55488);
}

  
void loop() {
    io.loop();
    downBtn.update();
    if (downBtn.pressed()) {
        Serial.println("down");
        io.signal("#homeButton", "esp8266");
    }     
}

