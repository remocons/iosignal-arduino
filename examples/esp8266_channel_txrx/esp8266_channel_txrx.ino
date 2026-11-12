/*
  ESP8266 (General, D1 mini) or ESP32 module + LED + Button

  https://github.com/remocons/iosignal-arduino

  An example of communicating between Arduinos and controlling them with a webapp.
    1. change the LED_PIN and BUTTON_PIN numbers according to your board.
    2. compile and upload to Arduino.
    3. set Wi-Fi over USB on https://test.iosignal.net/wifi/ (the board keeps it), or write it in WIFI_SSID and WIFI_PASS below.
       The same page can set another IOSignal server (host:port) and a device key; the server below is the default.
    4. Access https://test.iosignal.net with a modern web browser.

  [kr] 아두이노들간의 통신과 웹앱으로 제어하는 예제입니다.
    1. 보드에 따라 LED_PIN과 BUTTON_PIN 번호를 변경하세요.
    2. 아두이노에 컴파일하고 업로딩합니다.
    3. USB로 연결해 https://test.iosignal.net/wifi/ 에서 Wi-Fi를 넣습니다(보드가 저장). 또는 아래 WIFI_SSID, WIFI_PASS에 씁니다.
       같은 페이지에서 다른 IOSignal 서버(호스트:포트)와 장치 키도 넣을 수 있고, 아래 서버는 기본값입니다.
    4. 모던 웹브라우저로 https://test.iosignal.net 에 접속하세요.
 */

#if defined(ESP8266)
 #define BOARD_NAME "esp8266"
 #define BUTTON_PIN 14
#elif defined(ESP32)
 #define BOARD_NAME "esp32"
 #define BUTTON_PIN 12
#else
 #error "Select an ESP8266 or ESP32 board for this example."
#endif
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <Bounce2.h>

#define LED_PIN 2
#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASS "WIFI_PASS"

WiFiClient client;
IOSignal io;
IOSignalConfig config;
Bounce2::Button downBtn = Bounce2::Button();

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // active low 
  downBtn.attach(BUTTON_PIN, INPUT_PULLUP);
  downBtn.interval(5);           
  downBtn.setPressedState(LOW);  

  Serial.begin(115200);
  Serial.println();
  // Wi-Fi, server and device key: set over USB, or the sketch values.
  config.begin( Serial, BOARD_NAME " channel", WIFI_SSID, WIFI_PASS );
  config.setServer( "io.iosignal.net", 55488 );
  config.printServer();

  io.setRxBuffer( 200 );
  io.setLog( [](const char* line) { Serial.println(line); } );
  io.onReady( &onReady );
  io.onMessage( &onMessage );
  if ( config.key() ) io.auth( config.key() ); // a device key set over USB, else anonymous
  io.begin( &client, config.host(), config.port() );
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
    config.loop(); // USB setup requests, Wi-Fi reconnect
    if ( config.connected() ) io.loop();
    downBtn.update();
    if (downBtn.pressed()) {
        Serial.println("down");
        io.signal("#homeButton", BOARD_NAME);
    }    
}