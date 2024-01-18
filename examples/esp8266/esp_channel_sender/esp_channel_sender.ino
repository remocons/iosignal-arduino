/*
  ESP8266 General or D1 mini + Button

  Channel Message Publisher (Sender) Example
  1. connect to the server.
  2. Press a button to send a message to the "#homeButton" channel.
  3. Devices that have subscribed to the "#homeButton" channel receive the message.

  채널 메시지 발행자(송신자) 예제
  1. 서버에 접속합니다.
  2. 버튼을 누르면 "#homeButton" 채널에 메시지를 전송합니다.
  3. "#homeButton" 채널을 구독(subscribe)한 장치가 메시지를 수신합니다.

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
  io.begin( &client , "io.remocon.kr", 55488);
}

  
void loop() {
    io.loop();
    downBtn.update();
    if (downBtn.pressed()) {
        Serial.println("down");
        io.signal("#homeButton","down");
    }     
}

