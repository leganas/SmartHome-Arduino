#include <UIPEthernet.h>
#include <Vector.h>
#include <PubSubClient.h>


enum DeviceType {key, rele, dht_sensor};

class Device {
  public:
    byte pin;            // номер вывода
    byte mode = INPUT;  // режим работы
    DeviceType type;
    const char* name; // Имя устройства
    int state = 0; // Состояние устройства
};

class Key : public Device {
  public:
    Key(const char* _name, byte _pin);
};

class Rele : public Device {
  public:
    Rele(const char* _name, byte _pin);
};

class MQTT_Device {
  public:
    const char* topic_module;
    const char* topic_room;
    const char* topic_device;
    Device device;
    MQTT_Device(const char* _topic_module, const char* _topic_room,  const char* _topic_device,  Key _device);
    MQTT_Device(const char* _topic_module, const char* _topic_room,  const char* _topic_device,  Rele _device);
};

Key::Key(const char* _name, byte _pin) {
  name = _name;
  state = 0;
  type = key;
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

MQTT_Device::MQTT_Device(const char* _topic_module, const char* _topic_room, const char* _topic_device,  Key _device)
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

// определяем конфигурацию сети
byte mac[] = {0xAE, 0xB2, 0x26, 0xE4, 0x4A, 0x5C}; // MAC-адрес
byte ip [] = {192, 168, 88, 120};

EthernetClient ethClient; // объект клиент
PubSubClient client(ethClient);

const char* mqtt_server = "192.168.87.2";
const char* mqtt_login = "leganas";
const char* mqtt_password = "kj4cuetd";

Vector<MQTT_Device> mqtt_list;
long lastMsg = 0;
int lostConnectPi = 0;
char msg[50];

const String catStr(const char* str1, const char* str2) {
  char result[512];
  snprintf(result, sizeof result, "%s%s", str1, str2);
  String str = result;
  return str;
}

void setup_ethernet() {
  Ethernet.begin(mac,ip); // инициализация контроллера
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
    String clientId = "ArduinoLeonadroClient-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_login, mqtt_password)) {
      Serial.println("connected");
      for (int i = 0; i < mqtt_list.size(); i++) {
        String base = catStr(mqtt_list[i].topic_device, mqtt_list[i].topic_room);
        Serial.print("Connected to topic : ");
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
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Init device");

  mqtt_list.push_back({"/leonardo", "/Hall", "/Key1", (Key) {"0", 11}});// Выключатель зал 1й
  mqtt_list.push_back({"/leonardo", "/Hall", "/Key2", (Key) {"1", 12}});// Выключатель зал 2й
  mqtt_list.push_back({"/leonardo", "/Hall", "/Key3", (Key) {"2", 11}});// Выключатель спальня 
  mqtt_list.push_back({"/leonardo", "/Hall", "/Key4", (Key) {"3", 12}});// Выключатель прихожая
  mqtt_list.push_back({"/leonardo", "/Hall", "/Key5", (Key) {"4", 12}}); // Датчик движения
  mqtt_list.push_back({"/leonardo", "/Hall", "/rele1", (Rele) {"5", 13}}); // Зал 1е реле
  mqtt_list.push_back({"/leonardo", "/Hall", "/rele2", (Rele) {"6", 14}}); // Зал 2е реле
  mqtt_list.push_back({"/leonardo", "/Hall", "/rele3", (Rele) {"7", 13}}); // Спальня
  mqtt_list.push_back({"/leonardo", "/Hall", "/rele4", (Rele) {"8", 14}}); // Прихожая
  mqtt_list.push_back({"/leonardo", "/Hall", "/rele5", (Rele) {"9", 14}}); // Reset Rasspbery Pi
  setState(9,HIGH); // Включаем RP
  
  for (int i = 0; i < mqtt_list.size(); i++){
    pinMode(mqtt_list[i].device.pin, mqtt_list[i].device.mode);
  }
  setup_ethernet();
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
      case key:
      // По состоянию выключателей переключаем реле и за одно передаём их состояние
      // Всё это в Action
      // (как и раньше только передаётся еще и состояние выключателей) 
      // можно судить будет на стороне Mojordomo кто щёлкнул реле
        if (PubKeyState(&mqtt_list[i]) == true) Action(i);
        break;
      case rele:
      break;
    }
    long now = millis();
    if (now - lastMsg > 10000) { // Каждые 10 секунды проверяем жива ли RassperyPi)
       if (check_RP() == false) {
        lostConnectPi++;
        if (lostConnectPi > 5) {
          Serial.println("Rasspbery Pi power off");
          setState(9,LOW);
          delay(2000);
          setState(9,HIGH);
          Serial.println("Rasspbery Pi power on");
          lostConnectPi = 0;
          lastMsg = now;
        }
       } else {
         lostConnectPi = 0;
         lastMsg = now;
       }
    }
  }
}
 
boolean check_RP()
{
  IPAddress server(192,168,87,2);  // Rasspbery Pi
  EthernetClient testClient; // объект для теста связи
  testClient.setTimeout(1000);
  Serial.println("Check Rasspbery Pi");
  if (testClient.connect(server, 80)) {
    Serial.println("Rasspbery Pi - ready");
    return true;    
  } else {
    Serial.println("Rasspbery Pi - failed");
    return false;
  }
}

void setState(int id, int state){
    digitalWrite(mqtt_list[id].device.pin,state); 
    mqtt_list[id].device.state = state;
}

// Переключает состояние устройства на противоположное
int swithState(int id){
   if (mqtt_list[id].device.state == 0){ 
    setState(id, HIGH);
    return 1;
   } else {
    setState(id, LOW);
    return 0;
   }
}

// Некий экшен в качестве ID принемаем номер Device , эта функция всегда специфичная
void Action(int id){
  int id_rele = -1;
  switch (id) {
     case 0: // Если был счёлчок Key1
         id_rele = 5;
     break;
     case 1: // Если был счёлчок Key2
         id_rele = 6;
     break;
     case 2: // Если был счёлчок Key3
         id_rele = 7;
     break;
     case 3: // Если был счёлчок Key4
         id_rele = 8; // реле в холе
     break;
     case 4: // Если сработал объёмник
         id_rele = 8; // это объёмник (дёргаем реле в холе)
     break;
  }
  if (id_rele > -1) {
    swithState(id_rele);
    PubReleState(&mqtt_list[id_rele]);
  }
}

// Если состояние Кнопки изменилось публикует это дело в MQTT сеть (и возвращает true если менялось положение)
boolean PubKeyState(MQTT_Device *mqtt_dev){
  Key *dev = (Key*) &mqtt_dev->device;
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

