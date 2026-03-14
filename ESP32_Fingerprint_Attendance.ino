/*
 * ScanToAttend - IoT Classroom Attendance System
 * ESP32 + AS608 Fingerprint + DS3231 RTC + LCD 16x2 I2C + SD Card + Buzzer
 *
 * Features:
 * - 24/7 continuous fingerprint scanning (no button needed)
 * - Posts attendance to Laravel backend via REST API
 * - Offline queue to SD card when WiFi/backend is down
 * - Auto-sync queued records when connection restores
 * - RTC for accurate timestamps
 * - LCD display shows student info and time
 * - Buzzer feedback for scan results
 * - WiFi Manager for easy network setup
 * - Web server routes for remote enrollment/management
 *
 * Wiring:
 * - AS608 TX -> GPIO 16, RX -> GPIO 17, VCC -> 3.3V
 * - DS3231 SDA -> GPIO 21, SCL -> GPIO 22, VCC -> 3.3V
 * - LCD I2C SDA -> GPIO 21, SCL -> GPIO 22, VCC -> 5V
 * - SD Card CS -> GPIO 5, MOSI -> 23, MISO -> 19, SCK -> 18, VCC -> 3.3V
 * - Buzzer Signal -> GPIO 25, GND -> GND
 *
 * Required Libraries:
 * - WiFiManager, Adafruit Fingerprint, ESPAsyncWebServer, AsyncTCP
 * - RTClib, LiquidCrystal_I2C, ArduinoJson, SD
 */

#include "Config.h"
#include "Helpers.h"
#include "Buzzer.h"
#include "Display.h"
#include "RTC.h"
#include "Fingerprint.h"
#include "SDStorage.h"
#include "WiFiControl.h"
#include "Routes.h"

#include <ArduinoJson.h>

// Timer for LCD idle display refresh
unsigned long lastDisplayRefresh = 0;
const unsigned long DISPLAY_REFRESH_INTERVAL = 1000;  // Update clock every second

// Timer to return LCD to idle after showing scan result
unsigned long displayReturnTimer = 0;
bool displayShowingResult = false;
const unsigned long DISPLAY_RESULT_DURATION = 3000;  // Show result for 3 seconds

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n");
  Serial.println("╔═════════════════════════════════════════╗");
  Serial.println("║   ScanToAttend - Attendance System      ║");
  Serial.println("║   ESP32 + Fingerprint + RTC + LCD       ║");
  Serial.println("╚═════════════════════════════════════════╝");

  // Initialize LCD Display
  initDisplay();

  // Initialize Buzzer
  initBuzzer();

  // Initialize RTC
  if (!initRTC()) {
    showError("RTC Failed");
    Serial.println("⚠️ RTC not available. Timestamps will be empty.");
  }

  // Initialize Fingerprint Sensor
  mySerial.begin(57600, SERIAL_8N1, RXD2, TXD2);
  delay(100);
  finger.begin(57600);

  for (int i = 0; i < 5; i++) {
    if (finger.verifyPassword()) {
      sensorConnected = true;
      break;
    }
    Serial.println("Sensor not found, retrying...");
    delay(500);
  }

  if (sensorConnected) {
    finger.getTemplateCount();
    Serial.println("✅ Fingerprint sensor found!");
    Serial.print("   Capacity: ");
    Serial.print(finger.capacity);
    Serial.print(" | Stored: ");
    Serial.println(finger.templateCount);
  } else {
    Serial.println("❌ ERROR: Fingerprint sensor NOT found!");
    showError("No Sensor!");
    buzzerError();
    while (1) {
      blinkLED(5, 100);
      delay(2000);
    }
  }

  // Setup WiFi
  setupWiFi();

  // Initialize SD Card
  if (initSD()) {
    int queued = getQueueCount();
    if (queued > 0) {
      Serial.print("📦 Found ");
      Serial.print(queued);
      Serial.println(" queued records from last offline session.");
    }
  } else {
    Serial.println("⚠️ SD Card not available. Offline queueing disabled.");
  }

  // Setup Web Routes & Start Server
  setupRoutes();

  // Startup complete
  buzzerStartup();
  Serial.println("\n✅ System ready! Scanning 24/7...\n");

  // Print initial system status so serial monitor shows backend state immediately
  checkBackendStatus();
  printBackendStatus();
}

void loop() {
  // ========================================
  // 1. Background tasks
  // ========================================

  // Check backend connectivity
  checkBackendStatus();

  // Periodic SD card health check
  checkSDHealth();

  // Try to sync queued records when online
  if (backendConnected && sdCardReady && millis() - lastSyncAttempt > SYNC_INTERVAL) {
    lastSyncAttempt = millis();
    int queued = getQueueCount();
    if (queued > 0) {
      Serial.print("🔄 Auto-syncing ");
      Serial.print(queued);
      Serial.println(" queued records...");
      showSyncingDisplay(queued);
      showSyncingLED();
      int synced = syncQueuedRecords();
      if (synced > 0) {
        Serial.print("✅ Auto-synced ");
        Serial.print(synced);
        Serial.println(" records.");
      }
    }
  }

  // Return LCD to idle view after showing a result
  if (displayShowingResult && millis() - displayReturnTimer > DISPLAY_RESULT_DURATION) {
    displayShowingResult = false;
  }

  // Refresh idle display (clock)
  if (!displayShowingResult && millis() - lastDisplayRefresh > DISPLAY_REFRESH_INTERVAL) {
    lastDisplayRefresh = millis();
    showReady(getRTCDateForLCD(), getRTCTimeForLCD());
  }

  // ========================================
  // 2. Handle enrollment request (from web)
  // ========================================
  if (enrollRequested) {
    enrollRequested = false;

    if (!sensorConnected) {
      enrollError = "Sensor disconnected";
      enrollState = ENROLL_FAILED;
      showError("No Sensor!");
      buzzerError();
    } else if (!backendConnected) {
      enrollError = "Backend offline";
      enrollState = ENROLL_FAILED;
      showError("Backend Down");
      buzzerError();
    } else {
      Serial.println("📝 Starting enrollment for ID: " + String(enrollID));
      showEnrollStep(enrollID, "Place finger...");

      bool ok = enrollFingerprint(enrollID);

      if (ok) {
        showEnrollStep(enrollID, "Success!");
        buzzerSuccess();
        Serial.println("✅ Enrollment completed for ID: " + String(enrollID));
      } else {
        showEnrollStep(enrollID, "Failed!");
        buzzerError();
        Serial.println("❌ Enrollment failed: " + enrollError);
      }

      displayShowingResult = true;
      displayReturnTimer = millis();
    }
  }

  // ========================================
  // 3. Handle manual verify request (from web)
  // ========================================
  if (verifyRequested) {
    verifyRequested = false;

    if (!sensorConnected) {
      lastVerifiedID = -1;
    } else {
      unsigned long timeout = millis() + 15000;
      while (millis() < timeout) {
        int id = verifyFingerprint();
        if (id > 0) {
          lastVerifiedID = id;
          buzzerSuccess();
          break;
        } else if (id == -2) {
          lastVerifiedID = -2;
          buzzerError();
          break;
        }
        delay(100);
      }
    }
  }

  // ========================================
  // 4. Continuous 24/7 Fingerprint Scanning
  // ========================================
  if (sensorConnected && !enrollRequested && !verifyRequested) {
    uint8_t p = finger.getImage();

    if (p == FINGERPRINT_OK) {
      // Finger detected! Process it.
      p = finger.image2Tz();
      if (p != FINGERPRINT_OK) {
        // Bad image, ignore
        delay(100);
        return;
      }

      p = finger.fingerSearch();

      if (p == FINGERPRINT_OK) {
        int fpId = finger.fingerID;

        // Check cooldown (prevent duplicate scans)
        if (fpId == lastScannedID && millis() - lastScanTime < SCAN_COOLDOWN) {
          // Same finger scanned too quickly, ignore
          delay(200);
          return;
        }

        lastScannedID = fpId;
        lastScanTime = millis();

        String scannedAt = getRTCDateTime();
        String timeStr = getRTCTimeForLCD();

        Serial.print("👆 Fingerprint matched! ID: ");
        Serial.print(fpId);
        Serial.print(" at ");
        Serial.println(scannedAt);

        // Look up cached username for this fingerprint ID
        String displayName = usernameCache.count(fpId) ? usernameCache[fpId] : "FP Detected";

        // Try to POST to Laravel backend
        if (backendConnected) {
          // Build JSON payload
          JsonDocument doc;
          doc["fingerprint_id"] = fpId;
          doc["scanned_at"] = scannedAt;

          String payload;
          serializeJson(doc, payload);

          int httpCode = 0;
          String response = httpPostJson(checkInURL, payload, httpCode);

          Serial.print("Backend response (");
          Serial.print(httpCode);
          Serial.print("): ");
          Serial.println(response);

          if (httpCode == 200) {
            // Parse response to get username and session info
            JsonDocument resDoc;
            DeserializationError err = deserializeJson(resDoc, response);

            if (!err) {
              String username = resDoc["username"] | displayName;
              
              // Cache the username for offline use
              usernameCache[fpId] = username;
              displayName = username;

              showStudent(displayName, timeStr);
              buzzerSuccess();
              Serial.println("✅ " + username + " checked in.");
            } else {
              showStudent(displayName, timeStr);
              buzzerSuccess();
            }
          } else if (httpCode == 400) {
            // Parse error message
            JsonDocument resDoc;
            DeserializationError err = deserializeJson(resDoc, response);
            String msg = "Error";

            if (!err) {
              msg = resDoc["message"] | "Error";
              // Backend returns username for known students
              String username = resDoc["username"] | displayName;
              usernameCache[fpId] = username;
              displayName = username;
            }

            if (msg.indexOf("Already") >= 0 || msg.indexOf("already") >= 0) {
              showAlreadyIn(displayName);
              buzzerDuplicate();
              Serial.println("ℹ️ " + displayName + ": " + msg);
            } else if (msg.indexOf("No session") >= 0 || msg.indexOf("no session") >= 0) {
              showNoSession(displayName);
              buzzerNoSession();
              Serial.println("ℹ️ " + displayName + ": " + msg);
            } else {
              showError(msg);
              buzzerError();
              Serial.println("❌ " + msg);
            }
          } else {
            // Server error or unreachable - queue offline
            Serial.println("⚠️ Backend error, queueing offline...");
            if (sdCardReady) {
              queueAttendance(fpId, scannedAt);
              showOfflineSaved(displayName);
              buzzerOfflineSave();
            } else {
              showError("No SD & No Net");
              buzzerError();
            }
          }
        } else {
          // Backend offline - queue to SD card
          Serial.println("📦 Backend offline, queueing to SD card...");
          if (sdCardReady) {
            queueAttendance(fpId, scannedAt);
            showOfflineSaved(displayName);
            buzzerOfflineSave();
          } else {
            showError("No SD & No Net");
            buzzerError();
            Serial.println("❌ Cannot record! No backend and no SD card.");
          }
        }

        displayShowingResult = true;
        displayReturnTimer = millis();

        // Wait for finger removal before scanning again
        while (finger.getImage() != FINGERPRINT_NOFINGER) {
          delay(50);
        }

      } else if (p == FINGERPRINT_NOTFOUND) {
        // Unknown fingerprint
        Serial.println("❌ Unknown fingerprint");
        showUnknown();
        buzzerError();

        displayShowingResult = true;
        displayReturnTimer = millis();

        // Wait for finger removal
        while (finger.getImage() != FINGERPRINT_NOFINGER) {
          delay(50);
        }
      }
      // Other errors: just ignore and retry
    }
  }

  // ========================================
  // 5. WiFi reconnection
  // ========================================
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > 10000) {
      lastReconnectAttempt = millis();
      Serial.println("📡 WiFi disconnected! Attempting to reconnect...");
      showError("WiFi Lost");
      digitalWrite(STATUS_LED, LOW);

      WiFi.reconnect();
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi reconnected!");
        showWiFiConnected(WiFi.localIP().toString());
        digitalWrite(STATUS_LED, HIGH);
      } else {
        Serial.println("\n⚠️ WiFi reconnection failed. Will retry...");
      }
    }
  }

  delay(10);  // Small delay to prevent watchdog issues
}
