/*
 *  IOSignal Remocon example: the simplest Wi-Fi client (ESP8266, ESP32)
 *
 *  Joins the shared switch group #switch (switch3.1) and prints the
 *  shared state to serial (115200). No other hardware is needed.
 *  remocon.kr shows the group as one "공유 스위치" card.
 *
 *  Optional hardware, set as board settings (no rebuild; the board restarts to apply a pin):
 *    out       a pin that follows s1: an LED, or a relay   (-1: none)
 *    out_high  1: on is HIGH (most relay modules), 0: on is LOW (on-board LEDs)
 *    button    a pin with a button to GND that toggles s1  (-1: none)
 *  For example: ESP-01 relay board out=0 out_high=1, ESP-01 button button=0 (or 3 on a board whose
 *  button is on RX), D1 mini LED and button out=2 button=14, ESP32-C3 Super Mini LED out=8.
 *
 *  Setup: upload, then set Wi-Fi and these settings over USB on https://arduino.iosignal.net/wifi/
 *  (the board keeps them), or write WIFI_ID and WIFI_KEY below. Then open https://remocon.kr on the
 *  same router. The same page can set another IOSignal server and a device key.
 *
 *  [kr] 공유 스위치 그룹(#switch)에 참여해 상태를 시리얼로 출력합니다. 하드웨어 없이 통신만 확인합니다.
 *       보드 설정 out(출력 핀, s1을 따름), out_high(켜짐이 HIGH인지), button(버튼 핀, s1 토글)을 넣으면
 *       LED·릴레이·버튼 보드가 됩니다. 예: ESP-01 릴레이 out=0 out_high=1, D1 mini out=2 button=14.
 *
 *  Taeo Lee <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *  MIT License
 */

#if !defined(ESP8266) && !defined(ESP32)
 #error "Select an ESP8266 or ESP32 board for this example."
#endif
#include <IOSignal.h>
#include <IOSignalConfig.h>
#include <Bounce2.h>
#include <string.h>
#include <stdlib.h>
#include <IOSignalUi.h>
#define WIFI_ID  "WIFI_SSID"
#define WIFI_KEY "WIFI_PASS"
#define SERVER_URL "io.remocon.kr"
#define SERVER_PORT 55488
#define SWITCH_CH "#switch"
#define SWITCH_STATE SWITCH_CH "$state"

WiFiClient client;
IOSignal io;
IOSignalConfig config;
// $name, the device name and its app tab ("name:category"): board setting 'name'. Empty uses the sketch's
// default; without ':' there is no category (미분류). A change is sent at once.
IOSignalSetting nameSetting(config, "name", "Name & category (name:category)", "공유 스위치:HOME");
// Optional output and button (pins, -1 for none). Read at boot: a change restarts the board.
IOSignalSetting outSetting(config, "out", "Output pin, follows s1 (-1: none)", "-1", IOSignalSetting::INT, IOSignalSetting::RESTART);
IOSignalSetting outHighSetting(config, "out_high", "Output on is HIGH (relay 1, LED 0)", "0", IOSignalSetting::BOOL, IOSignalSetting::RESTART);
IOSignalSetting buttonSetting(config, "button", "Button pin, toggles s1 (-1: none)", "-1", IOSignalSetting::INT, IOSignalSetting::RESTART);

int outPin = -1, buttonPin = -1;
bool outHigh = false;
Bounce2::Button button;
char lastState[48] = "";  // the group's last $state, kept to toggle s1 and keep s2, s3

void sendInfo() {
  // $name "name:category" (the board setting 'name'); a group's $ui is the extended form (t=…).
  char name[64];
  remocon::formatNameSetting(name, sizeof(name), nameSetting.value(), "공유 스위치");
  io.signal(SWITCH_CH "$name", name);
  io.signal(SWITCH_CH "$ui", "t=switch3.1");
}

void setOutput(bool on) {
  if (outPin >= 0) digitalWrite(outPin, on == outHigh ? HIGH : LOW);
}

// A button press: the group's state with s1 flipped (s2, s3 kept), or "s1=1;s2=0;s3=0" before any state.
void toggleS1() {
  char next[48];
  const char* s1 = strstr(lastState, "s1=");
  if (!lastState[0] || !s1) {
    strcpy(next, "s1=1;s2=0;s3=0");
  } else {
    snprintf(next, sizeof(next), "%s", lastState);
    char* v = next + (s1 - lastState) + 3;
    *v = (*v == '1') ? '0' : '1';
  }
  Serial.print("BUTTON "); Serial.println(next);
  io.signal(SWITCH_STATE, next);
}

void setup() {
  Serial.begin(115200);
  // Wi-Fi, server and device key: set over USB, or the sketch values.
  config.begin( Serial, "Wi-Fi Simple", WIFI_ID, WIFI_KEY );
  nameSetting.maxLength(64);
  nameSetting.onChange([](const char*) { if (io.state == IO_READY) sendInfo(); });
  outSetting.range(-1, 39);
  buttonSetting.range(-1, 39);
  outPin = atoi(outSetting.value());
  outHigh = strcmp(outHighSetting.value(), "1") == 0;
  buttonPin = atoi(buttonSetting.value());
  if (outPin >= 0) { pinMode(outPin, OUTPUT); setOutput(false); }
  if (buttonPin >= 0) {
    button.attach(buttonPin, INPUT_PULLUP);
    button.interval(10);
    button.setPressedState(LOW);
  }
  config.setServer( SERVER_URL, SERVER_PORT );
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
  if ( buttonPin >= 0 ) {
    button.update();
    if ( button.pressed() && io.state == IO_READY ) toggleS1();
  }
}

void onReady()
{
  Serial.print("onReady cid: "); Serial.println( io.cid );
  io.subscribe( "#search" );
  io.subscribe( SWITCH_STATE );
  sendInfo();
  io.signal( "#notify", SWITCH_CH );
}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{
  Serial.print(">> signal tag: " ); Serial.print( tag );
  Serial.print(" type: " ); Serial.print( payloadType );
  Serial.print(" size: " ); Serial.println( payloadSize );
  if( strcmp(tag, "#search") == 0 ) io.signal( "#notify", SWITCH_CH );
  if( payloadType == IOSignal::PAYLOAD_TYPE::TEXT && payload && payloadSize && memchr(payload, 0, payloadSize) ){
    Serial.print("string payload: " ); Serial.println( (char *)payload  );
    if( strcmp( tag, SWITCH_STATE ) == 0 ){
      snprintf( lastState, sizeof(lastState), "%s", (const char*)payload );
      // Read only this device's key, and keep the output if the key is missing.
      const char* value = strstr( lastState, "s1=" );
      if( value ) setOutput( value[3] == '1' );
    }
  }
}
