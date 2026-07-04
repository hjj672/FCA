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
const float HUMIDITY_LOW_THRESHOLD = 87;
const float HUMIDITY_HIGH_THRESHOLD = 95.0;

const unsigned long HUMIDITY_CHECK_INTERVAL = 10000; 
const unsigned long CSV_LOG_INTERVAL = 300000; // 5 minutes
const unsigned long LIGHT_CYCLE_DURATION = 43200 /2 *1000; // 6 hours

// --- Relay States ---
const int HUMIDIFIER_ON_STATE = HIGH;
const int HUMIDIFIER_OFF_STATE = LOW;
const int RANDOM_RELAY_ON_STATE = HIGH;
const int RANDOM_RELAY_OFF_STATE = LOW;
const int LIGHT_ON_STATE = HIGH;
const int LIGHT_OFF_STATE = LOW;

// --- Objects ---
Adafruit_SHT31 sht31_1 = Adafruit_SHT31(&Wire);
Adafruit_SHT31 sht31_2 = Adafruit_SHT31(&Wire1);

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
// --- State Variables ---
bool isHumidifierOn = false;
bool isLightOn = false;
bool startupHumidityReached = false;
//bool humidityPrimed = false;

unsigned long lastHumidifierCheck = 0;
unsigned long lastLogTime = 0;
unsigned long lastHumTick = 0;
unsigned long humidifierOnTime = 0;

// Random Cycle Variables
enum RandomCycleState { STATE_OFF, STATE_ON };
RandomCycleState randomState = STATE_OFF;
unsigned long randomCycleLastChangeTime = 0;
unsigned long currentRandomOnDuration = 0;
unsigned long currentRandomOffDuration = 0;

// --- Function Declarations ---
void updateDisplay(float avgH, float avgT, float h1, float t1, float h2, float t2);
void handleDataLogging(unsigned long currentTime);
void handleRandomCycleRelay(unsigned long currentTime);
void handleLightSchedule(unsigned long currentTime);
void handleHumidifierControl(unsigned long currentTime);
bool getSensorReadings(float &h1, float &t1, float &h2, float &t2, float &avgH, float &avgT);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize relay pins
  pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
  pinMode(RELAY_RANDOM_1, OUTPUT);
  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  
  // Set all relays to OFF state
  digitalWrite(HUMIDIFIER_RELAY_PIN, HUMIDIFIER_OFF_STATE);
  digitalWrite(RELAY_RANDOM_1, RANDOM_RELAY_OFF_STATE);
  digitalWrite(LIGHT_RELAY_PIN, LIGHT_OFF_STATE);

  // Initialize I2C buses BEFORE sensor initialization
  Wire.begin(SDA_1, SCL_1, 100000);
  Wire1.begin(SDA_2, SCL_2, 100000);

  delay(100);
  
  // Initialize SHT31 sensors on separate I2C buses
  Serial.println("Initializing SHT31 sensors...");

  if (!sht31_1.begin(0x44)) {
    Serial.println("SHT31 (1) not found on I2C_1!");
  } else {
    Serial.println("SHT31 (1) initialized");
  }

  if (!sht31_2.begin(0x44)) {
    Serial.println("SHT31 (2) not found on I2C_2!");
  } else {
    Serial.println("SHT31 (2) initialized");
  }


  // Initialize SPIFFS for data logging
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS Mount Failed");
  } else {
    Serial.println("SPIFFS Mounted successfully");

    // Create CSV header if file doesn't exist
    File file = SPIFFS.open("/datalog.csv", FILE_READ);
    if(!file) {
      file = SPIFFS.open("/datalog.csv", FILE_WRITE);
      file.println("Uptime(ms),Temp1(C),Hum1(%),Temp2(C),Hum2(%),AvgT(C),AvgH(%),HumidifierOnTime(ms),HumidifierState,LightState,RandomRelayState");
      file.close();
    } else {
      file.close();
    }
  }

  

  // Initialize display (uses default I2C bus)
  u8g2.begin();
  u8g2.setContrast(120);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 10, "BOOTING...");
  u8g2.sendBuffer();


  randomSeed(esp_random());
  Serial.println("Setup complete");
}

void loop() {
  unsigned long currentMillis = millis();

  // Main control loop
  handleHumidifierControl(currentMillis);
  handleLightSchedule(currentMillis);
  handleDataLogging(currentMillis);
  
  //if (humidityPrimed) {
  handleRandomCycleRelay(currentMillis);
  //}
}

bool getSensorReadings(float &h1, float &t1, float &h2, float &t2, float &avgH, float &avgT) {
  // Read sensor 1
  h1 = sht31_1.readHumidity();
  t1 = sht31_1.readTemperature();
  
  // Read sensor 2
  h2 = sht31_2.readHumidity();
  t2 = sht31_2.readTemperature();

  bool ok1 = !isnan(h1) && !isnan(t1);
  bool ok2 = !isnan(h2) && !isnan(t2);

  if (ok1 && ok2) {
    avgH = (h1 + h2) / 2.0;
    avgT = (t1 + t2) / 2.0;
    return true;
  }

  if (ok1) {
    avgH = h1;
    avgT = t1;
    return true;
  }

  if (ok2) {
    avgH = h2;
    avgT = t2;
    return true;
  }

  return false;
}

void handleHumidifierControl(unsigned long currentTime) {
  if (currentTime - lastHumidifierCheck >= HUMIDITY_CHECK_INTERVAL) {
    lastHumidifierCheck = currentTime;
    
    float h1, t1, h2, t2, avgH, avgT;
    
    if (!getSensorReadings(h1, t1, h2, t2, avgH, avgT)) {
      Serial.println("Failed to read sensors");
      return;
    }

    // Accumulate humidifier ON time
    if (isHumidifierOn) {
      if (lastHumTick == 0) lastHumTick = currentTime;
      humidifierOnTime += (currentTime - lastHumTick);
      lastHumTick = currentTime;
    }


    // Startup: always humidify until the chamber reaches 92%
    if (!startupHumidityReached) {

      if (!isHumidifierOn) {
        isHumidifierOn = true;
        digitalWrite(HUMIDIFIER_RELAY_PIN, HUMIDIFIER_ON_STATE);
        lastHumTick = currentTime;
        Serial.println("Startup humidification");
      }

      if (avgH >= HUMIDITY_HIGH_THRESHOLD) {
        startupHumidityReached = true;
        isHumidifierOn = false;
        digitalWrite(HUMIDIFIER_RELAY_PIN, HUMIDIFIER_OFF_STATE);
        Serial.println("Startup humidity reached");
      }

    }
    // Normal operation
    else {
        // Control based on average humidity
        if (!isHumidifierOn && avgH < HUMIDITY_LOW_THRESHOLD) {
          isHumidifierOn = true;
          digitalWrite(HUMIDIFIER_RELAY_PIN, HUMIDIFIER_ON_STATE);
          lastHumTick = currentTime;
          Serial.println("Humidifier ON - AvgH: " + String(avgH, 2));
        } 
        else if (isHumidifierOn && avgH > HUMIDITY_HIGH_THRESHOLD) {
          isHumidifierOn = false;
          digitalWrite(HUMIDIFIER_RELAY_PIN, HUMIDIFIER_OFF_STATE);
          Serial.println("Humidifier OFF - AvgH: " + String(avgH, 2));
          /*
      if (!humidityPrimed) {
        humidityPrimed = true;
        randomCycleLastChangeTime = currentTime;
        currentRandomOffDuration = random(120, 150) * 1000UL;
        Serial.println("Humidity primed - Random relay enabled");
      }*/
        }
      }
    updateDisplay(avgH, avgT, h1, t1, h2, t2);
  }
}

void handleLightSchedule(unsigned long currentTime) {
  bool shouldBeOn = (currentTime % (LIGHT_CYCLE_DURATION * 2)) > LIGHT_CYCLE_DURATION;
  
  if (shouldBeOn && !isLightOn) {
    digitalWrite(LIGHT_RELAY_PIN, LIGHT_ON_STATE);
    isLightOn = true;
    Serial.println("Light ON");
  } 
  else if (!shouldBeOn && isLightOn) {
    digitalWrite(LIGHT_RELAY_PIN, LIGHT_OFF_STATE);
    isLightOn = false;
    Serial.println("Light OFF");
  }
}

void handleRandomCycleRelay(unsigned long currentTime) {
  unsigned long timeInState = currentTime - randomCycleLastChangeTime;

  if (randomState == STATE_OFF) {
    if (timeInState >= currentRandomOffDuration) {
      randomState = STATE_ON;
      digitalWrite(RELAY_RANDOM_1, RANDOM_RELAY_ON_STATE);
      randomCycleLastChangeTime = currentTime;
      currentRandomOnDuration = random(15, 20) * 60 * 1000UL;
      Serial.println("Random relay ON for " + String(currentRandomOnDuration / 60000) + " minutes");
    }
  } else {
    if (timeInState >= currentRandomOnDuration) {
      randomState = STATE_OFF;
      digitalWrite(RELAY_RANDOM_1, RANDOM_RELAY_OFF_STATE);
      randomCycleLastChangeTime = currentTime;
      currentRandomOffDuration = random(40, 50) * 60 * 1000UL;
      Serial.println("Random relay OFF for " + String(currentRandomOffDuration / 60000) + " minutes");
    }
  }
}

void handleDataLogging(unsigned long currentTime) {
  if (currentTime - lastLogTime >= CSV_LOG_INTERVAL) {
    lastLogTime = currentTime;
    
    float h1, t1, h2, t2, avgH, avgT;
    
    if (!getSensorReadings(h1, t1, h2, t2, avgH, avgT)) {
      Serial.println("Failed to read sensors for logging");
      return;
    }

    String logData = String(currentTime) + "," + 
                     String(t1, 2) + "," + String(h1, 2) + "," + 
                     String(t2, 2) + "," + String(h2, 2) + "," + 
                     String(avgT, 2) + "," + String(avgH, 2) + "," +
                     String(humidifierOnTime) + "," +
                     String(isHumidifierOn ? "ON" : "OFF") + "," +
                     String(isLightOn ? "ON" : "OFF") + "," +
                     String(randomState == STATE_ON ? "ON" : "OFF");
                     
    File file = SPIFFS.open("/datalog.csv", FILE_APPEND);
    if(file) {
      file.println(logData);
      file.close();
      Serial.println("Logged: " + logData);
    } else {
      Serial.println("Failed to open log file");
    }
    
    humidifierOnTime = 0;
  }
}

void updateDisplay(float avgH, float avgT, float h1, float t1, float h2, float t2) {
  u8g2.clearBuffer();
  
  u8g2.setCursor(0, 10);
  u8g2.print("Avg: ");
  u8g2.print(avgT, 1); 
  u8g2.print("C ");
  u8g2.print(avgH, 1); 
  u8g2.print("%");
  
  u8g2.setCursor(0, 22);
  u8g2.print("S1: ");
  u8g2.print(t1, 1); 
  u8g2.print("C ");
  u8g2.print(h1, 1); 
  u8g2.print("%");
  
  u8g2.setCursor(0, 34);
  u8g2.print("S2: ");
  u8g2.print(t2, 1); 
  u8g2.print("C ");
  u8g2.print(h2, 1); 
  u8g2.print("%");
  
  u8g2.setCursor(0, 46);
  u8g2.print("H:");
  u8g2.print(isHumidifierOn ? "ON " : "OFF");
  u8g2.print(" L:");
  u8g2.print(isLightOn ? "ON" : "OFF");
  
  u8g2.sendBuffer();
}
