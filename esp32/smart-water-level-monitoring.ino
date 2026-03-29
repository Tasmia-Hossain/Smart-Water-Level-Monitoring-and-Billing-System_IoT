#include <Wire.h>
#include <WiFiClient.h>
#include <EEPROM.h>

#define BLYNK_TEMPLATE_ID "TMPL6ehr6v3gK"
#define BLYNK_TEMPLATE_NAME "Water level monitoring system"
#define BLYNK_AUTH_TOKEN "Gd0X89ukzvnTnByK2lPWVtOPm7ceSvPR"
#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp32.h>

// Pins
#define LED2 4
#define LED3 5
#define LED4 18
#define trig 12
#define echo 13
#define relay 14
#define buzzer 27
#define FLOW_PIN 26

// Tank settings
int MaxLevel = 13;
int Level1 = (MaxLevel * 70) / 100;
int Level2 = (MaxLevel * 30) / 100;

// Lock system
int FULL_LEVEL = 2;     
int SAFE_LEVEL = 5;    

// Flow variables
volatile int pulseCount = 0;
float totalLiters = 0.0;
float bill = 0.0;
float calibrationFactor = 450.0;

#define EEPROM_SIZE 64

BlynkTimer timer;

char auth[] = "Gd0X89ukzvnTnByK2lPWVtOPm7ceSvPR";
char ssid[] = "Galaxy A35 5G 7C78";
char pass[] = "00000000";

// Lock state
bool autoLock = false;

// Interrupt
void IRAM_ATTR countPulse() {
  pulseCount++;
}

// EEPROM save/load
void saveData() {
  EEPROM.put(0, totalLiters);
  EEPROM.commit();
}

void loadData() {
  EEPROM.get(0, totalLiters);
  if (isnan(totalLiters)) totalLiters = 0;
}

// Flow
void calculateFlow() {
  static unsigned long lastTime = 0;

  if (millis() - lastTime >= 1000) {
    lastTime = millis();

    float liters = pulseCount / calibrationFactor;

    if (pulseCount > 2) {
      totalLiters += liters;
      saveData();
    }

    pulseCount = 0;

      bill = totalLiters * 0.8;

    Blynk.virtualWrite(V2, totalLiters);
    Blynk.virtualWrite(V3, bill);
  }
}

// Ultrasonic + control
int distance = 0;

void ultrasonic() {

  digitalWrite(trig, LOW);
  delayMicroseconds(4);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long t = pulseIn(echo, HIGH);
  distance = t / 58;

  int level = ((MaxLevel - distance)*100)/MaxLevel;

  if (distance <= MaxLevel)
    Blynk.virtualWrite(V0, level);
  else
    Blynk.virtualWrite(V0, 0);

  // LED
  if (Level1 <= distance) {
    digitalWrite(LED2, HIGH);
    digitalWrite(LED3, LOW);
    digitalWrite(LED4, LOW);
  }
  else if (Level2 <= distance && Level1 > distance) {
    digitalWrite(LED2, HIGH);
    digitalWrite(LED3, HIGH);
    digitalWrite(LED4, LOW);
  }
  else {
    digitalWrite(LED2, HIGH);
    digitalWrite(LED3, HIGH);
    digitalWrite(LED4, HIGH);
  }

  // FULL → lock + buzzer
  if (distance <= FULL_LEVEL) {

    digitalWrite(buzzer, HIGH);
    digitalWrite(relay, HIGH);   // motor OFF
    autoLock = true;

  }

  //  Unlock only when water significantly drops
  else if (distance >= SAFE_LEVEL) {

    autoLock = false;
    digitalWrite(buzzer, LOW);
  }
}

// Pump control
BLYNK_WRITE(V1) {

  bool Relay = param.asInt();

  if (autoLock) {
    digitalWrite(relay, HIGH);
    return;
  }

  if (Relay == 1)
    digitalWrite(relay, LOW);
  else
    digitalWrite(relay, HIGH);
}

// Reset
BLYNK_WRITE(V4) {
  if (param.asInt() == 1) {
    totalLiters = 0;
    bill = 0;
    saveData();

    Blynk.virtualWrite(V2, 0);
    Blynk.virtualWrite(V3, 0);
  }
}

void setup() {

  Serial.begin(115200);
  Blynk.begin(auth, ssid, pass);

  EEPROM.begin(EEPROM_SIZE);
  loadData();

  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  pinMode(relay, OUTPUT);
  pinMode(buzzer, OUTPUT);

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), countPulse, RISING);

  digitalWrite(relay, HIGH);

  timer.setInterval(1000L, calculateFlow);
  timer.setInterval(500L, ultrasonic);
}

void loop() {
  Blynk.run();
  timer.run();
}