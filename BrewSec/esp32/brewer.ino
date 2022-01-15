#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include <string.h>


//Queste librerie servono per il termometro DS18B20
#include "OneWire.h"
#include "DallasTemperature.h"

//CA certificate for SSL
const char* CA_cert = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIEBTCCAu2gAwIBAgIJAJEF6+cgWbH1MA0GCSqGSIb3DQEBCwUAMIGYMQswCQYD\n" \
"VQQGEwJJVDEOMAwGA1UECAwFSXRhbHkxDjAMBgNVBAcMBVJlbmRlMQ8wDQYDVQQK\n" \
"DAZVbmljYWwxDjAMBgNVBAsMBURpbWVzMRgwFgYDVQQDDA8xOTIuMTY4LjEwMi4x\n" \
"NjIxLjAsBgkqhkiG9w0BCQEWH2ZhYnJpemlvZ2FicmllbGVAcHJvdG9ubWFpbC5j\n" \
"b20wHhcNMjIwMTE0MTcwNjQ0WhcNMjcwMTE0MTcwNjQ0WjCBmDELMAkGA1UEBhMC\n" \
"SVQxDjAMBgNVBAgMBUl0YWx5MQ4wDAYDVQQHDAVSZW5kZTEPMA0GA1UECgwGVW5p\n" \
"Y2FsMQ4wDAYDVQQLDAVEaW1lczEYMBYGA1UEAwwPMTkyLjE2OC4xMDIuMTYyMS4w\n" \
"LAYJKoZIhvcNAQkBFh9mYWJyaXppb2dhYnJpZWxlQHByb3Rvbm1haWwuY29tMIIB\n" \
"IjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAwWYNWTMcfpsSwTxjidIpjIUd\n" \
"e6fFA8vMZN97HDm1R8J+t+6/ZCzS5LQj6cVH/djq+D3tEKZLvx4WgnX4hD+TjBCd\n" \
"M0pr3QhP8E37dqwkWJas+47tJUdcdpW73Ow5yp0Kv9LXThFS8fDqnma6FvrNznhV\n" \
"R67TPoVbCiu2N/7wsQ+NHBCIzWk4FN0f+43NVsWFDuLIo5pVXxANdITUqZNCuYWg\n" \
"0AbkdVZN075qT4G5ksONmPi4YzwnGBZ3uY/gvFHGvzwaTLlOrZtaawd8zBUIFj/E\n" \
"SlZZv8ie9XQisQe+VYZpzzUSLDpXFGObOegcXXk5kIHu5I9qds8hRfBqfJmaGwID\n" \
"AQABo1AwTjAdBgNVHQ4EFgQUEucfoG/fw8N1A5xyaHmzLPiNOT4wHwYDVR0jBBgw\n" \
"FoAUEucfoG/fw8N1A5xyaHmzLPiNOT4wDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0B\n" \
"AQsFAAOCAQEAjo3C5zbEb9S9BbVmW5qnhULae8LtZTEuY86yw22UJv5WT30mx5Qq\n" \
"w14KcOpcWJeAahJcL8jPUN5idiEIAIS8qwBZmrpm4jQB0qZ4IY9iQGLW9fu9tH/r\n" \
"k7AYCsvlFfQ2Mg84V73Hdu6UqeixYLu2XJZJRfB52mcRZonOZiN9iROKDHVLwPyV\n" \
"B/LtGVW73blny9IIdfx0Nmr5pr7At8VJJCL9O8kKuX9odmiLQo+Ywe67eQUXDPWJ\n" \
"rJdmxnvdvdpu465Pt3TFbN5xNdR0JsxhosNKKUCvVxfnv677QrPcdUqwvhQDAr8G\n" \
"ssy3fcuso/m6p29RILhUhlwi601KO8fbSA==\n" \
"-----END CERTIFICATE-----";

//Questa libreria serve per salvare dei valori nella EEPROM
#include <EEPROM.h>
#define EEPROM_SIZE 20
#define STATUS_ADDRESS 10

#define TEMP_LED 4
//Configurazione termometro 
#define SENSOR_PIN 13
OneWire oneWire(SENSOR_PIN);
DallasTemperature tempSensor(&oneWire);

//ID della scheda
const char deviceID[] = "10";

//Dati per la connessione WiFi
const char* ssid = "";
const char* pass = "";

//Dati per la configurazione MQTT
const char* mqtt_server = "192.168.102.162";
const int mqtt_port = 8883;
const char* mqtt_user = "esp32";
const char* mqtt_pass = "esp32";
const char* brm_LWT_topic = "resp/brm/3/0/11";

byte willQoS = 1;
char willTopic[60];
char willMessage[60];
boolean willRetain = true;

WiFiClientSecure espClient;
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
const long temperatureInterval = 60000; //milliseconds
/* Questo è il documento JSON che creiamo con tutti i valori ricevuti dai sensori.
 * Man mano che riceviamo valori, il documento cresce. Alla fine viene serializzato
 * ed inviato tramite MQTT
 */
DynamicJsonDocument jdoc(3600);
JsonArray array;

void setup() {

  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  setup_wifi();
  //setupNTP();
  setup_mqtt();
  // Init and get the time
  // setupTime();
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

  espClient.setCACert(CA_cert);
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

void sendBirthMessage(){
  StaticJsonDocument<1000> resp;
  resp["v"]="Online";
  uint8_t buffer[128];
  size_t n = serializeJson(resp, buffer);
  checkConnection();
  char topicString[60]= "resp/br/";
  strcat(topicString, deviceID);
  strcat(topicString, "/3/0/4");
  client.publish(topicString, buffer, n, true);
}

void createLWTData(){
  strcat(willTopic, "resp/br/");
  strcat(willTopic, deviceID);
  strcat(willTopic, "/3/0/4");
  StaticJsonDocument<1000> resp;
  resp["v"]="Offline";
  uint8_t buffer[128];
  size_t n = serializeJson(resp, buffer);
  serializeJson(resp, willMessage);
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
  if(value==0){
    digitalWrite(TEMP_LED, LOW);
    active=false;
  }
  else if(value==1)
    active=true;
  EEPROM.commit();
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived: ");
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
  String locationId = levels[1];
  String objectId=levels[3];
  String objectInstance=levels[4];
  String resId=levels[5];
  String observe=levels[6];

  StaticJsonDocument<1000> doc;
  StaticJsonDocument<1000> resp;
  
  if(locationId == "brm"){
    //we received a message from the Brewmaster
    //Since there's no deviceID, we rewrite the variables
    objectId = levels[2];
    resId = levels[4];
    if(objectId == "3" && resId == "11"){
      //We received an error code
      if(messageTemp == "2"){
        //This is the LWT. We proceed deactivating the observes and turning off the belt
        deactivateFermentor();
      }
      if(messageTemp == "0"){
        //The system woke up again. We reactivate the observes
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
        Serial.println(error.c_str());
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
      client.publish(anotherTopic, "", true);
      char topicString[60] = "resp/br/";
      strcat(topicString, deviceID);
      strcat(topicString, "/3/0");
      client.publish(topicString, buffer, n, true);
    }
  }
  else if(objectId=="3303"){
    //Serial.println("Temperature");
    //Temperature
    if(objectInstance=="0"){
      if(resId=="5700"){
        //we want to take the value
        if(observe=="observe"){
          //we want to observe it
          // Deserialize the JSON document
          Serial.println("3303_0_5700");
          Serial.println(messageTemp);
          DeserializationError error = deserializeJson(doc, messageTemp);
          
          // Test if parsing succeeds.
          if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.c_str());
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
          client.publish(anotherTopic, "", true);
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
        Serial.println(error.c_str());
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
    client.publish(anotherTopic, "", true);
  }
}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    char clientId[18] = "ESP32-Brewer-";
    strcat(clientId, deviceID);
    if (client.connect(clientId, mqtt_user, mqtt_pass, willTopic, willQoS, willRetain, willMessage)) {
      Serial.println("connected");
      getStatus();
      sendBirthMessage();
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
  if(!active){
    char topicString[20]= "cmd/br/";
    strcat(topicString, deviceID);
    strcat(topicString, "/#");
    client.unsubscribe(topicString);
    char onlyTopic[40] = "cmd/br/";
    strcat(onlyTopic, deviceID);
    strcat(onlyTopic, "/3/0");
    boolean res = client.subscribe(onlyTopic,1);
    if(res)
      Serial.println("Subscribed!");
    else
      Serial.println("Unable to subscribe");
  }
  else{
    char topicString[20]= "cmd/br/";
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

void deactivateFermentor(){
  //First of all, we deactivate the warming belt
  char topicBelt[30] = "br/";
  int tempID = atoi(deviceID)+1;
  char beltID[5];
  sprintf(beltID, "%d", tempID);
  //Serial.println(beltID);
  strcat(topicBelt, beltID);
  strcat(topicBelt, "/3306/0/cmnd/POWER");
  Serial.println(topicBelt);
  char *payload = "OFF";
  client.publish(topicBelt, payload, true);
  char topicLed[30] = "cmd/br/";
  strcat(topicLed, deviceID);
  strcat(topicLed, "/3311/0/5850");
  StaticJsonDocument<1000> doc;
  //doc["tstamp"]=getTime();
  doc["v"]="OFF";
  uint8_t buffer[128];
  size_t n = serializeJson(doc, buffer);
  client.publish(topicLed, buffer, n, true);
  //then, we deactivate eventual observeS
  observeTemperature = false;
}

void reactivateObserves(){
  observeTemperature = true;
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
  StaticJsonDocument<1000> doc;
  //doc["tstamp"]=getTime();
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
