/*
 *  IOSignal Remocon example: ESP-12F 4-channel relay board
 *
 *  remocon.kr shows it as a 4-channel relay (relay4.1).
 *    Relays 1-4 are on GPIO13, 12, 14 and 16. Commands: r1=1 ... r4=0, all=1, all=0.
 *    The names L (built-in LED, GPIO2) and A-D toggle an output; they are for IR
 *    routers that do not know the state.
 *    Its $name is the board setting "name" ("Relay4CH:HOME" by default); Remocon8266-button-controller can control this board.
 *
 *  Setup: connect a USB-TTL adapter to the programming port, hold IO0 to GND while
 *  resetting, upload, then release IO0 and reset. Set Wi-Fi over the same adapter on
 *  https://arduino.iosignal.net/wifi/ (the board keeps it), or put it (and an optional device
 *  key) in _secrets.h / edit WIFI_SSID and WIFI_PASS below. The page can also set another
 *  IOSignal server and a device key.
 *
 *  [kr] ESP-12F 4채널 릴레이 보드를 relay4 타입으로 제어합니다. 업로드할 때 IO0을 GND에 연결합니다.
 *
 *  Taeo Lee <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *  MIT License
 */


#include <Arduino.h>
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <IOSignalUi.h>

// Wi-Fi values (and an optional device key) can go in _secrets.h next to this sketch,
// which keeps them out of version control. Leave the placeholders to use the Wi-Fi set over USB.
#if __has_include("_secrets.h")
#include "_secrets.h"
#endif
#ifndef SECRET_WIFI_SSID
#define SECRET_WIFI_SSID "WIFI_SSID"
#define SECRET_WIFI_PASS "WIFI_PASS"
#endif
const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASS = SECRET_WIFI_PASS;

WiFiClient client;
IOSignalConfig config;
IOSignal io;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "Relay4CH:HOME");
void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  remocon::formatNameSetting(name, sizeof(name), nameSetting.value(), "Relay4CH");
  io.signal("@$name", name);
  io.signal("@$ui", "t=relay4.1");
}

char states[] = "10000";
char last_states[] = "10000";
uint8_t pinMap[] = { 2, 13, 12, 14, 16};


void publishState() {
  char text[32];
  snprintf(text, sizeof(text), "r1=%c;r2=%c;r3=%c;r4=%c", states[1], states[2], states[3], states[4]);
  io.signal("@$state", text);
}

void stateChange(int i){
  //local
  char state = states[i];
  if( state != last_states[i] ){
    digitalWrite( pinMap[i] , states[i] - '0' );
    last_states[i] = state;
  }
  Serial.print("states: ");
  Serial.println( states);

  // for remote peers.
  publishState();
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
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  pinMode(12, OUTPUT);
  digitalWrite(12, LOW);
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
  pinMode(14, OUTPUT);
  digitalWrite(14, LOW);
  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);


  Serial.begin(115200);
  Serial.println();
  // Wi-Fi, server and device key: set over USB, or the sketch values.
  config.begin( Serial, "Relay4CH", WIFI_SSID, WIFI_PASS );
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
#ifdef SECRET_ID_KEY
  config.setKey( SECRET_ID_KEY ); // optional key (id.key) from _secrets.h; one set over USB wins
#endif
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
    const char *c = (const char *)payload;
    if( c[0] == 'r' && c[1] >= '1' && c[1] <= '4' && c[2] == '=' && (c[3] == '0' || c[3] == '1') && c[4] == 0 ){
      states[c[1] - '0'] = c[3];
      stateChange(c[1] - '0');
    }else if( strcmp(c, "all=1") == 0 || strcmp(c, "all=0") == 0 ){
      for(int i = 1; i <= 4; i++){
        states[i] = c[4];
        stateChange(i);
      }
    // The name commands below are for IR routers, which do not know the state.
    }else if( strcmp((char *)payload, "L") == 0){
      toggle(0);
    }else if( strcmp((char *)payload, "A") == 0){
      toggle(1);
    }else if( strcmp((char *)payload, "B") == 0){
      toggle(2);
    }else if( strcmp((char *)payload, "C") == 0){
      toggle(3);
    }else if( strcmp((char *)payload, "D") == 0){
      toggle(4);
    }else{
      publishState(); // Unknown commands still get a reply.
    }
  }
      
}