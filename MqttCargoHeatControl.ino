// Automated Heat Controller with repot to MQTT broker  James Brandenburg 04-08-19

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>

  int powerRelay = D4;          //connect relay1 to D2
  char* heatStatus = "Off";
  char* prevHeatStatus;
  float humidity;
  float temp;
  float prevTemp;
  float prevHumidity;

  DHTesp dht;

  const char* ssid = "**********"; // edit this
  const char* password = "**********"; // edit this
  int wifiConnected = 0;
  const char* mqttServer = "broker.mqtt.server.com"; // edit this
  const int mqttPort = 1883; // edit this
  const char* mqttUser = "userName"; // edit this
  const char* mqttPassword = "password"; // edit this
  
  WiFiClient espClient;
  PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  delay(10);

  dht.setup(D3,DHTesp::DHT11);        //connect dht11 sensor to D1

  pinMode(powerRelay, OUTPUT);
  digitalWrite(powerRelay, LOW);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);   
  IPAddress ip(192, 168, 1, 112); // edit this
  IPAddress gateway(192,168,1,1); // edit this
  IPAddress subnet(255,255,255,0); // edit this
  WiFi.config(ip, gateway, subnet);
  setup_WiFi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
}

void loop() {
  humidity = dht.getHumidity();
  temp = ((dht.getTemperature()*1.8)+32);
  if (WiFi.status() != WL_CONNECTED) {
    setup_WiFi();
  }
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  if (temp >= 37) {
    digitalWrite(powerRelay, LOW);
    heatStatus = "Off";
  } else {
      digitalWrite(powerRelay, HIGH);
      heatStatus = "On";
    }
  if (heatStatus != prevHeatStatus) {
    client.publish("rv/cargo/heater/onOffStatus", heatStatus, true);
  }
  if (temp != prevTemp) {  
    client.publish("rv/cargo/heater/temperature", String(temp).c_str(), true);
  }
  if (humidity != prevHumidity) {  
    client.publish("rv/cargo/heater/humidity", String(humidity).c_str(), true);
  }
  prevHeatStatus = heatStatus;
  prevTemp = temp;
  prevHumidity = humidity;
  delay(5000);
}

void setup_WiFi() {
  Serial.print("Connecting to WiFi.");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }  
  Serial.print("Connected");
  Serial.println();
  wifiConnected = 1;
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("cargoHeater", "blim", "blimpass")) {
      Serial.print("Connected");
      Serial.println();
      if (heatStatus != prevHeatStatus) {
        client.publish("rv/cargo/heater/onOffStatus", heatStatus, true);
      }
      if (temp != prevTemp) {  
        client.publish("rv/cargo/heater/temperature", String(temp).c_str(), true);
      }
      if (humidity != prevHumidity) {  
        client.publish("rv/cargo/heater/humidity", String(humidity).c_str(), true);
      }      
      client.subscribe("rv/outside/weather/temperature");
    } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length){
}
