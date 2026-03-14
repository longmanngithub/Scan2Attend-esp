#ifndef SDSTORAGE_H
#define SDSTORAGE_H

#include "Config.h"
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

// ============================================
// SD Card Offline Queue Storage
// ============================================

const char* QUEUE_FILE = "/attendance_queue.jsonl";
const char* QUEUE_TEMP_FILE = "/attendance_queue_tmp.jsonl";

bool sdCardReady = false;

// -------- Check if SD Card Module is Connected --------
// Attempts SPI communication with the SD card slot.
// Returns true if the module responds on the SPI bus.

bool isSDModulePresent() {
  // SD.begin returns false if nothing responds on the CS pin
  // We try to begin; if it fails, the module/card is not there.
  return SD.begin(SD_CS);
}

// -------- Check if SD Card is Inserted & Accessible --------
// After SD.begin succeeds, verify we can actually read/write.

bool isSDCardAccessible() {
  if (!SD.begin(SD_CS)) return false;

  // Try to open root directory as a quick health check
  File root = SD.open("/");
  if (!root) {
    return false;
  }
  root.close();
  return true;
}

// -------- Periodic SD Card Health Check --------
// Call this from loop() to detect SD card removal during operation.

void checkSDHealth() {
  if (millis() - lastSDCheck < SD_CHECK_INTERVAL) return;
  lastSDCheck = millis();

  bool wasReady = sdCardReady;

  // Quick health check: try to open the queue file
  File test = SD.open(QUEUE_FILE, FILE_READ);
  if (test) {
    test.close();
    if (!wasReady) {
      // SD card was re-inserted or recovered
      sdCardReady = true;
      Serial.println("✅ SD Card recovered.");
    }
  } else {
    // Cannot access the card — try to re-initialize
    if (SD.begin(SD_CS)) {
      // Module responded but file missing — recreate it
      File f = SD.open(QUEUE_FILE, FILE_WRITE);
      if (f) {
        f.close();
        sdCardReady = true;
        if (!wasReady) Serial.println("✅ SD Card re-initialized.");
      } else {
        sdCardReady = false;
      }
    } else {
      // Module/card not responding
      if (wasReady) {
        Serial.println("⚠️ SD Card lost! Offline queueing disabled.");
      }
      sdCardReady = false;
    }
  }
}

// -------- Initialize SD Card --------

bool initSD() {
  Serial.println("Initializing SD card...");

  // Step 1: Check if the SD card module responds on SPI
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD Card initialization failed!");
    Serial.println("   Check: Is the SD module wired correctly? Is a card inserted?");
    sdCardReady = false;
    return false;
  }

  // Step 2: Verify the card is accessible (read test)
  File root = SD.open("/");
  if (!root) {
    Serial.println("❌ SD Card module detected but cannot read card.");
    Serial.println("   Check: Is the SD card inserted properly? Is it formatted as FAT32?");
    sdCardReady = false;
    return false;
  }
  root.close();

  Serial.println("✅ SD Card initialized.");

  // Step 3: Print card info
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.print("   Card size: ");
  Serial.print((uint32_t)cardSize);
  Serial.println(" MB");

  // Step 4: Check if queue file exists, create if not
  if (!SD.exists(QUEUE_FILE)) {
    File file = SD.open(QUEUE_FILE, FILE_WRITE);
    if (file) {
      file.close();
      Serial.println("   Created new queue file.");
    }
  }

  sdCardReady = true;
  return true;
}

// -------- Check for Duplicate in Queue --------
// Prevents writing the same fingerprint+timestamp combo to SD card twice.

bool isDuplicateInQueue(int fingerprintId, String scannedAt) {
  if (!sdCardReady) return false;

  File file = SD.open(QUEUE_FILE, FILE_READ);
  if (!file) return false;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() < 2) continue;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) continue;

    int fpId = doc["fingerprint_id"] | -1;
    String ts = doc["scanned_at"] | "";

    if (fpId == fingerprintId && ts == scannedAt) {
      file.close();
      return true;  // Duplicate found
    }
  }
  file.close();
  return false;
}

// -------- Queue Attendance Record --------
// Stores RTC timestamp. Checks for duplicates before writing.

bool queueAttendance(int fingerprintId, String scannedAt) {
  if (!sdCardReady) {
    Serial.println("❌ SD Card not ready, cannot queue.");
    return false;
  }

  // Check for duplicate before writing
  if (isDuplicateInQueue(fingerprintId, scannedAt)) {
    Serial.println("ℹ️ Duplicate record already in queue, skipping.");
    return true;  // Not an error, just skip
  }

  File file = SD.open(QUEUE_FILE, FILE_APPEND);
  if (!file) {
    Serial.println("❌ Failed to open queue file for writing.");
    // SD card may have been removed
    sdCardReady = false;
    return false;
  }

  // Create JSON record matching Laravel API format
  JsonDocument doc;
  doc["fingerprint_id"] = fingerprintId;
  doc["scanned_at"] = scannedAt;

  // Write JSON line
  serializeJson(doc, file);
  file.println();  // Newline for JSONL format
  file.close();

  Serial.print("📦 Queued attendance: FP ID ");
  Serial.print(fingerprintId);
  Serial.print(" at ");
  Serial.println(scannedAt);

  return true;
}

// -------- Get Queue Count --------

int getQueueCount() {
  if (!sdCardReady) return 0;

  File file = SD.open(QUEUE_FILE, FILE_READ);
  if (!file) return 0;

  int count = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 2) {  // Valid JSON line
      count++;
    }
  }
  file.close();
  return count;
}

// -------- Sync Queued Records to Laravel Backend --------
// Only removes records the server successfully processed.
// Records that fail validation (no session, not enrolled, etc.)
// are also removed since they will never succeed on retry.
// Records that fail due to network errors are kept for retry.

int syncQueuedRecords() {
  if (!sdCardReady) return 0;
  if (!backendConnected) return 0;

  int count = getQueueCount();
  if (count == 0) return 0;

  Serial.print("🔄 Syncing ");
  Serial.print(count);
  Serial.println(" queued records...");

  // Read all records into memory
  File file = SD.open(QUEUE_FILE, FILE_READ);
  if (!file) return 0;

  // Store individual lines so we can match them to server response
  String lines[100];  // Max 100 queued records at a time
  int lineCount = 0;

  // Build JSON array for batch sync
  String payload = "{\"records\":[";
  bool first = true;

  while (file.available() && lineCount < 100) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 2) {
      lines[lineCount] = line;
      lineCount++;
      if (!first) payload += ",";
      payload += line;
      first = false;
    }
  }
  file.close();
  payload += "]}";

  if (lineCount == 0) return 0;

  // POST to Laravel sync endpoint
  HTTPClient http;
  http.begin(String(syncURL));
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);  // 15 second timeout for batch

  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    String response = http.getString();

    // Parse response to check per-record results
    JsonDocument resDoc;
    DeserializationError error = deserializeJson(resDoc, response);

    if (!error) {
      int synced = resDoc["synced"] | 0;
      int failed = resDoc["failed"] | 0;

      Serial.print("✅ Synced ");
      Serial.print(synced);
      Serial.print(" records, ");
      Serial.print(failed);
      Serial.println(" failed validation.");

      // Server returned details for each record.
      // All records that reached the server are handled:
      // - success: recorded in DB
      // - failed (duplicate/no session/not enrolled): won't succeed on retry
      // So we can safely clear the entire queue that was sent.
      SD.remove(QUEUE_FILE);
      File newFile = SD.open(QUEUE_FILE, FILE_WRITE);
      if (newFile) newFile.close();

      http.end();
      return synced;
    }
  } else {
    // Network/server error — keep all records for retry
    Serial.print("❌ Sync failed (network). HTTP code: ");
    Serial.println(httpCode);
    if (httpCode > 0) {
      Serial.println(http.getString());
    }
  }

  http.end();
  return 0;
}

// -------- LED Patterns --------

void showOfflineLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;

  // Slow blink (1 second interval) for offline mode
  if (millis() - lastBlink > 1000) {
    lastBlink = millis();
    ledState = !ledState;
    digitalWrite(STATUS_LED, ledState ? HIGH : LOW);
  }
}

void showSyncingLED() {
  // Double blink pattern
  for (int i = 0; i < 2; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(100);
    digitalWrite(STATUS_LED, LOW);
    delay(100);
  }
  delay(400);
}

#endif
