#include "measurement_store.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#define SENSORS_PIN D7

OneWire oneWire1(SENSORS_PIN);
DallasTemperature sensors(&oneWire1);

const int numSensors = 4;
const int maxRecords = 7000;

const int INTERVAL_ADDRESS_IN_EEPROM = 0;
const int  MAX_TEMPERATURE_DIFF_IN_EEPROM = 4;

unsigned long lastMillis = 0;
int interval = 10000;
float temp_diff = 0.2;

unsigned long blink = 0;
int blink_type = 0;

const char* ssid = "YOUR-SSID";
const char* password = "YOUR-PASSWORD";

ESP8266WebServer server(80);
WiFiUDP udp;
const unsigned int udpPort = 4210;
unsigned long lastUdpMillis = 0;

unsigned int sensor_ids[] = {1, 2, 3, 4};

int readFromEEPROM(int address) {
  int value;
  EEPROM.get(address, value);
  return value;
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void handleBlink() {
  if (millis() - blink >= 100) {
    blink = millis();
    if (blink_type == 1) {
      digitalWrite(LED_BUILTIN, HIGH);
      blink_type = 0;
    } else {
      digitalWrite(LED_BUILTIN, LOW);
      blink_type = 1;
    }
  }
}

void updateSensorReadings(unsigned long millis_now) {
  if ((millis_now - lastMillis) >= (unsigned long)interval) {
    lastMillis = millis_now;

    sensors.requestTemperatures();
    for (unsigned int i = 0; i < numSensors; i++) {
      float temperature = sensors.getTempCByIndex(i);
      unsigned int sensor_id = sensor_ids[i];
      saveTemperature(temperature, millis_now, sensor_id, temp_diff);
    }
  }
}


void handleWebServer() {
  server.on("/status", []() {
    server.send(200, "text/html", "OK");
  });

  server.on("/setinterval", []() {
    int newInterval = server.arg("interval").toInt();
    if (newInterval > 0 && newInterval <= 60000) {
      interval = newInterval;
      EEPROM.put(INTERVAL_ADDRESS_IN_EEPROM, interval);
      EEPROM.commit();
      Serial.printf("Interval updated to %d milliseconds\n", interval);
      server.send(200, "text/html", "<p>Interval set</p>");
    } else {
      server.send(400, "text/html", "<p>Invalid interval</p>");
    }
  });

  server.on("/setTempDiff", []() {
    float new_temp_diff = server.arg("difference").toFloat();
    if (new_temp_diff > 0 && new_temp_diff <= 2) {
      temp_diff = new_temp_diff;
      EEPROM.put(MAX_TEMPERATURE_DIFF_IN_EEPROM, new_temp_diff);
      EEPROM.commit();
      Serial.printf("Temp diff updated to %f C\n", new_temp_diff);
      server.send(200, "text/html", "<p>Difference set</p>");
    } else {
      server.send(400, "text/html", "<p>Invalid differnce</p>");
    }
  });

  server.on("/exit", []() {
    Serial.println("Restarting device...");
    server.send(200, "text/html", "Exiting");
    ESP.restart();
  });

  server.on("/temp", []() {
    String sensor_id = server.arg("sensor_id");
    int limit = server.arg("limit").toInt();
    unsigned long currentMillis = millis();
    unsigned long time_frame = 0;
    bool filterByTime = server.hasArg("time");
    unsigned long requestTime = server.arg("time").toInt();

    if (filterByTime) {
      time_frame = currentMillis - requestTime;
    } else {
      time_frame = 0;
    }

    if (limit <= 0) {
      limit = 10;
    }

    if (limit > 100) {
      limit = 100;
    }

    String response = "{\"temperature_data\":[";
    int count = 0;
    int totalMatched = 0;

    for (int i = 1; i < maxRecords; i++) {

      SensTempTime temperatureData = getTemp(i);

      bool isWithinTimeFrame = !filterByTime || temperatureData.time <= time_frame;
      bool matchesSensorId = sensor_id.isEmpty() || temperatureData.sensor_id == sensor_id.toInt();

      if (temperatureData.sensor_id != 0 && isWithinTimeFrame && matchesSensorId && i != 0) {
        totalMatched++;
        if (count < limit) {
          if (count > 0) {
            response += ",";
          }
          response += "{\"t\":" + String(temperatureData.temp, 2) + ",\"ti\":" + String(temperatureData.time != 0 ? currentMillis - temperatureData.time : 0 ) + ",\"id\":" + temperatureData.sensor_id + "}";
          count++;
        }
      }
    }

    int remainCount = totalMatched - count;
    response += "],\"remain\":" + String(remainCount) + "}";
    server.send(200, "application/json", response);
  });

  server.begin();
}

void handleUDP() {
  if (millis() - lastUdpMillis > 1000) {
    int packetSize = udp.parsePacket();
    if (packetSize) {
      char packetBuffer[255];
      int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
      packetBuffer[len] = '\0';

      if (len > 0) {
        String packetString = String(packetBuffer);

        if (packetString.indexOf("DISCOVER") >= 0) {
          if (packetString.indexOf(WiFi.localIP().toString()) < 0) {
            IPAddress remoteIp = udp.remoteIP();
            Serial.print("Device with IP: ");
            Serial.print(remoteIp);
            Serial.println(" is attempting to connect...");

            udp.beginPacket(remoteIp, udpPort);
            String message = "'DEVICE' ," + String(numSensors) + ", [";
            for (int i = 0; i < numSensors; i++) {
              message += sensor_ids[i];
              if (i < numSensors - 1) message += ", ";
            }
            message += "]";
            udp.print(message);
            udp.endPacket();
            Serial.println("Response packet sent");
          }
        }
      } else {
        Serial.println("Error reading UDP packet");
      }
    }
    lastUdpMillis = millis();
  }
}

void setup() {
  pinMode(SENSORS_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  initBuff(maxRecords);

  sensors.begin();
  Serial.begin(9600);
  EEPROM.begin(512);

  EEPROM.get(INTERVAL_ADDRESS_IN_EEPROM, interval);
  EEPROM.get(MAX_TEMPERATURE_DIFF_IN_EEPROM, temp_diff);

  if (interval < 1000 || interval > 60000) {
    interval = 10000;
    EEPROM.put(INTERVAL_ADDRESS_IN_EEPROM, interval);
    EEPROM.commit();
    Serial.println("Interval set to default value: 10000 milliseconds");
  } else {
    Serial.printf("\nCurrent interval is %d milliseconds\n", interval);
  }

  if (temp_diff < 0.1 || temp_diff > 2.0) {
    temp_diff = 0.2;
    EEPROM.put(MAX_TEMPERATURE_DIFF_IN_EEPROM, temp_diff);
    EEPROM.commit();
    Serial.println("Temp diff set to default value: 0.2 C");
  } else {
    Serial.printf("Current temp diff is %f C\n", temp_diff);
  }

  connectToWiFi();
  udp.begin(udpPort);
  handleWebServer();
}


void loop() {
  server.handleClient();
  handleBlink();
  updateSensorReadings(millis());
  handleUDP();
}
