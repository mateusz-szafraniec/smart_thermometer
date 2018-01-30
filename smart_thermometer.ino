#include "config.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#define LED_R 15
#define LED_G 12
#define LED_B 13

#define ONE_WIRE_BUS 4 //marked (wrong) as GPIO5 on dev board

ESP8266WiFiMulti WiFiMulti;
OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);
float temperature;
HTTPClient http;

void setup(void)
{
  // start serial port
  Serial.begin(74880);
  Serial.println(F("IoT temperature sensor"));
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(LED_R, OUTPUT);
  setRGBLed(255, 255, 255);
  WiFi.mode(WIFI_STA);
  //http.setReuse(true);
  WiFiMulti.addAP(_SSID, _PASSWORD);

  sensors.begin();
  setRGBLed(0, 0, 0);
}

void loop(void)
{
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  Serial.print(F("Requesting temperatures..."));
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println(F("DONE"));
  temperature = sensors.getTempCByIndex(0);

  if (temperature < -120)
  {
    while (temperature < -120)
    {
      sensors.requestTemperatures();
      temperature = sensors.getTempCByIndex(0);
      ESP.wdtFeed(); // feed the dog
    }
  }
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
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    Serial.println(F("Connected"));
    http.begin(ADDRESS); //HTTP
#ifdef USER
    http.setAuthorization(USER, PSSWD);
#endif
    http.addHeader("Content-Type", "text/plain");
    int httpCode = http.POST(String(temperature, 3));
    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println(payload);
      }
    } else {
      Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }  else {
    Serial.println(F("not connected !!!"));
  }
  delay(5000);
}

void setRGBLed(int R, int G, int B) {
  if (R > 1023) R = 1023;
  if (G > 1023) G = 1023;
  if (B > 1023) B = 1023;
  analogWrite(LED_R, R);
  analogWrite(LED_G, G);
  analogWrite(LED_B, B);
}
