// Automated Heat Controller - James Brandenburg 03-20-19

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>

  int powerLED = D5;           //connect green power led to D5
  int wifiLED = D6;            //connect blue wifi led  to D6
  int lowHeatLED = D7;         //connect yellow low heat led to D7
  int highHeatLED = D10;        //connect red high heat led to D8
  int powerRelay = D4;          //connect relay1 to D1
  int heatRelay = D3;          //connect relay2 to D2
  char* heatStatus = "Off";
  char* prevHeatStatus;
  int wifiConnected = 0;
  float setTempTo = 61.00;
  float outsideTemp = 30.00;
  float humidity;
  float temp;
  float prevTemp;
  float prevHumidity;
  
  DHTesp dht;

  const char* ssid = "***********"; // edit this
  const char* password = "***********"; // edit this

  const char* mqttServer = "broker.mqtt.server.com"; // edit this
  const int mqttPort = 1883; // edit this
  const char* mqttUser = "userName"; // edit this
  const char* mqttPassword = "password"; // edit this
  const char* topicOne = "rv/outside/weather/temperature";
  const char* topicTwo = "rv/bedRoom/heater/setTempTo";
  WiFiClient espClient;
  PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  delay(10);

  dht.setup(D8,DHTesp::DHT11);        //connect dht11 sensor to D3

  pinMode(powerLED, OUTPUT);
  pinMode(wifiLED, OUTPUT);
  pinMode(lowHeatLED, OUTPUT);
  pinMode(highHeatLED, OUTPUT);  
  pinMode(powerRelay, OUTPUT);
  pinMode(heatRelay, OUTPUT); 
  
  digitalWrite(powerLED, HIGH);
  digitalWrite(wifiLED, LOW);
  digitalWrite(lowHeatLED, LOW);
  digitalWrite(highHeatLED, LOW);
  digitalWrite(powerRelay, LOW);
  digitalWrite(heatRelay, HIGH);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);   
  IPAddress ip(192, 168, 1, 111); // edit this
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
  
  if (outsideTemp <= 50.00){
    if (temp >= setTempTo) {
      digitalWrite(powerRelay, LOW);
      digitalWrite(heatRelay, HIGH);
      digitalWrite(lowHeatLED, LOW);
      digitalWrite(highHeatLED, LOW);
      heatStatus = "Off";
    } else {
        digitalWrite(powerRelay, HIGH);
        digitalWrite(heatRelay, HIGH);
        digitalWrite(lowHeatLED, LOW);
        digitalWrite(highHeatLED, HIGH);
        heatStatus = "On";
      }
  } else if (temp >= setTempTo){
      digitalWrite(powerRelay, LOW);
      digitalWrite(heatRelay, HIGH);
      digitalWrite(lowHeatLED, LOW);
      digitalWrite(highHeatLED, LOW);
      heatStatus = "Off";
    } else { 
        digitalWrite(powerRelay, HIGH);
        digitalWrite(heatRelay, LOW);
        digitalWrite(lowHeatLED, HIGH);
        digitalWrite(highHeatLED, LOW);
        heatStatus = "On";
      }
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

void setup_WiFi() {
  Serial.print("Connecting to WiFi.");
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(wifiLED, LOW); 
    delay(500);
    Serial.print(".");
  }  
  digitalWrite(wifiLED, HIGH);
  Serial.print("Connected");
  Serial.println();
  wifiConnected = 1;
}

void reconnect() {
  while (!client.connected()) {
    digitalWrite(wifiLED, LOW);
    Serial.print("Attempting MQTT connection...");
    if (client.connect("bedRoomHeater", "blim", "blimpass")) {
      Serial.print("Connected");
      digitalWrite(wifiLED, HIGH);
      Serial.println();
      client.publish("rv/bedRoom/heater/onOffStatus", heatStatus, true);
      client.publish("rv/bedRoom/heater/temperature", String(temp).c_str(), true);
      client.publish("rv/bedRoom/heater/humidity", String(humidity).c_str(), true);
      client.subscribe("rv/outside/weather/temperature");
      client.subscribe("rv/bedRoom/heater/setTempTo");
    } else {
        digitalWrite(wifiLED, LOW);
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        delay(5000);
      }
  }
}

void callback(char* topic, byte* payload, unsigned int length){
  digitalWrite(wifiLED, LOW);
  delay(500);
  digitalWrite(wifiLED, HIGH);
  delay(500);
  digitalWrite(wifiLED, LOW);
  delay(500);
  digitalWrite(wifiLED, HIGH); 
  Serial.print("Message recieved: ");
  if (!strcmp(topic, topicOne)) {
    String msg = "";
    for (int i = 0; i < length; i++){
      msg += (char)payload[i];
    }
    outsideTemp =  msg.toFloat();
    Serial.print("Outside Temp is: ");
    Serial.println(outsideTemp);
  }    
  if (!strcmp(topic, topicTwo)) {
    String msg = "";
    for (int i = 0; i < length; i++){
      msg += (char)payload[i];
    }
    setTempTo =  msg.toFloat();
    Serial.print("Temp set to: ");
    Serial.println(setTempTo);
  }    
}
