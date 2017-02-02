#include <PubSubClient.h>
#include <ESP8266WiFi.h>

///////////////////////////////////////////////////////////////////////////
// Wifi Parameters
///////////////////////////////////////////////////////////////////////////
const char* ssid = "OFFLINE";
const char* password = "kr0k0d!L3";

///////////////////////////////////////////////////////////////////////////
// MQTT Parameters
///////////////////////////////////////////////////////////////////////////
const char* mqttServer = "test.mosquitto.org";
const char* mqttUsername = "your_username";
const char* mqttPassword = "your_password";
const int mqttPort = 1883;

const char* hitTopic;
uint8_t mac[6];
  
void callbackMessageReceived(char* topic, byte* payload, unsigned int length) {
  String payloadString = String(topic);
}

WiFiClient wifiClient;
PubSubClient mqttClient(mqttServer, 1883, callbackMessageReceived, wifiClient);

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

String getTopicBase(const uint8_t* mac) {
  String topicName = "ESP8266/";
  for (int i = 0; i < 6; ++i) {
    topicName += String(mac[i], 16);
    if (i < 5)
      topicName += '-';
  }
  return topicName;
}

String getConfigurationTopic(const uint8_t* mac)
{
  String topicName = getTopicBase(mac);
  topicName += "/config";
  return topicName;
}

String getHitTopic(const uint8_t* mac)
{
   String topicName = getTopicBase(mac);
  topicName += "/hit";
  return topicName;
}

String getClientName(const uint8_t* mac)
{
  // Generate client name based on MAC address and last 8 bits of microsecond counter
  String clientName;
  clientName += "esp8266-";
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  return clientName;
}

void resubscribe()
{
  Serial.println("try to subscribe");
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  String subscriptionTopicString = getConfigurationTopic(mac);
  int subscriptionTopicLength = subscriptionTopicString.length() + 1; 
  
  char subscriptionTopicCharArray[subscriptionTopicLength];
  subscriptionTopicString.toCharArray(subscriptionTopicCharArray, subscriptionTopicLength);

  if (mqttClient.subscribe(subscriptionTopicCharArray)) {
    Serial.print("Subscribed to ");
    Serial.println(subscriptionTopicString);
  }
}

void setup() {
  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.println();

  ///////////////////////////////////////////////////////////////////////////
  // Wifi Setup
  ///////////////////////////////////////////////////////////////////////////
  
  Serial.print("Connecting to SSID: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.macAddress(mac);

  ///////////////////////////////////////////////////////////////////////////
  // MQTT setup
  ///////////////////////////////////////////////////////////////////////////
  
  String clientName;
  clientName = getClientName(mac);

  Serial.print("Connecting to MQTT-Server ");
  Serial.print(mqttServer);
  Serial.print(" with ClientId: ");
  Serial.println(clientName);

  mqttReconnect(true);
  hitTopic = getHitTopic(mac).c_str();
}

void mqttReconnect(bool abortIfFailed) {
  String clientName;
  clientName = getClientName(mac);
  
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect((char*) clientName.c_str(), mqttUsername, mqttPassword)) {
      Serial.println("connected. resubscribing");
      resubscribe();
      Serial.println("done");
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqttClient.state());
      if (abortIfFailed) {
        Serial.println("Will reset and try again...");
        abort();
      } else {
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
      }
      
    }
  }
}

long value = 0;

void loop() {
  if (!mqttClient.connected()) {
    delay(1000);
    mqttReconnect(false);
  }
  value = analogRead(0);
  mqttClient.loop();
  mqttClient.publish(hitTopic, String(value).c_str(), true);
}


