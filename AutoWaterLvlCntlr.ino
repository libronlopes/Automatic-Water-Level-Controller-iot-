#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,16,2);
//2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// Pins
#define gndPin 13
int triggerPin = 11;
int echoPin = 12;
int buttonPin = 7;
int buttonLedPin = 6;
int RelayPin = 5;

// Tank Details (in cms)
int tankDepth = 130;
int sensorSafety = 22;
int percentWaterCorrection;

// Ultrasonic Sensor Variables
int triggerState = LOW;
int printSensor = LOW;
int triggerInterval = 1;
int sensorReadInterval = 1000;
long previousSensorMillis = 0;

// Button Variables
int mode = 1;
int limit = 75; // 25, 50, 75
int ledState = LOW;
long previousMillisLight = 0;
long buttonLightBlinkInterval = 1000;
long buttonPressedTime = 0;
boolean buttonLedState = false;

// Button press debounce in milli-seconds
long modeChangeDebounce = 100;
long limitChangeDebounce = 2000;
long depthChangeDebounce = 10000;


// Pump Variables in milli-seconds
long pumpOnInterval = 3000;
long pumpOffInterval = 3000;
boolean pumpState = false;
long previousPumpMillis;
long currentPumpMillis;

// LCD Variables in milli-seconds
boolean checkModePrint = false;
boolean lcdBacklight = true;
boolean checkLimitUpdated = false;
long previousLcdMillis = 0;
long lcdBacklightInterval = 30000;

long depthUpdateInterval = 2000;
long prevDepthDisplayMillis = 0;
boolean safetyCutOff = false;
boolean forceStart = false;
long prevSafeCutOffMillis = 0;
boolean sensorError = false;
int sensorErrorCount = 0;

//BLYNK INITIALIZATION
#define BLYNK_PRINT Serial
#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>
#include <SoftwareSerial.h>
SoftwareSerial EspSerial(2, 3); // RX, TX
#define ESP8266_BAUD 9600
ESP8266 wifi(&Serial);
char auth[] = "2SN0-TZ4ZyMoC***************R";
char ssid[] = "";
char pass[] = "";

void setup() {
  
  
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buttonLedPin, OUTPUT);
  pinMode(RelayPin, OUTPUT);
  pinMode(triggerPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // additional ground pin
  pinMode(gndPin, OUTPUT);
  digitalWrite(gndPin, LOW);
  
  Serial.begin(9600);
  EspSerial.begin(ESP8266_BAUD);
  delay(10);
  Blynk.begin(auth, wifi, ssid, pass);
  Blynk.syncAll();
    
}

void loop() 
{
  //start Blynk
  Blynk.run();
  updateModeSetting();
  while (digitalRead(buttonPin) == LOW ) {
    delay(100);
    buttonPressedTime = buttonPressedTime + 100;
    previousLcdMillis = millis();
    if (buttonPressedTime >= depthChangeDebounce && mode == 1) {
      clearLCDSegment1A();
      clearLCDSegment1B();
      printLCDSegment1A("SET");
      printLCDSegment1B("TANK DEPTH");
      updateLcdPrint();
      int tempTankDepth = autoSensorReading();
      if (tempTankDepth != 0) {
        tankDepth = tempTankDepth - sensorSafety;
        clearLCDSegment2A();
        printLCDSegment2A("Depth:" + String(tankDepth));
        Blynk.virtualWrite(V2,tankDepth);
        delay(1000);
        clearLCDSegment2A();
        printLCDSegment2A("Depth Saved: " + String(tankDepth));
      }
    }  
      if (mode == 1 || mode == 3) {
        clearLCDSegment1A();
        clearLCDSegment1B();
        printLCDSegment1A("SET");
        switch (mode) {
          case 1:
            printLCDSegment1B("Entering");
            break;
          case 3:
            printLCDSegment1B("Exiting");
            break;
        }
      } else if (mode == 2) {
        clearLCDSegment1B();
        printLCDSegment1B("Starting");
      }
    }
  }
  if (buttonPressedTime >= limitChangeDebounce) 
  {
    if (mode == 1 || mode == 3) 
    {
      mode == 1 ? mode = 3 : mode = 2;
    } 
    else if (mode == 2) 
    {
      safetyCutOff = false;
      forceStart = true;
      clearLCDSegment1B();
      printLCDSegment1B("FORCESTART");
      delay(2000);
      turnOnPump();
    }
    updateLcdPrint();
  }
  else if (buttonPressedTime >= modeChangeDebounce) 
  {
    if (!lcdBacklight) 
    {
      turnOnBacklight();
    } 
    else 
    {
      updateLcdPrint();
      if (mode <= 2) 
      {
        mode == 2 ? mode = 0 : mode++;
      } 
      else 
      {
        if (mode == 3) 
        {
          limit == 75 ? limit = 25 : limit += 25;
          clearLCDSegment1A();
          clearLCDSegment1B();
          printLCDSegment1A("SET");
          printLCDSegment1B("Limit:" + String(limit) + "%");
          Blynk.virtualWrite(V3,limit);
          checkLimitUpdated = true;
        }
      }
    }
  }
  buttonPressedTime = 0;
}


//update app if mode is changed using hardware button.
void blynkModeChange()
{
  if(mode == 0)
  {
    Blynk.virtualWrite(V1,2);
  }
  else if(mode == 1)
  {
    Blynk.virtualWrite(V1,1);
  }
  else if(mode == 2)
  {
    Blynk.virtualWrite(V1,3);
  }
}


//send sensor reading to app.
BLYNK_READ(V0)
  {
   Blynk.virtualWrite(V0,percentWaterCorrection); 
  }

//update hardware from app.

BLYNK_WRITE(V1)
{  
  switch(param.asInt())
  {
    case 1:{
      updateLcdPrint();
      mode = 1; 
      updateModeSetting();
      if (!lcdBacklight) 
    {
      turnOnBacklight();
    }
      break;
    }
    case 2:{
      updateLcdPrint();
      mode = 0; 
      updateModeSetting();
      if (!lcdBacklight) 
    {
      turnOnBacklight();
    }
      break;
    }
    case 3:{
      updateLcdPrint();
      mode = 2; 
      updateModeSetting();
      if (!lcdBacklight) 
    {
      turnOnBacklight();
    }
      break;
    }
  }
}

BLYNK_WRITE(V2)
{
  tankDepth = param.asInt();
  clearLCDSegment2A();
  printLCDSegment2A("Depth:" + String(tankDepth));
  delay(1000);
  clearLCDSegment2A();
  printLCDSegment2A("Depth Saved: " + String(tankDepth));
  updateModeSetting();
}

BLYNK_WRITE(V3)
{
  limit = param.asInt();
  if(mode == 2)
  {
    clearLCDSegment1B();
    printLCDSegment1B("Limit:" + String(limit) + "%");
  }
  checkLimitUpdated = true;
  updateModeSetting(); 
 }




void updateModeSetting()
{
  switch (mode) {
    case 0: // ON Mode
      if (!checkModePrint) {
        clearLCDSegment1A();
        printLCDSegment1A("ON");
        updateLcdPrint();
        if (checkLimitUpdated) {
          updateLimitSetting();
        }
        if (!pumpState) {
          clearLCDSegment1B();
          printLCDSegment1B("TURNING ON");
        }
      }
      if (!pumpState) {
        turnOnPump();
      }
      break;

    case 1: // OFF Mode
      if (!checkModePrint) {
        clearLCDSegment1A();
        clearLCDSegment1B();
        printLCDSegment1A("OFF");
        updateLcdPrint();
        if (checkLimitUpdated) {
          updateLimitSetting();
        }
        if (pumpState) {
          clearLCDSegment1B();
          printLCDSegment1B("TURNING OFF");
        }
      }
      if (pumpState) {
        turnOffPump();
      }
      break;
    case 2: // AUTO Mode
      if (!checkModePrint) {
        clearLCDSegment1B();
        clearLCDSegment1A();
        printLCDSegment1A("AUTO");
        printLCDSegment1B("Limit:" + String(limit) + "%");
        clearLCDSegment2A();
        printLCDSegment2A("Initializing...");
        updateLcdPrint();
        blynkModeChange();
        if (checkLimitUpdated) {
          updateLimitSetting();
        }
        if (pumpState && !forceStart) {
          pumpState = false;
          digitalWrite(RelayPin, LOW);
        }
        forceStart = false;
      }
      break;
    case 3: // SET Mode
      if (!checkModePrint) {
        clearLCDSegment1B();
        clearLCDSegment1A();
        printLCDSegment1A("SET");
        printLCDSegment1B("Limit:" + String(limit) + "%");
        updateLcdPrint();
      }
      break;
    case 4:
      if (!checkModePrint) {
        clearLCDSegment1B();
        clearLCDSegment1A();
        printLCDSegment1A("SET");
        printLCDSegment1B("Depth:" + String(limit) + "%");
        updateLcdPrint();
      }
      break;
    default:
      break;
  }
  
  if (pumpState) {
    buttonLightBlink();
    if (mode == 2) {
      safeCutOff();
    }
  } else {
    if (lcdBacklight) {
      turnOffBacklight();
    }
  }
  mode <= 2 ? displayTankDetails() : clearLCDSegment2A();
}

void safeCutOff() {
  long safeCutOffInterval;
  switch (limit) {
    case 25:
      safeCutOffInterval = 2700000;
      break;
    case 50:
      safeCutOffInterval = 1800000;
      break;
    case 75:
      safeCutOffInterval = 1200000;
      break;
    default:
      safeCutOffInterval = 2700000;
  }
  long currentCutOffMillis = millis();
  if (currentCutOffMillis - prevSafeCutOffMillis > safeCutOffInterval)
  {
    prevSafeCutOffMillis = currentCutOffMillis;
    safetyCutOff = true;
    turnOffPump();
  }

}

void turnOffBacklight() {
  long currentLcdMillis = millis();
  if (currentLcdMillis - previousLcdMillis > lcdBacklightInterval) {
    previousLcdMillis = currentLcdMillis;
    lcdBacklight = false;
    lcd.noBacklight();
  }
}

void turnOnBacklight() {
  lcdBacklight = true;
  previousLcdMillis = millis();
  lcd.backlight();
}

void turnOnPump() {
  currentPumpMillis = millis();
  if (currentPumpMillis - previousPumpMillis > pumpOnInterval)
  {
    previousPumpMillis = currentPumpMillis;
    togglePumpState();
  }
  WidgetLED led1(V4);
  led1.on();
}

void turnOffPump() {
  currentPumpMillis = millis();
  if (currentPumpMillis - previousPumpMillis > pumpOffInterval)
  {
    previousPumpMillis = currentPumpMillis;
    togglePumpState();
  }
  WidgetLED led1(V4);
  led1.off();
}

void togglePumpState() {
  clearLCDSegment1B();
  previousLcdMillis = millis();
  if (pumpState) {
    printLCD(5, 0, "PUMP OFF");
    pumpState = !pumpState;
    digitalWrite(RelayPin, LOW);
    digitalWrite(buttonLedPin, LOW);
  } else {
    prevSafeCutOffMillis = millis();
    turnOnBacklight();
    printLCD(5, 0, "PUMP ON");
    pumpState = !pumpState;
    digitalWrite(RelayPin, HIGH);
  }
}

void displayTankDetails() 
{
  long currentDepthDisplayMillis = millis();
  if (currentDepthDisplayMillis - prevDepthDisplayMillis >= depthUpdateInterval) 
  {
    prevDepthDisplayMillis = currentDepthDisplayMillis;
    int currentDepth = autoSensorReading();
    if (currentDepth > 0 && !sensorError) 
    {
      sensorErrorCount = 0;
      clearLCDSegment2A();
      int percentWater = (tankDepth - (currentDepth - sensorSafety)) * 100 / tankDepth;
      percentWaterCorrection = percentWater >= 100 ? 100 : percentWater <= 0 ? 0 : percentWater;
      printLCDSegment2A("Water Level:" + String(percentWaterCorrection) + "%");
      if (mode == 2) 
      {
        if (percentWaterCorrection <= limit && !pumpState) 
        {
          if (!safetyCutOff) 
          {
            turnOnPump();
          } 
          else 
          {
            clearLCDSegment1B();
            printLCD(5, 0, "SAFE OFF");
          }
        } 
        else if (percentWaterCorrection >= 100 && pumpState) 
        { 
          turnOffPump();
          delay(1000);
          updateLcdPrint();
        }
      }
    } 
    else 
    {
      if (sensorErrorCount < 5 && mode == 2) 
      {
        sensorErrorCount++; 
      }
      if (sensorErrorCount >= 5 && mode == 2) 
      {
        sensorError = true;
        clearLCDSegment2A();
        printLCDSegment2A("Sensor Error");
        if (pumpState) 
        {
          turnOffPump(); 
        }
      }
    }
  }
}

void updateLimitSetting()
{
  checkLimitUpdated = !checkLimitUpdated;
//  clearLCDSegment2A();
//  printLCDSegment2A("Saving");
//  delay(1000);
//  clearLCDSegment2A();
//  printLCDSegment2A("Saved");
//  delay(1000);
//  clearLCDSegment2A();
}

int autoSensorReading() {
  unsigned long currentSensorMillis = millis();
  if (currentSensorMillis - previousSensorMillis >= triggerInterval) {
    previousSensorMillis = currentSensorMillis;
    triggerState == LOW ? triggerState = HIGH : triggerState = LOW;
  }
  digitalWrite(triggerPin, triggerState);
  int duration, distance;
  duration = pulseIn(echoPin, HIGH);
  distance = (duration / 2) / 29.1;
  return distance;
}

void buttonLightBlink() {
  long currentMillisLight = millis();
  digitalWrite(buttonLedPin, ledState);
  if (currentMillisLight - previousMillisLight >= buttonLightBlinkInterval) {
    // save the last time you blinked the LED
    previousMillisLight = currentMillisLight;
    ledState == LOW ? ledState = HIGH : ledState = LOW;
    digitalWrite(buttonLedPin, ledState);
  }
}

void updateLcdPrint() {
  checkModePrint = !checkModePrint;
}

void printLCD(int cursorRow, int cursorColumn, String message) {
  lcd.setCursor(cursorRow, cursorColumn);
  lcd.print(message);
}

void printLCDSegment1A(String message) {
  printLCD(0, 0, message);
}

void clearLCDSegment1A() {
  printLCD(0, 0, "    ");
}

void printLCDSegment1B(String message) {
  printLCD(5, 0, message);
}

void clearLCDSegment1B() {
  printLCD(5, 0, "           ");
}

void printLCDSegment2A(String message) {
  printLCD(0, 1, message);
}

void clearLCDSegment2A() {
  printLCD(0, 1, "                ");
}
