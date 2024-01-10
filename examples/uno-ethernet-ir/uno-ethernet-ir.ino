/*
 *  IOSignal Example. 
 *  Arduino Uno + Ethernet shield W5100 + IR Receiver
 * 
 *  Lee Dongeun <sixgen@gmail.com>
 *  https://github.com/remocons/iosignal-arduino
 *
 *  I used a library for a low-capacity NEC-compatible remote.
 *  If you are using a different remote, change the library
 *
 *  MIT License
 */


#include <Arduino.h>
#define IR_INPUT_PIN 3 // do not use 4,10,11,12(sd,spi)
#define NO_LED_FEEDBACK_CODE
#include "TinyIRReceiver.hpp" 
#include <SPI.h>
#include <Ethernet.h>
#include <string.h>
#include <IOSignal.h>

volatile struct TinyIRReceiverCallbackDataStruct sCallbackData;

// If you have multiple devices, you'll need to change the MAC address.
byte mac[]{0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x06};
EthernetClient client;
IOSignal io;
const char *name = "UnoR3-Eth-IR:HOME";
const char *ui = "on,off,toggle";

void setup()
{
  pinMode(5, OUTPUT); // LED
  pinMode(6, OUTPUT); // Relay
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);

  Serial.begin(115200);
  initPCIInterruptForTinyReceiver();

  Ethernet.init(10);
  Ethernet.begin(mac); // DHCP
  Serial.println(F("Init.."));
  Serial.print(F("IP:"));
  Serial.println(Ethernet.localIP());

  io.setRxBuffer(80);
  io.begin(&client, "io.remocon.kr", 55488 );
  io.onReady(&onReady);
  io.onMessage(&onMessage);
  // io.auth( "id_key" );

}

void deviceOn()
{
  digitalWrite(5, HIGH);
  digitalWrite(6, HIGH);
  io.signal("@$state", "on");
}

void deviceOff()
{
  digitalWrite(5, LOW);
  digitalWrite(6, LOW);
  io.signal("@$state", "off");
}

void deviceToggle()
{
  int nextState = !digitalRead(5);
  digitalWrite(5, nextState);
  digitalWrite(6, nextState);
  if (nextState)
  {
    io.signal("@$state", "on");
  }
  else
  {
    io.signal("@$state", "off");
  }
}

void loop()
{
  
    uint8_t conditionCode = io.update();
    if(conditionCode != 0 ){ 
        Serial.print("C");
        Serial.println( conditionCode);
        return;
    } 

    if (sCallbackData.justWritten)
    {
      sCallbackData.justWritten = false;

      if (sCallbackData.Flags == IRDATA_FLAGS_IS_REPEAT)
      {
        // Serial.println(F("Repeat"));
      }
      else
      {
        Serial.print(F("A="));
        Serial.print(sCallbackData.Address, HEX);
        Serial.print(F(" C="));
        Serial.println(sCallbackData.Command, HEX);

        switch (sCallbackData.Command)
        {
        // Change to the code value of your NEC compatible remote
        case 0x01 :
          io.signal("#screen", "next");
          break;
        case 0x08 :
          io.signal("#screen", "prev");
          break;
        case 0x05 :
          io.signal("#screen", "up");
          break;
        case 0 :
          io.signal("#screen", "down");
          break;
        case 0x1F :
          deviceToggle(); 
          break;
        case 0x1E :
          io.signal("cid1@", "toggle");
          break;
        case 0x1A :
          io.signal("cid2@", "toggle");
          break;
        default:
          io.signal("#screen", "playToggle");
        }
      }
    }

}

void onReady()
{
  Serial.print(F("onReady cid: "));
  Serial.println(io.cid);
  io.signal("@$state", "off");
  io.signal("@$name", name );
  io.signal("@$ui", ui);
  io.signal("#notify", io.cid);
  io.subscribe("#search");
}

void onMessage(char *tag, uint8_t payloadType, uint8_t *payload, size_t payloadSize)
{
  // signal message info
  Serial.print(F(">> signal tag: "));
  Serial.print(tag);
  Serial.print(F(" type: "));
  Serial.print(payloadType);
  Serial.print(F(" size: "));
  Serial.println(payloadSize);

  if (payloadType == IOSignal::PAYLOAD_TYPE::TEXT)
  {
    Serial.print(F("string payload: "));
    Serial.println((char *)payload);
  }

  if (strcmp(tag, "#search") == 0)
  {
    io.signal("#notify", io.cid);
  }

  if (strcmp(tag, "@ui") == 0)
  {
    io.signal2((char *)payload, "@ui", io.cid, ui);
  }

  if (strcmp(tag, "@") == 0)
  {
    if (strcmp((char *)payload, "on") == 0)
    {
      deviceOn();
    }
    else if (strcmp((char *)payload, "off") == 0)
    {
      deviceOff();
    }
    else if (strcmp((char *)payload, "toggle") == 0)
    {
      deviceToggle();
    }
  }
}

void handleReceivedTinyIRData(uint8_t aAddress, uint8_t aCommand, uint8_t aFlags)
{
  sCallbackData.Address = aAddress;
  sCallbackData.Command = aCommand;
  sCallbackData.Flags = aFlags;
  sCallbackData.justWritten = true;
}