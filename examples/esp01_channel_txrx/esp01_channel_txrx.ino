/*
  ESP01

  https://github.com/remocons/iosignal-arduino

 */

#include <ESP8266WiFi.h>
#include <IOSignal.h>
#include <Bounce2.h>

#define LED_PIN    1  // esp01 built-in LED
#define BUTTON_PIN   3 // Rx , external button

WiFiClient client;
IOSignal io;
Bounce2::Button downBtn = Bounce2::Button();

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // active low 
  downBtn.attach(BUTTON_PIN, INPUT_PULLUP);
  downBtn.interval(5);           
  downBtn.setPressedState(LOW);  

  WiFi.mode(WIFI_STA);
  WiFi.begin("WIFI_SSID", "WIFI_PASS");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  io.setRxBuffer( 200 );
  io.begin( &client , "io.iosignal.net", 55488);  
  // io.begin( &client , "192.168.0.204", 55488);
  io.onReady( &onReady );
  io.onMessage( &onMessage );
}


void onReady()
{
  io.subscribe("#homeButton");
}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{

  if( strcmp(tag, "#homeButton") == 0){
    if( digitalRead(LED_PIN ) == HIGH ){
      digitalWrite( LED_PIN , LOW );
    }else{
      digitalWrite( LED_PIN , HIGH );
    }
  }
   
}


void loop() {
    io.loop();
    downBtn.update();
    if (downBtn.pressed()) {
        io.signal("#homeButton","esp01");
    }    
}