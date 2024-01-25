/*
  Arduino Uno R3 WiFi + Button (external)

  https://github.com/remocons/iosignal-arduino
  
 */

#include "WiFiS3.h"
#include <Arduino.h>
#include <IOSignal.h>
#include "Arduino_LED_Matrix.h"
#include <Bounce2.h>

#define BUTTON_PIN D12

ArduinoLEDMatrix matrix;
WiFiClient client;
IOSignal io;
Bounce2::Button downBtn = Bounce2::Button();

void setup() {
  downBtn.attach(BUTTON_PIN, INPUT_PULLUP);
  downBtn.interval(5);           
  downBtn.setPressedState(LOW);  
  
  Serial.begin(115200);
  int status = WL_IDLE_STATUS;
  while (status != WL_CONNECTED) {
    Serial.print("WiFi Connecting...");
    status = WiFi.begin("WIFI_SSID", "WIFI_PASS");
    delay(10000);
  }

  io.setRxBuffer( 200 );
  io.begin( &client , "192.168.0.204", 55488);
  // io.begin( &client , "io.remocon.kr", 55488);  
  io.onReady( &onReady );
}



void onReady()
{
  Serial.print("onReady cid: ");
  Serial.println( io.cid );
}

void loop() {
    io.loop();
    downBtn.update();
    if (downBtn.pressed()) {
        Serial.println("down");
        io.signal("#homeButton","down");
    }     

}
