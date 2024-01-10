/*
 *  esp01-relay-button Example. 
 *
 *  Lee Dongeun <sixgen@gmail.com>
 *  https://github.com/remocons/remocon-arduino
 *
 *  MIT License
 */

#include <ESP8266WiFiMulti.h>
#include <IOSignal.h>
#include <Bounce2.h>

ESP8266WiFiMulti wifiMulti;
WiFiClient client;
IOSignal io;

Bounce2::Button aBtn = Bounce2::Button();

const char *name = "ESP01-Relay-Btn:HOME";
const char *ui = "Relay,LED";
char states[] = "11";
char last_states[] = "11";
uint8_t pinMap[] = { 0,1 }; 

void stateChange(int i){
  //local
  char state = states[i];
  if( state != last_states[i] ){
    digitalWrite( pinMap[i] , states[i] - '0' );
    last_states[i] = state;
  }

  //io
  io.signal("@$state", states );

} 


void toggle(int i){
  char state = states[i];
   if( state == '1'){
      states[i] = '0';
   }else{
      states[i] = '1';
   }
   stateChange(i);
}


void setup() {  
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH);

  pinMode(1, OUTPUT);
  digitalWrite(1, HIGH);  // active low

  // NOTICE. ESP01. 
  // You can't use Serial-tx and built-in LED together.
  // Serial.begin(115200);
  
  aBtn.attach( 2, INPUT_PULLUP ); 
  aBtn.interval(5); // debounce interval in milliseconds
  aBtn.setPressedState(LOW); 
  
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP( "WIFI_SSID", "WIFI_PASS");
  wifiMulti.addAP( "twesomego", "qwer1234");  
  // You can add multiple APs.  

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
  }
  delay(500);

  io.setRxBuffer( 200 );
  io.begin( &client , "io.remocon.kr", 55488);
  io.onReady( &onReady );
  io.onMessage( &onMessage );
  // io.auth( "ID_KEY" ); 

}


void loop() {
    uint8_t conditionCode = io.update();
    if(conditionCode == 0 ){       
      aBtn.update();
      if ( aBtn.pressed() ) {
        toggle(0);
        delay(100);
      }  
    }
}




void onReady()
{
  // Serial.print("onReady cid: ");
  // Serial.println( io.cid );
  io.signal("@$state", states );
  io.signal("@$ui", ui );
  io.signal("@$name", name );
  io.signal("#notify", io.cid );
  io.subscribe("#search");
  
}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{

  if( strcmp(tag, "#search") == 0){
    io.signal( "#notify", io.cid );
  }

  if( strcmp(tag, "@ui") == 0){
    io.signal2( (char *)payload, "@ui", io.cid , ui );
  }
      
  if( strcmp(tag, "@") == 0){
    if( strcmp((char *)payload, "Relay") == 0){
      toggle(0);
    }
    else if( strcmp((char *)payload, "LED") == 0){
      toggle(1);
    }
  }
      
}
