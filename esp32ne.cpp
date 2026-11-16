#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

const int MQ9_pin = 34;
const float RL = 10.0;
float R0 = 10.0;

#define F22_pin 35
#define dry_soil 3200
#define wet_soil 1200
const int dht_pin = 27;
DHT dht(dht_pin, DHT22);

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "";

WiFiClient espClient;
PubSubClient client(espClient);

int gas = 0, soil_raw = 0, moist = 0;
float rs = 0, airRatio = 0, humd = NAN, temp = NAN;
bool isTempNOTok = false, isHumdNOTok = false, isAirNOTok = false;
unsigned long lastRead = 0;

float readRs() {
  long sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(MQ9_pin);
  float v = (sum / 16.0f) * 3.3f / 4095.0f;
  if (v < 0.01f) v = 0.01f;
  return RL * (3.3f - v) / v;
}

void readSensors() {
  gas = analogRead(MQ9_pin);
  rs = readRs();
  soil_raw = analogRead(F22_pin);
  moist = constrain(map(soil_raw, dry_soil, wet_soil, 0, 100), 0, 100);
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Fail to load DHT"));
    return;
  }
  humd = h;
  temp = t;
  airRatio = rs / R0;
  isTempNOTok = temp <= 24 || temp >= 32;
  isHumdNOTok = humd <= 70 || humd >= 85;
  isAirNOTok = airRatio <= 0.7;

  char buf[256];
  snprintf(buf, sizeof(buf),
    "{\"gas\":%d,\"rs\":%.2f,\"ratio\":%.3f,\"soil\":%d,\"moist\":%d,\"humd\":%.1f,\"temp\":%.1f,"
    "\"tempBad\":%s,\"humdBad\":%s,\"airBad\":%s}",
    gas, rs, airRatio, soil_raw, moist, humd, temp,
    isTempNOTok ? "true" : "false", isHumdNOTok ? "true" : "false", isAirNOTok ? "true" : "false");

  Serial.print("Publish: ");
  Serial.println(buf);
  client.publish(mqtt_topic, buf);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("MQTT connecting...");
    String clientId = "esp32-wokwi-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry in 2s");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  dht.begin();

  WiFi.begin("#WifiName", "#WifiPassword", 6);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.print("\nIP: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, mqtt_port);

  Serial.println("Setup MQ9...");
  delay(3000);
  R0 = readRs();
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  if (millis() - lastRead >= 1000) {
    lastRead = millis();
    readSensors();
  }
}