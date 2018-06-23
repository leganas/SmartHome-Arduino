#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Update these with values suitable for your network.

const char* ssid = "LS_AP";
const char* password = "foreverls";
const char* mqtt_server = "192.168.87.2";
const char* mqtt_login = "leganas";
const char* mqtt_password = "kj4cuetd";

const char* mqtt_outTopic = "/esp8266/out";
const char* mqtt_inTopic = "/esp8266/in";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

int on_off = 1; // индикатор состояния

int inputPin = D1; // подключение кнопки в контактам D1 и GND. Можно выбрать любой пин на плате
int Lamp1Rele = D2; // подключение Rele Лампы №1
int val = 1; // включение/выключение хранения значени
int pr_val = 0; // предыдушие значение состояния кнопки

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    digitalWrite(Lamp1Rele, LOW);  // Turn the LED off by making the voltage HIGH
    on_off = 1;
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
    digitalWrite(Lamp1Rele, HIGH);  // Turn the LED off by making the voltage HIGH
    on_off = 0;
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
   
    if (client.connect(clientId.c_str(),mqtt_login,mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqtt_outTopic, "hello world");
      // ... and resubscribe
      client.subscribe(mqtt_inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  pinMode(inputPin,INPUT);
  pinMode(Lamp1Rele,OUTPUT);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

    val = digitalRead(inputPin); // чтение входных данных
    if (val != pr_val) {
      Serial.print("Pin 5 = ");
      Serial.println(val);
      pr_val = val;
      if (on_off == 0) { on_off = 1;} else {on_off = 0;}
      snprintf (msg, 75, "%ld", on_off);
      client.publish(mqtt_inTopic, msg);
    }


  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 75, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(mqtt_outTopic, msg);
  }
}
