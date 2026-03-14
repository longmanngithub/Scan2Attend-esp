#ifndef DISPLAY_H
#define DISPLAY_H

#include "Config.h"

// ============================================
// LCD 16x2 I2C Display Functions
// ============================================

/**
 * Initialize the LCD display.
 */
void initDisplay() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ScanToAttend");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  Serial.println("✅ LCD initialized.");
}

/**
 * Show the idle/ready screen with date and time.
 * Line 1: "22 Feb 2026"
 * Line 2: "Ready  08:30:00"
 */
void showReady(String date, String time) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(date);
  lcd.setCursor(0, 1);
  lcd.print("Ready  ");
  lcd.print(time);
}

/**
 * Show scanned student info.
 * Line 1: username (max 16 chars)
 * Line 2: "OK     08:30:00"
 */
void showStudent(String username, String time) {
  lcd.clear();
  lcd.setCursor(0, 0);

  // Truncate username to 16 chars for LCD
  if (username.length() > 16) {
    username = username.substring(0, 16);
  }
  lcd.print(username);

  lcd.setCursor(0, 1);
  lcd.print("OK     ");
  lcd.print(time);
}

/**
 * Show "Already In" message for duplicate scan.
 * Line 1: username
 * Line 2: "Already In"
 */
void showAlreadyIn(String username) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (username.length() > 16) {
    username = username.substring(0, 16);
  }
  lcd.print(username);
  lcd.setCursor(0, 1);
  lcd.print("Already In");
}

/**
 * Show "No Session" when there's no active session.
 * Line 1: username
 * Line 2: "No Session Now"
 */
void showNoSession(String username) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (username.length() > 16) {
    username = username.substring(0, 16);
  }
  lcd.print(username);
  lcd.setCursor(0, 1);
  lcd.print("No Session Now");
}

/**
 * Show unknown fingerprint.
 * Line 1: "Unknown Finger"
 * Line 2: "Try Again"
 */
void showUnknown() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Unknown Finger");
  lcd.setCursor(0, 1);
  lcd.print("Try Again");
}

/**
 * Show error message.
 * Line 1: "Error:"
 * Line 2: message (truncated to 16 chars)
 */
void showError(String message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Error:");
  lcd.setCursor(0, 1);
  if (message.length() > 16) {
    message = message.substring(0, 16);
  }
  lcd.print(message);
}

/**
 * Show offline mode indicator.
 * Line 1: "** OFFLINE **"
 * Line 2: "Saved Locally"
 */
void showOfflineDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("** OFFLINE **");
  lcd.setCursor(0, 1);
  lcd.print("Saved Locally");
}

/**
 * Show offline scan saved info.
 * Line 1: username
 * Line 2: "Saved Offline"
 */
void showOfflineSaved(String username) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (username.length() > 16) {
    username = username.substring(0, 16);
  }
  lcd.print(username);
  lcd.setCursor(0, 1);
  lcd.print("Saved Offline");
}

/**
 * Show syncing status.
 * Line 1: "Syncing..."
 * Line 2: "X records"
 */
void showSyncingDisplay(int count) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Syncing...");
  lcd.setCursor(0, 1);
  lcd.print(String(count) + " records");
}

/**
 * Show enrollment step.
 * Line 1: "Enroll ID: X"
 * Line 2: step message
 */
void showEnrollStep(int id, String step) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enroll ID: " + String(id));
  lcd.setCursor(0, 1);
  if (step.length() > 16) {
    step = step.substring(0, 16);
  }
  lcd.print(step);
}

/**
 * Show WiFi connecting status.
 */
void showWiFiConnecting() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Setup");
  lcd.setCursor(0, 1);
  lcd.print("Connect to AP...");
}

/**
 * Show WiFi connected with IP.
 */
void showWiFiConnected(String ip) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  lcd.setCursor(0, 1);
  if (ip.length() > 16) {
    ip = ip.substring(0, 16);
  }
  lcd.print(ip);
}

#endif
