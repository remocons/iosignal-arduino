/*
 *  IOSignal Example. 
 *  SparkFun ESP8266 Thing - Dev Board + Button
 *  You can use general ESP8266 board too.
 *
 *  Lee Dongeun <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *
 *  MIT License
 */


#include <Arduino.h>
#include <ESP8266WiFiMulti.h>
#include <IOSignal.h>
 
ESP8266WiFiMulti wifiMulti;
WiFiClient client;
IOSignal io;
const char *name = "SFE-thing-dev:home";
const char *ui = "on,off,toggle";

const int buttonPin = 2;
int lastButtonState = HIGH; 

void deviceOn(){
  digitalWrite(LED_BUILTIN, LOW); // Low means ON
  io.signal("@$state", "on" );
}

void deviceOff(){
  digitalWrite(LED_BUILTIN, HIGH);
  io.signal("@$state", "off" );
}

void deviceToggle(){
  int nextState = !digitalRead(LED_BUILTIN);
  digitalWrite(LED_BUILTIN, nextState);
   if( nextState){
    io.signal("@$state", "off" );
   }else{
    io.signal("@$state", "on" );
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

  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP( "WIFI_SSID", "WIFI_PASS");
  wifiMulti.addAP( "twesomego", "qwer1234");  
  // You can add multiple APs.  
  Serial.println();
  Serial.println();
  Serial.print("Wait for WiFi... ");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);

  io.setRxBuffer( 200 );
  io.begin( &client , "io.remocon.kr", 55488);
  io.onReady( &onReady );
  io.onMessage( &onMessage );
  // io.auth( "ID_KEY" ); 

}


void loop() {
  
      uint8_t conditionCode = io.update();
      if(conditionCode != 0 ){ 
          Serial.print("C");
          Serial.println( conditionCode); 
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
        io.signal("#screen", "next" );

        // type 3. To make a uni-cast transmission, you need to know the CID of the recipient.
        // io.signal("cid@", "click" );   // Follow the recipient's CID with the @ character.
        // io.signal("cid@button", "click" ); // You can add a topic after the @.
      }

}




void onReady()
{
  Serial.print("onReady cid: ");
  Serial.println( io.cid );
  io.signal("@$state", "off" );
  io.signal("@$ui", ui );
  io.signal("@$name", name );
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