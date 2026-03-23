#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

// Pin Definitions
#define ENCODER_CLK 18
#define ENCODER_DT  19
#define ENCODER_SW  5
#define BUZZER_PIN  23

// OLED Settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

RTC_DS3231 rtc;
Preferences prefs;

//using enums prevent massive if statements and modularity. Allows easy addition of modes without messing up previous modes
enum Mode { CLOCK, SET_HOUR, SET_MIN };
Mode currentMode = CLOCK;

int alarmHour = 7;
int alarmMin = 0;
int lastClkState;

void setup() {
  Serial.begin(115200);

  // Encoder & Buzzer Pins
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  //set RTC time
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Initialize I2C Devices
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  Serial.println("Testing!");
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 not found");
    while (1);
  }

  //get previously set alarms. saved using Prefrences.h
  prefs.begin("alarm-clock", false);
  alarmHour = prefs.getInt("h", 7);
  alarmMin = prefs.getInt("m", 0);

  lastClkState = digitalRead(ENCODER_CLK);
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

  // Show Temperature (Top Right)
  display.setTextSize(1);
  display.setCursor(85, 0);
  display.print(((rtc.getTemperature() * (9/5)) + 32), 1); //display fahrenheit
  display.print("F");

  // Show Current Time
  display.setTextSize(2);
  display.setCursor(15, 20);
  if (now.hour() < 10) display.print('0');
  display.print(now.hour());
  display.print(':');
  if (now.minute() < 10) display.print('0');
  display.print(now.minute());

  // Show Alarm Status / Set Mode
  display.setTextSize(1);
  display.setCursor(0, 50);
  
  bool blink = (millis() % 1000 < 500); // Create a 2Hz blink

  if (currentMode == CLOCK) {
    display.print("Alarm: ");
    if (alarmHour < 10) display.print('0');
    display.print(alarmHour);
    display.print(':');
    if (alarmMin < 10) display.print('0');
    display.print(alarmMin);
  } 
  else if (currentMode == SET_HOUR) {
    display.print("SET HOUR: ");
    if (blink) {
      if (alarmHour < 10) display.print('0');
      display.print(alarmHour);
    }
  } 
  else if (currentMode == SET_MIN) {
    display.print("SET MINUTE: ");
    if (blink) {
      if (alarmMin < 10) display.print('0');
      display.print(alarmMin);
    }
  }

  display.display();
}

void triggerAlarm() {
  for(int i=0; i<3; i++) {
    tone(BUZZER_PIN, 1500, 150); 
    delay(200);
    tone(BUZZER_PIN, 1200, 150);
    delay(200);
  }
}

void loop() {
  // 1. Cycle Modes on Button Press
  if (digitalRead(ENCODER_SW) == LOW) {
    delay(250); // Debounce
    if (currentMode == CLOCK) currentMode = SET_HOUR;
    else if (currentMode == SET_HOUR) currentMode = SET_MIN;
    else {
      currentMode = CLOCK;
      prefs.putInt("h", alarmHour);
      prefs.putInt("m", alarmMin);
    }
  }

  // 2. Adjust Values
  int change = readEncoder();
  if (change != 0) {
    if (currentMode == SET_HOUR) {
      alarmHour = (alarmHour + change + 24) % 24;
    } else if (currentMode == SET_MIN) {
      alarmMin = (alarmMin + change + 60) % 60;
    }
  }

  // 3. Check Alarm
  DateTime now = rtc.now();
  if (now.hour() == alarmHour && now.minute() == alarmMin && now.second() < 1) {
    triggerAlarm();
  }

  updateDisplay(now);
}