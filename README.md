# Beer4.0

--MQTT Topic Hierarchy

La struttura momentaneamente designata per i topic sarà la seguente:
  brewIoT/*DeviceName*/*DeviceNumber*/*TopicName*

Ad esempio, per l'esp Stocker, avremo un topic del genere:
  brewIoT/stocker/1/sensors
 

Ogni dispositivo avrà un nome. I nomi sono:
  1) brewer, per gli ESP32 attaccati alle cisterne
  2) stocker, per l'ESP32 in magazzino
  3) brewmaster, per il Raspberry PI
