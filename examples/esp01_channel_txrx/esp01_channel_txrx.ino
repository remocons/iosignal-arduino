/*
  ESP01

  https://github.com/remocons/iosignal-arduino

  An example of communicating between Arduinos and controlling them with a webapp.
    1. change the LED_PIN and BUTTON_PIN numbers according to your board.
    2. compile and upload to Arduino.
    3. GPIO1 (TX) drives the LED, so serial is off. Setup mode turns serial on instead of the LED
       until the next reset: when no Wi-Fi is kept yet, or the button is held in the first 3 s after power on.
       Then set Wi-Fi (and optionally another IOSignal server and a device key) over the USB-TTL
       adapter on https://test.iosignal.net/wifi/. Or write WIFI_SSID and WIFI_PASS below.
    4. Access https://test.iosignal.net with a modern web browser.

  [kr] 아두이노들간의 통신과 웹앱으로 제어하는 예제입니다.
    1. 보드에 따라 LED_PIN과 BUTTON_PIN 번호를 변경하세요.
    2. 아두이노에 컴파일하고 업로딩합니다.
    3. GPIO1(TX)을 LED로 쓰므로 시리얼은 꺼져 있습니다. 저장된 Wi-Fi가 없거나 켠 뒤 3초 안에 버튼을 누르고 있으면
       다음 리셋까지 LED 대신 시리얼을 켭니다(설정 모드). 이때 USB-TTL로 https://test.iosignal.net/wifi/ 에서
       Wi-Fi(그리고 원하면 다른 서버와 장치 키)를 넣습니다. 또는 아래 WIFI_SSID, WIFI_PASS에 씁니다.
    4. 모던 웹브라우저로 https://test.iosignal.net 에 접속하세요.
 */

#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <Bounce2.h>
#include <string.h>

#define LED_PIN       1
#define BUTTON_PIN    2
#define WIFI_SSID     "WIFI_SSID"
#define WIFI_PASS     "WIFI_PASS"

WiFiClient client;
IOSignal io;
IOSignalConfig config;
bool setupMode = false;  // serial on GPIO1/TX instead of the LED until the next reset
Bounce2::Button downBtn = Bounce2::Button();

void setup() {
  downBtn.attach(BUTTON_PIN, INPUT_PULLUP);
  downBtn.interval(5);           
  downBtn.setPressedState(LOW);  

  // Wi-Fi, server and device key: stored over USB, or the sketch values. No serial yet.
  config.begin( "esp01 channel", WIFI_SSID, WIFI_PASS );
  config.setServer( "io.iosignal.net", 55488 );

  // GPIO2 must be HIGH at reset, so the button is read after boot, not during it.
  setupMode = !config.wifiStored() && strcmp( WIFI_SSID, "WIFI_SSID" ) == 0;
  for( unsigned long start = millis(); !setupMode && millis() - start < 3000; delay(10) ){
    if( digitalRead( BUTTON_PIN ) == LOW ) setupMode = true;
  }
  if( setupMode ){
    Serial.begin(115200);
    config.attachSerial( Serial );
    Serial.println();
    Serial.println("ESP01 setup mode: set Wi-Fi on https://test.iosignal.net/wifi/");
    config.printServer();
  } else {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);  // active low 
  }

  io.setRxBuffer( 200 );
  io.onReady( &onReady );
  io.onMessage( &onMessage );
  if ( config.key() ) io.auth( config.key() ); // a device key set over USB, else anonymous
  io.begin( &client, config.host(), config.port() );
}


void onReady()
{
  io.subscribe("#homeButton");
}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{

  if( strcmp(tag, "#homeButton") == 0 && !setupMode ){
    if( digitalRead(LED_PIN ) == HIGH ){
      digitalWrite( LED_PIN , LOW );
    }else{
      digitalWrite( LED_PIN , HIGH );
    }
  }
   
}


void loop() {
    config.loop(); // USB setup requests (setup mode), Wi-Fi reconnect
    if ( config.connected() ) io.loop();
    downBtn.update();
    if (downBtn.pressed()) {
        io.signal("#homeButton","esp01");
    }    
}