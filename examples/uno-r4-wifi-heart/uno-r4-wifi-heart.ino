/*
 *  IOSignal Remocon example: Arduino UNO R4 WiFi heart
 *
 *  remocon.kr shows it as a light (light.1): on, off, toggle.
 *    On shows a heart on the LED matrix and lights the built-in LED.
 *    A button on D12 (to GND) toggles it and sends #screen playToggle,
 *    which plays or pauses a bayo.tv screen.
 *
 *  Setup: replace "WIFI_SSID" and "WIFI_PASS" in WiFi.begin(), upload,
 *  then open https://remocon.kr in a browser on the same router.
 *
 *  [kr] UNO R4 WiFi의 LED 매트릭스 하트를 전등 타입으로 켜고 끕니다. D12 버튼은 bayo.tv 재생 신호도 보냅니다.
 *
 *  Taeo Lee <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *  MIT License
 */


#include "WiFiS3.h"
#include <Arduino.h>
#include <IOSignal.h>
#include <string.h>
#include <Bounce2.h>
#include "Arduino_LED_Matrix.h"

#define BUTTON_PIN    D12

Bounce2::Button aBtn = Bounce2::Button();
ArduinoLEDMatrix matrix;
WiFiClient client;
IOSignal io;
const char *deviceName = "UnoR4-WiFi:HOME";  // $name "name:category" (device rules v2)
const char *deviceUi = "t=light.1";      // $ui: the official type

void publishState() { io.signal("@$state", digitalRead(LED_BUILTIN) ? "on=1" : "on=0"); }

int lastButtonState = HIGH; 

const uint32_t heart[] = { 0x3184a444, 0x44042081, 0x100a0040 };
const uint32_t fullOff[] = { 0, 0, 0 };

void deviceOn(){
  digitalWrite(LED_BUILTIN, HIGH);
  matrix.loadFrame(heart);
  io.signal("@$state", "on=1" );
}

void deviceOff(){
  digitalWrite(LED_BUILTIN, LOW);
  matrix.loadFrame(fullOff);
  io.signal("@$state", "on=0" );
}

void deviceToggle(){
  int nextState = !digitalRead(LED_BUILTIN);
   if( nextState){
      deviceOn();
   }else{
      deviceOff();
   }
}


void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  aBtn.attach( BUTTON_PIN, INPUT_PULLUP ); 
  aBtn.interval(5); // debounce interval in milliseconds
  aBtn.setPressedState(LOW); 
  
  WiFi.begin("WIFI_SSID", "WIFI_PASS");
  Serial.begin(115200);
  matrix.begin();
  matrix.loadFrame(heart);
  delay(500);

  io.setRxBuffer( 200 );
  io.begin( &client, "io.remocon.kr", 55488 );
  io.onReady( &onReady );
  io.onMessage( &onMessage );
  // io.auth("ID_KEY"); // Optional: use your own device key.

}


void loop() {
  
    io.loop();
    aBtn.update();
    if ( aBtn.pressed() ) {
      Serial.println(F("pressed"));
      deviceToggle();
      io.signal("#screen", "playToggle" );
      // io.signal("#screen", "next" );
    }  


}


void onReady()
{
  Serial.print("onReady cid: ");
  Serial.println( io.cid );
  io.subscribe("#search");
  io.signal("@$name", deviceName);
  io.signal("@$ui", deviceUi);
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

