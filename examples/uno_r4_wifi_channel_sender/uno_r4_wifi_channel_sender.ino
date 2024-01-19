/*
  Arduino Uno R3 WiFi + Button (external)

  Channel Message Publisher (Sender) Example
  1. connect to the server.
  2. Press a button to send a message to the "#homeButton" channel.
  3. Devices that have subscribed to the "#homeButton" channel receive the message.

  채널 메시지 발행자(송신자) 예제
  1. 서버에 접속합니다.
  2. 버튼을 누르면 "#homeButton" 채널에 메시지를 전송합니다.
  3. "#homeButton" 채널을 구독(subscribe)한 장치가 메시지를 수신합니다.
  
  https://github.com/remocons/iosignal-arduino
  
 */

#include "WiFiS3.h"
#include <Arduino.h>
#include <IOSignal.h>
#include "Arduino_LED_Matrix.h"
#include <Bounce2.h>

#define BUTTON_PIN D2
ArduinoLEDMatrix matrix;
WiFiClient client;
IOSignal io;
Bounce2::Button downBtn = Bounce2::Button();

void setup() {
  downBtn.attach(BUTTON_PIN, INPUT_PULLUP);
  downBtn.interval(5);           
  downBtn.setPressedState(LOW);  
  
  Serial.begin(115200);
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("WiFi Connecting...");
    status = WiFi.begin("WIFI_SSID", "WIFI_PASS");
    delay(10000);
  }

  io.setRxBuffer( 200 );
  io.begin( &client, "io.remocon.kr", 55488 );
  io.onReady( &onReady );
}


void loop() {
    io.loop();
    downBtn.update();
    if (downBtn.pressed()) {
        Serial.println("down");
        io.signal("#homeButton","down");
    }     

}


void onReady()
{
  Serial.print("onReady cid: ");
  Serial.println( io.cid );
}

