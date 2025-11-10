#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <time.h>

// =================================================================
// 1. GLOBAL CONFIGURATION
// =================================================================

// --- WiFi Configuration ---
const char* ssid = "Sanji";
const char* password = "Sj*697666";

// --- Google Sheets Configuration ---
const char* GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbzWyqRs6bDF1ma7yHKFlbKQalvfzpu_P2Cr7F59t2JRnk-BN0E5Zs6renBmPrUGG2BC/exec";

// --- NTP Time Configuration (Tehran timezone) ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 12600;  // UTC+3:30 for Tehran (3.5 * 3600)
const int daylightOffset_sec = 0;   // Iran doesn't use daylight saving currently

// --- Humidifier Control ---
const int HUMIDIFIER_RELAY_PIN = 14;
const int HUMIDIFIER_ON_STATE = LOW;
const int HUMIDIFIER_OFF_STATE = !HUMIDIFIER_ON_STATE;
const float HUMIDITY_LOW_THRESHOLD = 88.0;
const float HUMIDITY_HIGH_THRESHOLD = 95.0;

// --- Random Cycle Relay (Only Pin 32) ---
const int RELAY_RANDOM_1 = 32;
const int RANDOM_RELAY_ON_STATE = LOW;
const int RANDOM_RELAY_OFF_STATE = !RANDOM_RELAY_ON_STATE;

// --- Light Schedule Relay (Pin 33) ---
const int LIGHT_RELAY_PIN = 33;
const int LIGHT_ON_STATE = LOW;
const int LIGHT_OFF_STATE = !LIGHT_ON_STATE;
const int LIGHT_ON_HOUR = 7;   // 7 AM
const int LIGHT_OFF_HOUR = 19; // 7 PM

// --- SHT31 Sensor Configuration ---
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;
Adafruit_SHT31 sht31_1 = Adafruit_SHT31();
constexpr uint8_t SHT31_ADDRESS_1 = 0x44;

// --- Data Logging Configuration ---
const char* LOG_FILE_PATH = "/data_log.csv";
const unsigned long FILE_LOG_INTERVAL = 300000; // 5 minutes
const unsigned long GOOGLE_SHEETS_UPLOAD_INTERVAL = 300000; // 5 minutes

// =================================================================
// 2. GLOBAL STATE VARIABLES
// =================================================================
// --- System State ---
bool humidityPrimed = false;

// --- Timing ---
unsigned long lastHumidityCheckTime = 0;
unsigned long lastFileLogTime = 0;
unsigned long lastGoogleSheetsUploadTime = 0;
const unsigned long HUMIDITY_CHECK_INTERVAL = 10000; // 10 seconds
unsigned long lastWifiCheckTime = 0;
const unsigned long WIFI_CHECK_INTERVAL = 30000;
unsigned long lastLightCheckTime = 0;
const unsigned long LIGHT_CHECK_INTERVAL = 60000; // Check every minute

// --- Sensor Data for Logging ---
float hum1_sum = 0;
float temp1_sum = 0;
int readCount = 0;

// --- Humidifier State ---
bool isHumidifierOn = false;
unsigned long humidifierOnTime = 0;  // Total time humidifier has been ON (in milliseconds)
unsigned long lastHumidifierStateChange = 0;

// --- Random Cycle State (Single Relay) ---
enum RandomCycleState { STATE_OFF, STATE_ON };
RandomCycleState randomCycleState = STATE_OFF;
unsigned long randomCycleLastChangeTime = 0;
unsigned long currentRandomOnDuration = 0;
unsigned long currentRandomOffDuration = 0;

// --- Light Schedule State ---
bool isLightOn = false;

// =================================================================
// 3. HELPER FUNCTION DECLARATIONS
// =================================================================
void initSPIFFS();
void logDataToFile(float avgHum1, float avgTemp1, bool humidifierStatus);
void uploadToGoogleSheets(float avgHum1, float avgTemp1, bool humidifierStatus);
String getTimeString();
String getRealTimeString();
void handleHumidifierControl(unsigned long currentTime);
void handleRandomCycleRelay(unsigned long currentTime);
void handleLightSchedule(unsigned long currentTime);
void handleDataLogging(unsigned long currentTime);
void readAndAccumulateSensorData();
void printSystemStatus();
void handleWiFiConnection(unsigned long currentTime);
void initTimeSync();

// =================================================================
// 4. SETUP FUNCTION
// =================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n############################################");
  Serial.println("###   Comprehensive Control System Start   ###");
  Serial.println("############################################\n");
  Serial.println("--- System Initialization ---");

  // --- Initialize WiFi ---
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("[SETUP] Attempting to connect to SSID: %s\n", ssid);
  int wifi_tries = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_tries < 30) {
      delay(500);
      Serial.print(".");
      wifi_tries++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[SETUP] ✓ WiFi Connected. IP Address: %s\n", WiFi.localIP().toString().c_str());
      initTimeSync();
  } else {
      Serial.println("[SETUP] ✗ Failed to connect to WiFi. Will retry periodically in the background.");
  }

  // --- Initialize Relays ---
  pinMode(HUMIDIFIER_RELAY_PIN, OUTPUT);
  digitalWrite(HUMIDIFIER_RELAY_PIN, HUMIDIFIER_OFF_STATE);
  Serial.printf("[SETUP] ✓ Humidifier relay (GPIO %d) initialized to OFF.\n", HUMIDIFIER_RELAY_PIN);

  pinMode(RELAY_RANDOM_1, OUTPUT);
  digitalWrite(RELAY_RANDOM_1, RANDOM_RELAY_OFF_STATE);
  Serial.printf("[SETUP] ✓ Random cycle relay (GPIO %d) initialized to OFF.\n", RELAY_RANDOM_1);

  pinMode(LIGHT_RELAY_PIN, OUTPUT);
  digitalWrite(LIGHT_RELAY_PIN, LIGHT_OFF_STATE);
  Serial.printf("[SETUP] ✓ Light schedule relay (GPIO %d) initialized to OFF.\n", LIGHT_RELAY_PIN);

  // --- Initialize SHT31 Sensor ---
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!sht31_1.begin(SHT31_ADDRESS_1)) {
    Serial.printf("[SETUP] ✗ ERROR: SHT31 (0x%X) not found!\n", SHT31_ADDRESS_1);
  } else {
    Serial.printf("[SETUP] ✓ SHT31 sensor (0x%X) initialized.\n", SHT31_ADDRESS_1);
  }

  // --- Initialize Random Number Generator ---
  randomSeed(analogRead(0));
  Serial.println("[SETUP] ✓ Random cycle seeded.");

  // --- Initialize SPIFFS ---
  initSPIFFS();

  Serial.println("\n--- Initialization Complete ---");
  Serial.println("[MODE] System is in HUMIDITY_PRIME mode. Waiting for humidity to reach high threshold...");
  Serial.printf("[MODE] Target Humidity: > %.1f%%\n\n", HUMIDITY_HIGH_THRESHOLD);
  
  lastHumidifierStateChange = millis();
}

// =================================================================
// 5. MAIN LOOP (Orchestrator)
// =================================================================
void loop() {
  unsigned long currentTime = millis();

  handleWiFiConnection(currentTime);
  handleHumidifierControl(currentTime);
  handleLightSchedule(currentTime);
  handleDataLogging(currentTime);

  if (humidityPrimed) {
    handleRandomCycleRelay(currentTime);
  }

  // Non-blocking periodic status report every minute
  static unsigned long lastStatusPrintTime = 0;
  if (currentTime - lastStatusPrintTime > 60000) {
    lastStatusPrintTime = currentTime;
    printSystemStatus();
  }
}

// =================================================================
// 6. HELPER FUNCTION IMPLEMENTATIONS
// =================================================================

/**
 * @brief Initialize time synchronization with NTP server
 */
void initTimeSync() {
  Serial.println("[TIME] Synchronizing with NTP server...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[TIME] ✗ Failed to obtain time");
    return;
  }
  
  Serial.println("[TIME] ✓ Time synchronized successfully");
  Serial.printf("[TIME] Current Tehran time: %s\n", getRealTimeString().c_str());
}

/**
 * @brief Manages WiFi connection, attempting to reconnect if dropped.
 */
void handleWiFiConnection(unsigned long currentTime) {
  if (WiFi.status() != WL_CONNECTED && (currentTime - lastWifiCheckTime >= WIFI_CHECK_INTERVAL)) {
    lastWifiCheckTime = currentTime;
    Serial.println("[WIFI] Connection lost. Attempting to reconnect...");
    WiFi.reconnect();
    
    // Re-sync time after reconnection
    if (WiFi.status() == WL_CONNECTED) {
      initTimeSync();
    }
  }
}

/**
 * @brief Handle light schedule (Pin 33) - ON at 7 AM, OFF at 7 PM
 */
void handleLightSchedule(unsigned long currentTime) {
  if (currentTime - lastLightCheckTime < LIGHT_CHECK_INTERVAL) {
    return;
  }
  
  lastLightCheckTime = currentTime;
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[LIGHT] ✗ Failed to get time for light schedule");
    return;
  }
  
  int currentHour = timeinfo.tm_hour;
  bool shouldBeOn = (currentHour >= LIGHT_ON_HOUR && currentHour < LIGHT_OFF_HOUR);
  
  if (shouldBeOn && !isLightOn) {
    digitalWrite(LIGHT_RELAY_PIN, LIGHT_ON_STATE);
    isLightOn = true;
    Serial.printf("[LIGHT] ✓ Light turned ON at %02d:%02d (scheduled for 07:00-19:00)\n", 
                  timeinfo.tm_hour, timeinfo.tm_min);
  } else if (!shouldBeOn && isLightOn) {
    digitalWrite(LIGHT_RELAY_PIN, LIGHT_OFF_STATE);
    isLightOn = false;
    Serial.printf("[LIGHT] ✓ Light turned OFF at %02d:%02d (scheduled for 07:00-19:00)\n", 
                  timeinfo.tm_hour, timeinfo.tm_min);
  }
}

/**
 * @brief Prints a periodic, comprehensive status of the system to the serial monitor.
 */
void printSystemStatus() {
    Serial.println("\n----------- 1-MINUTE SYSTEM STATUS -----------");
    Serial.printf("Uptime: %s\n", getTimeString().c_str());
    Serial.printf("Real Time (Tehran): %s\n", getRealTimeString().c_str());

    // WiFi Status
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi Status: CONNECTED | IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("WiFi Status: DISCONNECTED\n");
    }

    Serial.printf("System Mode: %s\n", humidityPrimed ? "NORMAL_OPERATION" : "HUMIDITY_PRIME");
    
    // Humidifier Status with ON time
    Serial.printf("Humidifier (GPIO %d): %s", HUMIDIFIER_RELAY_PIN, isHumidifierOn ? "ON" : "OFF");
    if (isHumidifierOn) {
        unsigned long currentOnTime = (millis() - lastHumidifierStateChange) / 1000;
        Serial.printf(" (Current cycle: %lu sec)", currentOnTime);
    }
    Serial.printf(" | Total ON time: %lu sec (%.1f min)\n", 
                  humidifierOnTime / 1000, humidifierOnTime / 60000.0);

    // Light Schedule Status
    Serial.printf("Light Schedule (GPIO %d): %s (07:00-19:00)\n", 
                  LIGHT_RELAY_PIN, isLightOn ? "ON" : "OFF");

    if (humidityPrimed) {
        Serial.printf("Random Cycle State (GPIO %d): ", RELAY_RANDOM_1);
        switch(randomCycleState) {
            case STATE_OFF:
                Serial.printf("OFF (Next ON in: %lu s)\n", 
                            (currentRandomOffDuration - (millis() - randomCycleLastChangeTime)) / 1000);
                break;
            case STATE_ON:
                Serial.printf("ON (Time remaining: %lu s)\n", 
                            (currentRandomOnDuration - (millis() - randomCycleLastChangeTime)) / 1000);
                break;
        }
    } else {
        Serial.println("Random Cycle: PENDING (Waiting for prime)");
    }

    float lastHumidity = sht31_1.readHumidity();
    float lastTemp = sht31_1.readTemperature();
    if (!isnan(lastHumidity) && !isnan(lastTemp)) {
        Serial.printf("Sensor (Control): Temp=%.2f C, Hum=%.2f %%\n", lastTemp, lastHumidity);
    } else {
        Serial.println("Sensor (Control): Failed to read data.");
    }
    Serial.println("----------------------------------------------\n");
}

/**
 * @brief Manages the humidifier relay and checks for the initial prime condition.
 */
void handleHumidifierControl(unsigned long currentTime) {
  if (currentTime - lastHumidityCheckTime >= HUMIDITY_CHECK_INTERVAL) {
    lastHumidityCheckTime = currentTime;

    Serial.print("[HUM-CTRL] Checking humidity... ");
    float currentHumidity = sht31_1.readHumidity();

    if (isnan(currentHumidity)) {
      Serial.println("\n[HUM-CTRL] ✗ ERROR: Failed to read from SHT31. Skipping control cycle.");
      return;
    }
    Serial.printf("Current: %.2f%%. ", currentHumidity);

    // Update humidifier ON time tracking
    if (isHumidifierOn) {
      humidifierOnTime += (currentTime - lastHumidifierStateChange);
    }

    // Apply Control Logic
    if (currentHumidity < HUMIDITY_LOW_THRESHOLD && !isHumidifierOn) {
      isHumidifierOn = true;
      digitalWrite(HUMIDIFIER_RELAY_PIN, HUMIDIFIER_ON_STATE);
      lastHumidifierStateChange = currentTime;
      Serial.printf("ACTION: Below threshold (%.1f%%). Turning Humidifier ON.\n", HUMIDITY_LOW_THRESHOLD);
    } else if (currentHumidity > HUMIDITY_HIGH_THRESHOLD && isHumidifierOn) {
      humidifierOnTime += (currentTime - lastHumidifierStateChange);
      isHumidifierOn = false;
      digitalWrite(HUMIDIFIER_RELAY_PIN, HUMIDIFIER_OFF_STATE);
      lastHumidifierStateChange = currentTime;
      Serial.printf("ACTION: Above threshold (%.1f%%). Turning Humidifier OFF.\n", HUMIDITY_HIGH_THRESHOLD);
      Serial.printf("[HUM-CTRL] Total ON time: %lu seconds (%.1f minutes)\n", 
                    humidifierOnTime / 1000, humidifierOnTime / 60000.0);
    } else {
      Serial.println("No action needed.");
    }

    if (!humidityPrimed && currentHumidity > HUMIDITY_HIGH_THRESHOLD) {
      humidityPrimed = true;
      Serial.println("\n\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      Serial.printf("!!!  HUMIDITY PRIMED: Target %.1f%% reached.  !!!\n", HUMIDITY_HIGH_THRESHOLD);
      Serial.println("!!!  Starting Random Fan Cycle now. System in NORMAL mode.  !!!");
      Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

      randomCycleLastChangeTime = millis();
      currentRandomOffDuration = random(120, 150) * 1000UL;
      Serial.printf("[RAND-CYCLE] Initializing first cycle. Relay will be OFF for %lu ms.\n", currentRandomOffDuration);
    }
  }
}

/**
 * @brief Manages random cycle relay (GPIO 32 only) with specified timings.
 */
void handleRandomCycleRelay(unsigned long currentTime) {
  switch (randomCycleState) {
    case STATE_OFF:
      if (currentTime - randomCycleLastChangeTime >= currentRandomOffDuration) {
        Serial.printf("\n[RAND-CYCLE] OFF time finished (%lu ms). Starting ON cycle.\n", currentRandomOffDuration);
        digitalWrite(RELAY_RANDOM_1, RANDOM_RELAY_ON_STATE);
        randomCycleLastChangeTime = currentTime;
        randomCycleState = STATE_ON;
        currentRandomOnDuration = random(20, 27) * 1000UL;
        Serial.printf("[RAND-CYCLE] STATE -> ON. Relay %d ON for %lu ms\n", RELAY_RANDOM_1, currentRandomOnDuration);
      }
      break;

    case STATE_ON:
      if (currentTime - randomCycleLastChangeTime >= currentRandomOnDuration) {
        Serial.printf("\n[RAND-CYCLE] ON time finished (%lu ms). Starting OFF cycle.\n", currentRandomOnDuration);
        digitalWrite(RELAY_RANDOM_1, RANDOM_RELAY_OFF_STATE);
        randomCycleLastChangeTime = currentTime;
        randomCycleState = STATE_OFF;
        currentRandomOffDuration = random(240, 360) * 1000UL;
        Serial.printf("[RAND-CYCLE] STATE -> OFF. Relay %d OFF. Next ON in %lu ms.\n", RELAY_RANDOM_1, currentRandomOffDuration);
      }
      break;
  }
}

/**
 * @brief Manages reading sensors and logging data to file and Google Sheets every 5 minutes.
 */
void handleDataLogging(unsigned long currentTime) {
  readAndAccumulateSensorData();

  if (currentTime - lastFileLogTime >= FILE_LOG_INTERVAL) {
    lastFileLogTime = currentTime;

    if (readCount > 0) {
      Serial.printf("\n[LOGGING] 5-minute interval reached. Processing %d sensor readings for logging...\n", readCount);
      float avg_hum1 = hum1_sum / readCount;
      float avg_temp1 = temp1_sum / readCount;

      logDataToFile(avg_hum1, avg_temp1, isHumidifierOn);
      
      // Upload to Google Sheets if WiFi is connected
      if (WiFi.status() == WL_CONNECTED) {
        uploadToGoogleSheets(avg_hum1, avg_temp1, isHumidifierOn);
      } else {
        Serial.println("[GOOGLE-SHEETS] ✗ WiFi not connected. Skipping upload.");
      }

      hum1_sum = 0;
      temp1_sum = 0;
      readCount = 0;
      Serial.println("[LOGGING] Data accumulators have been reset for the next interval.");
    } else {
      Serial.println("\n[LOGGING] 5-minute interval reached, but no sensor data was accumulated. Skipping log entry.");
    }
  }
}

/**
 * @brief Reads SHT31 sensor and accumulates data for logging.
 */
void readAndAccumulateSensorData() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < 2000) return;
  lastRead = millis();

  float h1 = sht31_1.readHumidity();
  float t1 = sht31_1.readTemperature();
  
  if (!isnan(h1)) { hum1_sum += h1; }
  if (!isnan(t1)) { temp1_sum += t1; }
  
  readCount++;
}

/**
 * @brief Upload data to Google Sheets
 */
void uploadToGoogleSheets(float avgHum1, float avgTemp1, bool humidifierStatus) {
  if (strlen(GOOGLE_SCRIPT_URL) < 10) {
    Serial.println("[GOOGLE-SHEETS] ✗ Google Script URL not configured!");
    return;
  }

  HTTPClient http;
  http.begin(GOOGLE_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");

  String timestamp = getRealTimeString();
  String statusStr = humidifierStatus ? "ON" : "OFF";
  
  String jsonPayload = "{";
  jsonPayload += "\"timestamp\":\"" + timestamp + "\",";
  jsonPayload += "\"humidity\":" + String(avgHum1, 2) + ",";
  jsonPayload += "\"temperature\":" + String(avgTemp1, 2) + ",";
  jsonPayload += "\"humidifier_status\":\"" + statusStr + "\"";
  jsonPayload += "}";

  Serial.println("[GOOGLE-SHEETS] Uploading data...");
  Serial.printf("[GOOGLE-SHEETS] Payload: %s\n", jsonPayload.c_str());

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("[GOOGLE-SHEETS] ✓ Upload successful! Response code: %d\n", httpResponseCode);
    Serial.printf("[GOOGLE-SHEETS] Response: %s\n", response.c_str());
  } else {
    Serial.printf("[GOOGLE-SHEETS] ✗ Upload failed! Error: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

/**
 * @brief Logs averaged data to SPIFFS file.
 */
void logDataToFile(float avgHum1, float avgTemp1, bool humidifierStatus) {
  File file = SPIFFS.open(LOG_FILE_PATH, FILE_APPEND);
  if (!file) {
    Serial.println("[LOGGING] ✗ ERROR: Failed to open log file for appending!");
    return;
  }

  String timestamp = getRealTimeString();
  String statusStr = humidifierStatus ? "ON" : "OFF";

  file.printf("%s,%.2f,%.2f,%s\n",
              timestamp.c_str(), avgHum1, avgTemp1, statusStr.c_str());
  file.close();

  Serial.printf("[LOGGING] ✓ Data successfully logged to %s. Averages: H=%.1f, T=%.1f\n",
                LOG_FILE_PATH, avgHum1, avgTemp1);
}

/**
 * @brief Gets formatted real time string from NTP
 */
String getRealTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time-Unavailable";
  }
  
  char timeStr[64];
  sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d",
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday,
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec);
  
  return String(timeStr);
}

/**
 * @brief Gets formatted uptime string (using millis)
 */
String getTimeString() {
  unsigned long seconds = millis() / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;

  char timeStr[32];
  sprintf(timeStr, "%luD %02lu:%02lu:%02lu", days, hours % 24, minutes % 60, seconds % 60);
  return String(timeStr);
}

/**
 * @brief Initializes SPIFFS file system
 */
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("[SETUP] ✗ ERROR: SPIFFS Mount Failed!");
    return;
  }
  Serial.println("[SETUP] ✓ SPIFFS initialized successfully.");

  if (SPIFFS.exists(LOG_FILE_PATH)) {
      Serial.printf("[SETUP] ✓ Log file '%s' already exists.\n", LOG_FILE_PATH);
  } else {
    Serial.printf("[SETUP] Log file '%s' not found. Creating new file...\n", LOG_FILE_PATH);
    File file = SPIFFS.open(LOG_FILE_PATH, FILE_WRITE);
    if (file) {
      file.println("Timestamp,Humidity(%),Temp(C),Humidifier_Status");
      file.close();
      Serial.println("[SETUP] ✓ Log file created successfully.");
    } else {
      Serial.println("[SETUP] ✗ ERROR: Failed to create log file!");
    }
  }
}
