#ifdef ESP8266 || ESP32
  #define ISR_PREFIX ICACHE_RAM_ATTR
#else
  #define ISR_PREFIX
#endif

#if !(defined(ESP_NAME))
  #define ESP_NAME "relay"
#endif

#include <Arduino.h>

/* WiFi */
#include <ESP8266WiFi.h> // WIFI support
#include <ArduinoOTA.h> // Updates over the air
char hostname[32] = {0};

/* mDNS */
#include <ESP8266mDNS.h>

/* WiFi Manager */
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> 

/* MQTT */
#include <PubSubClient.h>
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// LED
int ledState = LOW;
unsigned long ledNextRun = 0;

/* Misc */
unsigned long triggerTimeout = 0;

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println(F("Config Mode"));
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  digitalWrite(LED_BUILTIN, LOW);
}

void reconnect() {
  Serial.print("MQTT Connecting");
  while (!mqtt.connected()) {
    if (mqtt.connect(hostname)) {
      Serial.println();
      Serial.println("MQTT connected");
      mqtt.subscribe(hostname);
    } else {
      Serial.print(".");
      delay(1000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message arrived");
  Serial.print("  ");
  Serial.print(topic);
  Serial.print(" ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if ((char)payload[0] == '0') {
    digitalWrite(D1, LOW);
  }
  else if ((char)payload[0] == '1') {
    digitalWrite(D1, HIGH);
  }
  else {
    digitalWrite(D1, HIGH);
    triggerTimeout = millis() + 1000; // Number of milliseconds to activate relay.
  }
}

void setup() {
  /* LED */
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // off

  /* Serial */
  Serial.begin(9600);
  Serial.println(F("Setup..."));
  
  /* WiFi */
  sprintf(hostname, "%s-%06X", ESP_NAME, ESP.getChipId());
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  if(!wifiManager.autoConnect(hostname)) {
    Serial.println(F("WiFi Connect Failed"));
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  Serial.println(hostname);
  Serial.print(F("  "));
  Serial.println(WiFi.localIP());
  Serial.print(F("  "));
  Serial.println(WiFi.macAddress());

  /* OTA */
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("End"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) { Serial.println("Auth Failed"); } 
    else if (error == OTA_BEGIN_ERROR) { Serial.println("Begin Failed"); } 
    else if (error == OTA_CONNECT_ERROR) { Serial.println("Connect Failed"); } 
    else if (error == OTA_RECEIVE_ERROR) { Serial.println("Receive Failed"); } 
    else if (error == OTA_END_ERROR) { Serial.println("End Failed"); }
  });
  ArduinoOTA.begin();

  /* MQTT */

  // Discover MQTT broker via mDNS
  Serial.print(F("Finding MQTT Server"));
  while (MDNS.queryService("mqtt", "tcp") == 0) {
    delay(1000);
    Serial.print(F("."));
    ArduinoOTA.handle();
  }
  Serial.println();

  Serial.println(F("MQTT: "));
  Serial.print(F("  "));
  Serial.println(MDNS.hostname(0));
  Serial.print(F("  "));
  Serial.print(MDNS.IP(0));
  Serial.print(F(":"));
  Serial.println(MDNS.port(0));

  mqtt.setServer(MDNS.IP(0), MDNS.port(0));
  mqtt.setCallback(callback);

  /* Pins */
  pinMode(D1, OUTPUT);
  digitalWrite(D1, LOW);
}

void loop() {
  /* OTA */
  ArduinoOTA.handle();

  /* MQTT */
  if (!mqtt.connected())
  {
    reconnect();
  }
  mqtt.loop();

  /* mDNS */
  MDNS.update();

  if (triggerTimeout != 0 && triggerTimeout < millis()) {
    digitalWrite(D1, LOW);
    triggerTimeout = 0;
  }

  // LED
  unsigned long currentMillis = millis();
  if (ledNextRun < currentMillis) {
    if (ledState == LOW) {
      ledState = HIGH;
      ledNextRun = currentMillis + 1000;
    } else {
      ledState = LOW;
    }
    digitalWrite(LED_BUILTIN, ledState);
  }
}