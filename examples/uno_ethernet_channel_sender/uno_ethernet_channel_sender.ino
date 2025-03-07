
/*
 Arduino Uno R3 + Ethernet shield W5100 + Button

  https://github.com/remocons/iosignal-arduino
  
  An example of communicating between Arduinos and controlling them with a webapp.
    1. change the LED_PIN and BUTTON_PIN numbers according to your board.
    2. set WIFI_SSID and KEY when using wifi.
    3. iosignal server information is used without modification. If you use another server, change the server address and port number.
    4. compile and upload to Arduino.
    5. Access http://test.iosignal.net with a modern web browser.

  [kr] 아두이노들간의 통신과 웹앱으로 제어하는 예제입니다.
    1. 보드에 따라 LED_PIN과 BUTTON_PIN 번호를 변경하세요.
    2. wifi 사용시 WIFI_SSID와 KEY 를 설정하세요.
    3. iosignal 서버 정보는 수정 없이 그대로 사용합니다. 다른 서버를 사용할 경우 서버 주소와 포트번호를 변경해줍니다.
    4. 아두이노에 컴파일하고 업로딩합니다.
    5. 모던 웹브라우저로  http://test.iosignal.net 에 접속하세요.

 */

#include <SPI.h>
#include <Ethernet.h>
#include <string.h>
#include <IOSignal.h>
#include <Bounce2.h>

#define BUTTON_PIN  2

// If you have multiple devices, you'll need to change the MAC address.
byte mac[]{0, 0, 0, 0, 0, 0x07}; 
EthernetClient client;
IOSignal io;

Bounce2::Button downBtn = Bounce2::Button();

void setup() {
  downBtn.attach(BUTTON_PIN, INPUT_PULLUP);
  downBtn.interval(5);           
  downBtn.setPressedState(LOW);  

  Serial.begin(115200);
  Serial.println(F("Init.."));

  Ethernet.init(10);
  Ethernet.begin(mac); // DHCP
  Serial.print(F("IP:"));
  Serial.println(Ethernet.localIP());
  
  io.setRxBuffer( 80 );
  io.begin( &client , "io.iosignal.net", 55488);  
  // io.begin( &client , "192.168.0.204", 55488);

}


void loop() {
  io.loop();
  downBtn.update();
  if (downBtn.pressed()) {
    Serial.println("down");
    io.signal("#homeButton","uno3-eth");
  }     
}

