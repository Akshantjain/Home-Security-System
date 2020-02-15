// Group 59 - Security System Music Player (CS362 - Fall19)
// + Quan Tran (qtranm2)
// + Son Nguyen (snguye49)
// + Akshant Jain (ajain78)

// Third Arduino (SLAVE_2)
// This one should be put near the kitchen inside the user's house
// Connects with gas sensor, photoresistor, LCD, buzzer

#include <LiquidCrystal.h>  // for LCD
#include <MQ2.h>            // for MQ2 gas sensor
#include <Wire.h>           // for I2C
#include "pitches.h"        // pre-defined frequencies

typedef unsigned int uint;

/* ------------ MQ2 ------------ */
const uint GAS_PIN = A0;
MQ2 mq2(GAS_PIN);

const int LPG_THRESHOLD = 3000;     // liquefied petroleum gas
const int CO_THRESHOLD = 20000;     // carbon monoxide
const int SMOKE_THRESHOLD = 20000;  // smoke

/* ------------ OTHER ------------ */
const int MASTER = 0x0;
const int SLAVE_1 = 0x1;
const int SLAVE_2 = 0x2;

const uint BUTTON_PIN = 2;
const uint BUZZER_PIN = 5;
const uint RED_LED_PIN = 6;

LiquidCrystal lcd(12, 11, 10, 9, 8, 7);

unsigned long lcdTime;

// state
// 0: idle, read value from sensors
// 1: waiting response from master
// 2: alarm activated
// 3: alarm deactivated -> state 0
volatile uint state = 0;

bool systemActivated = false;

/* ------------ SETUP ------------ */
void setup() {
  Wire.begin(SLAVE_2);           // join I2C bus with address #2
  Wire.onReceive(receiveEvent);  // register receive event

  lcd.begin(16, 2);  // initialize LCD
  lcd.clear();
  printToLcd("WELCOME TO", "SSMP");

  pinMode(RED_LED_PIN, OUTPUT);  // red LED
  pinMode(BUTTON_PIN, INPUT);    // button
  pinMode(BUZZER_PIN, OUTPUT);   // buzzer

  attachInterrupt(0, buttonPressed, CHANGE);  // attach interrupt for button

  Serial.begin(9600);
  Serial.println("[DEBUG] Third Arduino (SLAVE_2) initialized");

  mq2.begin();
}

/* ------------ MAIN ------------ */
void loop() {
  if (state == 1) {
    //Serial.println("[DEBUG] Waiting response from master");
  }

  if (state == 2) {
    activateAlarm(true);
  }

  if (state == 3) {
    activateAlarm(false);
    state = 1;
  }

  if (state != 1) {
    if (!systemActivated) return;

    // get the value from the gas sensor
    //float *values = mq2.read(false);
    int lpg = mq2.readLPG();
    int co = mq2.readCO();
    int smoke = mq2.readSmoke();

    if (lpg >= LPG_THRESHOLD || co >= CO_THRESHOLD || smoke >= SMOKE_THRESHOLD) {
      Wire.beginTransmission(MASTER);
      Wire.write("gas");
      Wire.endTransmission();
      state = 1;
    }

    if (millis() - lcdTime > 3000) {
      printToLcd("WELCOME TO", "SSMP");
    }
  }
}

void buttonPressed() {
  // deactivate the alarm
  if (state == 2) {
    state = 3;
    printToLcd("ALARM IS", "DEACTIVATED");
  }
}

void printToLcd(String first, String second) {
  if (first.length() > 16) {
    Serial.println("[WARN] First string print to LCD too long");
  }
  if (second.length() > 16) {
    Serial.println("[WARN] Second string print to LCD too long");
  }

  // padding
  int pad1 = (16 - first.length()) / 2;
  int pad2 = (16 - second.length()) / 2;
  if (pad1 < 0) pad1 = 0;
  if (pad2 < 0) pad2 = 0;

  String tmp1, tmp2;
  for (int i = 0; i < pad1; i++) tmp1 += " ";
  tmp1 += first;
  for (int i = 0; i < pad2; i++) tmp2 += " ";
  tmp2 += second;

  // print to LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(tmp1.c_str());
  lcd.setCursor(0, 1);
  lcd.print(tmp2.c_str());

  lcdTime = millis();
}

void activateAlarm(bool shouldAlarm) {
  if (shouldAlarm) {
    // blinking LED and buzzing
    digitalWrite(RED_LED_PIN, HIGH);
    delay(200);
    digitalWrite(RED_LED_PIN, LOW);
    delay(200);

    // notes in the melody:
    int melody[] = {
        NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4};

    // note durations: 4 = quarter note, 8 = eighth note, etc.:
    int noteDurations[] = {
        4, 8, 8, 4, 4, 4, 4, 4};
    for (int thisNote = 0; thisNote < 8; thisNote++) {
      // to calculate the note duration, take one second divided by the note type.
      //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
      int noteDuration = 1000 / noteDurations[thisNote];
      tone(BUZZER_PIN, melody[thisNote], noteDuration);

      // to distinguish the notes, set a minimum time between them.
      // the note's duration + 30% seems to work well:
      int pauseBetweenNotes = noteDuration * 1.30;
      delay(pauseBetweenNotes);
      // stop the tone playing:
      noTone(BUZZER_PIN);
    }
  } else {
    // turn off the LED and stop buzzing
    digitalWrite(RED_LED_PIN, LOW);
    noTone(BUZZER_PIN);
  }
}

void receiveEvent(int howMany) {
  // read the incoming data
  String data;
  while (howMany--) {
    data += Wire.read();
  }
  //Serial.print("data: ");
  //Serial.println(data.c_str());

  // activate alarm
  if (data.indexOf("alarm") >= 0) {
    state = (data == "activate alarm") ? 2 : 0;
  }

  // activate system
  if (data.indexOf("system") >= 0) {
    systemActivated = (data == "system activated") ? true : false;
  }
}