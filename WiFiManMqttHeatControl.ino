// Automated Heat Controller - James Brandenburg 04-23-2019
#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <DHTesp.h>

int powerLED = D5;           //connect green power led to D5
int wifiLED = D6;            //connect blue wifi led  to D6
int lowHeatLED = D7;         //connect yellow low heat led to D7
int highHeatLED = D10;        //connect red high heat led to D8
int powerRelay = D4;          //connect relay1 to D1
int heatRelay = D3;          //connect relay2 to D2
int triggerWifiSetup;
char* heatStatus = "Off";
char* prevHeatStatus;
bool initialConfig = false;
bool wifiConnected = false;
float setTempTo = 61.00;
float outsideTemp = 30.00;
float humidity;
float temp;
float prevTemp;
float prevHumidity;

DHTesp dht;

const char* CONFIG_FILE = "/config.json";
bool readConfigFile();
bool writeConfigFile();

const char* mqttServer = "192.168.1.110";
const int mqttPort = 1883;
const char* mqttDevName = "bedRoomHeater";
const char* mqttUser = "blim";
const char* mqttPassword = "blimpass";
const char* topicOne = "rv/outside/weather/temperature";
const char* topicTwo = "rv/bedRoom/heater/setTempTo";
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  delay(10);
  setup_Hardware();
  setup_FS();
  setup_WiFi();
  setup_MQTT();
}

void loop() {
  if (!wifiConnected)  {
    if (initialConfig) {
      Serial.println("Configuration portal requested");
      digitalWrite(wifiLED, LOW);
      ledControl(powerLED, "flash");
      WiFiManager wifiManager;
      wifiManager.setConfigPortalTimeout(60);
      if (!wifiManager.startConfigPortal()) {
        Serial.println("Not connected to WiFi but continuing anyway.");
        initialConfig = false;
      } else {
        // If you get here you have connected to the WiFi
        Serial.println("Connected...yeey :)");
      }
      writeConfigFile();
      ESP.restart();
      delay(5000);
    }
  }
  delay(2000);
  humidity = dht.getHumidity();
  temp = ((dht.getTemperature() * 1.8) + 32);
  Serial.println (temp);
  heatControl(temp, setTempTo, outsideTemp);
  if (wifiConnected) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    publishStatus();
  }
}

void setup_Hardware() {
  dht.setup(D8, DHTesp::DHT11);       //connect dht11 sensor to D3
  pinMode(powerLED, OUTPUT);
  pinMode(wifiLED, OUTPUT);
  pinMode(lowHeatLED, OUTPUT);
  pinMode(highHeatLED, OUTPUT);
  pinMode(powerRelay, OUTPUT);
  pinMode(heatRelay, OUTPUT);
  pinMode(triggerWifiSetup, INPUT_PULLUP);
  digitalWrite(powerLED, HIGH);
  digitalWrite(wifiLED, LOW);
  digitalWrite(lowHeatLED, LOW);
  digitalWrite(highHeatLED, LOW);
  digitalWrite(powerRelay, LOW);
  digitalWrite(heatRelay, HIGH);
}

void setup_FS() {
  bool result = SPIFFS.begin();
  Serial.println("SPIFFS opened: " + result);
  if (!readConfigFile()) {
    Serial.println("Failed to read configuration file, using default values");
  }
}

void setup_WiFi() {
  if (WiFi.SSID() == "") {
    Serial.println("We haven't got any access point credentials, so get them now");
    initialConfig = true;
  } else {
    WiFi.mode(WIFI_STA);
    unsigned long startedAt = millis();
    Serial.print("After waiting ");
    int connRes = WiFi.waitForConnectResult();
    float waited = (millis() - startedAt);
    Serial.print(waited / 1000);
    Serial.print(" secs in setup() connection result is ");
    Serial.println(connRes);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect, finishing setup anyway");
    digitalWrite(wifiLED, LOW);
    wifiConnected = false;
  } else {
    digitalWrite(wifiLED, HIGH);
    wifiConnected = true;
    Serial.print("Local ip: ");
    Serial.println(WiFi.localIP());
  }
}

void setup_MQTT() {
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void reconnect() {
  while (!client.connected()) {
    digitalWrite(wifiLED, LOW);
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqttDevName, mqttUser, mqttPassword)) {
      Serial.print("Connected");
      digitalWrite(wifiLED, HIGH);
      Serial.println();
      client.subscribe(topicOne);
      client.subscribe(topicTwo);
    } else {
      digitalWrite(wifiLED, LOW);
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void publishStatus() {
  if (heatStatus != prevHeatStatus) {
    client.publish("rv/bedRoom/heater/onOffStatus", heatStatus, true);
  }
  if (temp != prevTemp) {
    client.publish("rv/bedRoom/heater/temperature", String(temp).c_str(), true);
  }
  if (humidity != prevHumidity) {
    client.publish("rv/bedRoom/heater/humidity", String(humidity).c_str(), true);
  }
  prevHeatStatus = heatStatus;
  prevTemp = temp;
  prevHumidity = humidity;
}

void callback(char* topic, byte* payload, unsigned int length) {
  ledControl(wifiLED, "flash");
  Serial.print("Message recieved: ");
  if (!strcmp(topic, topicOne)) {
    String msg = "";
    for (int i = 0; i < length; i++) {
      msg += (char)payload[i];
    }
    outsideTemp =  msg.toFloat();
    Serial.print("Outside Temp is: ");
    Serial.println(outsideTemp);
  }
  if (!strcmp(topic, topicTwo)) {
    String msg = "";
    for (int i = 0; i < length; i++) {
      msg += (char)payload[i];
    }
    setTempTo =  msg.toFloat();
    Serial.print("Temp set to: ");
    Serial.println(setTempTo);
  }
}

void heatControl( float sensorTemp, float setTemp, float outTemp) {
  if (outTemp <= 50.00) {
    if (sensorTemp >= (setTemp + 1)) {
      digitalWrite(powerRelay, LOW);
      digitalWrite(heatRelay, HIGH);
      digitalWrite(lowHeatLED, LOW);
      digitalWrite(highHeatLED, LOW);
      heatStatus = "Off";
    } else {
      digitalWrite(powerRelay, HIGH);
      digitalWrite(heatRelay, LOW);
      digitalWrite(lowHeatLED, LOW);
      digitalWrite(highHeatLED, HIGH);
      heatStatus = "On";
    }
  } else if (sensorTemp >= (setTemp + 1)) {
    digitalWrite(powerRelay, LOW);
    digitalWrite(heatRelay, HIGH);
    digitalWrite(lowHeatLED, LOW);
    digitalWrite(highHeatLED, LOW);
    heatStatus = "Off";
  } else {
    digitalWrite(powerRelay, HIGH);
    digitalWrite(heatRelay, HIGH);
    digitalWrite(lowHeatLED, HIGH);
    digitalWrite(highHeatLED, LOW);
    heatStatus = "On";
  }
}

void ledControl(int ledName, char* ledAction) {
  if (ledAction == "flash") {
    int currentLedStatus = ledName;
    delay(500);
    digitalWrite(ledName, LOW);
    delay(500);
    digitalWrite(ledName, HIGH);
    delay(500);
    digitalWrite(ledName, LOW);
    delay(500);
    digitalWrite(ledName, HIGH);
    ledName = currentLedStatus;
  }
}

bool readConfigFile() {
  // this opens the config file in read-mode
  File f = SPIFFS.open(CONFIG_FILE, "r");
  if (!f) {
    Serial.println("Configuration file not found");
    return false;
  } else {
    // we could open the file
    size_t size = f.size();
    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size]);

    // Read and store file contents in buf
    f.readBytes(buf.get(), size);
    // Closing file
    f.close();
    // Using dynamic JSON buffer which is not the recommended memory model, but anyway
    // See https://github.com/bblanchon/ArduinoJson/wiki/Memory%20model
    DynamicJsonBuffer jsonBuffer;
    // Parse JSON string
    JsonObject& json = jsonBuffer.parseObject(buf.get());
    // Test if parsing succeeds.
    if (!json.success()) {
      Serial.println("JSON parseObject() failed");
      return false;
    }
    json.printTo(Serial);

    //    // Parse all config file parameters, override
    //    // local config variables with parsed values
    //    if (json.containsKey("thingspeakApiKey")) {
    //      strcpy(thingspeakApiKey, json["thingspeakApiKey"]);
    //    }
    //
    //    if (json.containsKey("sensorDht22")) {
    //      sensorDht22 = json["sensorDht22"];
    //    }
    //
    //    if (json.containsKey("pinSda")) {
    //      pinSda = json["pinSda"];
    //    }
    //
    //    if (json.containsKey("pinScl")) {
    //      pinScl = json["pinScl"];
    //    }
  }
  Serial.println("\nConfig file was successfully parsed");
  return true;
}

bool writeConfigFile() {
  Serial.println("Saving config file");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  //  // JSONify local configuration parameters
  //  json["thingspeakApiKey"] = thingspeakApiKey;
  //  json["sensorDht22"] = sensorDht22;
  //  json["pinSda"] = pinSda;
  //  json["pinScl"] = pinScl;

  // Open file for writing
  File f = SPIFFS.open(CONFIG_FILE, "w");
  if (!f) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.prettyPrintTo(Serial);
  // Write data to file and close it
  json.printTo(f);
  f.close();

  Serial.println("\nConfig file was successfully saved");
  return true;
}
