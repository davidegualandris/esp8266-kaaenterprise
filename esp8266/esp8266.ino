#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

const char* ssid = "Guala's iPhone";        // WiFi name
const char* password = "davide123";    // WiFi password
const char* mqtt_server = "mqtt.cloud.kaaiot.com";
const String TOKEN = "esp8266_2";        // Endpoint token - you get (or specify) it during device provisioning
const String APP_VERSION = "btngtro547tsntf25rtg-v1";  // Application version - you specify it during device provisioning

const unsigned long sendingPeriod = 1 * 1 * 1000UL;
static unsigned long lastPublish = 0 - sendingPeriod;

WiFiClient espClient;
PubSubClient client(espClient);

// DHT sensor
#define DHTPIN D1        // sensor I/O pin, eg. D1 (D0 and D4 are already used by board LEDs)
#define DHTTYPE DHT11    // sensor type DHT 11 

// Initialize DHT sensor
DHT dht = DHT(DHTPIN, DHTTYPE);

void setup() {
  dht.begin();
  
  Serial.begin(115200);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  setup_wifi();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastPublish >= sendingPeriod) {
    lastPublish += sendingPeriod;
    
    // read temperature and humidity takes about 250 milliseconds!
    float h = dht.readHumidity();     // humidity percentage
    float t = dht.readTemperature();  // temperature Celsius

    // check if everything is ok
    if (isnan(h) || isnan(t)) {  // readings failed, skip
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }

    // read gas level
    float g = analogRead(A0); // gas level in %

    // check if everything is ok, again
    if (isnan(g)){
      Serial.println(F("Failed to read from MQ-5 sensor!"));
      return;
    }
    
    DynamicJsonDocument telemetry(1023);
    telemetry.createNestedObject();

    telemetry[0]["temperature"] = t;
    telemetry[0]["humidity"] = h;
    telemetry[0]["co2"] = g;

    String topic = "kp1/" + APP_VERSION + "/dcx/" + TOKEN + "/json";
    client.publish(topic.c_str(), telemetry.as<String>().c_str());
    //Serial.println("Published on topic: " + topic);
    Serial.println("Values: " + telemetry.as<String>());
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("\nHandling command message on topic: %s\n", topic);

  DynamicJsonDocument doc(1023);
  deserializeJson(doc, payload, length);
  JsonVariant json_var = doc.as<JsonVariant>();

  DynamicJsonDocument commandResponse(1023);
  for (int i = 0; i < json_var.size(); i++) {

    unsigned int command_id = json_var[i]["id"].as<unsigned int>();
    const char* pay = json_var[i]["payload"].as<char*>();
    Serial.println("command id " + String(command_id));
    //Serial.printf("\nJSON: %s\n", pay.c_str());
    
    commandResponse.createNestedObject();
    commandResponse[i]["id"] = command_id;
    commandResponse[i]["statusCode"] = 200;
    commandResponse[i]["payload"] = "done";
  }

  String responseTopic = "kp1/" + APP_VERSION + "/cex/" + TOKEN + "/result/SWITCH";
  client.publish(responseTopic.c_str(), commandResponse.as<String>().c_str());
  Serial.println("Published response to SWITCH command on topic: " + responseTopic);
}

void setup_wifi() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.println();
    Serial.printf("Connecting to [%s]", ssid);
    WiFi.begin(ssid, password);
    connectWiFi();
  }
}

void connectWiFi() {
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    char *client_id = "client-id-123ab";
    if (client.connect(client_id)) {
      Serial.println("Connected to WiFi");
      // ... and resubscribe
      subscribeToCommand();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void subscribeToCommand() {
  String topic_def = "kp1/" + APP_VERSION + "/cex/" + TOKEN + "/command/";
  String topic = topic_def + "SWITCH";
  client.subscribe(topic.c_str());
  Serial.println("Subscribed on topic: " + topic);
}
