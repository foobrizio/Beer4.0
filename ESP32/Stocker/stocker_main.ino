#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#include <WiFi.h>
#include <PubSubClient.h>

#include <string.h>




// DHT11 Configuration (temperature / humitity)
#define DHTTYPE DHT11
#define DHTPIN 27
DHT dht(DHTPIN,DHTTYPE);

// Flame and Light sensors Configuration
#define LIGHT_PIN 34 //Analog
#define FLAME_PIN 14 //Digital

//Dati per la connessione WiFi
const char* ssid = "Gabriele-2.4GHz";
const char* pass = "i8Eo6zdPhvfqgzPVKo85hWP1";
//const char* ssid = "dlink-8D53";
//const char* pass = "Nuvola&Penny_53";

//Dati per la configurazione MQTT
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

//Dati per la connessione al server NTP (per l'orario)
//WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP);

// Qeusto boolean attiva o disattiva la scheda
boolean active=true;
// Questi altri boolean implementano l'observe dei vari dati
boolean observeTemperature= true;
boolean observeHumidity = true;
boolean observeLight = true;
boolean observeFlame = true;
// Il tick è la durata base di delayTime per il nostro ESP32
unsigned long tick=1000;

//Qui abbiamo 4 coppie diverse counter-soglia per i diversi tipi di dati
uint8_t tempTickCounter=0;
uint8_t tempTickQty=10; 
uint8_t humTickCounter = 0;
uint8_t humTickQty =10;
uint8_t lightTickCounter=0;
uint8_t lightTickQty=30; 
uint8_t flameTickCounter=0;
uint8_t flameTickQty=3; 

/* Questo è il documento JSON che creiamo con tutti i valori ricevuti dai sensori.
 * Man mano che riceviamo valori, il documento cresce. Alla fine viene serializzato
 * ed inviato tramite MQTT
 */
DynamicJsonDocument jdoc(3600);
JsonArray array;

void setup() {

  Serial.begin(115200);
  setup_wifi();
  //setupNTP();
  setup_mqtt();
  setupSensors();
}

/*
 * Questo metodo serve ad implementare la connessione I2C con il bme280 
 */
void setupSensors(){
  
  dht.begin();
  pinMode(LIGHT_PIN, INPUT_PULLUP);
  pinMode(FLAME_PIN, INPUT_PULLUP);
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
    delay(500);
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
  reconnect();
}

/*
 * Configurazione del server NTP per recuperare l'orario da internet
 */
void setupNTP(){
  //timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  //timeClient.setTimeOffset(3600);
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.println("Message arrived.");
  String messageTemp;
  
  //Qui convertiamo il byte* message in una stringa
  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }
  //Qui convertiamo il char* topic in un char[]
  char myTopic[50];
  strcpy(myTopic, topic);

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
  String objectId=levels[4];
  String objectInstance=levels[5];
  String resId=levels[6];
  String observe=levels[7];

  StaticJsonDocument<200> doc;
  StaticJsonDocument<200> resp;
  
  if(objectId=="3301"){
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
          char buffer[128];
          size_t n = serializeJson(resp, buffer);
          client.publish("resp/brewIoT/st/0/3301/0/5700", buffer, n);
          
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
          char buffer[128];
          size_t n = serializeJson(resp, buffer);
          client.publish("resp/brewIoT/st/0/3303/0/5700", buffer, n);
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
          char buffer[128];
          size_t n = serializeJson(resp, buffer);
          client.publish("resp/brewIoT/st/0/3304/0/5700", buffer, n);
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
          char buffer[128];
          size_t n = serializeJson(resp, buffer);
          client.publish("resp/brewIoT/st/0/503/0/5700", buffer, n);
        }
        else processFlame();
      }
    }
  }
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    String clientId = "ESP32-Stocker-0";
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      delay(1000);
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

void subscribe(){
  //With the # wildcard we subscribe to all the subtopics of each sublevel. 
  boolean res = client.subscribe("cmd/brewIoT/st/0/#");
  if(res){
    Serial.println("Subscribed!");
  }
  else{
    Serial.println("Unable to subscribe");
  }
}


void loop() {

  client.loop();
  if(active){
    //Serial.println("Active");
    if(observeTemperature){
      //Qui dentro entriamo se l'observe per la temp è attivo
      tempTickCounter++;
      if(tempTickCounter>=tempTickQty){
        //Time to read and send a new temperature data.
        processTemperature();
        tempTickCounter=0;
      }
    }
    if(observeHumidity){
      //Qui entriamo se l'observe per l'umidità è attivo
      humTickCounter++;
      if(humTickCounter>=humTickQty){
        //Time to read and send a humidity update.
        processHumidity();
        humTickCounter=0;

      }
    }
    if(observeLight){
      //Qui entriamo se l'observe per la luminosità è attivo
      lightTickCounter++;
      if(lightTickCounter>=lightTickQty){
        //Time to read and send an illuminance update.
        processLight();
        lightTickCounter=0;
      }
    }
    if(observeFlame){
      //Qui entriamo se l'observe per la flame detection è attivo
      flameTickCounter++;
      if(flameTickCounter>=flameTickQty){
        //Time to read and send flame updates.
        processFlame();
        flameTickCounter=0;
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
  doc["v"]=tempString;
  char buffer[128];
  size_t n = serializeJson(doc, buffer);
  client.publish("data/brewIoT/st/0/3303/0/5700", buffer, n);
}

void processHumidity(){
  Serial.println("Sending humidity");
  float humidity= dht.readHumidity();
  char humString[8];
  dtostrf(humidity, 1, 2, humString);
  StaticJsonDocument<200> doc;
  doc["v"]=humString;
  char buffer[128];
  size_t n = serializeJson(doc, buffer);
  client.publish("data/brewIoT/st/0/3304/0/5700", buffer, n);
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
  doc["v"]=lightString;
  char buffer[128];
  size_t n = serializeJson(doc, buffer);
  client.publish("data/brewIoT/st/0/3301/0/5700", buffer, n);
}

void processFlame(){
  Serial.println("Sending flame");
  boolean flame = digitalRead(FLAME_PIN);
  StaticJsonDocument<200> doc;
  doc["v"]=String(flame);
  char buffer[128];
  size_t n = serializeJson(doc, buffer);
  client.publish("data/brewIoT/st/0/503/0/5700", buffer, n);
  //client.publish("data/brewIoT/st/0/503/0/5700", flame);
}

int64_t get_time() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}
