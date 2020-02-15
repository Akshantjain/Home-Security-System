// Group 59 - Security System Music Player (CS362 - Fall19)
// + Quan Tran (qtranm2)
// + Son Nguyen (snguye49)
// + Akshant Jain (ajain78)

// Second Arduino (MASTER)
// This one should be safely put inside the user's house (living room,...)
// Connects with LCD, LED, buzzer and button

#include <LiquidCrystal.h>  // for LCD
#include <Wire.h>           // for I2C
#include "pitches.h"        // pre-defined frequencies

typedef unsigned int uint;

/* ------------ OTHER ------------ */
const int MASTER = 0x0;
const int SLAVE_1 = 0x1;
const int SLAVE_2 = 0x2;

const uint BUTTON_PIN = 2;
const uint BUZZER_PIN = 5;
const uint RED_LED_PIN = 6;

LiquidCrystal lcd(12, 11, 10, 9, 8, 7);

String receivedPassword;      // to store received password
uint wrongPasswordCount = 0;  // number of wrong password input
unsigned long lcdTime;

// state
// 0: idle, waiting data from slave
// 1: check password
// 2: alarm activated
// 3: alarm deactivated -> state 0
// 4: setup the password
volatile uint state = 4;

// user can change password from Serial Monitor
String PASSWORD = "1234";

String threatMessage;  // threat status
String mainMode;       // to store the mode

volatile bool systemActivated = false;

// melodies
int m1[] = {NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4};
int n1[] = {4, 8, 8, 4, 4, 4, 4, 4};

int m2[] = {NOTE_E3, NOTE_E3, NOTE_F3, NOTE_G3, NOTE_G3, NOTE_F3, NOTE_E3, NOTE_D3, NOTE_C3, NOTE_C3, NOTE_D3, NOTE_E3, NOTE_E3, 0, NOTE_D3, NOTE_D3};
int n2[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2};

// handle input from serial Monitor
const byte numChars = 32;
char receivedData[numChars];  // to store the input from Serial Monitor
char data[numChars];          // make a copy of the received input
boolean newData = false;      // new data is received from the Serial Monitor
boolean parsed = false;       // data was parsed and validated successfully

bool printed = false;  // printed the message
bool welcome = false;  // printed the welcome message

/* ------------ SETUP ------------ */
void setup() {
  Wire.begin(MASTER);            // join I2C bus with address #2
  Wire.onReceive(receiveEvent);  // register receive event

  // initialize LCD
  lcd.begin(16, 2);
  lcd.clear();

  pinMode(RED_LED_PIN, OUTPUT);  // red LED
  pinMode(BUTTON_PIN, INPUT);    // button
  pinMode(BUZZER_PIN, OUTPUT);   // buzzer

  attachInterrupt(0, buttonPressed, CHANGE);  // attach interrupt for button

  Serial.begin(9600);
  Serial.println("[DEBUG] Second Arduino (MASTER) initialized");
}

/* ------------ MAIN ------------ */
void loop() {
  if (state == 0) {
    if (!welcome) {
      printToLcd("WELCOME TO", "SSMP");
      delay(2000);
      welcome = true;
    }
    if (systemActivated) {
      printToLcd("SYSTEM", "ACTIVATED");
      delay(200);
    } else {
      printToLcd("SYSTEM", "DEACTIVATED");
      delay(200);
    }
    // do nothing
  }

  if (state == 1) {
    // verify password
    if (receivedPassword == PASSWORD) {
      printToLcd("PASSWORD IS", "CORRECT");
      Serial.println("[DEBUG] Password is correct");
      if (mainMode == "A") {
        systemActivated = true;

        Wire.beginTransmission(SLAVE_1);
        Wire.write("system activated");
        Wire.endTransmission();

        Wire.beginTransmission(SLAVE_2);
        Wire.write("system activated");
        Wire.endTransmission();
      } else if (mainMode == "B") {
        systemActivated = false;

        Wire.beginTransmission(SLAVE_1);
        Wire.write("system deactivated");
        Wire.endTransmission();

        Wire.beginTransmission(SLAVE_2);
        Wire.write("system deactivated");
        Wire.endTransmission();
      }

      Wire.beginTransmission(SLAVE_1);
      Wire.write("pwd correct");
      Wire.endTransmission();
    } else {
      printToLcd("PASSWORD IS", "NOT CORRECT");
      Serial.println("[DEBUG] Password is incorrect");
      wrongPasswordCount++;

      Wire.beginTransmission(SLAVE_1);
      Wire.write("pwd incorrect");
      Wire.endTransmission();
    }
    if (wrongPasswordCount == 2) {
      state = 2;
    }
    receivedPassword = "";
    state = 0;
  }

  if (state == 2) {
    if (systemActivated) {
      announceThreat(true);
      activateAlarm(true);
    } else {
      state = 0;
    }
  }

  if (state == 3) {
    announceThreat(false);
    activateAlarm(false);
    state = 0;
  }

  if (state == 4) {
    if (!printed) {
      printToLcd("ENTER PASSWORD", "FORMAT: *1234A#");
      Serial.println("[DEBUG] Please setup the system password");
      Serial.println("[DEBUG] in following format: <*1234A#>");
      printed = true;
    }

    /*
    getInputFromSerialMonitor();

    // if we receive new input from serial monitor
    if (newData == true) {
      strcpy(data, receivedData);
      newData = false;
      parsed = true;
    }

    if (parsed) {
      Serial.println("serial monitor:");
      Serial.print(data);
      PASSWORD = data;
    }
    */
  }
}

void getInputFromSerialMonitor() {
  // get the input from Serial Monitor based on start and end indicator
  static boolean inProgress = false;
  static byte ndx = 0;
  char startIndicator = '<';
  char endIndicator = '>';
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();

    if (inProgress == true) {
      if (rc != endIndicator) {
        receivedData[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      } else {
        receivedData[ndx] = '\0';  // terminate the string
        inProgress = false;
        ndx = 0;
        newData = true;
      }
    } else if (rc == startIndicator) {
      inProgress = true;
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

void announceThreat(bool shouldAnnounce) {
  Wire.beginTransmission(SLAVE_1);
  Wire.write((shouldAnnounce) ? "activate alarm" : "deactivate alarm");
  Wire.endTransmission();

  Wire.beginTransmission(SLAVE_2);
  Wire.write((shouldAnnounce) ? "activate alarm" : "deactivate alarm");
  Wire.endTransmission();
}

/*
void playMusic(int index) {
  vector<int> melody = melodies.at(index);
  vector<int> noteDurations = durations.at(index);
  for (int i = 0; i < melody.size(); i++) {
    // to calculate the note duration, take one second divided by the note type
    int noteDuration = 1000 / noteDurations.at(i);
    tone(BUZZER_PIN, melody.at(i), noteDuration);

    // to distinguish the notes, set a minimum time between them
    // the note's duration + 30% seems to work well
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    // stop the tone playing
    noTone(BUZZER_PIN);
  }
}
*/

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

void activateAlarm(bool shouldAlarm) {
  if (shouldAlarm) {
    // blinking LED and buzzing
    digitalWrite(RED_LED_PIN, HIGH);
    delay(200);
    digitalWrite(RED_LED_PIN, LOW);
    delay(200);

    playMusic(rand() % 2);
  } else {
    // turn off the LED and stop buzzing
    digitalWrite(RED_LED_PIN, LOW);
    noTone(BUZZER_PIN);
  }
}

void printToLcd(String first, String second) {
  threatMessage = first + "," + second;
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

void receiveEvent(int howMany) {
  // read the incoming data
  String data;
  while (howMany--) {
    data += Wire.read();
  }
  Serial.print("data: ");
  Serial.println(data.c_str());

  // "pass:*1234A#"
  int passIdx = data.indexOf("pass");
  if (passIdx >= 0) {
    // contains password
    passIdx += 6;  // now idx at first digit
    String pass = data.substring(passIdx, 4);
    passIdx += 4;  // now idx at mode
    String mode = data.substring(passIdx, 1);
    mainMode = mode;
    if (state != 4) {
      receivedPassword = pass;
      state = 1;
    } else {
      PASSWORD = pass;
      state = 0;
    }
    Serial.print("PASS: ");
    Serial.println(pass.c_str());
    Serial.print("MODE: ");
    Serial.println(mode.c_str());
    //printToLcd("PASS: " + pass, "MODE: " + mode);
    //delay(2000);
  }

  if (state == 4) {
    return;  // ignore incoming data
  }

  if (data.indexOf("dist") >= 0) {
    printToLcd("SUSPICIOUS", "PERSON DETECTED");
    state = 2;
  }

  // lumi + gas
  if (data.indexOf("fire") >= 0) {
    printToLcd("FIRE RISK", "DETECTED");
    state = 2;
  }

  if (data.indexOf("lumi") >= 0) {
    printToLcd("BRIGHT LIGHT", "DETECTED");
    state = 2;
  }

  if (data.indexOf("gas") >= 0) {
    printToLcd("GAS LEAK", "DETECTED");
    state = 2;
  }

  // SLAVE_2 sent deactivate signal
  if (data.indexOf("deactivate") >= 0) {
    state = 3;
    printToLcd("ALARM IS", "DEACTIVATED");
    delay(1000);
  }
}
