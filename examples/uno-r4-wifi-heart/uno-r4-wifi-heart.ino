/*
 *  IOSignal Example. 
 *  Arduino Uno R4 WiFi
 *
 *  Lee Dongeun <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *
 *  MIT License
 */


#include "WiFiS3.h"
#include <Arduino.h>
#include <IOSignal.h>
#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;
WiFiClient client;
IOSignal io;
const char *name = "UnoR4-WiFi:HOME";
const char *ui = "on,off,toggle";

const int buttonPin = 2;
int lastButtonState = HIGH; 

const uint32_t heart[] = { 0x3184a444, 0x44042081, 0x100a0040 };
const uint32_t fullOff[] = { 0, 0, 0 };

void deviceOn(){
  digitalWrite(LED_BUILTIN, HIGH);
  matrix.loadFrame(heart);
  io.signal("@$state", "on" );
}

void deviceOff(){
  digitalWrite(LED_BUILTIN, LOW);
  matrix.loadFrame(fullOff);
  io.signal("@$state", "off" );
}

void deviceToggle(){
  int nextState = !digitalRead(LED_BUILTIN);
   if( nextState){
      deviceOn();
   }else{
      deviceOff();
   }
}

int isPressed(){
  int currentState = digitalRead(buttonPin);
  if(lastButtonState == HIGH && currentState == LOW){
    lastButtonState = LOW;
    return 1;
  } 
  else{
    lastButtonState = currentState;
    return 0;
  } 
}

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  WiFi.begin("WIFI_SSID", "WIFI_PASS");
  Serial.begin(115200);
  matrix.begin();
  matrix.loadFrame(heart);
  delay(500);

  io.setRxBuffer( 200 );
  io.begin( &client, "io.remocon.kr", 55488 );
  io.onReady( &onReady );
  io.onMessage( &onMessage );
  // io.auth( "id_key" );

}


void loop() {
  
    if(io.update() == 0 ){ 
       
      if(isPressed()){
        Serial.println(F("pressed"));
        deviceToggle();
        // type 1. Multicasting to a public channel
        // io.signal("public_button", "click" );  // simple channel
        // io.signal("public#button", "click" );  // Separate channel names and a topic with # marks.

        // type 2. Multicasting to a Private HOME_CHANNEL 
        // Omitting the channel name allows devices with the same global IP address to communicate.
        // io.signal("#button", "click" );  // Omit the channel name and separate it from the topic with a # marker.
        io.signal("#screen", "playToggle" );
        // io.signal("#screen", "next" );

        // type 3. To make a uni-cast transmission, you need to know the CID of the recipient.
        // io.signal("cid@", "click" );   // Follow the recipient's CID with the @ character.
        // io.signal("cid@button", "click" ); // You can add a topic after the @.
      }
    } 

}


void onReady()
{
  Serial.print("onReady cid: ");
  Serial.println( io.cid );
  io.signal("@$state", "off" );
  io.signal("@$name", name );
  io.signal("@$ui", ui );
  io.signal("#notify", io.cid );
  io.subscribe("#search");
  
}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{

  // signal message info
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

  if( strcmp(tag, "#search") == 0){
    io.signal( "#notify", io.cid );
  }

  if( strcmp(tag, "@ui") == 0){
    io.signal2( (char *)payload, "@ui", io.cid , ui );
  }
      
  if( strcmp(tag, "@") == 0){
    if( strcmp((char *)payload, "on") == 0){
      deviceOn();
    }else if( strcmp((char *)payload, "off") == 0){
      deviceOff();
    }else if( strcmp((char *)payload, "toggle") == 0){
      deviceToggle();
    }
  }
      
}

