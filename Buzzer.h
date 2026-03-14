#ifndef BUZZER_H
#define BUZZER_H

#include "Config.h"

// ============================================
// Passive Buzzer Sound Functions
// Uses LEDC PWM for tone generation on ESP32
// ============================================

#define BUZZER_CHANNEL 0

/**
 * Initialize the buzzer pin.
 */
void initBuzzer() {
  ledcAttach(BUZZER_PIN, 2000, 8);  // Attach pin with default freq and 8-bit resolution
  ledcWrite(BUZZER_PIN, 0);          // Start silent
  Serial.println("✅ Buzzer initialized on GPIO " + String(BUZZER_PIN));
}

/**
 * Play a tone at a given frequency for a duration.
 */
void playTone(int frequency, int duration) {
  ledcWriteTone(BUZZER_PIN, frequency);
  delay(duration);
  ledcWriteTone(BUZZER_PIN, 0);  // Stop tone
}

/**
 * Success melody: two ascending notes.
 * Played when fingerprint is recognized and attendance recorded.
 */
void buzzerSuccess() {
  playTone(1047, 100);  // C6
  delay(50);
  playTone(1319, 100);  // E6
  delay(50);
  playTone(1568, 150);  // G6
  ledcWriteTone(BUZZER_PIN, 0);
}

/**
 * Error melody: three descending notes.
 * Played when fingerprint is not recognized.
 */
void buzzerError() {
  playTone(440, 150);   // A4
  delay(50);
  playTone(330, 150);   // E4
  delay(50);
  playTone(262, 200);   // C4
  ledcWriteTone(BUZZER_PIN, 0);
}

/**
 * Duplicate beep: short flat tone.
 * Played when student already checked in.
 */
void buzzerDuplicate() {
  playTone(880, 80);    // A5
  delay(80);
  playTone(880, 80);    // A5
  ledcWriteTone(BUZZER_PIN, 0);
}

/**
 * No session beep: low warning tone.
 * Played when no active session found for scanned student.
 */
void buzzerNoSession() {
  playTone(350, 300);   // Low warning
  ledcWriteTone(BUZZER_PIN, 0);
}

/**
 * Startup jingle.
 */
void buzzerStartup() {
  playTone(523, 80);    // C5
  delay(30);
  playTone(659, 80);    // E5
  delay(30);
  playTone(784, 80);    // G5
  delay(30);
  playTone(1047, 120);  // C6
  ledcWriteTone(BUZZER_PIN, 0);
}

/**
 * Offline save beep: single medium tone.
 * Played when attendance saved to SD card in offline mode.
 */
void buzzerOfflineSave() {
  playTone(660, 150);   // E5
  ledcWriteTone(BUZZER_PIN, 0);
}

#endif
