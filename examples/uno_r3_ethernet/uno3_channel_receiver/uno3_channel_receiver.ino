
/*
  Arduino Uno + Ethernet shield W5100 + LED

  Channel message subscriber (receiver) example
  1. connect to the server.
  2. Press a button to send a message to the "#homeButton" channel.
  3. Devices that have subscribed to the "#homeButton" channel receive the message.

  채널 메시지 구독자(수신자) 예제
  1. 서버에 접속합니다.
  2. 버튼을 누르면 "#homeButton" 채널에 메시지를 전송합니다.
  3. "#homeButton" 채널을 구독(subscribe)한 장치가 메시지를 수신합니다.

 */

#include <SPI.h>
#include <Ethernet.h>
#include <string.h>
#include <IOSignal.h>

#define LED_PIN    3

// If you have multiple devices, you'll need to change the MAC address.
byte mac[]{0, 0, 0, 0, 0, 0x08}; 
EthernetClient client;
IOSignal io;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // active High

  Serial.begin(115200);
  Serial.println(F("Init.."));

  Ethernet.init(10);
  Ethernet.begin(mac); // DHCP
  Serial.print(F("IP:"));
  Serial.println(Ethernet.localIP());
  
  io.setRxBuffer( 80 );
  io.begin( &client, "io.remocon.kr", 55488);
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
}

