# BrewIoT

--MQTT Topic Hierarchy

La struttura dei topic è parte integrante della semantica OMA lwM2M, quindi seguirà questa dicitura:
/{ObjectID}/{ObjectInstance}/{ResourceID}.

Ogni ID è un intero di 16-bit, mentre le istanze sono da 8 bit. I vari ID possono essere reperiti dall'OMA Registry online:
http://openmobilealliance.org/wp/OMNA/LwM2M/LwM2MRegistry.html .
L'oggetto rappresenta il dispositivo, mentre la risorsa è il tipo di sensore. Quindi avremo un topic per ogni tipo di dato raccolto.
Il payload invece sarà strutturato da una stringa JSON.

Ogni dispositivo avrà un nome. I nomi sono:
  1) brewer, per gli ESP32 attaccati alle cisterne
  2) stocker, per l'ESP32 in magazzino
  3) brewmaster, per il Raspberry PI
