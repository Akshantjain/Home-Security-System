// Group 59 - Security System Music Player (CS362 - Fall19)
// + Quan Tran - qtranm2
// + Son Nguyen - snguye49
// + Akshant Jain - ajain78

// First Arduino (SLAVE_1)
// This one should be put near the entrance of the user's house
// Connects with 4x4 keypad, ultrasonic sensor, photoresistor, buzzer and LEDs

#include <Keypad.h>   // for 4x4 Keypad
#include <NewPing.h>  // for HCSR04 ultrasonic sensor
#include <Wire.h>     // for I2C
#include "pitches.h"  // pre-defined frequencies

typedef unsigned int uint;

/* ------------ HC-SR04 ------------ */
const uint HCSR04_TRIGGER_PIN = 11;
const uint HCSR04_ECHO_PIN = 10;
const uint HCSR04_MAX_DISTANCE = 200;  // 2m max

NewPing hcsr04(HCSR04_TRIGGER_PIN, HCSR04_ECHO_PIN, HCSR04_MAX_DISTANCE);

/* ------------ KEYPAD ------------ */
const byte KP_ROWS = 4;  // number of rows of keypad
const byte KP_COLS = 4;  // number of columns of keypad

const char keymap[KP_ROWS][KP_COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};

byte rowPins[KP_ROWS] = {7, 6, 5, 4};    // rows 0 to 3
byte colPins[KP_COLS] = {3, 2, 15, 16};  // columns 0 to 3

Keypad keypad = Keypad(makeKeymap(keymap), rowPins, colPins, KP_ROWS, KP_COLS);

/* ------------ OTHER ------------ */
const int MASTER = 0x0;
const int SLAVE_1 = 0x1;
const int SLAVE_2 = 0x2;

const uint RED_LED_PIN = 8;
const uint GREEN_LED_PIN = 9;
const uint PHOTO_PIN = A0;
const uint BUZZER_PIN = 12;

const uint DISTANCE_THRESHOLD = 10;
const uint LUMINANCE_THRESHOLD = 900;

String userInputPassword;  // to store the input password from user
unsigned long ledTime;

unsigned long distanceTime;
bool distanceTooNear = false;

unsigned long luminanceTime;
bool luminanceTooBright = false;

// melodies
int m1[] = {NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4};
int n1[] = {4, 8, 8, 4, 4, 4, 4, 4};

int m2[] = {NOTE_E3, NOTE_E3, NOTE_F3, NOTE_G3, NOTE_G3, NOTE_F3, NOTE_E3, NOTE_D3, NOTE_C3, NOTE_C3, NOTE_D3, NOTE_E3, NOTE_E3, 0, NOTE_D3, NOTE_D3};
int n2[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2};

// state
// 0: idle, waiting for '*'
// 1: '*' received, waiting user to input passcode
// 2: '#' received, user confirmed passcode, send to the master
// 3: waiting respond from master
// 4: alarm activated
// 5: alarm deactivated -> state 0
volatile uint state = 0;

bool setupFirstTime = true;  // setup first time

bool systemActivated = false;

/* ------------ SETUP ------------ */
void setup() {
  Wire.begin(SLAVE_1);           // join I2C bus with address #1
  Wire.onReceive(receiveEvent);  // register receive event

  pinMode(RED_LED_PIN, OUTPUT);    // red LED
  pinMode(GREEN_LED_PIN, OUTPUT);  // green LED

  Serial.begin(9600);
  Serial.println("[DEBUG] First Arduino (SLAVE_1) initialized");
}

/* ------------ MAIN ------------ */
void loop() {
  if (state == 0) {
    if (keypad.getKey() == '*') {
      state = 1;
      userInputPassword += '*';
      Serial.println("[DEBUG] * received, state = 1");
    }
  }

  if (state == 1) {
    char input = keypad.getKey();
    if (input) {
      Serial.print("[DEBUG] input: ");
      Serial.println(input);

      // 0 to 9
      if (isdigit(input)) {
        userInputPassword += input;
      }

      // 'A', 'B', 'C', 'D'
      if (isalpha(input)) {
        size_t len = userInputPassword.length();
        if (len == 5) {
          userInputPassword += input;
        } else if (len < 5) {
          Serial.println("[WARN] You have to put 4-digit passcode first");
        } else {
          Serial.println("[WARN] You already chose a mode, press # to confirm");
        }
      }

      // user confirmed with #
      if (input == '#') {
        // inputPwd should be *1234A#, len = 6
        if (userInputPassword.length() == 6) {
          userInputPassword += input;
          state = 2;
          Serial.print("[DEBUG] # received, state = 2, inputPwd: ");
          Serial.println(userInputPassword.c_str());
        } else {
          Serial.println("[WARN] # received but password is not in valid format");
          state = 0;
          userInputPassword = "";
          Serial.println("[DEBUG] inputPwd cleared, reset to state = 0");
        }
      }
    }
  }

  if (state == 2) {
    // send to master
    String pwd = "pass:" + userInputPassword;

    Wire.beginTransmission(MASTER);
    Wire.write(pwd.c_str());
    Wire.endTransmission();

    userInputPassword = "";
    if (setupFirstTime) {
      state = 0;
      setupFirstTime = false;
      Serial.println("[DEBUG] first time setup, password sent to master, cleared, state = 0");
    } else {
      state = 3;
      Serial.println("[DEBUG] password sent to master, cleared, state = 3");
    }
  }

  if (state == 3) {
    //Serial.println("[DEBUG] Waiting response from master");
  }

  if (state == 4) {
    activateAlarm(true);
  }

  if (state == 5) {
    activateAlarm(false);
    state = 0;
  }

  // get the distance from the HC-SR04 sensor
  if (state != 3) {
    if (!systemActivated) return;

    const uint distance = hcsr04.ping_cm();
    if (distance > 0 && distance < HCSR04_MAX_DISTANCE) {
      if (distance < DISTANCE_THRESHOLD) {
        if (!distanceTooNear) {
          distanceTooNear = true;
          distanceTime = millis();
        }
        // if distance is too short in 1s
        if (distanceTooNear) {
          if (millis() - distanceTime > 1000) {
            Wire.beginTransmission(MASTER);
            Wire.write("dist");
            Wire.endTransmission();
            distanceTooNear = false;
            state = 3;
          }
        }
      } else {
        distanceTooNear = false;
      }
    }

    // get the luminance from the photoresistor
    const int luminance = analogRead(PHOTO_PIN);
    if (luminance > 0) {
      if (luminance > LUMINANCE_THRESHOLD) {
        if (!luminanceTooBright) {
          luminanceTooBright = true;
          luminanceTime = millis();
        }
        // if light is too bright in 2s
        if (luminanceTooBright) {
          if (millis() - luminanceTime > 2000) {
            Wire.beginTransmission(MASTER);
            Wire.write("lumi");
            Wire.endTransmission();
            luminanceTooBright = false;
            state = 3;
          }
        }
      } else {
        luminanceTooBright = false;
      }
    }

    // turn off the LEDs after 3s
    if (digitalRead(GREEN_LED_PIN) == HIGH || digitalRead(RED_LED_PIN) == HIGH) {
      if (millis() - ledTime > 3000) {
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, LOW);
      }
    }
  }
}

/*
void playMusic(int index) {
  if (index == 0) {
    for (int i = 0; i < 8; i++) {
      // to calculate the note duration, take one second divided by the note type
      int noteDuration = 1000 / n1[i];
      tone(BUZZER_PIN, m1[i], noteDuration);

      // to distinguish the notes, set a minimum time between them
      // the note's duration + 30% seems to work well
      int pauseBetweenNotes = noteDuration * 1.30;
      delay(pauseBetweenNotes);
      // stop the tone playing
      noTone(BUZZER_PIN);
    }
  } else {
    for (int i = 0; i < 15; i++) {
      // to calculate the note duration, take one second divided by the note type
      int noteDuration = 1000 / n2[i];
      tone(BUZZER_PIN, m2[i], noteDuration);

      // to distinguish the notes, set a minimum time between them
      // the note's duration + 30% seems to work well
      int pauseBetweenNotes = noteDuration * 1.30;
      delay(pauseBetweenNotes);
      // stop the tone playing
      noTone(BUZZER_PIN);
    }
  }
}
*/

void activateAlarm(bool shouldAlarm) {
  if (shouldAlarm) {
    // blinking both LEDs
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, HIGH);
    delay(200);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    delay(200);
  } else {
    // turn off the LEDs
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
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

  // handle response from master after sending password
  if (data.indexOf("pwd") >= 0) {
    digitalWrite((data == "pwd correct") ? GREEN_LED_PIN : RED_LED_PIN, HIGH);
    ledTime = millis();
    state = 0;
  }

  // activate alarm
  if (data.indexOf("alarm") >= 0) {
    state = (data == "activate alarm") ? 4 : 0;
  }

  // activate system
  if (data.indexOf("system") >= 0) {
    systemActivated = (data == "system activated") ? true : false;
  }
}