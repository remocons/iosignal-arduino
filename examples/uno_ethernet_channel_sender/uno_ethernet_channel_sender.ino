
/*
 Arduino Uno + Ethernet shield W5100 + Button

  https://github.com/remocons/iosignal-arduino
  
 */

#include <SPI.h>
#include <Ethernet.h>
#include <string.h>
#include <IOSignal.h>
#include <Bounce2.h>

#define BUTTON_PIN  2

// If you have multiple devices, you'll need to change the MAC address.
byte mac[]{0, 0, 0, 0, 0, 0x07}; 
EthernetClient client;
IOSignal io;

Bounce2::Button downBtn = Bounce2::Button();

void setup() {
  downBtn.attach(BUTTON_PIN, INPUT_PULLUP);
  downBtn.interval(5);           
  downBtn.setPressedState(LOW);  

  Serial.begin(115200);
  Serial.println(F("Init.."));

  Ethernet.init(10);
  Ethernet.begin(mac); // DHCP
  Serial.print(F("IP:"));
  Serial.println(Ethernet.localIP());
  
  io.setRxBuffer( 80 );
  io.begin( &client , "192.168.0.204", 55488);
  // io.begin( &client , "io.remocon.kr", 55488);  

}


void loop() {
  io.loop();
  downBtn.update();
  if (downBtn.pressed()) {
    Serial.println("down");
    io.signal("#homeButton","down");
  }     
}

