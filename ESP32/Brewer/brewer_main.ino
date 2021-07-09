#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

#include <WiFi.h>
#include <PubSubClient.h>

#include <string.h>

//Questa libreria serve per l'NTP
#include <time.h>

//Queste librerie servono per il termometro DS18B20
#include "OneWire.h"
#include "DallasTemperature.h"

#define TEMP_LED 4
//Configurazione termometro 
#define SENSOR_PIN 13
OneWire oneWire(SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

//ID della scheda
const char deviceID[] = "10";

//Dati per la connessione WiFi
const char* ssid = "Gabriele-2.4GHz";
const char* pass = "i8Eo6zdPhvfqgzPVKo85hWP1";

//Dati per la configurazione MQTT
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

byte willQoS = 0;
char willTopic[60];
char willMessage[60];
boolean willRetain = false;

WiFiClient espClient;
PubSubClient client(espClient);

//Dati per la connessione al server NTP (per l'orario)
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;


boolean active=true;
boolean observeTemperature= true;

// Il tick è la durata base di delayTime per il nostro ESP32
unsigned long tick=1000;

long lastConnectionCheck=0;
long lastTemperatureCheck=0;

const long connectionInterval = 5000; //milliseconds
const long temperatureInterval = 30000; //milliseconds
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
  // Init and get the time
  setupTime();
  setupWires();
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
  createLWTData();
  reconnect();
  delay(2000);
  StaticJsonDocument<200> resp;
  resp["v"]="rebooted";
  uint8_t buffer[128];
  size_t n = serializeJson(resp, buffer);
  checkConnection();
  char topicString[60]= "resp/br/";
  strcat(topicString, deviceID);
  strcat(topicString, "/3/0/4");
  client.publish(topicString, buffer, n, true);
}

/*
 * Configurazione del server NTP per recuperare l'orario da internet
 */
void setupTime(){
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println(getTime());
}

void setupWires(){
  tempSensor.begin();
  pinMode(TEMP_LED,OUTPUT);
  digitalWrite(TEMP_LED,LOW);
}

void checkConnection(){
  if(!client.connected()){
    reconnect();
  }
}

void createLWTData(){
  strcat(willTopic, "resp/br/");
  strcat(willTopic, deviceID);
  strcat(willTopic, "/3/0/11");
  StaticJsonDocument<20> errorDoc;
  errorDoc["error"]=2;
  serializeJson(errorDoc, willMessage);
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.println("Message arrived.");
  String messageTemp;
  
  //Qui convertiamo il byte* message in una stringa
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
  String objectId=levels[3];
  String objectInstance=levels[4];
  String resId=levels[5];
  String observe=levels[6];

  StaticJsonDocument<200> doc;
  StaticJsonDocument<200> resp;

  if(objectId=="3303"){
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
          char topicString[60]= "resp/br/";
          strcat(topicString, deviceID);
          strcat(topicString, "/3303/0/5700");
          client.publish(topicString, buffer, n, false);
          //Serial.println(anotherTopic);
          client.publish(anotherTopic, NULL, true);
        }
        else processTemperature();
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
    client.publish(anotherTopic, NULL, true);
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    char clientId[18] = "ESP32-Brewer-";
    strcat(clientId, deviceID);
    Serial.println(clientId);
    if (client.connect(clientId, willTopic, willQoS, willRetain, willMessage)) {
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
  char topicString[60]= "cmd/br/";
  strcat(topicString, deviceID);
  strcat(topicString, "/#");
  Serial.println(topicString);  
  boolean res = client.subscribe(topicString,1);
  if(res){
    Serial.println("Subscribed!");
  }
  else{
    Serial.println("Unable to subscribe");
  }
}

void loop() {
  // put your main code here, to run repeatedly:
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
        processTemperature();
        now=millis();
        lastTemperatureCheck = now;
      }
    }
  }
  delay(tick);
}

void processTemperature(){
  Serial.println("Sending temperature..");
  tempSensor.requestTemperatures(); 
  float temperatureC = tempSensor.getTempCByIndex(0);
  char tempString[8];
  dtostrf(temperatureC, 1, 2, tempString);
  StaticJsonDocument<200> doc;
  doc["tstamp"]=getTime();
  doc["v"]=temperatureC;
  uint8_t buffer[128];
  size_t n = serializeJson(doc, buffer);
  checkConnection();
  Serial.print("Temperature: ");
  Serial.println(temperatureC);
  char topicString[60]= "data/br/";
  strcat(topicString, deviceID);
  strcat(topicString, "/3303/0/5700");
  client.publish(topicString, buffer, n, false);
}

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
}

