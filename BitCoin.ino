/****************************
Name:         Seamus de Cleir
Date:         18/10/2017
Description:  This program uses the ESP2866 and MAX2719 to display current Bitcoin
              prices every 12 hours.
License:      MIT License
******************************/

#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>
#include <SPI.h>
#include "LedControl.h"

// Coin Desk website
#define COIN_DESK "api.coindesk.com"

// Wifi Login in
const char* ssid     = "your_ssid";
const char* password = "your_password";

// Times for delay
const unsigned long SECOND = 1000;
const unsigned long HOUR = 3600*SECOND;

// GPIO pins used mapped for the NodeMCU ESP8266 version 2
// pin D5 is connected to the DataIn
// pin D7 is connected to the CLK
// pin D6 is connected to LOAD
LedControl lc=LedControl(D5,D7,D6,1);

void setup()
{
  // Outputs serial to 115200 for debugging
  Serial.begin(115200);

  // Initialize the MAX7219 device
  lc.shutdown(0,false);   // Enable display
  lc.setIntensity(0,15);  // Set brightness level (0 is min, 15 is max)
  lc.clearDisplay(0);     // Clear display register

  // Connects to wifi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

// Used to store the JSON file from Coin Base
static char jsonBuf[4096];

// showBusTimes function
bool showBitcoinPrice(char *json);

void loop()
{
  // Connect through port 443
  const int httpsPort = 443;
  const char* fingerprint = "CF 05 98 89 CA FF 8E D8 5E 5C E0 C2 E4 F7 E6 C3 C7 50 DD 5C";


  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure httpclient;
  Serial.print("connecting to ");
  Serial.println(COIN_DESK);
  if (!httpclient.connect(COIN_DESK, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  // Requests URL data
  httpclient.print(String("GET ") + "/v1/bpi/currentprice.json" + " HTTP/1.1\r\n" +
               "Host: " + COIN_DESK + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
  // Collect http response headers and content from Coin Base
  // HTTP headers are discarded.
  // The content is formatted in JSON and is left in jsonBuf.
  int jsonLen = 0;
  bool skip_headers = true;
  while (httpclient.connected() || httpclient.available()) {
    if (skip_headers) {
      String aLine = httpclient.readStringUntil('\n');
      //Serial.println(aLine);
      // Blank line denotes end of headers
      if (aLine.length() <= 1) {
        skip_headers = false;
      }
    }
    else {
      int bytesIn;
      bytesIn = httpclient.read((uint8_t *)&jsonBuf[jsonLen], sizeof(jsonBuf) - jsonLen);
      Serial.print(F("bytesIn ")); Serial.println(bytesIn);
      if (bytesIn > 0) {
        jsonLen += bytesIn;
        if (jsonLen > sizeof(jsonBuf)) jsonLen = sizeof(jsonBuf);
      }
      else if (bytesIn < 0) {
        Serial.print(F("read error "));
        Serial.println(bytesIn);
      }
    }
    delay(1);
  }
  httpclient.stop();

  // Switches the LED screen on
  lc.shutdown(0, false);

  // Starts the showBitcoinPrice function
  showBitcoinPrice(jsonBuf);

  // Leaves the LED screen on for 12 hours and then updates the price
  delay(HOUR*12);
}

bool showBitcoinPrice(char *json)
{
  StaticJsonBuffer<3*1024> jsonBuffer;

  // Skip characters until first '{' found
  char *jsonStart = strchr(json, '{');

  // Checks to see if jsonStart is empty
  if (jsonStart == NULL) {
    Serial.println(F("JSON data missing"));
    return false;
  }

  json = jsonStart;

  // Parse json file
  JsonObject& root = jsonBuffer.parseObject(json);

  // Throws an error if json is not properly parsed
  if (!root.success()) {
    Serial.println(F("jsonBuffer.parseObject() failed"));
    return false;
  }

  // Extracts the data in Euros
  String bitcoin = root["bpi"]["EUR"]["rate_float"];
  Serial.println(bitcoin);

  // Prints to screen
  for(int i = 0;i < bitcoin.length(); i++) {

      // Breaks if we reach a decimal point (bit lazy)
      if(bitcoin[i]=='.'){break;}

      lc.setChar(0, 7-i, bitcoin[i], false);
    }


  return true;
}
