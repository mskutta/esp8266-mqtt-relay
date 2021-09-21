#ifdef ESP8266 || ESP32
  #define ISR_PREFIX ICACHE_RAM_ATTR
#else
  #define ISR_PREFIX
#endif

#if !(defined(ESP_NAME))
  #define ESP_NAME "relay"
#endif

#include <Arduino.h>

#include <ESP8266WiFi.h> // WIFI support
//#include <ESP8266mDNS.h> // For network discovery
//#include <WiFiUdp.h> // OSC over UDP
#include <ArduinoOTA.h> // Updates over the air

// WiFi Manager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> 

// MQTT
#include <PubSubClient.h>

// LED
#include <Ticker.h>
Ticker ticker;
int ledState = LOW;
unsigned long ledNextRun = 0;

/* WIFI */
char hostname[32] = {0};

/* MQTT */
WiFiClient wifiClient;
PubSubClient client(wifiClient);
const char* broker = "10.81.95.165";

/* Misc */
unsigned long triggerTimeout = 0;

void tick()
{
  //toggle state
  int state = digitalRead(LED_BUILTIN);  // get the current state of GPIO1 pin
  digitalWrite(LED_BUILTIN, !state);     // set pin to the opposite state
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println(F("Config Mode"));
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
  ticker.attach(0.2, tick);
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("MQTT Connecting...");
    if (client.connect(hostname)) {
      Serial.println("MQTT connected");
      client.subscribe(ESP_NAME); // "relay/#"
    } else {
      Serial.print(".");
      delay(1000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

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
  
  /* Serial and I2C */
  Serial.begin(9600);

  /* LED */
  pinMode(LED_BUILTIN, OUTPUT);

  /* Function Select */
  Serial.println(ESP_NAME);
  
  /* WiFi */
  sprintf(hostname, "%s-%06X", ESP_NAME, ESP.getChipId());
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  if(!wifiManager.autoConnect(hostname)) {
    Serial.println("WiFi Connect Failed");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  Serial.println(hostname);
  Serial.print(F("  "));
  Serial.print(WiFi.localIP());
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

  pinMode(D1, OUTPUT);
  digitalWrite(D1, LOW);

  /* MQTT */
  client.setServer(broker, 1883);
  client.setCallback(callback);
}

void loop() {
  ArduinoOTA.handle();
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

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