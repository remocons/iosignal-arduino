/*
 *  IOSignal Remocon example: ESP-01 relay and button
 *
 *  remocon.kr shows it as a 2-way switch (switch2.1):
 *    s1 = relay on GPIO0, s2 = LED on GPIO1 (both active low).
 *    The button on GPIO2 toggles the relay.
 *  GPIO1 is the TX pin, so this sketch normally does not use serial.
 *  GPIO0 and GPIO2 must be HIGH at boot; check the relay and button wiring.
 *
 *  Setup mode: when the board keeps no Wi-Fi yet, when the settings page talks to it over
 *  RX (GPIO3, unused here), or when the button is held during the first 3 seconds after power on,
 *  GPIO1 is TX instead of the LED until the next reset.
 *  Set Wi-Fi over the USB-TTL adapter on https://arduino.iosignal.net/wifi/ (the board keeps it);
 *  the page can also set another IOSignal server and a device key. A board that already keeps
 *  Wi-Fi (from another sketch too) answers the page within a few seconds after Connect.
 *  Or write the Wi-Fi in WIFI_SSID and WIFI_PASS below. Then open https://remocon.kr.
 *
 *  [kr] ESP-01의 릴레이(GPIO0)와 LED(GPIO1)를 2구 스위치로 제어하고, GPIO2 버튼으로 릴레이를 토글합니다.
 *       Wi-Fi가 없거나, 설정 페이지가 RX로 요청을 보내거나, 켠 뒤 3초 안에 버튼을 누르고 있으면
 *       설정 모드(시리얼 켜짐, LED 꺼짐)가 됩니다.
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

#define RELAY_PIN    0
#define LED_PIN      1
#define BUTTON_PIN   2

#define WIFI_SSID    "WIFI_SSID"
#define WIFI_PASS    "WIFI_PASS"

IOSignalConfig config;
WiFiClient client;
IOSignal io;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "ESP01-Relay-Btn:HOME");
void sendInfo() {
  // $name "name:category" (the board setting 'name'), $ui the type or the buttons (device rules v2).
  char name[64];
  remocon::formatNameSetting(name, sizeof(name), nameSetting.value(), "ESP01-Relay-Btn");
  io.signal("@$name", name);
  io.signal("@$ui", "t=switch2.1");
}

Bounce2::Button aBtn = Bounce2::Button();

// 1 means on, whatever the pin polarity.
char states[] = "00";
char last_states[] = "00";
// The relay module and the LED both turn on with LOW. Change false/true to match your relay.
const bool activeLow[] = { true, true };
uint8_t pinMap[] = { RELAY_PIN, LED_PIN };
bool setupMode = false;  // serial on GPIO1/TX instead of the LED until the next reset

void startSetupMode() {
  setupMode = true;
  pinMode( LED_PIN, SPECIAL );  // GPIO1 back to TX
  config.attachSerial( Serial );
  Serial.println();
  Serial.println("ESP01 setup mode: set Wi-Fi on https://arduino.iosignal.net/wifi/");
  config.printServer();
}

// In normal mode RX is still read: the settings page sends Improv packets ("IMPROV...") and asks
// again every few seconds, so after the first header the board answers the next request.
void watchSerial() {
  static const char HEADER[] = "IMPROV";
  static uint8_t matched = 0;
  while ( !setupMode && Serial.available() ) {
    char c = Serial.read();
    matched = c == HEADER[matched] ? matched + 1 : ( c == HEADER[0] ? 1 : 0 );
    if ( matched == 6 ) startSetupMode();
  }
}

void publishState() {
  char text[23];
  snprintf(text, sizeof(text), "s1=%c;s2=%c", states[0], states[1]);
  io.signal("@$state", text);
}

void stateChange(int i){
  //local
  char state = states[i];
  if( state != last_states[i] ){
    if( !(setupMode && pinMap[i] == LED_PIN) ) digitalWrite( pinMap[i] , (states[i] == '1') != activeLow[i] );
    last_states[i] = state;
  }

  //publish device states
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
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);  // relay off (active low); GPIO0 must be HIGH at boot

  aBtn.attach( BUTTON_PIN, INPUT_PULLUP ); 
  aBtn.interval(5); // debounce interval in milliseconds
  aBtn.setPressedState(LOW); 

  // Wi-Fi, server and device key: stored over USB, or the sketch values. No serial yet.
  config.begin( "ESP01-Relay-Btn", WIFI_SSID, WIFI_PASS );
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  config.setServer( "io.remocon.kr", 55488 );

  // You can't use Serial-tx and the LED together: serial only in setup mode.
  // GPIO2 must be HIGH at reset, so the button is read after boot, not during it.
  setupMode = !config.wifiStored() && strcmp( WIFI_SSID, "WIFI_SSID" ) == 0;
  for( unsigned long start = millis(); !setupMode && millis() - start < 3000; delay(10) ){
    if( digitalRead( BUTTON_PIN ) == LOW ) setupMode = true;
  }
  Serial.begin(115200);  // RX is read in both modes; TX only in setup mode
  if( setupMode ){
    startSetupMode();
  } else {
    pinMode( LED_PIN , OUTPUT);   // GPIO1 leaves TX for the LED
    digitalWrite( LED_PIN, HIGH);  // active low
  }

  io.setRxBuffer( 200 );
  io.onReady( &onReady );
  io.onMessage( &onMessage );
  if ( config.key() ) io.auth( config.key() ); // a device key set over USB, else anonymous
  io.begin( &client, config.host(), config.port() );
}


void loop() {
    watchSerial(); // the settings page switches a running board to setup mode
    config.loop(); // USB setup requests (setup mode), Wi-Fi reconnect
    if ( config.connected() ) io.loop();   
    aBtn.update();
    if ( aBtn.pressed() ) {
      toggle(0);
      delay(100);
    }  

}




void onReady()
{
  // NOTICE. 
  // The ESP01's serial communication pins are used as IO pins, so serial communication is not available.
  // Serial.print("onReady cid: ");
  // Serial.println( io.cid );

  io.subscribe("#search");
  sendInfo();
  publishState();
  io.signal("#notify", io.cid);
  
}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{

  if( strcmp(tag, "#search") == 0){
    io.signal( "#notify", io.cid );
  }

  // Ignore non-text or unterminated payloads before using string APIs.
  if (payloadType != IOSignal::PAYLOAD_TYPE::TEXT || !payload ||
      !payloadSize || !memchr(payload, 0, payloadSize)) return;

  if( strcmp(tag, "@") == 0){
    // s1=1 sets an output, all=1 / all=0 sets both (device rules). Unknown commands still get a reply.
    const char *c = (const char *)payload;
    int i = c[0] == 's' ? c[1] - '1' : -1;
    if( i >= 0 && i < 2 && c[2] == '=' && (c[3] == '0' || c[3] == '1') && c[4] == 0 ){
      states[i] = c[3];
      stateChange(i);
    }else if( strcmp(c, "all=1") == 0 || strcmp(c, "all=0") == 0 ){
      // Both outputs, then one state: stateChange() of the last one publishes it.
      states[0] = states[1] = c[4];
      if( states[0] != last_states[0] ){
        if( !(setupMode && pinMap[0] == LED_PIN) ) digitalWrite( pinMap[0] , (states[0] == '1') != activeLow[0] );
        last_states[0] = states[0];
      }
      stateChange(1);
    }else{
      publishState();
    }
  }
      
}
