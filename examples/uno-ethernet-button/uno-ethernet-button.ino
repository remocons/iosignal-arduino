
/*
 *  IOSignal Example. 
 *  Arduino Uno + Ethernet shield W5100 + Button
 * 
 *  Lee Dongeun <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *
 *  MIT License
 */

#include <SPI.h>
#include <Ethernet.h>
#include <string.h>
#include <IOSignal.h>
#include <Bounce2.h>

// If you have multiple devices, you'll need to change the MAC address.
byte mac[]{0, 0, 0, 0, 0, 0x07}; 
EthernetClient client;
IOSignal io;
const char *name = "UnoR3-EthBtn:HOME";
const char *ui = "on,off,toggle";

const int buttonPin = 2;
int lastButtonState = HIGH; 

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(5, OUTPUT); // LED
  pinMode(6, OUTPUT); // Relay
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);

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
  // io.auth( "id_key" );

}

void deviceOn(){
   digitalWrite( 5, HIGH);
   digitalWrite( 6, HIGH);
  io.signal("@$state", "on" );
}

void deviceOff(){
   digitalWrite(5, LOW);
   digitalWrite(6, LOW);
  io.signal("@$state", "off" );
}

void deviceToggle(){
  int nextState = !digitalRead(5);
  digitalWrite(5, nextState);
  digitalWrite(6, nextState);
   if(nextState){
    io.signal("@$state", "on" );
   }else{
    io.signal("@$state", "off" );
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

void loop() {

      uint8_t conditionCode = io.update();
      if(conditionCode != 0 ){ 
        Serial.print("C");
        Serial.println( conditionCode );
          return;
      } 
      
      if(isPressed()){
        Serial.println(F("pressed"));
        // type 1. Multicasting to a public channel
        // io.signal("public_button", "click" );  // simple channel
        // io.signal("public#button", "click" );  // Separate channel names and a topic with # marks.

        // type 2. Multicasting to a Private HOME_CHANNEL 
        // Omitting the channel name allows devices with the same global IP address to communicate.
        // io.signal("#button", "click" );  // Omit the channel name and separate it from the topic with a # marker.
        io.signal("#screen", "playToggle" );

        // type 3. To make a uni-cast transmission, you need to know the CID of the recipient.
        // io.signal("cid@", "click" );   // Follow the recipient's CID with the @ character.
        // io.signal("cid@button", "click" ); // You can add a topic after the @.
      }

}


void onReady()
{
  Serial.print(F("onReady cid: "));
  Serial.println( io.cid );
  io.signal("@$state", "off" );
  io.signal("@$ui", ui );
  io.signal("@$name", name );
  io.signal("#notify", io.cid );
  io.subscribe("#search");
  
}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{
  Serial.print(F(">> signal tag: ") );
  Serial.print( tag );
  Serial.print(F(" type: ") );
  Serial.print( payloadType );
  Serial.print(F(" size: " ));
  Serial.println( payloadSize );

  if( payloadType == IOSignal::PAYLOAD_TYPE::TEXT ){  
    Serial.print(F("string payload: ") );
    Serial.println( (char *)payload  );
  }

  if( strcmp(tag, "#search") == 0){
      io.signal( "#notify", io.cid );
  }
      
  if( strcmp(tag, "@ui") == 0){
      io.signal2( (const char*)payload, "@ui", io.cid , ui);
  }
      
  if( strcmp(tag, "@") == 0){
    if( strcmp((const char*)payload, "on") == 0){
      deviceOn();
    }else if( strcmp((const char*)payload, "off") == 0){
      deviceOff();
    }else if( strcmp((const char*)payload, "toggle") == 0){
      deviceToggle();
    }
  }
      
}


