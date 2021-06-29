#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#include <WiFi.h>
#include <PubSubClient.h>


// DHT11 Configuration (temperature / humitity)
#define DHTTYPE DHT11
#define DHTPIN 27
DHT dht(DHTPIN,DHTTYPE);

// Flame and Light sensors Configuration
#define LIGHT_PIN 12 //Analog
#define FLAME_PIN 14 //Digital

//Dati per la connessione WiFi
const char* ssid = "ssid";
const char* pass = "pass;

//Dati per la configurazione MQTT
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

//Dati per la connessione al server NTP (per l'orario)
//WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP);

// Il tick è la durata base di delayTime per il nostro ESP32
unsigned long tick=1000;

uint8_t sensorTickCounter=0; //Questo è il counter per le misurazioni col bme
uint8_t sensorTickQty=10; //Questo è il numero di tick da ottenere per avere una misurazione bme

uint8_t flushTickCounter=0; //Questo è il counter per il flush dei dati via MQTT.
uint8_t flushTickQty=60; //Ogni tot tick inviamo tutti i dati raccolti via MQTT.


//Qui definiamo una Mappa per raccogliere e salvare i dati ricevuti dai sensori
typedef struct{
  float temperature;
  float humidity;
  uint8_t light;
  byte flame;
  long time;
}Map;

//Creazione della Mappa e del relativo puntatore.
Map myMap[3600];
uint8_t mapPointer= 0;

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
   
  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }
 
  //Da qui controlliamo i messaggi che riceviamo.
  if (String(topic) == "brewIoT/stocker/1/command") {

    if(messageTemp == "refresh"){
      flush();
    }
    else if(messageTemp == "off"){
      Serial.println("off");
    }
    else if(messageTemp == "on"){
      Serial.println("on");
    }
  }
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    String clientId = "Esp32-Brewer";
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
  boolean result = client.subscribe("brewIoT/stocker/1/command");
  if(result){
    Serial.println("Subscribed!!!");
  }
}

void loop() {

  client.loop();
  sensorTickCounter++;
  flushTickCounter++;

  if(sensorTickCounter == sensorTickQty){
    //Se entriamo qui, allora facciamo una misurazione e la salviamo in locale.
    measureSensors();
  }

  if(flushTickCounter >= flushTickQty){
    //Se entriamo qui, allora facciamo una flush dei dati locali via MQTT
    byte result = flush();
    if(result == 0){
      //La flush è avvenuta con successo
      flushTickCounter = 0;
      mapPointer = 0;
    }
  }
  delay(tick);
}

void measureSensors(){
  uint8_t light = senseLight();
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
uint8_t senseLight(){
  int l = analogRead(LIGHT_PIN);
  return map(l,4095,0,0,100);
}
/*
 * Il metodo flush() serve per inviare tutti i dati raccolti dall'ESP via MQTT.
 * Una volta inviati, i dati locali vengono cancellati.
 * Il metodo ritorna l'esito dell'operazione. Se l'esito è positivo, allora i dati
 * locali possono essere eliminati, altrimenti vengono conservati.
 */
byte flush(){
  
  if (!client.connected()) {
    reconnect();
  }
  client.publish("brewIoT/stocker/1/sensors","yayaya");
  return 0;
}
