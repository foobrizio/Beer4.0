# BrewIoT

--MQTT Topic Hierarchy

La struttura dei topic è parte integrante della semantica OMA lwM2M, quindi seguirà questa dicitura:
brewIoT/{DeviceID}/{DeviceInstance}/{ObjectID}/{ObjectInstance}/{ResourceID}.

Ogni ID è un intero di 16-bit, mentre le istanze sono da 8 bit. I vari ID possono essere reperiti dall'OMA Registry online:
http://openmobilealliance.org/wp/OMNA/LwM2M/LwM2MRegistry.html .
L'oggetto rappresenta il dispositivo, mentre la risorsa è il tipo di sensore. Quindi avremo un topic per ogni tipo di dato raccolto.
Il payload invece sarà strutturato da una stringa JSON.

Ecco una lista di ID che possono fare al caso nostro:
3    : Device;
503  : Fire Alarm;
3301 : Luminosità ( Illuminance);
3303 : Temperatura;
3304 : Umidità;
3306 : Actuation (On/Off);
3312 : Power Control (Relay);
5601 : Min Range Value;
5602 : Max Range Value;
5700 : Sensor Value; //Il valore del sensore
5701 : Sensor Units; //L'unità di misura del valore del sensore
5751 : Sensor Type
5850 : On/Off;

10278: High Temperature Alarm;
10350: Light;
10353: Warning Light;

Ogni dispositivo avrà un nome. I nomi sono:
  1) brewer, per gli ESP32 attaccati alle cisterne
  2) stocker, per l'ESP32 in magazzino
  3) brewmaster, per il Raspberry PI


Un esempio di topic sarà il seguente:

brewIoT/st/0/3304/0/5700

st è il dispositivo stocker, seguito dall'istanza del dispositivo (perchè potremo averne più di uno), 0 in questo caso. 3304/0 è l'istanza 0 dell'oggetto Umidità, perchè potremmo avere più di un sensore di questo tipo sul dispositivo, e infine 5700 è il valore del sensore.

Il fullTopic avrà, in aggiunta, un prefisso che serve a specificare il tipo di messaggio che viaggia da esso. Avremo tre tipi di prefissi:
  1) cmd, dove invieremo comandi o richieste di valori;
  2) resp, dove invieremo eventuali feedback di responso ad eventuali comandi;
  3) data, dove invieremo i dati dei sensori, magari richiesti in precedenza.

Un esempio di fullTopic sono i seguenti:

cmd/brewIoT/st/0/3304/0/5700 (per inviare la richiesta di leggere il valore temperatura);
data/brewIoT/st/0/3304/0/5700 (per inviare i dati del sensore come risposta).
