#include "config.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>

//WiFiServer server(80);
ESP8266WebServer HTTP(80);

#define LED_R 15
#define LED_G 12
#define LED_B 13
#define LED_CONFIG 2

#define ONE_WIRE_BUS 4 //marked (wrong) as GPIO5 on dev board

OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);
float temperature;
HTTPClient httpclient;
WiFiManager wifiManager;

//for LED status
#include <Ticker.h>
Ticker ticker;

void tick()
{
  //toggle state
  int state = digitalRead(LED_CONFIG);  // get the current state of GPIO1 pin
  digitalWrite(LED_CONFIG, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

long lastUpdated;
void setup(void)
{
  // start serial port
  Serial.begin(74880);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  Serial.println("IoT temperature sensor");
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_CONFIG, OUTPUT);

  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);
  //reset settings - for testing
  //wifiManager.resetSettings();
  setRGBLed(255, 255, 255);
  sensors.begin();
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setMinimumSignalQuality(10);
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "SmartThermometer"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("SmartThermometer")) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  Serial.println(WiFi.localIP());
  ticker.detach();
  //keep LED on
  digitalWrite(LED_CONFIG, LOW);
  setRGBLed(0, 0, 0);
  
  if (!MDNS.begin("thermometer")) {
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("mDNS responder started");

  HTTP.on("/index.html", HTTP_GET, []() {
    String temp;
    temp = "Temperature: ";
    temp = temp + String(temperature, 3);
    HTTP.send(200, "text/plain", temp);
  });
  HTTP.on("/", HTTP_GET, []() {
    String temp;
    temp = "Temperature: ";
    temp = temp + String(temperature, 3);
    HTTP.send(200, "text/plain", temp);
  });
  HTTP.on("/reset", HTTP_GET, []() {
    HTTP.send(200, "text/plain", "OK!");
    ESP.reset();
  });
  HTTP.on("/factory", HTTP_GET, []() {
    HTTP.send(200, "text/plain", "OK!");
    wifiManager.resetSettings();
    ESP.reset();
  });
  HTTP.on("/configure", HTTP_GET, []() {
    HTTP.send(200, "text/plain", "OK!");
    if (!wifiManager.startConfigPortal("OnDemandAP")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
  });
  HTTP.on("/description.xml", HTTP_GET, []() {
    SSDP.schema(HTTP.client());
  });
  
  // Start TCP (HTTP) server
  HTTP.begin();
  Serial.println("TCP server started");

  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);

  Serial.printf("Starting SSDP...\n");
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName("Smart Thermometer");
  SSDP.setSerialNumber("001");
  SSDP.setURL("index.html");
  SSDP.setModelName("ST001");
  SSDP.setModelNumber("001");
  SSDP.setManufacturer("Mateusz Szafraniec");
  SSDP.setManufacturerURL("http://szafraniec.net.pl");
  SSDP.setDeviceType("upnp:rootdevice");
  SSDP.begin();
}

void loop(void)
{
  if ((millis() - lastUpdated) > 10000) {
    // call sensors.requestTemperatures() to issue a global temperature
    // request to all devices on the bus
    Serial.print(F("Requesting temperatures..."));
    sensors.requestTemperatures(); // Send the command to get temperatures
    Serial.println(F("DONE"));
    temperature = sensors.getTempCByIndex(0);

    /*
    if (temperature < -120)
    {
      while (temperature < -120)
      {
        sensors.requestTemperatures();
        temperature = sensors.getTempCByIndex(0);
        ESP.wdtFeed(); // feed the dog
      }
    }
    */
    
    Serial.printf("Temperature: %0.3f \r\n", temperature);

    if (temperature < 36)
    {
      setRGBLed(0, 0, 1023);
    } else if (temperature > 37.5)
    {
      setRGBLed(1023, 0, 0);
    } else if ((temperature > 36) && (temperature < 37.5))
    {
      setRGBLed(0, 1023, 0);
    }

    // wait for WiFi connection
    if ((WiFi.status() == WL_CONNECTED)) {
      Serial.println(F("Connected"));
      httpclient.begin(ADDRESS); //HTTP
#ifdef USER
      httpclient.setAuthorization(USER, PSSWD);
#endif
      httpclient.addHeader("Content-Type", "text/plain");
      int httpCode = httpclient.POST(String(temperature, 3));
      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] POST... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK) {
          String payload = httpclient.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTP] POST... failed, error: %s\n", httpclient.errorToString(httpCode).c_str());
      }
      httpclient.end();
    }  else {
      Serial.println(F("not connected !!!"));
    }
    lastUpdated = millis();
  }
  delay(1);
  HTTP.handleClient();
  delay(1);
}

void setRGBLed(int R, int G, int B) {
  if (R > 1023) R = 1023;
  if (G > 1023) G = 1023;
  if (B > 1023) B = 1023;
  analogWrite(LED_R, R);
  analogWrite(LED_G, G);
  analogWrite(LED_B, B);
}
