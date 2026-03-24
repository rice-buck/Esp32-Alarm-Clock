#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <DHT.h>

// Pin Definitions
#define ENCODER_CLK 18 //Out A
#define ENCODER_DT  19 //Out B
#define ENCODER_SW  5  //Switch 
#define BUZZER_PIN  23 
#define DHTPIN 4

//Humidity sensor
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// OLED Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

RTC_DS3231 rtc;
Preferences prefs;

//using enums prevent massive if statements and modularity. Allows easy addition of modes without messing up previous modes
enum Mode { CLOCK, SET_HOUR, SET_MIN};
Mode currentMode = CLOCK;

//default alarm values
int alarmHour = 7;
int alarmMin = 0;

// 16x16 Alarm Bell Icon
const unsigned char bell_icon[] PROGMEM = {
  0x01, 0x00, 0x03, 0x80, 0x07, 0xc0, 0x07, 0xc0, 0x0f, 0xe0, 0x0f, 0xe0, 
  0x0f, 0xe0, 0x1f, 0xf0, 0x1f, 0xf0, 0x1f, 0xf0, 0x3f, 0xf8, 0x7f, 0xfc, 
  0x7f, 0xfc, 0x1f, 0xf0, 0x07, 0xc0, 0x03, 0x80
};

//Rotation value
int lastClkState;

//DHT values
unsigned long lastDHTRead = 0;
float h = 0, t = 0;

//update screen variables
bool needsUpdate = false;
int lastSecond = -1;

//button variables 
bool alarmActive = true;
const int LONG_PRESS_TIME = 1000; // milliseconds
int lastSWState = digitalRead(ENCODER_SW);
unsigned long pressedTime = 0;

void setup() {
  Serial.begin(115200);

  // Encoder & Buzzer Pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize I2C Devices
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  //set RTC time (only need this line uncommented if setting first time ever on RTC module)
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

   dht.begin();

  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 not found");
    while (1);
  }

  //get previously set alarms. saved using Prefrences.h
  prefs.begin("alarm-clock", false);
  alarmHour = prefs.getInt("h", 7);
  alarmMin = prefs.getInt("m", 0);

  lastClkState = digitalRead(ENCODER_CLK); //HIGH when unpressed
  display.clearDisplay();
}

// --- ENCODER LOGIC ---
int readEncoder() {
  int change = 0;
  int currentClkState = digitalRead(ENCODER_CLK);

  if (currentClkState != lastClkState && currentClkState == LOW) {
    //if CLK and DT are different, the knob moved clockwise
    if (digitalRead(ENCODER_DT) != currentClkState) {
      change = 1; // Clockwise
    } else {
      change = -1; // Counter-clockwise
    }
  }
  lastClkState = currentClkState;
  return change;
}

// --- DISPLAY LOGIC ---
void updateDisplay(DateTime now) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Only read DHT every 2 seconds to prevent lag
  if (millis() - lastDHTRead > 2000) {
    h = dht.readHumidity();
    t = dht.readTemperature();
    lastDHTRead = millis();
  }
  // Show Temperature (Top Left)
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  display.setTextSize(1);
  display.setCursor(1, 0); //upper left
  //second parameter is decimal place 
  display.print(((t * (9.0/5.0)) + 32), 1); //display fahrenheit
  display.print("F");

  //Show Humidity (Top Right)
  display.setTextSize(1);
  display.setCursor(84, 0); //upper right
  display.print(h, 0); //display humidity
  display.print("%");

  // Main Time Display (12-Hour Format)
  int displayHour = now.hour();
  String ampm = (displayHour >= 12) ? "PM" : "AM";
  if (displayHour == 0) displayHour = 12; // Midnight
  else if (displayHour > 12) displayHour -= 12;

  // Show Current Time
  display.setTextSize(2);
  display.setCursor(15, 20);
  if (displayHour < 10) display.print('0');
  display.print(displayHour);
  display.print(':');
  if (now.minute() < 10) display.print('0');
  display.print(now.minute());

  display.setTextSize(1);
  display.print(ampm);

  //display alarm ON/OFF
  display.setCursor(90, 50);
  if(alarmActive) display.drawBitmap(110, 45, bell_icon, 16, 16, SSD1306_WHITE);

  //conversions for 12hr time
  display.setTextSize(1);
  display.setCursor(0, 50);
  int alarmTwelve = alarmHour;
  String ampmAlarm = (alarmHour >= 12) ? "PM" : "AM";
  if(alarmTwelve == 0) alarmTwelve = 12; //when midnight
  else if (alarmTwelve > 12) alarmTwelve -= 12;

  // Show Alarm Status
  if (currentMode == CLOCK) {
    display.print("Alarm: ");
    if (alarmTwelve < 10) display.print('0');
    display.print(alarmTwelve);
    display.print(':');
    if (alarmMin < 10) display.print('0');
    display.print(alarmMin);
    display.print(ampmAlarm);
  } 

  else if (currentMode == SET_HOUR) {
    display.print("SET HOUR: ");
    //if (blink) {
      if (alarmTwelve < 10) display.print('0');
      display.print(alarmTwelve);
      display.print(ampmAlarm);
    //}
  } 
  else if (currentMode == SET_MIN) {
    display.print("SET MINUTE: ");
    //if (blink) {
      if (alarmMin < 10) display.print('0');
      display.print(alarmMin);
    //}
  }

  display.display();
}

void triggerAlarm() {
  for(int i=0; i<5; i++) {
    tone(BUZZER_PIN, 4500, 1000); 
    delay(1500);
    tone(BUZZER_PIN, 2500, 500);
    delay(1500);
  }
}


void loop() {
  DateTime now = rtc.now();

  // Only needs to update once a second
  if (now.second() != lastSecond) {
    needsUpdate = true;
    lastSecond = now.second();
  }

  // Check if there are any changes on the encoder
  int change = readEncoder();
  if (change != 0) {
    needsUpdate = true; //update when knob is turned
    if (currentMode == SET_HOUR) {
      alarmHour = (alarmHour + change + 24) % 24;
    } else if (currentMode == SET_MIN) {
      alarmMin = (alarmMin + change + 60) % 60;
    }
  }

 int currentSWstate = digitalRead(ENCODER_SW);
    // Detect when button is first pressed
    if (currentSWstate == LOW && lastSWState == HIGH) {
      pressedTime = millis();
    }
    //Detect when button is released
    else if(currentSWstate == HIGH && lastSWState == LOW){ 
      long pressDuration = millis() - pressedTime;

      if(pressDuration > LONG_PRESS_TIME){
        alarmActive = !alarmActive;
        needsUpdate = true;
      }

      else if(pressDuration < LONG_PRESS_TIME) {
      delay(250); 
      needsUpdate = true; //update when knob is pressed
      if (currentMode == CLOCK) currentMode = SET_HOUR;
      else if (currentMode == SET_HOUR) currentMode = SET_MIN;
      else {
        currentMode = CLOCK;
        prefs.putInt("h", alarmHour);
        prefs.putInt("m", alarmMin);
        }
      }
    }
  lastSWState = currentSWstate; //toggle SW state

  // Only updates the screen when necessary
  if (needsUpdate) {
    updateDisplay(now);
    needsUpdate = false; 
  }

  // Trigger Alarm
  if (now.hour() == alarmHour && now.minute() == alarmMin && now.second() < 1 && alarmActive == true) {
    triggerAlarm();
    }
  }