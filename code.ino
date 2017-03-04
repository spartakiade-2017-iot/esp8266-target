#include <PubSubClient.h>
#include <ESP8266WiFi.h>

///////////////////////////////////////////////////////////////////////////
// Network Parameters
///////////////////////////////////////////////////////////////////////////
const char* ssid = "<< WIFI SSID >>>";
const char* password = "<< WIFI PASSWORD >>";
uint8_t mac[6];

WiFiClient wifiClient;

///////////////////////////////////////////////////////////////////////////
// MQTT Parameters
///////////////////////////////////////////////////////////////////////////
const char* mqttServer    = "192.168.0.253";
const int   mqttPort      = 1883;
const char* hitTopic      = "target/1/hit";
const char* configTopic   = "target/1/config";
const char* clientName    = "target1";

long value = 0;
long maxValue = 0;
long threshold = 8;
long average = 0;

long loopCounter = 0;
long maxLoops = 10;
long totalValue = 0;
bool hit = false;

void blink() {
  digitalWrite(2, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(50);              // wait for a second
  digitalWrite(2, LOW);    // turn the LED off by making the voltage LOW
  delay(50);              // wait for a second
}


void callbackMessageReceived(char* topic, byte* payload, unsigned int length) {

  // maxLoops threshold
  for (int i=0; i<10; i++){
    blink();
  }
  long newMaxLoops = int(payload[0]);
  long newThreshold = int(payload[1]);

  Serial.print("new config - maxLoops: ");
  Serial.print(newMaxLoops);
  Serial.print(", Threshold: ");
  Serial.println(newThreshold);

  maxLoops = newMaxLoops;
  threshold = newThreshold;
}

PubSubClient mqttClient;



void setup() {
  Serial.begin(115200);

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  pinMode(15, OUTPUT);
  digitalWrite(15, 1);

  initWifi();
  initMQTT();
}

void resubscribe()
{
  uint8_t mac[6];
  WiFi.macAddress(mac);

  mqttClient.subscribe(configTopic);
  Serial.println("subscribed to " + String(configTopic));
}

///////////////////////////////////////////////////////////////////////////
// Init Wifi
///////////////////////////////////////////////////////////////////////////
void initWifi() {
  Serial.print("wifi init. SSID: ");
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
}

///////////////////////////////////////////////////////////////////////////
// Init MQTT
///////////////////////////////////////////////////////////////////////////
void initMQTT() {
  mqttClient.setClient(wifiClient);
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callbackMessageReceived);
  mqttReconnect();
}

///////////////////////////////////////////////////////////////////////////
// MQTT reconnect
///////////////////////////////////////////////////////////////////////////
void mqttReconnect() {
  Serial.print("Connecting to MQTT-Server ");
  Serial.print(mqttServer);
  Serial.print(":");
  Serial.print(mqttPort);
  Serial.print(" with ClientId: ");
  Serial.println(clientName);

  // Loop until we're connected
  while (!mqttClient.connected()) {
    Serial.print(".");
    if (mqttClient.connect(clientName)) {
      Serial.println("");
      Serial.println("connecting to MQTT server was successful!");
      resubscribe();
    }
    delay(500);
  }
}

void loop() {
  value = analogRead(A0);

  totalValue += value;

  if (loopCounter > maxLoops) {
    average = totalValue / maxLoops;
    totalValue = 0;
    loopCounter = 0;
  }

  if (average > threshold) {
    hit = true;
  } else {
    if (hit == true) {
      hit = false;
      mqttClient.publish(hitTopic,"hit");
      blink();
      delay(150);
    }
  }
  if (!mqttClient.connected()) {
    mqttReconnect();
  }

  mqttClient.loop();
  loopCounter++;
}

//////////////////////////////////////////////////////////////////////////////////////////

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
