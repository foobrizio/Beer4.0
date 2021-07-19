#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#include <WiFi.h>
#include <PubSubClient.h>

#include <string.h>

//Questa libreria serve per l'NTP
//#include <time.h>

//Questa libreria serve per salvare dei valori nella EEPROM
#include <EEPROM.h>
#define EEPROM_SIZE 20
#define STATUS_ADDRESS 10

// DHT11 Configuration (temperature / humitity)
#define DHTTYPE DHT11
#define DHTPIN 27
DHT dht(DHTPIN,DHTTYPE);

// Flame and Light sensors Configuration
#define LIGHT_PIN 34 //Analog
#define FLAME_PIN 14 //Digital

// LED configuration
#define TEMP_LED 4
#define HUM_LED 0
#define LIGHT_LED 2
#define FLAME_LED 15


//ID della scheda
const char deviceID[] = "0";

//Dati per la connessione WiFi
const char* ssid = "Gabriele-2.4GHz";
//const char* ssid = "tplink-2.4GHz";
const char* pass = "i8Eo6zdPhvfqgzPVKo85hWP1";

//Dati per la configurazione MQTT
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* brm_LWT_topic = "resp/brm/3/0/11";

byte willQoS = 0;
char willTopic[60];
char willMessage[60];
boolean willRetain = true;

WiFiClient espClient;
PubSubClient client(espClient);

//Dati per la connessione al server NTP (per l'orario)
//const char* ntpServer = "pool.ntp.org";
//const long  gmtOffset_sec = 3600;
//const int   daylightOffset_sec = 3600;

// Qeusto boolean attiva o disattiva la scheda
boolean active=true;
// Questi altri boolean implementano l'observe dei vari dati
boolean observeTemperature= true;
boolean observeHumidity = true;
boolean observeLight = true;
boolean observeFlame = true;
// Il tick è la durata base di delayTime per il nostro ESP32
unsigned long tick=1000;

//Qui abbiamo 4 coppie diverse interval-timestamp per i diversi tipi di dati
long lastTemperatureCheck;
long lastHumidityCheck;
long lastLightCheck;
long lastFlameCheck;

const long temperatureInterval=60000;
const long humidityInterval=60000;
const long lightInterval=60000;
const long flameInterval=3000;

//Questa coppia serve per il controllo della connessione
long lastConnectionCheck;
const long connectionInterval = 5000;

/* Questo è il documento JSON che creiamo con tutti i valori ricevuti dai sensori.
 * Man mano che riceviamo valori, il documento cresce. Alla fine viene serializzato
 * ed inviato tramite MQTT
 */
DynamicJsonDocument jdoc(3600);
JsonArray array;

void setup() {

  Serial.begin(115200);
  // initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);
  setup_wifi();
  //setupNTP();
  setup_mqtt();
  setupWires();
  // Init and get the time
  //setupTime();
}

/*
 * Questo metodo serve ad implementare la connessione I2C con il bme280 
 */
void setupWires(){
  
  dht.begin();
  pinMode(LIGHT_PIN, INPUT_PULLUP);
  pinMode(FLAME_PIN, INPUT_PULLUP);

  pinMode(TEMP_LED, OUTPUT);
  pinMode(HUM_LED, OUTPUT);
  pinMode(LIGHT_LED, OUTPUT);
  pinMode(FLAME_LED, OUTPUT);

  digitalWrite(TEMP_LED, LOW);
  digitalWrite(HUM_LED, LOW);
  digitalWrite(LIGHT_LED, LOW);
  digitalWrite(FLAME_LED, LOW);
}

/*
 * Questo serve per connettersi alla rete WiFi
 */
void setup_wifi(){

  delay(10);
  // We start by connecting to a WiFi network
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid,pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(700);
    Serial.println(WiFi.status());
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("Connected to the WiFi network!!");
}

/*
 * Configurazione del broker MQTT.
 */
void setup_mqtt(){
  client.setServer(mqtt_server,mqtt_port);
  client.setCallback(callback);
  createLWTData();
  reconnect();
  delay(2000);
}

/*
 * Configurazione del server NTP per recuperare l'orario da internet
 */
 /*
void setupTime(){
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println(getTime());
}*/

void createLWTData(){
  strcat(willTopic, "resp/st/");
  strcat(willTopic, deviceID);
  strcat(willTopic, "/3/0/4");
  StaticJsonDocument<20> respDoc;
  respDoc["v"]="Offline";
  serializeJson(respDoc, willMessage);
}

void getStatus(){
  int status = EEPROM.read(STATUS_ADDRESS);
  Serial.print("Status: ");
  if(status==0){
    Serial.println("Not active");
    active=false;
  }
  else if(status==1){
    Serial.println("active");
    active=true;
  }
}

void setStatus(uint8_t value){
  EEPROM.write(STATUS_ADDRESS,value);
  if(value==0)
    active=false;
  else if(value==1)
    active=true;
  EEPROM.commit();
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.println("Message arrived.");
  String messageTemp;
  
  //Qui u il byte* message in una stringa
  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }
  Serial.println(messageTemp);
  //Qui convertiamo il char* topic in un char[]
  char myTopic[50];
  char anotherTopic[50];
  strcpy(myTopic, topic);
  strcpy(anotherTopic, topic);

  //Qui tokenizziamo il token per navigarlo attraverso i vari levels
  String levels[10]; //Qui dentro avremo i nostri livelli
  char *ptr = NULL;
  byte index = 0;
  ptr = strtok(myTopic, "/");  // takes a list of delimiters
  while(ptr != NULL){
    levels[index] = ptr;
    index++;
    ptr = strtok(NULL, "/");  // takes a list of delimiters
  }
  String locationId=levels[1];
  String objectId=levels[3];
  String objectInstance=levels[4];
  String resId=levels[5];
  String observe=levels[6];

  StaticJsonDocument<200> doc;
  StaticJsonDocument<200> resp;

  if(locationId == "brm"){
    //we received a message from the Brewmaster
    //Since there's no deviceID, we rewrite the variables
    objectId = levels[2];
    resId = levels[4];
    if(objectId == "3" && resId == "11"){
      //We received an error code
      if(messageTemp == "2"){
        //This is the LWT. We proceed deactivating the observes and turning off the belt
        deactivate();
      }
      else if(messageTemp == "0"){
        //The system woke up again. We reactivate observes
        reactivateObserves();
      }
    }
  }
  if(objectId=="3"){
    //Device itself
    if(objectInstance=="0"){
      DeserializationError error = deserializeJson(doc, messageTemp);
      // Test if parsing succeeds.
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      String value = doc["v"];
      if(value == "ON"){
        resp["v"]=1;
        setStatus(1);
        subscribe();
      }
      else if (value == "OFF"){
        resp["v"] =0;
        setStatus(0);
        subscribe();
      }
      else return;
      uint8_t buffer[128];
      size_t n = serializeJson(resp, buffer);
      checkConnection();
      client.publish(anotherTopic, NULL, true);
      char topicString[60] = "resp/st/";
      strcat(topicString, deviceID);
      strcat(topicString, "/3/0");
      client.publish(topicString, buffer, n, true);
    }
  }
  else if(objectId=="3301"){
    //Illuminance
    if(objectInstance=="0"){
      if(resId=="5700"){
        //we want to take the value
        if(observe=="observe"){
          //we want to observe it
          // Deserialize the JSON document
          DeserializationError error = deserializeJson(doc, messageTemp);
          // Test if parsing succeeds.
          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
          }
          String value = doc["v"];
          resp["type"]="observe";    
          if(value == "ON"){
            resp["v"]="ON";
            observeLight= true;
          }
          else if(value == "OFF"){
            resp["v"]="OFF";
            observeLight = false;
          }
          else return;
          uint8_t buffer[128];
          size_t n = serializeJson(resp, buffer);
          checkConnection();
          client.publish(anotherTopic, NULL, true);
          char topicString[60] = "resp/st/";
          strcat(topicString, deviceID);
          strcat(topicString, "/3301/0/5700");
          client.publish(topicString, buffer, n, false);
        }
        else processLight();
      }
    }
  }

  else if(objectId=="3303"){
    //Temperature
    if(objectInstance=="0"){
      if(resId=="5700"){
        //we want to take the value
        if(observe=="observe"){
          //we want to observe it
          // Deserialize the JSON document
          DeserializationError error = deserializeJson(doc, messageTemp);
          // Test if parsing succeeds.
          if (error) {
            
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
          }
          String value = doc["v"];
          resp["type"]="observe";
          if(value == "ON"){
            resp["v"]="ON";
            observeTemperature= true;
          }
          else if(value == "OFF"){
            resp["v"]="OFF";
            observeTemperature = false;
          }
          else return;
          uint8_t buffer[128];
          size_t n = serializeJson(resp, buffer);
          checkConnection();
          client.publish(anotherTopic, NULL, true);
          char topicString[60] = "resp/st/";
          strcat(topicString, deviceID);
          strcat(topicString, "/3303/0/5700");
          client.publish(topicString, buffer, n, false);
        }
        else processTemperature();
      }
    }
  }


  else if(objectId=="3304"){
    //Humidity
    if(objectInstance=="0"){
      if(resId=="5700"){
        //we want to take the value
        if(observe=="observe"){
          //we want to observe it
          // Deserialize the JSON document
          DeserializationError error = deserializeJson(doc, messageTemp);
          // Test if parsing succeeds.
          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
          }
          String value = doc["v"];
          resp["type"]="observe";
          if(value == "ON"){
            resp["v"]="ON";
            observeHumidity= true;
          }
          else if(value == "OFF"){
            resp["v"]="OFF";
            observeHumidity = false;
          }
          else return;
          uint8_t buffer[128];
          size_t n = serializeJson(resp, buffer);
          checkConnection();
          client.publish(anotherTopic, NULL, true);
          char topicString[60] = "resp/st/";
          strcat(topicString, deviceID);
          strcat(topicString, "/3304/0/5700");
          client.publish(topicString, buffer, n, false);
        }
        else processHumidity();
      }
    }
  }


  else if(objectId=="503"){
    //Flame sensor
    if(objectInstance=="0"){
      if(resId=="5700"){
        //we want to take the value
        if(observe=="observe"){
          //we want to observe it
          // Deserialize the JSON document
          DeserializationError error = deserializeJson(doc, messageTemp);
          // Test if parsing succeeds.
          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
          }
          String value = doc["v"];
          resp["type"]="observe";
          if(value == "ON"){
            resp["v"]="ON";
            observeFlame= true;
          }
          else if(value == "OFF"){
            resp["v"]="OFF";
            observeFlame = false;
          }
          else return;
          uint8_t buffer[128];
          size_t n = serializeJson(resp, buffer);
          checkConnection();
          client.publish(anotherTopic, NULL, true);
          char topicString[60] = "resp/st/";
          strcat(topicString, deviceID);
          strcat(topicString, "/503/0/5700");
          client.publish(topicString, buffer, n, false);
        }
        else processFlame();
      }
    }
  }

  else if(objectId=="3311"){
    //LEDs
    Serial.println("Checking LEDS");
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, messageTemp);
    // Test if parsing succeeds.
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
    String value = doc["v"];
    if(objectInstance=="0"){
      //tempLED
      if(resId=="5850"){
        if(value=="ON"){
          digitalWrite(TEMP_LED, HIGH);
        }
        else if(value=="OFF"){
          digitalWrite(TEMP_LED, LOW);
        }
      }
    }
    else if(objectInstance=="1"){
      //humLED
      if(resId=="5850"){
        if(value=="ON"){
          digitalWrite(HUM_LED, HIGH);
        }
        else if(value=="OFF"){
          digitalWrite(HUM_LED, LOW);
        }
      }
    }
    else if(objectInstance=="2"){
      //lightLED
      if(resId=="5850"){
        if(value=="ON"){
          digitalWrite(LIGHT_LED, HIGH);
        }
        else if(value=="OFF"){
          digitalWrite(LIGHT_LED, LOW);
        }
      }
    }
    else if(objectInstance=="3"){
      //flameLED
      if(resId=="5850"){
        if(value=="ON"){
          digitalWrite(FLAME_LED, HIGH);
        }
        else if(value=="OFF"){
          digitalWrite(FLAME_LED, LOW);
        }
      }
    }
    client.publish(anotherTopic, NULL, true);
  }
}

void checkConnection(){
  if(!client.connected())
    reconnect();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    char clientId[20] = "ESP32-Stocker-";
    strcat(clientId, deviceID);
    if (client.connect(clientId, willTopic, willQoS, willRetain, willMessage)) {
      Serial.println("connected");
      getStatus();
      delay(1000);
      sendBirthMessage();
      subscribe();
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  } 
}


void sendBirthMessage(){
  StaticJsonDocument<200> resp;
  resp["v"]="Online";
  uint8_t buffer[128];
  size_t n = serializeJson(resp, buffer);
  checkConnection();
  char topicString[60]= "resp/st/";
  strcat(topicString, deviceID);
  strcat(topicString, "/3/0/4");
  client.publish(topicString, buffer, n, true);
}
void subscribe(){
  //With the # wildcard we subscribe to all the subtopics of each sublevel. 
  
  if(!active){
    char topicString[20]= "cmd/st/";
    strcat(topicString, deviceID);
    strcat(topicString, "/#");
    client.unsubscribe(topicString);
    char onlyTopic[40] = "cmd/st/";
    strcat(onlyTopic, deviceID);
    strcat(onlyTopic, "/3/0");
    boolean res = client.subscribe(onlyTopic,1);
    if(res)
      Serial.println("Subscribed!");
    else
      Serial.println("Unable to subscribe");
  }
  else{
    char topicString[20]= "cmd/st/";
    strcat(topicString, deviceID);
    strcat(topicString, "/#");  
    boolean res = client.subscribe(topicString, 1);
    if(res)
      Serial.println("Subscribed!");
    else
      Serial.println("Unable to subscribe");
  }
  client.subscribe(brm_LWT_topic, 1);
}


void deactivate(){
  char topicLED[30] = "cmd/st/";
  strcat(topicLED, deviceID);
  for(int i = 0;i<4;i++){
    char tempTopic[30];
    strcpy(tempTopic, topicLED);
    char devLED[3];
    sprintf(devLED, "%d", i);
    strcat(tempTopic,"/3311/");
    strcat(tempTopic, devLED);
    strcat(tempTopic,"5850");
    StaticJsonDocument<200> doc;
    doc["v"]="OFF";
    uint8_t buffer[128];
    size_t n = serializeJson(doc, buffer);
    Serial.println(tempTopic);
    client.publish(tempTopic, buffer, n, true);
  }
  
  observeTemperature = false;
  observeHumidity = false;
  observeLight = false;
  observeFlame = false;
}

void reactivateObserves(){
  observeTemperature = true;
  observeHumidity = true;
  observeLight = true;
  observeFlame = true;
}
void loop() {

  client.loop();
  long now = millis();
  if(now - lastConnectionCheck >= connectionInterval){
    checkConnection();
    now=millis();
    lastConnectionCheck= now;
  }
  if(active){
    //Serial.println("Active");
    if(observeTemperature){
      //Qui dentro entriamo se l'observe per la temp è attivo
      if(now - lastTemperatureCheck >= temperatureInterval){
        //Time to read and send a new temperature data.
        processTemperature();
        now=millis();
        lastTemperatureCheck = now;
      }
    }
    if(observeHumidity){
      //Qui entriamo se l'observe per l'umidità è attivo
      if(now - lastHumidityCheck >= humidityInterval){
        //Time to read and send a humidity update.
        processHumidity();
        now = millis();
        lastHumidityCheck = now;
      }
    }
    if(observeLight){
      //Qui entriamo se l'observe per la luminosità è attivo
      if(now - lastLightCheck >= lightInterval){
        //Time to read and send an illuminance update.
        processLight();
        now = millis();
        lastLightCheck = now;
      }
    }
    if(observeFlame){
      //Qui entriamo se l'observe per la flame detection è attivo
      if(now - lastFlameCheck >= flameInterval){
        //Time to read and send flame updates.
        processFlame();
        now = millis();
        lastFlameCheck = now;
      }
    }
  }
  delay(tick);
}

/*
 * This method processes a Temperature request. It gets the value from the sensor and,
 * the, it sends it through the respective MQTT topic. The following 4 methods work
 * in the same way.
 */
void processTemperature(){
  Serial.println("Sending temperature..");
  float temperature= dht.readTemperature();
  char tempString[8];
  dtostrf(temperature, 1, 2, tempString);
  StaticJsonDocument<200> doc;
  //doc["time"]=getTime();
  doc["v"]=tempString;
  uint8_t buffer[128];
  size_t n = serializeJson(doc, buffer);
  checkConnection();
  char topicString[60] = "data/st/";
  strcat(topicString, deviceID);
  strcat(topicString, "/3303/0/5700");
  Serial.println(temperature);
  client.publish(topicString, buffer, n, false);
}

void processHumidity(){
  Serial.println("Sending humidity");
  float humidity= dht.readHumidity();
  char humString[8];
  dtostrf(humidity, 1, 2, humString);
  StaticJsonDocument<200> doc;
  //doc["time"]=getTime();
  doc["v"]=humString;
  uint8_t buffer[128];
  size_t n = serializeJson(doc, buffer);
  checkConnection();
  char topicString[60] = "data/st/";
  strcat(topicString, deviceID);
  strcat(topicString, "/3304/0/5700");
  Serial.println(humidity);
  client.publish(topicString, buffer, n, false);
}

/*
 * This method calculates a percentage of the detected light, from 100 (Very Bright)
 * to 0 (Dark), and forwards it through the specific MQTT topic
 */
void processLight(){
  Serial.println("Sending light");
  int l = analogRead(LIGHT_PIN);
  uint8_t value= map(l,4095,0,0,100);
  char lightString[8];
  dtostrf(value, 1, 2, lightString);
  StaticJsonDocument<200> doc;
  //doc["time"]=getTime();
  doc["v"]=lightString;
  uint8_t buffer[128];
  size_t n = serializeJson(doc, buffer);
  checkConnection();
  char topicString[60] = "data/st/";
  strcat(topicString, deviceID);
  strcat(topicString, "/3301/0/5700");
  Serial.println(l);
  client.publish(topicString, buffer, n, false);
}

void processFlame(){
  Serial.println("Sending flame");
  boolean flame = digitalRead(FLAME_PIN);
  StaticJsonDocument<200> doc;
  //doc["time"]=getTime();
  doc["v"]=String(flame);
  uint8_t buffer[128];
  size_t n = serializeJson(doc, buffer);
  checkConnection();
  char topicString[60] = "data/st/";
  strcat(topicString, deviceID);
  strcat(topicString, "/503/0/5700");
  Serial.println(flame);
  client.publish(topicString, buffer, n, false);
}
/* 
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    return(0);
  }
  time(&now);
  return now;
}*/
