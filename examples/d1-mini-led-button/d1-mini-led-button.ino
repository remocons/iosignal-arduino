/*
 *  IOSignal Remocon example: ESP8266 D1 mini
 *
 *  remocon.kr shows it as a light (light.1): on, off, toggle.
 *    The built-in LED (GPIO2, active low) is the light.
 *    A button on GPIO14 (D5, to GND) toggles it.
 *
 *  Wi-Fi: upload, then set it over USB on https://arduino.iosignal.net/wifi/ (the board keeps it),
 *  or write it in the sketch (then the sketch values are used at every boot). The same page can
 *  set another IOSignal server and a device key.
 *  Then open https://remocon.kr in a browser on the same router.
 *
 *  [kr] D1 mini의 내장 LED를 전등 타입으로 켜고 끕니다. D5 버튼으로도 토글합니다.
 *
 *  Taeo Lee <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *  MIT License
 */

#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <string.h>
#include <Bounce2.h>
#include <IOSignalUi.h>
#define BUTTON_PIN    14
#define BUILTIN_LED   2

Bounce2::Button aBtn = Bounce2::Button();
IOSignalConfig config;
WiFiClient client;
IOSignal io;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "D1-mini:HOME");
void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  remocon::formatNameSetting(name, sizeof(name), nameSetting.value(), "D1-mini");
  io.signal("@$name", name);
  io.signal("@$ui", "t=light.1");
}


void publishState() { io.signal("@$state", digitalRead(BUILTIN_LED) == LOW ? "on=1" : "on=0"); }

void deviceOn(){
  digitalWrite(BUILTIN_LED, LOW);
  io.signal("@$state", "on=1" );
}

void deviceOff(){
  digitalWrite(BUILTIN_LED, HIGH);
  io.signal("@$state", "on=0" );
}

void deviceToggle(){
  int nextState = !digitalRead(BUILTIN_LED);
  Serial.println( nextState);
  digitalWrite(BUILTIN_LED, nextState);
   if( !nextState){  // active low
    deviceOn();
   }else{
    deviceOff();
   }
}


void setup() {  
  pinMode( BUILTIN_LED , OUTPUT);
  digitalWrite( BUILTIN_LED , HIGH);  //
  
  aBtn.attach( BUTTON_PIN, INPUT_PULLUP ); 
  aBtn.interval(5); // debounce interval in milliseconds
  aBtn.setPressedState(LOW); 

  Serial.begin(115200);
  Serial.println();
  // Wi-Fi, server and device key: set over USB, or the sketch values.
  config.begin( Serial, "D1-mini", "WIFI_SSID", "WIFI_PASS" );
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer( "io.remocon.kr", 55488 );
  config.printServer();
  io.setRxBuffer( 200 );
  io.setLog( [](const char* line) { Serial.println(line); } );
  io.onReady( &onReady );
  io.onMessage( &onMessage );
  if ( config.key() ) io.auth( config.key() ); // a device key set over USB, else anonymous
  io.begin( &client, config.host(), config.port() );
}


void loop() {
  config.loop(); // USB setup requests, Wi-Fi reconnect
  if ( config.connected() ) io.loop();
  aBtn.update();
  if ( aBtn.pressed() ) {
    deviceToggle();
    Serial.println("button");
    delay(100);
  }  
}

void onReady()
{
  Serial.print("onReady cid: ");
  Serial.println( io.cid );
  io.subscribe("#search");
  sendInfo();
  publishState();
  io.signal("#notify", io.cid);
  
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

  if( payloadType == IOSignal::PAYLOAD_TYPE::TEXT && payload && payloadSize && memchr(payload, 0, payloadSize) ){
    Serial.print("string payload: " );
    Serial.println( (char *)payload  );
  }

  if( strcmp(tag, "#search") == 0){
    io.signal( "#notify", io.cid );
  }

  // Ignore non-text or unterminated payloads before using string APIs.
  if (payloadType != IOSignal::PAYLOAD_TYPE::TEXT || !payload ||
      !payloadSize || !memchr(payload, 0, payloadSize)) return;

  if( strcmp(tag, "@") == 0){
    if( strcmp((char *)payload, "on") == 0){
      deviceOn();
    }else if( strcmp((char *)payload, "off") == 0){
      deviceOff();
    }else if( strcmp((char *)payload, "toggle") == 0){
      deviceToggle();
    } else {
      publishState(); // Unknown commands still get a reply.
    }
  }
      
}
