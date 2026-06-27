#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <U8g2lib.h>
#include "SPIFFS.h"
#include "FS.h"

// --- Hardware Pins ---
const int HUMIDIFIER_RELAY_PIN = 14;
const int RELAY_RANDOM_1 = 32; 
const int LIGHT_RELAY_PIN = 33; 

// I2C 1 (Default)
const int SDA_1 = 21;
const int SCL_1 = 22;
// I2C 2
const int SDA_2 = 25;
const int SCL_2 = 26;

// --- Thresholds & Timings ---
const float HUMIDITY_LOW_THRESHOLD = 77.5;
const float HUMIDITY_HIGH_THRESHOLD = 88.0;

const unsigned long HUMIDITY_CHECK_INTERVAL = 10000; 
const unsigned long CSV_LOG_INTERVAL = 300000; // 5 minutes
const unsigned long LIGHT_CYCLE_DURATION = 43200000; // 12 hours ($12 \times 60 \times 60 \times 1000$)

// --- Relay States ---
const int HUMIDIFIER_ON_STATE = HIGH;
const int HUMIDIFIER_OFF_STATE = LOW;
const int RANDOM_RELAY_ON_STATE = HIGH;
const int RANDOM_RELAY_OFF_STATE = LOW;
const int LIGHT_ON_STATE = HIGH;
const int LIGHT_OFF_STATE = LOW;

// --- Objects ---
TwoWire I2C_1 = TwoWire(0);
TwoWire I2C_2 = TwoWire(1);
Adafruit_SHT31 sht31_1 = Adafruit_SHT31();
Adafruit_SHT31 sht31_2 = Adafruit_SHT31();
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// --- State Variables ---
bool isHumidifierOn = false;
bool isLightOn = false;
bool humidityPrimed = false;

unsigned long lastHumidifierCheck = 0;
unsigned long lastLogTime = 0;
unsigned long lastHumTick = 0;
unsigned long humidifierOnTime = 0; // Total ON time in ms

// Random Cycle Variables
enum RandomCycleState { STATE_OFF, STATE_ON };
RandomCycleState randomState = STATE_OFF;
unsigned long randomCycleLastChangeTime = 0;
unsigned long currentRandomOnDuration = 0;
unsigned long currentRandomOffDuration = 0;

void readCSV();
void listFiles();
void updateDisplay(float h, float t);
void handleDataLogging(unsigned long currentTime) ;
void handleRandomCycleRelay(unsigned long currentTime) ;
void handleLightSchedule(unsigned long currentTime) ;
void handleHumidifierControl(unsigned long currentTime) ;
bool getHumidity(float &humidity, float &temperature) ;


void setup() {
  Serial.begin(115200);
  
  pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
  pinMode(RELAY_RANDOM_1, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  
  digitalWrite(HUMIDIFIER_RELAY_PIN, HUMIDIFIER_OFF_STATE);
  digitalWrite(RELAY_RANDOM_1, RANDOM_RELAY_OFF_STATE);
  digitalWrite(LIGHT_RELAY_PIN, LIGHT_OFF_STATE);

  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS Mount Failed");
  } else {
    Serial.println("SPIFFS Mounted successfully");

    // Create header if file doesn't exist
    File file = SPIFFS.open("/datalog.csv", FILE_READ);
    if(!file) {
      file = SPIFFS.open("/datalog.csv", FILE_WRITE);
      file.println("Uptime(ms),Temp1(C),Hum1(%),Temp2(C),Hum2(%),AvgT(C),AvgH(%),HumidifierOnTime(ms)");
      file.close();
    } else {
      file.close();
    }
  }

  listFiles();
  readCSV();

  // Initialize I2C buses
  I2C_1.begin(SDA_1, SCL_1);
  I2C_2.begin(SDA_2, SCL_2);

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 10, "BOOTING...");
  u8g2.sendBuffer();

  if (!sht31_1.begin(0x44)) Serial.println("SHT31 (1) not found!");
  if (!sht31_2.begin(0x44)) Serial.println("SHT31 (2) not found!");

  randomSeed(esp_random());
}

void loop() {
  unsigned long currentMillis = millis();

  handleHumidifierControl(currentMillis);
  handleLightSchedule(currentMillis);
  handleDataLogging(currentMillis);
  
  if (humidityPrimed) {
    handleRandomCycleRelay(currentMillis);
  }
}

bool getHumidity(float &humidity, float &temperature) {
    float h1 = sht31_1.readHumidity();
    float t1 = sht31_1.readTemperature();

    float h2 = sht31_2.readHumidity();
    float t2 = sht31_2.readTemperature();

    bool ok1 = !isnan(h1) && !isnan(t1);
    bool ok2 = !isnan(h2) && !isnan(t2);

    if (ok1 && ok2) {
        humidity = (h1 + h2) / 2.0;
        temperature = (t1 + t2) / 2.0;
        return true;
    }

    if (ok1) {
        humidity = h1;
        temperature = t1;
        return true;
    }

    if (ok2) {
        humidity = h2;
        temperature = t2;
        return true;
    }

    return false;
}

void handleHumidifierControl(unsigned long currentTime) {
  if (currentTime - lastHumidifierCheck >= HUMIDITY_CHECK_INTERVAL) {
    lastHumidifierCheck = currentTime;
    
    float h1 = sht31_1.readHumidity();
    
    if (isnan(h1)) return;

    if (isHumidifierOn) {
      if (lastHumTick == 0) lastHumTick = currentTime;
      humidifierOnTime += (currentTime - lastHumTick);
      lastHumTick = currentTime;
    }

    if (!isHumidifierOn && h1 < HUMIDITY_LOW_THRESHOLD) {
      isHumidifierOn = true;
      digitalWrite(HUMIDIFIER_RELAY_PIN, HUMIDIFIER_ON_STATE);
      lastHumTick = currentTime;
    } 
    else if (isHumidifierOn && h1 > HUMIDITY_HIGH_THRESHOLD) {
      isHumidifierOn = false;
      digitalWrite(HUMIDIFIER_RELAY_PIN, HUMIDIFIER_OFF_STATE);
      
      if (!humidityPrimed) {
        humidityPrimed = true;
        randomCycleLastChangeTime = currentTime;
        currentRandomOffDuration = random(120, 150) * 1000UL;
      }
    }
    updateDisplay(h1, sht31_1.readTemperature());
  }
}

void handleLightSchedule(unsigned long currentTime) {
  // 12 hours ON, 12 hours OFF based on uptime
  bool shouldBeOn = (currentTime % (LIGHT_CYCLE_DURATION * 2)) < LIGHT_CYCLE_DURATION;
  
  if (shouldBeOn && !isLightOn) {
    digitalWrite(LIGHT_RELAY_PIN, LIGHT_ON_STATE);
    isLightOn = true;
  } else if (!shouldBeOn && isLightOn) {
    digitalWrite(LIGHT_RELAY_PIN, LIGHT_OFF_STATE);
    isLightOn = false;
  }
}

void handleRandomCycleRelay(unsigned long currentTime) {
  unsigned long timeInState = currentTime - randomCycleLastChangeTime;

  if (randomState == STATE_OFF) {
    if (timeInState >= currentRandomOffDuration) {
      randomState = STATE_ON;
      digitalWrite(RELAY_RANDOM_1, RANDOM_RELAY_ON_STATE);
      randomCycleLastChangeTime = currentTime;
      currentRandomOnDuration = random(10, 15)* 60 * 1000UL;
    }
  } else {
    if (timeInState >= currentRandomOnDuration) {
      randomState = STATE_OFF;
      digitalWrite(RELAY_RANDOM_1, RANDOM_RELAY_OFF_STATE);
      randomCycleLastChangeTime = currentTime;
      currentRandomOffDuration = random(45, 60) * 60 * 1000UL;
    }
  }
}

void handleDataLogging(unsigned long currentTime) {
  if (currentTime - lastLogTime >= CSV_LOG_INTERVAL) {
    lastLogTime = currentTime;
    
    float t1 = sht31_1.readTemperature();
    float h1 = sht31_1.readHumidity();
    float t2 = sht31_2.readTemperature();
    float h2 = sht31_2.readHumidity();

    String logData = String(currentTime) + "," + 
                     String(t1, 2) + "," + String(h1, 2) + "," + 
                     String(t2, 2) + "," + String(h2, 2) + "," + 
                     String(humidifierOnTime);
                     
    File file = SPIFFS.open("/datalog.csv", FILE_APPEND);
    if(file) {
      file.println(logData);
      file.close();
      Serial.println("Logged: " + logData);
    }
    
    // Reset humidifier accumulator for the next log period
    humidifierOnTime = 0;
  }
}

void updateDisplay(float h, float t) {
  u8g2.clearBuffer();
  u8g2.setCursor(0, 10);
  u8g2.print("T1: "); u8g2.print(t, 1); u8g2.print(" C");
  u8g2.setCursor(0, 22);
  u8g2.print("H1: "); u8g2.print(h, 1); u8g2.print(" %");
  u8g2.setCursor(0, 34);
  u8g2.print("Humidifier: "); u8g2.print(isHumidifierOn ? "ON" : "OFF");
  u8g2.setCursor(0, 46);
  u8g2.print("Light: "); u8g2.print(isLightOn ? "ON" : "OFF");
  u8g2.sendBuffer();
}

void listFiles() {
  File root = SPIFFS.open("/");
  File file = root.openNextFile();

  while (file) {
    Serial.printf("File: %s  Size: %d bytes\n", file.name(), file.size());
    file = root.openNextFile();
  }
}

void readCSV() {
  File file = SPIFFS.open("/log.csv", "r");
  if (!file) {
    Serial.println("CSV file not found.");
    return;
  }

  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}