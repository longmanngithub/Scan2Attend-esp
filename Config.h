#ifndef CONFIG_H
#define CONFIG_H

#include <WiFi.h>
#include <WiFiManager.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <Adafruit_Fingerprint.h>
#include <Preferences.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <map>

// ============================================
// Pin Definitions
// ============================================

// AS608 Fingerprint Sensor (UART2)
#define RXD2 16
#define TXD2 17

// Status LED (built-in)
#define STATUS_LED 2

// Passive Buzzer
#define BUZZER_PIN 25

// SD Card (SPI) - uses default SPI pins
#define SD_CS 5
// MOSI = 23, MISO = 19, SCK = 18 (default ESP32 SPI)

// I2C shared bus (DS3231 RTC + LCD 16x2)
// SDA = 21, SCL = 22 (default ESP32 I2C)

// ============================================
// LCD Configuration
// ============================================
#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

// ============================================
// Global Objects
// ============================================
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
AsyncWebServer server(80);
Preferences preferences;
WiFiManager wifiManager;
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

// ============================================
// Global State Variables
// ============================================

// Sensor & Connection
bool sensorConnected = false;
bool backendConnected = true;  // Assume true initially
bool rtcConnected = false;

// Enrollment state (triggered via web routes)
volatile bool enrollRequested = false;
volatile bool verifyRequested = false;

// Timers
unsigned long heartbeatTimer = 0;
const unsigned long HEARTBEAT_INTERVAL = 5000;  // Check every 5 seconds

// Offline Mode
bool offlineMode = false;
unsigned long lastSyncAttempt = 0;
const unsigned long SYNC_INTERVAL = 30000;  // Try sync every 30 seconds

// SD Card Health Check
unsigned long lastSDCheck = 0;
const unsigned long SD_CHECK_INTERVAL = 10000;  // Check SD card every 10 seconds

// Continuous Scanning
const unsigned long SCAN_COOLDOWN = 4000;  // 4 seconds between same finger scans
unsigned long lastScanTime = 0;
int lastScannedID = -1;

// ==== SERVER IP CONFIGURATION ====
#define DEFAULT_SERVER_IP "192.168.1.1"

char serverIP[40] = "";
char backendBaseURL[120] = "";    // e.g. "http://192.168.1.1/api"
char heartbeatURL[160] = "";      // e.g. "http://192.168.1.1/api/heartbeat"
char checkInURL[160] = "";        // e.g. "http://192.168.1.1/api/student/check-in"
char syncURL[160] = "";           // e.g. "http://192.168.1.1/api/attendance/sync"

// Enrollment tracking
int enrollDuplicateID = -1;
uint16_t enrollID = 0;
int lastVerifiedID = -1;

// Enrollment State Machine
enum EnrollState {
  ENROLL_IDLE,
  ENROLL_WAITING_FIRST,
  ENROLL_WAITING_REMOVE,
  ENROLL_WAITING_SECOND,
  ENROLL_PROCESSING,
  ENROLL_SUCCESS,
  ENROLL_FAILED
};

volatile EnrollState enrollState = ENROLL_IDLE;
String enrollError = "";

// Flag for saving WiFi config
bool shouldSaveConfig = false;

// Username cache: fingerprint_id -> username
std::map<int, String> usernameCache;

// SD Card ready flag (defined in SDStorage.h)
extern bool sdCardReady;

// ============================================
// Helper: Build Backend URLs
// ============================================
void buildBackendURLs() {
  snprintf(backendBaseURL, sizeof(backendBaseURL), "http://%s/api", serverIP);
  snprintf(heartbeatURL, sizeof(heartbeatURL), "%s/heartbeat", backendBaseURL);
  snprintf(checkInURL, sizeof(checkInURL), "%s/student/check-in", backendBaseURL);
  snprintf(syncURL, sizeof(syncURL), "%s/attendance/sync", backendBaseURL);
}

#endif
