#include <Vector.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

enum DeviceType {button, rele, dht_sensor};

class Device {
  public:
    byte pin;            // номер вывода
    byte mode = INPUT;  // режим работы
    DeviceType type;
    const char* name; // Имя устройства
    int state = 0; // Состояние устройства
};

class Button : public Device {
  public:
    Button(const char* _name, byte _pin);
};

class Rele : public Device {
  public:
    Rele(const char* _name, byte _pin);
};

class DHT_Sensor : public Device {
  public:
    int temperature;
    int humidity;
    const char* mqtt_temperature;
    const char* mqtt_humidity;
    DHT* dht; // Ссылка на экземпляр датчика
    DHT_Sensor(const char* _name, const char* _mqtt_temperature, const char* _mqtt_humidity,  byte _pin, DHT* _dht);
//    void sethumidity (int humidity);
};

class MQTT_Device {
  public:
    const char* topic_module;
    const char* topic_room;
    const char* topic_device;
    Device device;
    MQTT_Device(const char* _topic_module, const char* _topic_room,  const char* _topic_device,  Button _device);
    MQTT_Device(const char* _topic_module, const char* _topic_room,  const char* _topic_device,  Rele _device);
    MQTT_Device(const char* _topic_module, const char* _topic_room,  const char* _topic_device,  DHT_Sensor _device);
};

class WiFi_Setting {
  public:
    const char* ssid;
    const char* password;

    WiFi_Setting(const char* ssid, const char* password);
};

WiFi_Setting::WiFi_Setting(const char* _ssid, const char* _password) {
  ssid = _ssid;
  password = _password;
};

Button::Button(const char* _name, byte _pin) {
  name = _name;
  state = 0;
  type = button;
  pin = _pin;
  mode = INPUT;
}

Rele::Rele(const char* _name, byte _pin) {
  name = _name;
  state = 0;
  type = rele;
  pin = _pin;
  mode = OUTPUT;
}

DHT_Sensor::DHT_Sensor(const char* _name, const char* _mqtt_temperature, const char* _mqtt_humidity, byte _pin, DHT* _dht) {
  pin = _pin;
  mode = INPUT;
  name = _name;
  type = dht_sensor;
  dht = _dht;
  mqtt_temperature = _mqtt_temperature;
  mqtt_humidity = _mqtt_humidity;
}

MQTT_Device::MQTT_Device(const char* _topic_module, const char* _topic_room, const char* _topic_device,  Button _device)
{
  topic_module = _topic_module;
  topic_room = _topic_room;
  topic_device = _topic_device;
  device = _device;
}

MQTT_Device::MQTT_Device(const char* _topic_module, const char* _topic_room, const char* _topic_device,  Rele _device)
{
  topic_module = _topic_module;
  topic_room = _topic_room;
  topic_device = _topic_device;
  device = _device;
}

MQTT_Device::MQTT_Device(const char* _topic_module, const char* _topic_room, const char* _topic_device,  DHT_Sensor _device)
{
  topic_module = _topic_module;
  topic_room = _topic_room;
  topic_device = _topic_device;
  device = _device;
}

WiFiClient espClient;
PubSubClient client(espClient);

const char* mqtt_server = "192.168.87.2";
const char* mqtt_login = "leganas";
const char* mqtt_password = "kj4cuetd";
Vector<MQTT_Device> mqtt_list;

DHT dht_1(D1, DHT11); // Реальный датчик
MQTT_Device dht1 = {"/esp8266", "/Bathroom", "/dht1", (DHT_Sensor) {"/Датчик температуры и влажности", "/temperature", "/humidity", D1, &dht_1}};
MQTT_Device btn1 = {"/esp8266", "/Bathroom", "/button1", (Button) {"/Выключатель вентилятор", D0}};
MQTT_Device btn2 = {"/esp8266", "/Bathroom", "/button2", (Button) {"/Выключатель бойлер", D5}};
MQTT_Device rele1 = {"/esp8266", "/Bathroom", "/rele1", (Rele) {"/Реле вентилятор", D2}};
MQTT_Device rele2 = {"/esp8266", "/Bathroom", "/rele2", (Rele) {"/Реле бойлер", D3}};

long lastMsg = 0;
char msg[50];

const String catStr(const char* str1, const char* str2) {
  char result[512];
  snprintf(result, sizeof result, "%s%s", str1, str2);
  String str = result;
  return str;
}

void setup_wifi() {
  WiFi_Setting wifiSetting("LS_AP", "foreverls");
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifiSetting.ssid);
  WiFi.begin(wifiSetting.ssid, wifiSetting.password);
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

  // Ищем топик реле (его будем щёлкать если пришло изменение)
  int releNumber = -1;
  for (int i = 0; i < mqtt_list.size(); i++){
    if (mqtt_list[i].device.type == rele) {
      Rele *dev = (Rele*) &mqtt_list[i].device;
      String base = catStr(mqtt_list[i].topic_module,mqtt_list[i].topic_room);
      String dev_name(mqtt_list[i].topic_device);
      if (strcmp(topic,(base + dev_name).c_str())==0) releNumber = i;
    }
    if (releNumber > -1) break;
  }
  if (releNumber > -1) {
    if ((char)payload[0] == '1' && mqtt_list[releNumber].device.state == 0) swithState(releNumber);
    if ((char)payload[0] == '0' && mqtt_list[releNumber].device.state == 1) swithState(releNumber);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_login, mqtt_password)) {
      Serial.println("connected");
      for (int i = 0; i < mqtt_list.size(); i++) {
        String base = catStr(mqtt_list[i].topic_device, mqtt_list[i].topic_room);
        Serial.print("Подписываемся на топик : ");
        Serial.println(catStr(base.c_str(), mqtt_list[i].topic_device).c_str());
        client.subscribe(catStr(base.c_str(), mqtt_list[i].topic_device).c_str());
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
  Serial.begin(115200);
  Serial.println("Init device");
  mqtt_list.push_back(dht1);
  mqtt_list.push_back(btn1);
  mqtt_list.push_back(btn2);
  mqtt_list.push_back(rele1);
  mqtt_list.push_back(rele2);
  
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  for (int i = 0; i < mqtt_list.size(); i++){
    if (mqtt_list[i].device.type == dht_sensor) {
      DHT_Sensor *dev = (DHT_Sensor*) &mqtt_list[i].device;
      DHT *sensor = (DHT*) &dev;
      sensor->begin();
    } else pinMode(mqtt_list[i].device.pin, mqtt_list[i].device.mode);
  }
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Опрашиваем устройства (и публикуем состояния в MQTT брокера)
  for (int i = 0; i <  mqtt_list.size(); i++) {
    switch (mqtt_list[i].device.type) {
      case button:
      // По состоянию выключателей переключаем реле и за одно передаём их состояние
      // Всё это в Action
      // (как и раньше только передаётся еще и состояние выключателей) 
      // можно судить будет на стороне Mojordomo кто щёлкнул реле
        if (PubButtonState(&mqtt_list[i]) == true) Action(i);
        break;
      case rele:
      // Постоянно публиковать походу не нужно, дальше разберёмся может добавлю предыдущее состояние реле
//        PubReleState(&mqtt_list[i]);
        break;
      case dht_sensor:
        long now = millis();
        if (now - lastMsg > 2000) { // Каждые 2 секунды публикуем данные DHT сенсора(ов)
          lastMsg = now;
          dht_sensor_pub(&mqtt_list[i]);
        }
        break;
    }
  }
}

// Переключает состояние устройства на противоположное
int swithState(int id){
   if (mqtt_list[id].device.state == 0){ 
    digitalWrite(mqtt_list[id].device.pin,HIGH); 
    mqtt_list[id].device.state = 1;
    return 1;
   } else {
    digitalWrite(mqtt_list[id].device.pin,LOW);
    mqtt_list[id].device.state = 0;
    return 0;
   }
}

// Некий экшен в качестве ID принемаем номер Device , эта функция всегда специфичная
void Action(int id){
  switch (id) {
     case 1: // Если был счёлчок button1
         swithState(3);
         PubReleState(&mqtt_list[3]);
     break;
     case 2: // Если был счёлчок button2
         swithState(4);
         PubReleState(&mqtt_list[4]);
     break;
  }
}

// Если состояние Кнопки изменилось публикует это дело в MQTT сеть (и возвращает true если менялось положение)
boolean PubButtonState(MQTT_Device *mqtt_dev){
  Button *dev = (Button*) &mqtt_dev->device;
  int current_rele_state = digitalRead(dev->pin);
  if (current_rele_state != dev->state) {
    String base = catStr(mqtt_dev->topic_module,mqtt_dev->topic_room);
    snprintf (msg, 75, "%ld", dev->state);
    Serial.print("Publish message: ");
    Serial.print(catStr(base.c_str(),mqtt_dev->topic_device).c_str()); Serial.print(" - ");
    Serial.println(msg);
    client.publish(catStr(base.c_str(),mqtt_dev->topic_device).c_str(), msg);
    return true;
  }
  return false;
}

// Публикуем текущее состояние реле
void PubReleState(MQTT_Device *mqtt_dev){
  Rele *dev = (Rele*) &mqtt_dev->device;
  String base = catStr(mqtt_dev->topic_module,mqtt_dev->topic_room);
  snprintf (msg, 75, "%ld", dev->state);
  Serial.print("Publish message: ");
  Serial.print(catStr(base.c_str(),mqtt_dev->topic_device).c_str()); Serial.print(" - ");
  Serial.println(msg);
  client.publish(catStr(base.c_str(),mqtt_dev->topic_device).c_str(), msg);
}

// Публикует информацию от датчика температуры и влажности
void dht_sensor_pub(MQTT_Device *mqtt_dev){
  DHT_Sensor *dev = (DHT_Sensor*) &mqtt_dev->device;
  DHT *sensor = (DHT*) &dev;
  dev->humidity = sensor->readHumidity();
  if (dev->humidity <= 100) {
    String base = catStr(mqtt_dev->topic_module,mqtt_dev->topic_room);
    snprintf (msg, 75, "%ld", dev->humidity);
    Serial.print("Publish message: ");
    String dev_name(mqtt_dev->topic_device);
    base = base+dev_name;
    Serial.println(catStr(base.c_str(),dev->mqtt_humidity).c_str());
    Serial.println(msg);
    String base_device();
    client.publish(catStr(base.c_str(),dev->mqtt_humidity).c_str(), msg);
  }
  dev->temperature = sensor->readTemperature();
  if (dev->temperature <= 100) {
    String base = catStr(mqtt_dev->topic_module,mqtt_dev->topic_room);
    snprintf (msg, 75, "%ld", dev->temperature);
    Serial.print("Publish message: ");
    String dev_name(mqtt_dev->topic_device);
    base = base+dev_name;
    Serial.println(catStr(base.c_str(),dev->mqtt_temperature).c_str());
    Serial.println(msg);
    client.publish(catStr(base.c_str(),dev->mqtt_temperature).c_str(), msg);
  }
}


