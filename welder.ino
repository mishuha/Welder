#include <EEPROM.h>
#define CLK 5
#define DIO 4
#include "TimedAction.h"
#include "GyverTM1637.h"
#include "button.h"
GyverTM1637 disp(CLK, DIO);

const int setupPin = 2; //setup button
const int counterPin = 3; //Feedback coil input 
// const int ledPin = 13;
const int powerPin = 6; //out to welder
const int buzzerPin = 7;
const int debugPin = 8; //if set activates display of impulse counter from counterPin
const int startPin = 9; //start button

button startBtn(startPin); 
button setupBtn(setupPin); 

const int settingsImpulseLimit = 100; //max value for setting, in sinwave half-periods (0.1ms per impulse if 100Hz)

//const char* mainModes[] = {"main", "welding", "setup", "debug"};
volatile char* MODE = "main";

const char* setupModes[] = {"setupInit", "setupDelay", "setupMain"};
volatile char* setupMODE = setupModes[0];
const int maxSetupIndex = sizeof(setupModes) / sizeof(setupModes[0]) - 1;
volatile int curSetupIndex = 0;

volatile bool setupIsPressed = false, setupMode = false;
volatile bool startIsPressed = false, weldingMode = false;
volatile bool mutex = false; // blocking flag while choosing or running any mode
bool debug = false;

int impulses = 0; // impulse counter
int impulseDuration = 10; // millisec

unsigned long FuseTimer = 100; //ms, fuse for welding > defined time
unsigned long initTimer = 0, delayTimer = 0, mainTimer = 0; //ms, timers for welding
unsigned long startime = 0;

volatile int initCycleImpulses = 0; // Preheat impulses
volatile int delayCycleTime = 0;    // time betwin preheat and main cycles
volatile int mainCycleImpulses = 0; // welding impulses in main cycle

// encoder variables
volatile bool encFlag = 0;    // флаг поворота
volatile byte reset = 0, last = 0;

volatile byte ledState = LOW;
bool dispBlink = false;


void updateLCD(int x, char* type='default') {
    disp.clear();
    disp.displayInt(x);
}

void useEncoder (volatile int& var) {
    byte state = (PINC & 0b00000011);    // A1+A0 (PORT C) - encoder
    if (reset && state == 0b11) {
        int prevCount = var;
        if (last == 0b10) var++;
        else if (last == 0b01) var--;
        reset = 0;
    }
    if (!state) reset = 1;
        last = state;
}

//Encoder processing on low level
ISR (PCINT1_vect) {
    if (MODE == "setup") { //setup
        if (setupMODE == "setupInit") {
            useEncoder(initCycleImpulses);
            if (initCycleImpulses < 0) initCycleImpulses = 0;
            if (initCycleImpulses > settingsImpulseLimit) initCycleImpulses = settingsImpulseLimit;
        }
        else if (setupMODE == "setupDelay") {
            useEncoder(delayCycleTime);
            if (delayCycleTime < 0) delayCycleTime = 0;
            if (delayCycleTime > settingsImpulseLimit) delayCycleTime = settingsImpulseLimit;
        }
        else if (setupMODE == "setupMain") {
            useEncoder(mainCycleImpulses);
            if (mainCycleImpulses < 0) mainCycleImpulses = 0;
            if (mainCycleImpulses > settingsImpulseLimit) mainCycleImpulses = settingsImpulseLimit;
        }
    }
}

void displaySetupData() {
    if (setupMODE == "setupInit") {
        updateLCD(initCycleImpulses);
        disp.displayByte(0, 0x01); //mode sign
        }
    if (setupMODE == "setupDelay") {
        updateLCD(delayCycleTime);
        disp.displayByte(0, 0x41);
        }
    if (setupMODE == "setupMain") {
        updateLCD(mainCycleImpulses);
        disp.displayByte(0, 0x49);
        }
    dispBlink = !dispBlink;
    if (dispBlink) disp.clear();
}

void startWelding() {
    delay(500);
    disp.displayByte(0x48,_r,_u,_n);
    delay(1000);
    initTimer = initCycleImpulses*impulseDuration;
    delayTimer = delayCycleTime*impulseDuration;
    mainTimer = mainCycleImpulses*impulseDuration;
    
    startime = millis();
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(powerPin, HIGH);
    impulses = 0;
    //waiting for feedback signal
    while (impulses == 0) {
        Serial.println("stub"); // magic row, "MODE" and "impulses" variable are unable to update without Serial call
        // exit if no signal
        if (millis() - startime > initTimer + 2*impulseDuration) {
            digitalWrite(powerPin, LOW);
            digitalWrite(buzzerPin, LOW);
            disp.displayByte(0x48,_E,_r,_r);
            delay(1500);
            alarmBuzzer();
            MODE = "main";
            return;
        }
    }
    //start timer on impulses signal
    startime = millis();
    while (millis() - startime < initTimer) delay(1);
    digitalWrite(powerPin, LOW);
    digitalWrite(buzzerPin, LOW);
    //delay    
    delay(delayTimer);
    //main
    startime = millis();
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(powerPin, HIGH);
    impulses = 0;
    //waiting for feedback signal
    while (impulses == 0) {
        Serial.println("stub");
        // exit if no signal
        if (millis() - startime > mainTimer + 2*impulseDuration) {
            digitalWrite(powerPin, LOW);
            digitalWrite(buzzerPin, LOW);
            disp.displayByte(0x48,_E,_r,_r);
            delay(1500);
            alarmBuzzer();
            MODE = "main";
            return;
        }
    }
    //start timer on impulses signal
    startime = millis();
    while (millis() - startime < mainTimer) delay(1);
    digitalWrite(powerPin, LOW);
    digitalWrite(buzzerPin, LOW);
    MODE = "main";
}

void alarmBuzzer() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(buzzerPin, HIGH);
        delay(200);
        digitalWrite(buzzerPin, LOW);
        delay(25);
    }
}
    
void impulseCounter() {
//   if (MODE == "welding") Serial.println("welding");
//   if (MODE == "setup") Serial.println("setup");
//   if (MODE == "main") Serial.println("main");
//   if (MODE == "debug") Serial.println("debug");
    if (MODE == "welding") {
          Serial.println(impulses); // magic row, "MODE" and "impulses" variable are unable to update without Serial call
      impulses++;
    }
}

void initEEPROM() {
    if (EEPROM.read(10) == 255) { //mark address "10" as inited
        saveEEPROM();
        EEPROM.put(10, 0);
    }
}
void saveEEPROM() {
    EEPROM.put(0, initCycleImpulses);
    EEPROM.put(2, mainCycleImpulses);
    EEPROM.put(4, delayCycleTime);
    }
    
void loadEEPROM() {
    EEPROM.get(0, initCycleImpulses);
    EEPROM.get(2, mainCycleImpulses);
    EEPROM.get(4, delayCycleTime);
    }
//debug function
void counter() { 
    impulses++;
}
//debug function
void checkCounter() { 
    updateLCD(impulses);
    startime = millis();
    impulses = 0;
}

TimedAction displaySetupDataAction     = TimedAction(150, displaySetupData);
// TimedAction weldingFuseAction          = TimedAction(10, weldingFuse);
// Debug, impulse counter display:
TimedAction checkCounterAction         = TimedAction(1000, checkCounter); 


void setup() {
    pinMode(powerPin, OUTPUT);
    pinMode(debugPin, INPUT_PULLUP);
    // pinMode(setupPin, INPUT);
    // pinMode(startPin, INPUT);
    Serial.begin(500000);
    if (digitalRead(debugPin) == LOW) {
      MODE = "debug";
    }
    disp.clear();
    disp.brightness(2);    // яркость, 0 - 7 (минимум - максимум)
    attachInterrupt(digitalPinToInterrupt(counterPin), impulseCounter, RISING);
    if (MODE == "debug") {
        attachInterrupt(digitalPinToInterrupt(counterPin), counter, RISING);
    }
    PCICR   |= B00000010;			// "PCIE1" enabeled (Port C)
    PCMSK1  |= B00000111;			//  A2, A1, A0 will trigger interrupt
    
    initEEPROM();
    loadEEPROM();
    
    // Serial.println(mainCycleImpulses);
    disp.displayByte(_dash, _dash, _dash, _dash);
    delay(500);
}

void loop() {
    if (MODE == "main") {
        disp.displayInt(mainCycleImpulses); // main mode indicator
        disp.displayByte(0, 0x63); //mode sign
    }
    if (MODE == "setup") {
        displaySetupDataAction.check();
    }
    if (MODE == "debug") {
        checkCounterAction.check();
    }
    //enter settings
    if (setupBtn.click() && MODE != "debug") { 
        if (MODE == "main") {
            curSetupIndex = 0;
            MODE = "setup";
        }
        if (curSetupIndex > maxSetupIndex) { //exit settings
            curSetupIndex = 0;
            MODE = "main";
            saveEEPROM();
        }
        setupMODE = setupModes[curSetupIndex];
        curSetupIndex += 1;
    }
    //start
    if (startBtn.click()) {
        if (MODE == "main") {
            curSetupIndex = 0;
            MODE = "welding";
//            if (MODE == "main") Serial.println("main1");
//            if (MODE == "welding") Serial.println("welding1");
            startWelding();
        }
    }
}
