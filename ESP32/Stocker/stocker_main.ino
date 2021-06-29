#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#include "WiFi.h"
#include <PubSubClient.h>


// DHT11 Configuration (temperature / humitity)
#define DHTTYPE DHT11
#define DHTPIN 27
DHT dht(DHTPIN,DHTTYPE);

// Flame and Light sensors Configuration
#define LIGHT_PIN 12 //Analog
#define FLAME_PIN 14 //Digital

//Dati per la connessione WiFi
const char* ssid = "Gabriele-2.4GHz";
const char* password = "i8Eo6zdPhvfqgzPVKo85hWP1";

//Dati per la configurazione MQTT
const char* mqtt_server = "broker.hivemq.com";
const int mqttPort = 1883;
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

//Dati per la connessione al server NTP (per l'orario)
//WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP);

// Il tick è la durata base di delayTime per il nostro ESP32
unsigned long tick=1000;

unsigned char sensorTickCounter=0; //Questo è il counter per le misurazioni col bme
unsigned char sensorTickQty=10; //Questo è il numero di tick da ottenere per avere una misurazione bme

unsigned char flushTickCounter=0; //Questo è il counter per il flush dei dati via MQTT.
unsigned char flushTickQty=60; //Ogni tot tick inviamo tutti i dati raccolti via MQTT.


//Qui definiamo una Mappa per raccogliere e salvare i dati ricevuti dai sensori
typedef struct{
  float temperature;
  float humidity;
  int light;
  byte flame;
  long time;
}Map;

//Creazione della Mappa e del relativo puntatore.
Map myMap[3600];
unsigned char mapPointer= 0;

void setup() {

  Serial.begin(115200);

  connectToWiFi();
  setupNTP();
  setupMQTT();
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
void connectToWiFi(){

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("Connected to the WiFi network!!");
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

/*
 * Configurazione del broker MQTT.
 */
void setupMQTT(){
  client.setServer(mqtt_server,mqttPort);
  client.setCallback(callback);
}



void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
   
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
 
  //Da qui controlliamo i messaggi che riceviamo.
  if (String(topic) == "beer4.0/stocker/1/output") {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
    }
    else if(messageTemp == "off"){
      Serial.println("off");
    }
  }
}

void loop() {

  sensorTickCounter++;
  flushTickCounter++;

  if(sensorTickCounter == sensorTickQty){
    //Se entriamo qui, allora facciamo una misurazione e la salviamo in locale.
    measureSensors();
  }

  if(flushTickCounter >= flushTickQty){
    //Se entriamo qui, allora facciamo una flush dei dati locali via MQTT
    char result = flush();
    if(result == 0){
      //La flush è avvenuta con successo
      flushTickCounter = 0;
      mapPointer = 0;
    }
  }

  delay(tick);
}


void measureSensors(){
  byte light = senseLight();
  byte flame = digitalRead(FLAME_PIN);
  myMap[mapPointer].temperature= dht.readTemperature();
  myMap[mapPointer].humidity = dht.readHumidity();
  myMap[mapPointer].light= light;
  myMap[mapPointer].flame = flame;
  myMap[mapPointer].time = 0L;
  mapPointer++;
  sensorTickCounter = 0;
}

/*
 * This method returns a percentage of the detected light, from 100 (Very Bright)
 * to 0 (Dark)
 */
int senseLight(){
  int l = analogRead(LIGHT_PIN);
  return map(l,4095,0,0,100);
}
/*
 * Il metodo flush() serve per inviare tutti i dati raccolti dall'ESP via MQTT.
 * Una volta inviati, i dati locali vengono cancellati.
 * Il metodo ritorna l'esito dell'operazione. Se l'esito è positivo, allora i dati
 * locali possono essere eliminati, altrimenti vengono conservati.
 */
char flush(){

}
