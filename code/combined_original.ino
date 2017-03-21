#include <PubSubClient.h>
#include <ESP8266WiFi.h>

///////////////////////////////////////////////////////////////////////////
// Network Parameter                                                     //
///////////////////////////////////////////////////////////////////////////

WiFiClient wifiClient;
const char* ssid          = "IOT-WS";
const char* password      = "Spartakiade2017";

///////////////////////////////////////////////////////////////////////////
// MQTT Parameter                                                        //
///////////////////////////////////////////////////////////////////////////

PubSubClient mqttClient;
const char* mqttServer    = "192.168.30.10";
const int   mqttPort      = 1883;
const char* mqttUsername  = "spartakus";
const char* mqttPassword  = "berlinworkshop";
const String targetId     = "3";
const String clientName   = "target" + targetId;

const String topicBase    = "target/" + targetId + "/";
const String topicHit     = topicBase + "hit";
const String topicPing    = topicBase + "ping";
const String topicLed     = topicBase + "led";

///////////////////////////////////////////////////////////////////////////
// Loop Parameter                                                        //
///////////////////////////////////////////////////////////////////////////

long value                = 0;
long maxValue             = 0;
long threshold            = 30;
long average              = 0;

long loopCounter          = 0;
long maxLoops             = 5;
long totalValue           = 0;
int  hitId                = 0;
unsigned long timeMillis;

///////////////////////////////////////////////////////////////////////////
// Pin Belegung                                                          //
///////////////////////////////////////////////////////////////////////////

const int EXTERN_LED_PIN  = 3;  // Angesteckte LED auf D0
const int INTERN_LED_PIN  = 2;  // Blaue Idee auf dem Chip
const int ANALOG_PIN      = 17; // Angesteckter Lautsprecher auf Analog0
const int POWER_PIN       = 15; // Wenn die WioNode am USB betrieben wird muss die USB-Spannung durchgeschalten werden

///////////////////////////////////////////////////////////////////////////
// Blink                                                                 //
///////////////////////////////////////////////////////////////////////////

void blinkInternLED() {
  digitalWrite(INTERN_LED_PIN, HIGH);
  delay(50);
  digitalWrite(INTERN_LED_PIN, LOW);
  delay(50);
}

///////////////////////////////////////////////////////////////////////////
// Externe LED anschalten                                                //
///////////////////////////////////////////////////////////////////////////

void switchOnExternLED() {
  Serial.println("Externe LED anschalten");
  pinMode(EXTERN_LED_PIN, OUTPUT);
  digitalWrite(EXTERN_LED_PIN, HIGH);
}

///////////////////////////////////////////////////////////////////////////
// Externe LED ausschalten                                               //
///////////////////////////////////////////////////////////////////////////

void switchOffExternLED() {
  Serial.println("Externe LED ausschalten");
  digitalWrite(EXTERN_LED_PIN, LOW);
}

///////////////////////////////////////////////////////////////////////////
// MQTT Callback Funktion                                                //
///////////////////////////////////////////////////////////////////////////

void callbackMessageReceived(char* topic, byte* payload, unsigned int length) {
  if (payload[0] == 49) { // Byte 49 = String "1"
    switchOnExternLED();
  } else {
    switchOffExternLED();
  }
}

///////////////////////////////////////////////////////////////////////////
// MQTT (re)subscribe
///////////////////////////////////////////////////////////////////////////

void resubscribe()
{
  mqttClient.subscribe(topicLed.c_str());
  Serial.println("subscribed to topic");
}

///////////////////////////////////////////////////////////////////////////
// Init Wifi
///////////////////////////////////////////////////////////////////////////

void initWifi() {
  Serial.print("wifi init. SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
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
// MQTT (re)connect
///////////////////////////////////////////////////////////////////////////
void mqttReconnect() {
  Serial.print("Connecting to MQTT-Server ");
  Serial.print(mqttServer);
  Serial.print(":");
  Serial.print(mqttPort);
  Serial.print(" with ClientId: ");
  Serial.println(clientName);

  // Reconnect Loop
  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientName.c_str(), mqttUsername, mqttPassword)) {
      Serial.println("connecting to MQTT server was successful!");
      resubscribe();
    } else {
      Serial.print(".");
      delay(250);
    }
  }
}

///////////////////////////////////////////////////////////////////////////
// Den Mittelwert über mehrere Messpunkte ermitteln                      //
///////////////////////////////////////////////////////////////////////////

bool isAverageFullyCalculated(long value) {

  totalValue += value;

  if (loopCounter > maxLoops) {
    average = totalValue / maxLoops;
    totalValue = 0;
    loopCounter = 0;
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////
// Pseudo - breathing                                                         //
///////////////////////////////////////////////////////////////////////////

void breathing() {
  if (millis() - timeMillis > 3000) {
    delay(100);
    timeMillis = millis();
  }
}

///////////////////////////////////////////////////////////////////////////
// Arduino - Setup                                                       //
///////////////////////////////////////////////////////////////////////////

void setup() {
  // Serial.begin(115200);
  timeMillis = millis();

  /**
   * Bei einem Betrieb am USB muss der PIN 15 gesetzt werden, da sonst der
   * Strom nicht bei den Digital-Pins ankommt
   */
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);

  /**
   * Die Interne Status-LED soll nicht leuchten
   */
  pinMode(INTERN_LED_PIN, OUTPUT);
  digitalWrite(INTERN_LED_PIN, LOW);

  initWifi();
  initMQTT();
}

///////////////////////////////////////////////////////////////////////////
// Arduino - Loop
///////////////////////////////////////////////////////////////////////////

void loop() {


  if (!mqttClient.connected()) {
    mqttReconnect();
  }

  mqttClient.loop();

  /**
   * Der delay ist ein wichtiger Parameter. Er bestimmt maßgeblich die Abtastrate
   * und sollte nicht zu hoch sein!
   */
  loopCounter++;

  /**
   * Auslesen des umfunktionierten Laustsprechers
   */
  value = analogRead(ANALOG_PIN);
  delayMicroseconds(50);

  /**
   * Den Wert kann man sich an dieser Stelle mal ausgeben lassen und im Plotter anzeigen lassen
   */

  // Serial.println(value);

  /**
   * Die WioNode ist mit der Abtastrate recht gut ausgelastet. Der PubSubClient findet keine
   * CPU-Zeit um das eigentliche MQTT interne PING-Paket zu senden. Um die Verbindung trotzdem
   * zu halten schicken wir hier aller paar Sekunden eine Message. Was die Verbindung auch
   * aufrecht erhalten sollte
   */
  breathing();

  if (!isAverageFullyCalculated(value)) {
    return;
  }

  if (average > threshold) {
    Serial.println("hit " + String(average));

    /**
     * Publish an den Broker auf target/X/hit mit der HitId.
     */
    mqttClient.publish(topicHit.c_str(), String(hitId++).c_str());
    average = 0;
    blinkInternLED();

    /**
     * Nach dem ersten Ausschlag des Wertes sollte der Loop kurz unterbrocken werden damit es keine
     * doppelt gezählten Treffer gibt.
     */
    delay(250);
  }
}
