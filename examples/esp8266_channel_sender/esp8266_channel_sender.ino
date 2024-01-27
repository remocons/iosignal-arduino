/*
  ESP8266 General or D1 mini + Button

  https://github.com/remocons/iosignal-arduino
  
 */

#include <ESP8266WiFi.h>
#include <IOSignal.h>
#include <Bounce2.h>

#define BUTTON_PIN   14 // D5 (D1 mini )

WiFiClient client;
IOSignal io;
Bounce2::Button downBtn = Bounce2::Button();

void setup() {
  downBtn.attach(BUTTON_PIN, INPUT_PULLUP);
  downBtn.interval(5);           
  downBtn.setPressedState(LOW);  

  Serial.begin(115200);
  Serial.println();
  Serial.print("Connecting... ");

  WiFi.mode(WIFI_STA);
  WiFi.begin("WIFI_SSID", "WIFI_PASS");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  io.setRxBuffer( 200 );
  io.begin( &client , "192.168.0.204", 55488);
  // io.begin( &client , "io.remocon.kr", 55488);  
}

  
void loop() {
    io.loop();
    downBtn.update();
    if (downBtn.pressed()) {
        Serial.println("down");
        io.signal("#homeButton", "esp8266");
    }     
}

