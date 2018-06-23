#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Update these with rele_states suitable for your network.
const char* ssid = "LS_AP";
const char* password = "foreverls";
const char* mqtt_server = "192.168.87.2";
const char* mqtt_login = "leganas";
const char* mqtt_password = "kj4cuetd";
WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];

const char* mqtt_module = "/esp8266";
const char* mqtt_room[] = {"/Bathroom","/Bathroom"};
const char* mqtt_device[] = { "/rele1","/rele2" }; // Топик реле1 - реле2
const char* mqtt_OutputParameter[] = {"/humidity","/temperature"}; // Топик для датчика температуры и влажности
const char* mqtt_obiem = "/obiem"; // Топик сработки объёмника

int key_pin[] = {D0,D5}; // Массив номеров контактов к которым подключены выключатели
int rele_pin[] = {D2,D3}; // Массив номеров контактов к которым подключены реле
int rele_state[] = {1,1}; // Текущее состояние реле
int key_state[] = {0,0}; // Текущее состояние выключателей

int DHTPIN = D1; // 
DHT dht(DHTPIN, DHT11);

int temperature = 0;
int humidity = 0;


//int obiem_pin = D5;
//int last_obiem = 0;

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


  int releNumber = -1;
  for (int i = 0; i <= 1; i++){
   String base = catStr(mqtt_module,mqtt_room[i]);
   String device = mqtt_device[i];
   if (strcmp(topic,(base + device).c_str())==0) {
    releNumber = i;
    break;    
   }
  }


  Serial.print("Переключаем реле : ");
  Serial.println(releNumber);
  Serial.print("Номер пина : ");
  Serial.println(rele_pin[releNumber]);

  if (releNumber != -1) {
   // Switch on the LED if an 1 was received as first character
   if ((char)payload[0] == '1') {
    if (releNumber == 0) digitalWrite(BUILTIN_LED, LOW);   // Светодиод на ESP будет связан только с 1м реле
    digitalWrite(rele_pin[releNumber], LOW);  // Выключаем реле с нужным номером
    rele_state[releNumber] = 1;
   } else {
    if (releNumber == 0) digitalWrite(BUILTIN_LED, HIGH);  // Светодиод на ESP будет связан только с 1м реле
    digitalWrite(rele_pin[releNumber], HIGH);  // Включаем реле с нужным номером
    rele_state[releNumber] = 0;
   }
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

//    client.publish(base.c_str(), "hello");
      // ... and resubscribe
      for (int i = 0; i < (sizeof(mqtt_device) / sizeof(char *)); i++){
         String base = catStr(mqtt_module,mqtt_room[i]);
         Serial.print("Подписываемся на топик : ");
         Serial.println(catStr(base.c_str(),mqtt_device[i]).c_str());
         client.subscribe(catStr(base.c_str(),mqtt_device[i]).c_str());
      }
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
  Serial.println("Init device");
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  for (int i = 0; i < (sizeof(key_pin) / sizeof(int)); i++) pinMode(key_pin[i],INPUT);
  for (int i = 0; i < (sizeof(rele_pin) / sizeof(int)); i++){
    pinMode(rele_pin[i],OUTPUT);
    digitalWrite(rele_pin[i], HIGH);
  }

  dht.begin();
//  pinMode(obiem_pin,INPUT);
  
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

const String catStr(const char* str1,const char* str2){
    char result[512];
    snprintf(result, sizeof result, "%s%s", str1, str2);
    String str = result;
    return str;
}


void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Перебираем состояние выключателей
  for (int i = 0; i < (sizeof(key_pin) / sizeof(int)); i++) {
    String base = catStr(mqtt_module,mqtt_room[i]);
    int current_key_state = digitalRead(key_pin[i]); // чтение входных данных
    if (current_key_state != key_state[i]) { // если текущее положение выключателя не совпадает предыдущим
      Serial.print("Rele = ");
      Serial.print(i, DEC);
      Serial.print(" | ");
      Serial.print(" было ");
      Serial.print(rele_state[i]);
      Serial.print(" стало ");
      if (rele_state[i] == 0) rele_state[i] = 1; else rele_state[i] = 0;  // Делаем свич реле
      Serial.println(rele_state[i]);
      key_state[i] = current_key_state;
      snprintf (msg, 75, "%ld", rele_state[i]);
      client.publish(catStr(base.c_str(),mqtt_device[i]).c_str(), msg);
    }
  }


  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;

    humidity = dht.readHumidity();
    if (humidity <= 100) {
      String base = catStr(mqtt_module,mqtt_room[0]);
      snprintf (msg, 75, "%ld", humidity);
      Serial.print("Publish message: ");
      Serial.println(catStr(base.c_str(),mqtt_OutputParameter[0]).c_str());
      Serial.println(msg);
      client.publish(catStr(base.c_str(),mqtt_OutputParameter[0]).c_str(), msg);
    }
    
    temperature = dht.readTemperature();;
    if (temperature <= 100) {
      String base = catStr(mqtt_module,mqtt_room[1]);
      snprintf (msg, 75, "%ld", temperature);
      Serial.print("Publish message: ");
      Serial.println(catStr(base.c_str(),mqtt_OutputParameter[1]).c_str());
      Serial.println(msg);
      client.publish(catStr(base.c_str(),mqtt_OutputParameter[1]).c_str(), msg);
    }
  }


//    int obiem = digitalRead(obiem_pin);

//    if (last_obiem != obiem) {
//      last_obiem = obiem;
//      snprintf (msg, 75, "%ld", obiem);
//      Serial.print("Publish message: ");
//      Serial.println(catStr(base.c_str(),"/obiem").c_str());
//      Serial.println(msg);
    
//      if (obiem == 1) {
//        client.publish(catStr(base.c_str(),mqtt_obiem).c_str(), "true");
//        } else {
//        client.publish(catStr(base.c_str(),mqtt_obiem).c_str(), "false");
//      };
//    }
}
