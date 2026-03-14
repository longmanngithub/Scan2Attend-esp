#ifndef RTC_H
#define RTC_H

#include "Config.h"

// ============================================
// DS3231 RTC Module Functions
// ============================================

/**
 * Initialize the DS3231 RTC module.
 * Returns true if connected successfully.
 */
bool initRTC() {
  if (!rtc.begin()) {
    Serial.println("❌ DS3231 RTC not found!");
    rtcConnected = false;
    return false;
  }

  rtcConnected = true;

  // Check if RTC lost power and needs time set
  if (rtc.lostPower()) {
    Serial.println("⚠️ RTC lost power, setting time from compile time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  DateTime now = rtc.now();
  Serial.print("✅ RTC initialized. Current time: ");
  Serial.print(now.year());
  Serial.print("-");
  if (now.month() < 10) Serial.print("0");
  Serial.print(now.month());
  Serial.print("-");
  if (now.day() < 10) Serial.print("0");
  Serial.print(now.day());
  Serial.print("T");
  if (now.hour() < 10) Serial.print("0");
  Serial.print(now.hour());
  Serial.print(":");
  if (now.minute() < 10) Serial.print("0");
  Serial.print(now.minute());
  Serial.print(":");
  if (now.second() < 10) Serial.print("0");
  Serial.println(now.second());

  return true;
}

/**
 * Get current date/time as ISO 8601 string.
 * Format: "2026-02-22T08:30:00"
 * Used for the scanned_at field in the Laravel API.
 */
String getRTCDateTime() {
  if (!rtcConnected) {
    // Fallback: return empty (backend can use server time)
    return "";
  }

  DateTime now = rtc.now();
  char buf[25];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
    now.year(), now.month(), now.day(),
    now.hour(), now.minute(), now.second());
  return String(buf);
}

/**
 * Get formatted date string for LCD display.
 * Format: "22 Feb 2026" (fits 16 chars)
 */
String getRTCDateForLCD() {
  if (!rtcConnected) return "No RTC";

  DateTime now = rtc.now();
  const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d %s %04d", now.day(), months[now.month()-1], now.year());
  return String(buf);
}

/**
 * Get formatted time string for LCD display.
 * Format: "08:30:00" or "08:30 AM"
 */
String getRTCTimeForLCD() {
  if (!rtcConnected) return "No RTC";

  DateTime now = rtc.now();
  char buf[10];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  return String(buf);
}

/**
 * Manually set RTC time (can be called from web route).
 */
void setRTCTime(int year, int month, int day, int hour, int minute, int second) {
  rtc.adjust(DateTime(year, month, day, hour, minute, second));
  Serial.println("RTC time updated.");
}

#endif
