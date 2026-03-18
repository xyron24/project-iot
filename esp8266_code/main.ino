#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#define PIR D2
#define RELAY D1
#define LDR A0
#define DHTPIN D6
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "Vaibhav's Wifi";
const char* password = "21345678";  
const char* mqtt_server = "dev.coppercloud.in";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMotionTime = 0;
unsigned long holdTime = 3000;

bool manualMode = false;
bool relayState = false;  

// timing for sensor update
unsigned long lastLoop = 0;
const long interval = 2000;


// ================= WIFI =================
void connectWiFi() {

  Serial.print("Connecting WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
}


// ================= MQTT CALLBACK =================
void callback(char* topic, byte* payload, unsigned int length) {

  String message = "";

  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message arrived: ");
  Serial.println(message);

  if (String(topic) == "home/relay/control") {

    if (message == "ON") {
      manualMode = true;
      relayState = true;
    }
    else if (message == "OFF") {
      manualMode = true;
      relayState = false;
    }
    else if (message == "AUTO") {
      manualMode = false;
      // Force timer to expire immediately
      lastMotionTime = millis() - holdTime;
      Serial.println("Auto mode enabled");
    }
  }
}


// ================= MQTT RECONNECT =================
void reconnectMQTT() {

  while (!client.connected()) {

    Serial.print("Connecting MQTT...");

    if (client.connect(("NodeMCU_" + String(ESP.getChipId())).c_str())) {

      Serial.println("Connected");

      client.subscribe("home/relay/control");
    } 
    else {

      Serial.print("Failed rc=");
      Serial.print(client.state());
      Serial.println(" retrying...");
      delay(2000);
    }
  }
}


// ================= SETUP =================
void setup() {

  Serial.begin(9600);

  pinMode(PIR, INPUT);
  pinMode(RELAY, OUTPUT);

  digitalWrite(RELAY, HIGH);

  dht.begin();

  connectWiFi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}


// ================= LOOP =================
void loop() {

  if (!client.connected()) {
    reconnectMQTT();
  }

  client.loop();

  // ===== FAST RELAY UPDATE =====
  digitalWrite(RELAY, relayState ? LOW : HIGH);


  // ===== SENSOR LOGIC EVERY 2 SEC =====
  if (millis() - lastLoop >= interval) {

    lastLoop = millis();

    int motion = digitalRead(PIR);

    if (motion == HIGH) {
      lastMotionTime = millis();
    }

    bool humanPresent = (millis() - lastMotionTime < holdTime);

    int ldrValue = analogRead(LDR);
    float lightPercent = (ldrValue / 1023.0) * 100.0;

    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    if (!manualMode) {

      if (humanPresent && lightPercent <= 70.0) {
        relayState = true;
      }
      else {
        relayState = false;
      }
    }

    // Publish
    char ldrPayload[20];
    dtostrf(lightPercent, 4, 2, ldrPayload);
    client.publish("home/ldr", ldrPayload);

    client.publish("home/pir", humanPresent ? "1" : "0");

    char dhtPayload[50];
    sprintf(dhtPayload, "{\"temp\":%.2f,\"hum\":%.2f}", temp, hum);
    client.publish("home/dht", dhtPayload);

    client.publish("home/relay", relayState ? "ON" : "OFF");

    Serial.print("ManualMode: ");
    Serial.print(manualMode);
    Serial.print(" Relay: ");
    Serial.println(relayState ? "ON" : "OFF");
  }
}
