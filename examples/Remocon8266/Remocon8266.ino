/*
 *  IOSignal Example. 
 *  Remocon8266 board
 
 *  Lee Dongeun <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *
 *  MIT License
 */


#include <ESP8266WiFiMulti.h>
#include <IOSignal.h>
#include <Bounce2.h>
#include <U8g2lib.h>
#include <Wire.h>

// Buttons
#define BTN_UP     16   // D0  SW1 *EXTERNAL HARDWARE PULLUP
#define BTN_LEFT    0   // D3  SW2
#define BTN_DOWN   14   // D5  SW3
#define BTN_RIGHT  13   // D7  SW4

//I2C  OLED & else
#define PIN_SDA     4   // D2
#define PIN_SDL     5   // D1

//OUTPUT
#define OUT1       15   // D8  LED, NeoPixel, Servo or else.
#define OUT2       12   // D6 

ESP8266WiFiMulti wifiMulti;
WiFiClient client;
IOSignal io;

const char *name = "Remocon8266:HOME";
const char *ui = "L,A,B";
char states[] = "100";
char last_states[] = "100";
uint8_t pinMap[] = { LED_BUILTIN , OUT1, OUT2 };

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// Define button objects
Bounce2::Button upBtn = Bounce2::Button();
Bounce2::Button leftBtn = Bounce2::Button();
Bounce2::Button downBtn = Bounce2::Button();
Bounce2::Button rightBtn = Bounce2::Button();

void stateChange(int i){
  //local state
  char state = states[i];
  if( state != last_states[i] ){
    digitalWrite( pinMap[i] , states[i] - '0' );
    last_states[i] = state;
  }
  Serial.print("states: ");
  Serial.println( states);

  //publish device states
  io.signal("@$state", states );

  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "Remote Info");
  u8g2.setCursor(0, 25);
  u8g2.print("CID: ");
  u8g2.print( io.cid);
  u8g2.setCursor(0, 40);
  u8g2.print("States: ");
  u8g2.print( states);
  u8g2.sendBuffer();
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


void check_events(void) {
  upBtn.update();
  leftBtn.update();
  downBtn.update();
  rightBtn.update();
}

void handle_events(void) {
  // 0 = not pushed, 1 = pushed
  if (upBtn.pressed()) {
    Serial.println("up");
    io.signal("#screen","playToggle");
  } else if (leftBtn.pressed()) {
    Serial.println("left");
    io.signal("#screen","left");
  } else if (downBtn.pressed()) {
    Serial.println("down");
    io.signal("#screen","down");
  } else if (rightBtn.pressed()) {
    Serial.println("right");
    io.signal("#screen","right");
    // io.signal("cid@","D");
  }
}

void u8g2_prepare(void) {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

// logo
uint16_t p[][2] = {
  { 42, 32 },
  { 61, 32 },
  { 67, 7 },
  { 76, 17 },
  { 66, 56 },
  { 76, 46 }
};

void draw_logo() {
  // 128x64 frame
  u8g2.drawTriangle(p[0][0], p[0][1], p[1][0], p[1][1], p[2][0], p[2][1]);
  u8g2.drawTriangle(p[1][0], p[1][1], p[2][0], p[2][1], p[3][0], p[3][1]);
  u8g2.drawTriangle(p[0][0], p[0][1], p[1][0], p[1][1], p[4][0], p[4][1]);
  u8g2.drawTriangle(p[1][0], p[1][1], p[4][0], p[4][1], p[5][0], p[5][1]);
  u8g2.drawDisc(84, 30, 5, U8G2_DRAW_ALL);
}

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // active low 
  pinMode(OUT1, OUTPUT);
  digitalWrite(OUT1, LOW); 
  pinMode(OUT2, OUTPUT);
  digitalWrite(OUT2, LOW); 

  upBtn.attach(BTN_UP, INPUT); // external pullup
  upBtn.interval(5);           
  upBtn.setPressedState(LOW);  

  leftBtn.attach(BTN_LEFT, INPUT_PULLUP); 
  leftBtn.interval(5);              
  leftBtn.setPressedState(LOW);     

  downBtn.attach(BTN_DOWN, INPUT_PULLUP);
  downBtn.interval(5);           
  downBtn.setPressedState(LOW);  

  rightBtn.attach(BTN_RIGHT, INPUT_PULLUP);
  rightBtn.interval(5);           
  rightBtn.setPressedState(LOW);  

  Serial.begin(115200);
  Serial.flush();

  u8g2.begin();
  u8g2_prepare();

  u8g2.clearBuffer(); 
    draw_logo();
    u8g2.sendBuffer();
    delay(800);
 
  u8g2.clearBuffer(); 
    u8g2.setFont(u8g2_font_profont22_mf);
    u8g2.drawStr(0, 20, "REMOCON.KR");
    u8g2.sendBuffer();
    delay(2000);

  // u8g2.setFont(u8g2_font_ncenB08_tr);  // choose a suitable font
  u8g2.setFont(u8g2_font_t0_13_mr);  // choose a suitable font
  u8g2.clearBuffer(); 
    u8g2.drawStr(0, 20, "Connecting..");
    u8g2.sendBuffer();
  
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP( "WIFI_SSID", "WIFI_PASS");
  wifiMulti.addAP( "twesomego", "qwer1234");  
  // You can add multiple APs.  
  Serial.println();
  Serial.println();
  Serial.print("Wait for WiFi... ");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  delay(500);

  io.setRxBuffer( 200 );
  io.begin( &client , "io.remocon.kr", 55488);
  io.onReady( &onReady );
  io.onMessage( &onMessage );
  // io.auth( "ID_KEY" ); 

}

  
void loop() {

    uint8_t conditionCode = io.update();
    if(conditionCode != 0 ){ 
      Serial.print("C");
      Serial.println( conditionCode );
        return;
    } 

    check_events();
    handle_events();
    
}




void onReady()
{
  Serial.print("onReady cid: ");
  Serial.println( io.cid );
  io.signal("@$state", states );
  io.signal("@$ui", ui );
  io.signal("@$name", name );
  io.signal("#notify", io.cid );
  io.subscribe("#search");
  
  u8g2.clearBuffer();
    u8g2.setCursor(0, 0);
    u8g2.print("Ready CID: ");
    u8g2.print(io.cid);
    u8g2.drawStr(0, 15, "remote control:");
    u8g2.drawStr(0, 30, "https://remocon.kr");
    u8g2.drawStr(0, 45, "IOSignal IoT");
    u8g2.sendBuffer();
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

  if( payloadType == IOSignal::PAYLOAD_TYPE::TEXT ){  
    Serial.print("string payload: " );
    Serial.println( (char *)payload  );
  }

  if( strcmp(tag, "#search") == 0){
    io.signal( "#notify", io.cid );
  }

  if( strcmp(tag, "@ui") == 0){
    io.signal2( (char *)payload, "@ui", io.cid , ui );
  }

  if( strcmp(tag, "@msg") == 0){

    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "msg");
    u8g2.setCursor(0, 25);
    u8g2.print( (char *)payload );
    u8g2.sendBuffer();

  }
      
  if( strcmp(tag, "@") == 0){
    if( strcmp((char *)payload, "L") == 0){
      toggle(0);
    }else if( strcmp((char *)payload, "A") == 0){
      toggle(1);
    }else if( strcmp((char *)payload, "B") == 0){
      toggle(2);
    }
  }
      
}
