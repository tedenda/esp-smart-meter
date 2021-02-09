#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <SoftwareSerial.h>
#include "HanReader.h"
#include "web/AmsWebServer.h"
#include "hw/chipSetup.h"

SoftwareSerial swSer1;

AmsWebServer webServer;
HanReader hanReader(&swSer1);

void setup() {
  Serial.setRxBufferSize(DLMS_READER_BUFFER_SIZE);     
  Serial.begin(115200);

  swSer1.begin(115200, SWSERIAL_8N1, D5, D6, false, DLMS_READER_BUFFER_SIZE);

  pinMode(TRIGGER_PIN, INPUT);      
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output

  // Mount filesystem
  if (!LittleFS.begin()) {
    Serial.println("Failed to start filesystem");
    LittleFS.end();
    delay(2000);
    ESP.reset();
  }

  LittleFS.remove("/log.txt");

  // Setup wifi and webserver
  wifiSetup();
  webServer.setup();
  ArduinoOTA.begin();
  hanReader.saveData();
}

void loop()
{
  static uint32_t lastUpdate = 0xFFFFFFFF; // Test wraparound at startup instead of waiting 50 days
  static uint16_t scheduleState = 0;
  static bool dataRecieved = false;
  unsigned long now = millis();
  // Reading serial data should be uninterrupted
  // When a serial read is deteced other stuff is delayed 13 ms
  // using a timeout not divisible by 1000 (even) and a prime number
  // reduce the risk of having the server working while data is recieved
  if (hanReader.read())
  {
    lastUpdate = now;
    dataRecieved = true;
  }
  else
  {
    if (now - lastUpdate > 13)
    {
      lastUpdate = now;
      if (dataRecieved)
      {
        hanReader.saveData();
        dataRecieved = false;
      }
      else
      {
        switch (scheduleState++)
        {
        case 0:
          resetChipOnTrigger();
          break;
        case 1:
          MDNS.update();
          break;
        case 2:
          webServer.loop();
          break;
        case 3:
          ArduinoOTA.handle();
          break;
        default:
          yield();
          scheduleState = 0;
          break;
        }
      }
    }
  }
}