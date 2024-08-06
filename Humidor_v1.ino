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

String VERSION_STR = "Zigarren-Hans V0.0.5";

bool IS_TEST = false;
bool IS_HUMIDOR_SETUP_MODE = false;

const float HUMIDITY_LIMIT = 68.0F;

// humidity steps
const float HUMIDITY_LOW = 55;
const float HUMIDITY_MID = 60;
const float HUMIDITY_MIDHIGH = 65;
const float HUMIDITY_HIGH = HUMIDITY_LIMIT;

float temp_hum_val_sensor0[2] = { 0.0F, 0.0F };
float temp_hum_val_sensor1[2] = { 0.0F, 0.0F };

int humifidyCountGlobal = 0;
int timeToWaitSecHumidify = 0;

// history avg hum. sensors
float avg_both_humidity_sensors[5] = { 0.0F, 0.0F, 0.0F, 0.0F, 0.0F };

unsigned long timeStampHumidifyProcess = 0;

// Methods
// ============================================================================================================================================

// base status == idle
enum Status { Humidify,
              Idle,
              Stopped,
              Error };
Status currStatus;

char dateTimeString[25];
void createRunningTime(char* buffer, size_t bufferSize) {
  unsigned long currentMillis = millis();
  unsigned long seconds = currentMillis / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;

  unsigned long remainingSeconds = seconds % 60;
  unsigned long remainingMinutes = minutes % 60;
  unsigned long remainingHours = hours % 24;

  snprintf(buffer, bufferSize, "%02ld d %02ld:%02ld:%02ld", days, remainingHours, remainingMinutes, remainingSeconds);
}

void printMillisTimeSerialAndLcd(unsigned long millis) {
  unsigned long seconds = millis / 1000;
  millis %= 1000;
  unsigned long minutes = seconds / 60;
  seconds %= 60;
  unsigned long hours = minutes / 60;
  minutes %= 60;

  lcd.print(F("Last:\n"));
  lcd.print(hours);
  lcd.print(F("h "));
  lcd.print(minutes);
  lcd.print(F("m "));
  lcd.print(seconds);
  lcd.print(F("s "));

  lcd.print(F("@ "));
  lcd.print(timeToWaitSecHumidify);
  lcd.print(F("s\n"));

  Serial.print(F("Last:\n"));
  Serial.print(hours);
  Serial.print(F("h "));
  Serial.print(minutes);
  Serial.print(F("m "));
  Serial.print(seconds);
  Serial.print(F("s "));

  // watering time
  Serial.print(F("@ "));
  Serial.print(timeToWaitSecHumidify);
  Serial.print(F("s\n"));
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
  lcd.println(VERSION_STR);
  lcd.println(F("==================="));
}

void printNewLCDScreenAndConsole(const char *toPrint) {
  printHeadLines();

  lcd.println(toPrint);
  Serial.println(toPrint);

  const char* statusStr;
  switch (currStatus) {
    case Stopped:
      statusStr = "--Stopped!--";
      break;
    case Idle:
      statusStr = "--Idle!--";
      break;
    case Humidify:
      timeStampHumidifyProcess = millis();
      statusStr = "--Humidifying!--";
      break;
    case Error:
      statusStr = "--Error!--";
      break;
  }

  lcd.println(statusStr);
  Serial.println(statusStr);

  lcd.print(F("Humidified: "));
  lcd.print(humifidyCountGlobal);
  lcd.print(F(" x\n"));

  Serial.print(F("Humidified: "));
  Serial.print(humifidyCountGlobal);
  Serial.print(F(" x\n"));

  if (timeStampHumidifyProcess != 0) {
    unsigned long timeBetweenNowAndLast = millis() - timeStampHumidifyProcess;
    printMillisTimeSerialAndLcd(timeBetweenNowAndLast);
  } else {
    lcd.println(F("Not yet humidified!"));
    Serial.println(F("Not yet humidified!"));
  }
}

// function buttons: red-stop all, green-start all
void checkButtons(int &redButtonRead, int &greenButtonRead) {
  redButtonRead = digitalRead(PIN_IN_RED_BUTTON_IN);
  greenButtonRead = digitalRead(PIN_IN_GREEN_BUTTON_IN);
}

// keep history, if average history differs too much from target value, increase humidity time
float adjustHumidifyingTime(float sensorAvg) {
  int sizeArray = sizeof(avg_both_humidity_sensors)/sizeof(avg_both_humidity_sensors[0]);

  // shuffle
  for (int i = 0; i < sizeArray - 1; i++ ) {
  // 0<-1 .. 3<-4
    avg_both_humidity_sensors[i] = avg_both_humidity_sensors[i+1];
  }
  // 4<-new
  avg_both_humidity_sensors[sizeArray-1] = sensorAvg; // new value

  // debug array content
  //Serial.println("SIZE " + String(avg_both_humidity_sensors[4]) + " " + String(avg_both_humidity_sensors[3]) + " " + String(avg_both_humidity_sensors[2]) + " " + String(avg_both_humidity_sensors[1]) + " " + String(avg_both_humidity_sensors[0]));

  // check which data is available, calculate avg delta from all old values
  float sum = 0.0F;
  float count = 0.0F;
  for (int i = 0; i < sizeArray; i++) {
    if (avg_both_humidity_sensors[i] != 0.0F) {
      sum += avg_both_humidity_sensors[i];
      count += 1.0F;
    }
  }
  float historyAverage = 0.0F;
  if (count != 0.0F) {
    historyAverage = sum / count;
  }

  // check deviation from average to target value
  float deviation = ((HUMIDITY_LIMIT - historyAverage) / historyAverage) * 100.0F;

  // if deviation >5%, increase by absolute value (this is reset each run to default, might to be enhanced..)
  unsigned int increaseTimeSec = 5;

  if (deviation > 5.0F) {
    timeToWaitSecHumidify += increaseTimeSec;
    Serial.print(F("Deviation too high: "));
    Serial.println(deviation);
    //Serial.println(timeToWaitSecHumidify);
  }
  return timeToWaitSecHumidify;
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
  Serial.println(F("Setup done!"));

  if(IS_TEST)
  {
    timeStampHumidifyProcess = millis();
  }
}

// use timed loops to check buttons
void checkDHTSensors() {

  // first wait 20min here
  int timeToWaitSec = 20 * 60; // maybe: move hc to adjustable...
  if (IS_TEST) {
    // set to 20s
    timeToWaitSec = 20;
  }

  // check all live sensors
  Serial.print(F("Waiting :"));
  Serial.println(String(timeToWaitSec));

  delayActiveSeconds(timeToWaitSec);

  // calculate data
  dht1.readTempAndHumidity(temp_hum_val_sensor0);
  dht2.readTempAndHumidity(temp_hum_val_sensor1);

  float humidityS1 = temp_hum_val_sensor0[0];
  float humidityS2 = temp_hum_val_sensor1[0];

  // check if sensors ok, otherwise error!
  if ((humidityS1 == 0.0F) || (humidityS2 == 0.0F)) {
    Serial.println(F("Error, sensors not working!"));
      setError();
  }

  float divHumidity = (humidityS1 - humidityS2) / humidityS1 * 100;
  
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

      // run motor 2 min
      int runMotorBeforeHumidifyTimeSec = 2*60;
      if (IS_TEST) {
        runMotorBeforeHumidifyTimeSec = 1;
      }

      // start motor "on"
      motor5.drive(100);

      // time to run before humidify 
      delayActiveSeconds(runMotorBeforeHumidifyTimeSec);

      // -------humidify on
      float humidityAverage = (humidityS1 + humidityS2) / 2.0F;

      // set default/base times

      // <55%
      if(humidityAverage < HUMIDITY_LOW) {
        timeToWaitSecHumidify = 25;
      // >= 55 < 60%
      } else if ((humidityAverage >= HUMIDITY_LOW) && (humidityAverage < HUMIDITY_MID)) {
        timeToWaitSecHumidify = 20;
      // >= 60 < 65%
      } else if ((humidityAverage >= HUMIDITY_MID) && (humidityAverage < HUMIDITY_MIDHIGH)) {
        timeToWaitSecHumidify = 15;
      // >= 65 < 70%
      } else if ((humidityAverage >= HUMIDITY_MIDHIGH) && (humidityAverage < HUMIDITY_HIGH)) {
        timeToWaitSecHumidify = 10;
      }

      timeToWaitSecHumidify = adjustHumidifyingTime(humidityAverage);

      // humidor new, setup
      if(IS_HUMIDOR_SETUP_MODE) {
        timeToWaitSecHumidify = 60;
      }

      // humidify
      digitalWrite(PIN_OUT_WATER_ATOMIZER, HIGH);  // atomize full

      // humidify until...
      if (IS_TEST) {
        // if test, only do one second
        delayActiveSeconds(1);
      } else {
        delayActiveSeconds(timeToWaitSecHumidify);
      }

      // stop humidifier
      digitalWrite(PIN_OUT_WATER_ATOMIZER, LOW);  // atomization stopped
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
  Serial.println(F("========================================================="));
}


// only one active delay, check ALL sensors here and print!
void delayActiveSeconds(unsigned long elapsedTimeSec) {
  unsigned long elapsedTimeMillis = elapsedTimeSec * 1000;

  unsigned long startTime = millis();
  while (startTime + elapsedTimeMillis > millis()) {

    // Debug
    //adjustTimeBySensorHistory(123.0F);

    // always: check buttons pressed
    int redButtonRead, greenButtonRead;
    checkButtons(redButtonRead, greenButtonRead);

    if (redButtonRead == PRESSED && (currStatus == Idle || currStatus == Humidify)) {
      //Serial.println("Red button pressed!");
      setStop();

      // if test, start directly
    } else if (greenButtonRead == PRESSED && (currStatus == Stopped || currStatus == Error)) {
      //Serial.println("Green button pressed!");
      setStart();
    }

    // always: check DHT sensor 0 + 1 / hum 0, temp 1, write to global
    dht1.readTempAndHumidity(temp_hum_val_sensor0);
    dht2.readTempAndHumidity(temp_hum_val_sensor1);

    float humidityS1 = temp_hum_val_sensor0[0];
    float tempS1 = temp_hum_val_sensor0[1];
    float humidityS2 = temp_hum_val_sensor1[0];
    float tempS2 = temp_hum_val_sensor1[1];

    char buffer[256];
    snprintf(buffer, sizeof(buffer),
         "Sensor 1\nHumidity: %d.%02d %%\nTemp.: %d.%02d C\n===================\n"
         "Sensor 2\nHumidity: %d.%02d %%\nTemp.: %d.%02d C\n===================",
         (int)humidityS1, (int)(humidityS1 * 100) % 100,
         (int)tempS1, (int)(tempS1 * 100) % 100,
         (int)humidityS2, (int)(humidityS2 * 100) % 100,
         (int)tempS2, (int)(tempS2 * 100) % 100);

    printNewLCDScreenAndConsole(buffer);

    // check motor
    if (digitalRead(FAULTn) == LOW) {
      byte result = motor5.getFault();
      if (result & FAULT) {

        // stop at motor fault
        printNewLCDScreenAndConsole("Motor 0 fault!");
        setStop();
        if (result & OCP) Serial.println(F("Chip overcurrent!"));
        if (result & ILIMIT) Serial.println(F("Load current limit!"));
        if (result & UVLO) Serial.println(F("Undervoltage!"));
        if (result & OTS) Serial.println(F("Over temp!"));
        break;
      }
    }

    Serial.flush();
    delay(2000);
  }
}
