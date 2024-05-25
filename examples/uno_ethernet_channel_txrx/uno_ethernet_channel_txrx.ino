
/*
  Arduino Uno R3 + Ethernet shield W5100 + LED

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

#include <SPI.h>
#include <Ethernet.h>
#include <string.h>
#include <IOSignal.h>
#include <Bounce2.h>

#define LED_PIN    3
#define BUTTON_PIN  2

// If you have multiple devices, you'll need to change the MAC address.
byte mac[]{0, 0, 0, 0, 0, 0x81}; 
EthernetClient client;
IOSignal io;
Bounce2::Button downBtn = Bounce2::Button();

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // active High
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
  io.onReady( &onReady );
  io.onMessage( &onMessage );
}


void onReady()
{
  Serial.print("onReady cid: ");
  Serial.println( io.cid );
  io.subscribe("#homeButton");
}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{

  Serial.print(">> signal tag: " );
  Serial.print( tag );
  Serial.print(" type: " );
  Serial.print( payloadType );
  Serial.print(" size: " );
  Serial.println( payloadSize );

  if( strcmp(tag, "#homeButton") == 0){
    if( digitalRead(LED_PIN ) == HIGH ){
      digitalWrite( LED_PIN , LOW );
    }else{
      digitalWrite( LED_PIN , HIGH );
    }
  }
   
}


void loop() {
  io.loop();   
  downBtn.update();
  if (downBtn.pressed()) {
    Serial.println("down");
    io.signal("#homeButton", "uno3-eth" );
  }   
}

