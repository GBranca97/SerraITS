#include <Arduino.h>
#include <Wire.h>
#include "SparkFunHTU21D.h"
// #include <Adafruit_Sensor.h>
#define SENDIST 5 // distanza sensore cm


//=>Guardando schema esp32 adc2 non è usabile mentre si usa il wifi,
// quindi solamente adc1 è dipsonibile.
//Ricordarsi inoltre che ESP32 va in 3.3 VOLT non 5v come arduino

// pin sensori piante
#define Terreno_Pianta_1 36
#define Terreno_Pianta_2 39
#define Terreno_Pianta_3 32
#define Terreno_Pianta_4 33

// pin relay elettrovalvole
#define Elettrovalvola1 19
#define Elettrovalvola2 23
#define Elettrovalvola3 18
#define Elettrovalvola4 5
#define Pompa 17
#define Ventola 17

// pin ultrasuoni
#define trigUS 14
#define echoUS 12

HTU21D SensoreTempSerra; //oggetto umidità 

const int soglia_critica = 800*4; // Soglia di umidità terreno alla quale si accenderà la valvola
int soglia_temperatura = 25;

// dichiarazione variabili umidità piante
int umiditPianta1;
int umiditPianta2;
int umiditPianta3;
int umiditPianta4;
bool irrigazioneAttiva; // segnala se è in atto una o più irrigazioni

int tempAria;
int umAria;
int umTerreno;
int lvlAcqua;
int variabileTest;
long timeNow;

// variabili livello acqua
long tempo = 0;
long tempoUltrasuoni = 0;
long oldTimePiante = 0;
long oldTimeTemp = 0;
float dist = 0;
byte height = 20, wHeight = 0, lvlAcquaPerc;

// dichiarazione funzioni
void umiditaTerreno(int SensorePianta, int elettrovalvola, int numeroPianta, int *umiditPianta);
// void display();
void livelloAcqua();
void TempHumAria();

void setup()
{
  Serial.begin(250000);
  SensoreTempSerra.begin();
  pinMode(Elettrovalvola1, OUTPUT);
  pinMode(Elettrovalvola2, OUTPUT);
  pinMode(Elettrovalvola3, OUTPUT);
  pinMode(Elettrovalvola4, OUTPUT);
  pinMode(Ventola, OUTPUT);
  digitalWrite(Ventola, HIGH);
  digitalWrite(Elettrovalvola1, HIGH);


  pinMode(trigUS, OUTPUT);
  pinMode(echoUS, INPUT);
}



void loop()
{
  String printString = "";
  tempo = millis();
  if (oldTimeTemp == 0 || (tempo - oldTimeTemp) > 1800000) // check serbatoio e temperatura ogni 30 minuti
   {
    oldTimeTemp = millis();
    livelloAcqua();
    TempHumAria();
  }
 // Irrigazione();
  if (oldTimePiante == 0 || (tempo - oldTimePiante) > 3600000 || irrigazioneAttiva) // check ogni 60 minuti o costante se l'irrigazione è attiva
  {
    oldTimePiante = millis();
    umiditaTerreno(Terreno_Pianta_1, Elettrovalvola1, 1, &umiditPianta1);
    umiditaTerreno(Terreno_Pianta_2, Elettrovalvola2, 2, &umiditPianta2);
    umiditaTerreno(Terreno_Pianta_3, Elettrovalvola3, 3, &umiditPianta3);
    umiditaTerreno(Terreno_Pianta_4, Elettrovalvola4, 4, &umiditPianta4);
    if (irrigazioneAttiva)
      livelloAcqua(); //check che non prosciugo il serbatoio mentre la pompa è accesa
  }

  // controllo temperatura ventilazione
  if (tempAria >= soglia_temperatura)
    digitalWrite(Ventola, LOW);
   if(digitalRead(Ventola) == LOW){
    TempHumAria();
    if (tempAria == (soglia_temperatura - 2))
      digitalWrite(Ventola, HIGH);
   } 
    

  // scrittura valori in seriale
  printString = printString + "Sensore pianta 1" + ":Umidità:\t" + String(umiditPianta1) + "\n" +
                "Sensore pianta 2" + ":Umidità:\t" + String(umiditPianta2) + "\n" +
                "Sensore pianta 3" + ":Umidità:\t" + String(umiditPianta3) + "\n" +
                "Sensore pianta 4" + ":Umidità:\t" + String(umiditPianta4) + "\n" +
                "Livello Acqua:\t" + String(lvlAcquaPerc) + "%\n" +
                "Umidità aria: " + String(umAria) + "\tTemp. aria:\t" + String(tempAria) + "° C";
  Serial.println(printString);
  delay(2000);
}

void umiditaTerreno(int TerrenoPianta, int elettrovalvola, int numeroPianta, int *umiditPianta)
{
  // controllo suolo pianta
  *umiditPianta = analogRead(TerrenoPianta); // Legge il valore analogico del sensore umidità terreno
  if (*umiditPianta >= soglia_critica && lvlAcquaPerc > 15)
  {
    digitalWrite(elettrovalvola, LOW); // Accendi eletrrovalvola
    irrigazioneAttiva = true; // segnalo che è attiva una irrigazione
    digitalWrite(Pompa,LOW);
    delayMicroseconds(300);
  }
  else if (*umiditPianta > 900 || lvlAcquaPerc <= 15)
  {
    digitalWrite(elettrovalvola, HIGH); // Spegni eletrrovalvola
    if (digitalRead(Elettrovalvola1) == HIGH && digitalRead(Elettrovalvola2) == HIGH && digitalRead(Elettrovalvola3) == HIGH && digitalRead(Elettrovalvola4) == HIGH){
      irrigazioneAttiva = false; // se tutte le elettrovalvole sono spente non c'è irrigazione => non serve fare il check costante per spegnere l'irrigazione
      digitalWrite(Pompa, HIGH); //spegnimento pompa => nessuna pianta sta irrigando
    }
   }
}


void livelloAcqua()
{
  // Mandiamo il segnale
  digitalWrite(trigUS, LOW);
  delayMicroseconds(2);
  digitalWrite(trigUS, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigUS, LOW);

  // Riceviamo il segnale
  tempoUltrasuoni = pulseIn(echoUS, HIGH);

  // Calcoliamo la distanza in cm dal tempo di risposta
  dist = tempoUltrasuoni * 0.017;

  // Togliamo la distanza tra il sensore e il livello massimo dell'acqua
  dist -= SENDIST;

  // Calcoliamo l'altezza dell'acqua
  wHeight = height - dist;

  // Serial.print(wHeight);
  // Serial.print(" cm livello acqua\n");

  // Trasformiamo l'altezza dell'acqua in una percentuale
  lvlAcquaPerc = wHeight * 100 / height;
}

void TempHumAria()
{
  tempAria = SensoreTempSerra.readTemperature();
  umAria = SensoreTempSerra.readHumidity();
}

// void Irrigazione(){
//   if(irrigazioneAttiva && (digitalRead(Elettrovalvola1)==LOW || digitalRead(Elettrovalvola2)==LOW || digitalRead(Elettrovalvola3)==LOW || digitalRead(Elettrovalvola4)==LOW)){
//     digitalWrite(Pompa,HIGH);
//     delayMicroseconds(300);
//   }
//   else{
//     digitalWrite(Pompa,LOW);
//     delayMicroseconds(300);
//   }
// }
