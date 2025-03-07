/*
  ESP8266 General or D1 mini + LED

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

#include <ESP8266WiFi.h>
#include <IOSignal.h>
#include <Bounce2.h>

#define LED_PIN    2  // D4 (D1 mini built-in LED)

WiFiClient client;
IOSignal io;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // active low 

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
  io.onReady( &onReady );
  io.onMessage( &onMessage );
}

  
void loop() {
    io.loop(); 
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
