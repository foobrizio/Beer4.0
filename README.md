# Beer4.0

--MQTT Topic Hierarchy

La struttura momentaneamente designata per i topic sarà la seguente:
  Beer4.0/<DeviceName>/<DeviceNumber>/<TopicName>

Ad esempio, per l'esp Stocker, avremo un topic del genere:
  Beer4.0/Stocker/1/Sensors
 
  
  
  
Ogni dispositivo avrà un nome. I nomi sono:
  1) Brewer, per gli ESP32 attaccati alle cisterne
  2) Stocker, per l'ESP32 in magazzino
  3) BrewMaster, per il Raspberry PI
