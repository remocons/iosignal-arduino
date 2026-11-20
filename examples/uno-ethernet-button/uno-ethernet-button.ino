/*
 *  IOSignal Remocon example: Arduino UNO + W5100 Ethernet shield + button
 *
 *  remocon.kr shows it as a light (light.1): on, off, toggle.
 *    The light is an LED on D5 and a relay on D6, switched together.
 *    A button on D2 (to GND) toggles them and sends #screen playToggle,
 *    which plays or pauses a bayo.tv screen.
 *
 *  Setup: plug in the shield and a LAN cable (DHCP). With several boards,
 *  give each a different MAC address. Upload, then open https://remocon.kr
 *  in a browser on the same router.
 *
 *  [kr] UNO와 W5100 실드로 D5 LED와 D6 릴레이를 전등 타입으로 제어합니다. D2 버튼은 bayo.tv 재생 신호도 보냅니다.
 *
 *  Taeo Lee <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
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
const char *deviceName = "Uno-Btn:HOME";  // $name "name:category" (device rules v2)
const char *deviceUi = "t=light.1";      // $ui: the official type

void publishState() { io.signal("@$state", digitalRead(5) ? "on=1" : "on=0"); }

const int buttonPin = 2;
Bounce2::Button button;

void setup() {
  button.attach(buttonPin, INPUT_PULLUP);
  button.interval(5);
  button.setPressedState(LOW);
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
  // io.auth("ID_KEY"); // Optional: use your own device key.

}

void deviceOn(){
   digitalWrite( 5, HIGH);
   digitalWrite( 6, HIGH);
  io.signal("@$state", "on=1" );
}

void deviceOff(){
   digitalWrite(5, LOW);
   digitalWrite(6, LOW);
  io.signal("@$state", "on=0" );
}

void deviceToggle(){
  int nextState = !digitalRead(5);
  digitalWrite(5, nextState);
  digitalWrite(6, nextState);
   if(nextState){
    io.signal("@$state", "on=1" );
   }else{
    io.signal("@$state", "on=0" );
   }
}

void loop() {

      io.loop();
      button.update();
      if(button.pressed()){
        Serial.println(F("pressed"));
        deviceToggle();
        io.signal("#screen", "playToggle" );
      }

}


void onReady()
{
  Serial.print(F("onReady cid: "));
  Serial.println( io.cid );
  io.subscribe("#search");
  io.signal("@$name", deviceName);
  io.signal("@$ui", deviceUi);
  publishState();
  io.signal("#notify", io.cid);
  
}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{
  Serial.print(F(">> signal tag: ") );
  Serial.print( tag );
  Serial.print(F(" type: ") );
  Serial.print( payloadType );
  Serial.print(F(" size: " ));
  Serial.println( payloadSize );

  if( payloadType == IOSignal::PAYLOAD_TYPE::TEXT && payload && payloadSize && memchr(payload, 0, payloadSize) ){
    Serial.print(F("string payload: ") );
    Serial.println( (char *)payload  );
  }

  if( strcmp(tag, "#search") == 0){
      io.signal( "#notify", io.cid );
  }
      
  // Ignore non-text or unterminated payloads before using string APIs.
  if (payloadType != IOSignal::PAYLOAD_TYPE::TEXT || !payload ||
      !payloadSize || !memchr(payload, 0, payloadSize)) return;

  if( strcmp(tag, "@") == 0){
    if( strcmp((const char*)payload, "on") == 0){
      deviceOn();
    }else if( strcmp((const char*)payload, "off") == 0){
      deviceOff();
    }else if( strcmp((const char*)payload, "toggle") == 0){
      deviceToggle();
    } else {
      publishState(); // Unknown commands still get a reply.
    }
  }
      
}


