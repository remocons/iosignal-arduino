/*
  Arduino Uno R3 WiFi + built-in LED Matrix

  Channel message subscriber (receiver) example
  1. connect to the server.
  2. Press a button to send a message to the "#homeButton" channel.
  3. Devices that have subscribed to the "#homeButton" channel receive the message.

  채널 메시지 구독자(수신자) 예제
  1. 서버에 접속합니다.
  2. 버튼을 누르면 "#homeButton" 채널에 메시지를 전송합니다.
  3. "#homeButton" 채널을 구독(subscribe)한 장치가 메시지를 수신합니다.
  
  https://github.com/remocons/iosignal-arduino
  
 */

#include "WiFiS3.h"
#include <Arduino.h>
#include <IOSignal.h>
#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;
WiFiClient client;
IOSignal io;

const uint32_t heart[] = { 0x3184a444, 0x44042081, 0x100a0040 };
const uint32_t fullOff[] = { 0, 0, 0 };
int heartStatus = 1;

void setup() {
  
  Serial.begin(115200);
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("WiFi Connecting...");
    status = WiFi.begin("WIFI_SSID", "WIFI_PASS");
    delay(10000);
  }
  matrix.begin();
  matrix.loadFrame(heart);
  delay(500);

  io.setRxBuffer( 200 );
  io.begin( &client, "io.remocon.kr", 55488 );
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

void toggleHeart(){
    heartStatus = !heartStatus; // toggle 1 <-> 0
    if( heartStatus){
      matrix.loadFrame(heart);
    }else{
      matrix.loadFrame(fullOff);
    }
}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{

  Serial.print(">> signal tag: " );
  Serial.print( tag );
  Serial.print(" type: " );
  Serial.print( payloadType );
  Serial.print(" size: " );
  Serial.println( payloadSize );

  if( payloadType == IOSignal::PAYLOAD_TYPE::TEXT ){  
    Serial.print("string payload: " );
    Serial.println( (char *)payload  );
  }

  if( strcmp(tag, "#homeButton") == 0){
    toggleHeart();
  }

}

