
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include "WiFi.h"
#include <PubSubClient.h>

// BME280 Temperature and Humidity sensor stuff
Adafruit_BME280 bme; // I2C

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


// Il tick è la durata base di delayTime per il nostro ESP32
unsigned long tick=1000;

unsigned char bmeTickCounter=0; //Questo è il counter per le misurazioni col bme
unsigned char bmeTickQty=10; //Questo è il numero di tick da ottenere per avere una misurazione bme

unsigned char flushTickCounter=0; //Questo è il counter per il flush dei dati via MQTT.
unsigned char flushTickQty=60; //Ogni tot tick inviamo tutti i dati raccolti via MQTT.

void setup() {

  Serial.begin(115200);
  connectToWifi();
  bmeStart();
  // put your setup code here, to run once:

}

/*
 * Questo metodo serve ad implementare la connessione I2C con il bme280 
 */
void bmeStart(){
  
  bool status;
  status = bme.begin(0x76);  
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  Serial.println("BME280 Sensor found.");
}


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
 
  // Feel free to add more if statements to control more GPIOs with MQTT
 
  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
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

  float temp;
  float hum;

  bmeTickCounter++;
  flushTickCounter++;
  if(bmeTickCounter == bmeTickQty){
    //Se entriamo qui, allora facciamo una misurazione e la salviamo in locale.
    temp = bme.readTemperature();
    hum = bme.readHumidity();
    bmeTickCounter = 0;
  }

  if(flushTickCounter == flushTickQty){
    //Se entriamo qui, allora facciamo una flush dei dati locali via MQTT
    flush();
    flushTickCounter = 0;    
  }
}

/*
 * Il metodo flush() serve per inviare tutti i dati raccolti dall'ESP via MQTT.
 * Una volta inviati, i dati locali vengono cancellati.
 */
void flush(){

}

