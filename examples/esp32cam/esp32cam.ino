#include <WiFi.h>
#include "esp_camera.h"
#include <IOSignal.h>

// ====== 사용자 설정 ======
const char* ssid     = "WIFI_SSID";
const char* password = "WIFI_PASS";

// 전역 TCP 클라이언트
WiFiClient client;
IOSignal io;
// ====== ESP32-CAM (AI Thinker) 핀 설정 ======
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


// ====== 카메라 초기화 함수 ======
void camera_setup() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    config.fb_location = CAMERA_FB_IN_PSRAM; 
    // 캡처 해상도
    config.frame_size = FRAMESIZE_VGA;  // 640x480
    config.jpeg_quality = 10;           // 낮을수록 고화질(용량 큼)
    config.fb_count = 1;

    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Camera init failed!");
        while (true);
    }
}



void sendFrameSignal() {

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }

    io.signal("#capture_image", fb->buf , fb->len );
    io.signal_e2e("#e2e_cam", fb->buf , fb->len , "XjK6608lG9ihqW8IrfLq" );

    Serial.print("Sent frame (");
    Serial.print(fb->len);
    Serial.println(" bytes)");

    esp_camera_fb_return(fb);
}


// ====== SETUP ======
void setup() {
    Serial.begin(115200);

    // WiFi 연결
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nWiFi connected.");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

    // 카메라 초기화
    camera_setup();

    // 서버 연결
    io.setRxBuffer( 200 );
    io.begin( &client , "io.iosignal.net", 55488);  
    // io.begin( &client , "192.168.0.204", 55488);
    io.onReady( &onReady );
    io.onMessage( &onMessage );

}


void onReady()
{
  Serial.print("onReady cid: ");
  Serial.println( io.cid );

}

void onMessage( char *tag, uint8_t payloadType, uint8_t* payload, size_t payloadSize)
{

  Serial.print(">> signal tag: " );
  Serial.print( tag );
  Serial.print(" type: " );
  Serial.print( payloadType );
  Serial.print(" size: " );
  Serial.println( payloadSize );
   
}

const int UTC9 = 3600 * 9; // KR
// ====== LOOP ======
uint32_t last = 0;
void loop() {
    io.loop();
    if( millis() - last >= 100 ){
        last = millis();
        sendFrameSignal();
        boho_print_time( io.getUnixTime()+ UTC9, io.getMilTime());
    }
}
