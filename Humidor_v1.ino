
/*
 * Cigar humidifier, using water atomizer.
 * ======================================
 * 'Zigarren-Hans' / 'Cigar-Hans'
 * (c) maha / maha1337 
 * 
 * This code is prepared to run on Arduino Uno with IPS display.
 * Running with water atomizer, two humidity sensors, motor control and dual-buttons.
 * 
 * Note: I did not spend too much time developing this code.
 * It does its necessary job, improvements are always welcome.
 */

// display
#include <Adafruit_GFX.h>
#include <Arduino_ST7789_Fast.h>

// motor driver
#include <SparkFunMiniMoto.h>  // Include the MiniMoto library

// DHT sensor
#include "Grove_Temperature_And_Humidity_Sensor.h"

// others
#include <stdio.h>

// type DHT sensor
#define DHTTYPE DHT22  // DHT 22  (AM2302)

// serial clock
#define SCK 7

// serial data pin
#define SDA 8  // LCD display

// hygro/temp sensors
#define DHTPIN1 4 
#define DHTPIN2 6

// buttons
#define PIN_IN_RED_BUTTON_IN 2
#define PIN_IN_GREEN_BUTTON_IN 3
#define PRESSED 0
#define RELEASED 1

// water atomizer
#define PIN_OUT_WATER_ATOMIZER 5

// motor fault pin
#define FAULTn 16  // Pin used for fault detection.

// create display
Arduino_ST7789 lcd = Arduino_ST7789(SCK, SDA);

// DHT sensors
DHT dht1(DHTPIN1, DHTTYPE);  //   DHT11 DHT21 DHT22
DHT dht2(DHTPIN2, DHTTYPE);  //   DHT11 DHT21 DHT22

// mini moto instances, depends on port on base shield
//MiniMoto motor0(0xC4); // A1 = 1, A0 = clear
//MiniMoto motor1(0xC0); // A1 = 1, A0 = 1 (default)
//MiniMoto motor2(0xC4);
//MiniMoto motor3(0xC6);
//MiniMoto motor4(0xC8);
MiniMoto motor5(0xCA);
//MiniMoto motor6(0xCC);
//MiniMoto motor7(0xCE);
//MiniMoto motor8(0xD0);


// globals (other)
// ============================================================================================================================================

bool IS_TEST = false;
bool IS_HUMIDOR_SETUP_MODE = false;

float HUMIDITY_LIMIT = 70.0F;
// float HUMIDITY_LIMIT = 85.0F;

// humidity steps
float HUMIDITY_LOW = 55;
float HUMIDITY_MID = 60;
float HUMIDITY_MIDHIGH = 65;
float HUMIDITY_HIGH = HUMIDITY_LIMIT;

float temp_hum_val_sensor0[2] = { 0.0F, 0.0F };
float temp_hum_val_sensor1[2] = { 0.0F, 0.0F };

int humifidyCountGlobal = 0;


// Methods
// ============================================================================================================================================

// base status == idle
enum Status { Humidify,
              Idle,
              Stopped,
              Error };
Status currStatus;


// addressing too much memory by the char[]!
String createRunningTime() {

  unsigned long currentMillis = millis();
  unsigned long seconds = currentMillis / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;

  unsigned long remainingSeconds = seconds % 60;
  unsigned long remainingMinutes = minutes % 60;
  unsigned long remainingHours = hours % 24;

  char dateTimeString[25];
  sprintf(dateTimeString, "%02ld d %02ld:%02ld:%02ld", days, remainingHours, remainingMinutes, remainingSeconds);

  return String(dateTimeString);
}

void setError() {
  currStatus = Error;
  lcd.setTextColor(RED, BLACK);

  // if running, emergency stop
  digitalWrite(PIN_OUT_WATER_ATOMIZER, LOW);  // atomization stopped
  motor5.stop();
}

void setStop() {
  currStatus = Stopped;
  lcd.setTextColor(RED, BLACK);

  // if running, stop
  digitalWrite(PIN_OUT_WATER_ATOMIZER, LOW);  // atomization stopped
  motor5.stop();
}

void setStart() {
  currStatus = Idle;
  lcd.setTextColor(GREEN, BLACK);
}

// lcd, 240 x 240, 14 chars - size 3
void printHeadLines() {

  lcd.clearScreen();
  lcd.setCursor(0, 0);
  lcd.println("Zigarren-Hans V0.0.3");
  lcd.println("===================");
}

void printNewLCDScreenAndConsole(String toPrint) {

  printHeadLines();

  lcd.println(toPrint);
  Serial.println(toPrint);

  if (currStatus == Stopped) {
    lcd.println("-- Stopped! --");
  } else if (currStatus == Idle) {
    lcd.println("-- Idle! --");
  } else if (currStatus == Humidify) {
    lcd.println("-- Humidifying! --");
  } else if (currStatus == Error) {
    lcd.println("-- Error! --");
  }

  // print running time, not relevant...
  //String runningTime = createRunningTime();
  //Serial.println(String(runningTime));
  //lcd.println("RT: " + String(runningTime));
  //Serial.flush();

  Serial.println("Humidified: " + String(humifidyCountGlobal) + String(" x"));
  lcd.println("\nHumidified: " + String(humifidyCountGlobal) + String(" x"));
}


// function buttons: red-stop all, green-start all
int* checkButtons() {

  static int buttonResult[2];
  int redButtonRead = digitalRead(PIN_IN_RED_BUTTON_IN);      // read signal
  int greenButtonRead = digitalRead(PIN_IN_GREEN_BUTTON_IN);  // read signal
  buttonResult[0] = redButtonRead;
  buttonResult[1] = greenButtonRead;
  return buttonResult;
}

void setup(void) {

  // setup pins atomizer, buttons
  pinMode(PIN_OUT_WATER_ATOMIZER, OUTPUT);
  pinMode(PIN_IN_RED_BUTTON_IN, INPUT);
  pinMode(PIN_IN_GREEN_BUTTON_IN, INPUT);

  // setup display
  lcd.init();
  lcd.fillScreen(BLACK);

  lcd.setCursor(0, 0);
  lcd.setTextColor(GREEN, BLACK);
  lcd.setTextSize(2);
  lcd.clearScreen();

  // setup local serial monitor
  Serial.begin(9600);

  //Wire.begin();
  dht1.begin();
  dht2.begin();

  // motor
  pinMode(FAULTn, INPUT);

  // init status, set to run at (re) powering
  setStart();
  Serial.println("Setup done!");
}


// use timed loops to check buttons
void checkDHTSensors() {

  // first wait 20min here
  int timeToWaitSec = 20 * 60; // move hc to adjustable...
  if (IS_TEST) {
    // set to 20s
    timeToWaitSec = 20;
  }

  // check all live sensors
  Serial.println("Waiting :" + String(timeToWaitSec));
  delayActiveSeconds(timeToWaitSec);

  // calculate data
  dht1.readTempAndHumidity(temp_hum_val_sensor0);
  dht2.readTempAndHumidity(temp_hum_val_sensor1);

  float humidityS1 = temp_hum_val_sensor0[0];
  float humidityS2 = temp_hum_val_sensor1[0];

  // check if sensors ok, otherwise error!
  if ((humidityS1 == 0.0F) || (humidityS2 == 0.0F)) {
      Serial.println("Error, sensors not working!");
      setError();
  }

  float divHumidity = (humidityS1 - humidityS2) / humidityS1 * 100;
  Serial.println(String("Div Humidity: ") + String(divHumidity));
  
  // div between sensors < 10%
  if ((divHumidity < 10) && (currStatus != Error) && (currStatus != Stopped)) {
    currStatus = Idle;

    // sensors both <70% - only humidify if both <70%
    if ((humidityS1 < HUMIDITY_LIMIT) && (humidityS2 < HUMIDITY_LIMIT)) {
      currStatus = Humidify;
    } else {
      currStatus = Idle;
    }

    // motor start, atomizer go
    if (currStatus == Humidify) {
        humifidyCountGlobal++;
        Serial.println("Status: Humidify!");

      // run motor 2 min
      int runMotorBeforeHumidifyTimeSec = 2*60;
      if (IS_TEST) {
        runMotorBeforeHumidifyTimeSec = 5;
      }

      // start motor "on"
      motor5.drive(100);
      Serial.println("Motor running");

      // time to run before humidify 
      delayActiveSeconds(runMotorBeforeHumidifyTimeSec);

      // -------humidify on
      float humidityAverage = (humidityS1 + humidityS2) / 2.0F;
      Serial.println((String("Average Humidity: ") + String(humidityAverage)));

/*
TODO: maybe create incremental watering, based on history
*/

      int timeToWaitSecHumidify = 30;
      // <55%
      if(humidityAverage < HUMIDITY_LOW) {
         timeToWaitSecHumidify = 20;
      // >= 55 < 60%
      } else if ((humidityAverage >= HUMIDITY_LOW) && (humidityAverage < HUMIDITY_MID)) {
         timeToWaitSecHumidify = 15;
      // >= 60 < 65%
      } else if ((humidityAverage >= HUMIDITY_MID) && (humidityAverage < HUMIDITY_MIDHIGH)) {
         timeToWaitSecHumidify = 10;
       // >= 65 < 70%
      } else if ((humidityAverage >= HUMIDITY_MIDHIGH) && (humidityAverage < HUMIDITY_HIGH)) {
         timeToWaitSecHumidify = 5;
      }

      // humidor new, setup
      if(IS_HUMIDOR_SETUP_MODE) {
        timeToWaitSecHumidify = 60;
      }

      // humidify if test
      if (IS_TEST) {
        timeToWaitSecHumidify = 5;
      }

      Serial.println("Water running");
      digitalWrite(PIN_OUT_WATER_ATOMIZER, HIGH);  // atomize full

      // humidify until...
      delayActiveSeconds(timeToWaitSecHumidify);

      // stop humidifier
      digitalWrite(PIN_OUT_WATER_ATOMIZER, LOW);  // atomization stopped
      Serial.println("Water stopped");
      // -------humidify off

      // run motor after humidify
      int runMotorAfterHumidify = 2 * 60;

      if (IS_TEST) {
        runMotorAfterHumidify = 10;
      }
      // time to run motor while and after humidify
      delayActiveSeconds(runMotorAfterHumidify);

      // stop motor
      motor5.stop();
      Serial.println("Motor stopped");

      // change status
      currStatus = Idle;
    }

  } else {
    setStop();
    currStatus = Error;
    printNewLCDScreenAndConsole("Sensors div too high!");
  }
}


void loop() {

    // *check buttons always in active delay loop *

    if ((currStatus == Error) || (currStatus == Stopped)) {
      // not running, waiting
      delayActiveSeconds(2);
    } else if ((currStatus == Idle) || (currStatus == Humidify)) {
      // running
      checkDHTSensors();
    }

  // do not do delays here...
  Serial.println("=========================================================");
}


// only one active delay, check ALL sensors here and print!
void delayActiveSeconds(unsigned long elapsedTimeSec) {
  unsigned long elapsedTimeMillis = elapsedTimeSec * 1000;

  unsigned long startTime = millis();
  while (startTime + elapsedTimeMillis > millis()) {

    // always: check buttons pressed
    int* buttonResult = checkButtons();
    int redButtonRead = buttonResult[0];
    int greenButtonRead = buttonResult[1];

    if (redButtonRead == PRESSED && (currStatus == Idle || currStatus == Humidify)) {
      Serial.println("Red button pressed!");
      setStop();

      // if test, start directly
    } else if (greenButtonRead == PRESSED && (currStatus == Stopped || currStatus == Error)) {
      Serial.println("Green button pressed!");
      setStart();
    }

    // always: check DHT sensor 0 + 1 / hum 0, temp 1, write to global
    dht1.readTempAndHumidity(temp_hum_val_sensor0);
    dht2.readTempAndHumidity(temp_hum_val_sensor1);

    float humidityS1 = temp_hum_val_sensor0[0];
    float tempS1 = temp_hum_val_sensor0[1];
    float humidityS2 = temp_hum_val_sensor1[0];
    float tempS2 = temp_hum_val_sensor1[1];

    String sensor1Str = String("Sensor 1") + String("\n") + String("Humidity: ") + String(humidityS1) + String(" %") + String("\n") + String("Temp.: ") + String(tempS1) + String(" C") + String("\n") + String("===================\n");
    String sensor2Str = String("Sensor 2") + String("\n") + String("Humidity: ") + String(humidityS2) + String(" %") + String("\n") + String("Temp.: ") + String(tempS2) + String(" C") + String("\n") + String("===================\n");

    String toPrint = sensor1Str + sensor2Str;
    //Serial.println(toPrint);
    printNewLCDScreenAndConsole(toPrint);


    // check motor
    if (digitalRead(FAULTn) == LOW) {
      byte result = motor5.getFault();
      if (result & FAULT) {

        // stop at motor fault
        printNewLCDScreenAndConsole("Motor 0 fault!");
        setStop();
        if (result & OCP) Serial.println("Chip overcurrent!");
        if (result & ILIMIT) Serial.println("Load current limit!");
        if (result & UVLO) Serial.println("Undervoltage!");
        if (result & OTS) Serial.println("Over temp!");
        break;
      }
    }

    delay(2000);
  }
}