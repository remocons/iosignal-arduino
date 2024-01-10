/*
 *  ESP8266 D1 mini
 *  Lee Dongeun <sixgen@gmail.com>
 *  https://github.com/remocons/remocon-signal-arduino
 *
 *  MIT License
 */

#include <ESP8266WiFiMulti.h>
#include <IOSignal.h>
#include <Bounce2.h>
#define BUTTON_PIN 13 // D7
#define BUILTIN_LED 2 // D4

Bounce2::Button aBtn = Bounce2::Button();
ESP8266WiFiMulti wifiMulti;
WiFiClient client;
IOSignal io;

const char *name = "D1-mini:home";
const char *ui = "on,off,toggle";

void deviceOn(){
  digitalWrite(BUILTIN_LED, LOW);
  io.signal("@$state", "on" );
}

void deviceOff(){
  digitalWrite(BUILTIN_LED, HIGH);
  io.signal("@$state", "off" );
}

void deviceToggle(){
  int nextState = !digitalRead(BUILTIN_LED);
  digitalWrite(BUILTIN_LED, nextState);
   if( nextState){
    io.signal("@$state", "off" );
   }else{
    io.signal("@$state", "on" );
   }
}


void setup() {  
  pinMode( BUILTIN_LED , OUTPUT);
  digitalWrite( BUILTIN_LED , HIGH);  //
  
  aBtn.attach( BUTTON_PIN, INPUT_PULLUP ); 
  aBtn.interval(5); // debounce interval in milliseconds
  aBtn.setPressedState(LOW); 

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
    if(io.update() == 0 ){
      aBtn.update();
      if ( aBtn.pressed() ) {
        deviceToggle();
        delay(100);
      }  
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
