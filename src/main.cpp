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
enum Mode { CLOCK, SET_HOUR, SET_MIN };
Mode currentMode = CLOCK;

int alarmHour = 7;
int alarmMin = 0;
int lastClkState;
unsigned long lastDHTRead = 0;
float h = 0, t = 0;

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

  // Show Alarm Status / Set Mode
  display.setTextSize(1);
  display.setCursor(0, 50);
  
  //bool blink = (millis() % 1000 < 500); // Create a 2Hz blink

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
    //if (blink) {
      if (alarmHour < 10) display.print('0');
      display.print(alarmHour);
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
    delay(200);
    tone(BUZZER_PIN, 4500, 1000);
    delay(200);
  }
}

void loop() {
  // Cycle Modes with each button press
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

  // Adjust Values
  int change = readEncoder();
  if (change != 0) {
    if (currentMode == SET_HOUR) {
      alarmHour = (alarmHour + change + 12) % 12;
    } else if (currentMode == SET_MIN) {
      alarmMin = (alarmMin + change + 60) % 60;
    }
  }

  // Check if currrent time is alarm time 
  DateTime now = rtc.now();
  if (now.hour() == alarmHour && now.minute() == alarmMin && now.second() < 1) { //only triggers alarm while second is 0, otherwise would trigger 60 times
    triggerAlarm();
  }

  updateDisplay(now);
}