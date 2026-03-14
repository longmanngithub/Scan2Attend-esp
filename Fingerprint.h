#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include "Config.h"
#include "Helpers.h"

// -------- Fingerprint Logic --------

bool enrollFingerprint(uint16_t id) {
  int p = -1;
  
  // Step 1: Wait for first finger
  enrollState = ENROLL_WAITING_FIRST;
  Serial.println("Waiting for finger (1st scan)...");

  unsigned long timeout = millis() + 30000; // 30 second timeout
  while (p != FINGERPRINT_OK && millis() < timeout) {
    p = finger.getImage();
    delay(50);
  }
  
  if (p != FINGERPRINT_OK) {
    enrollState = ENROLL_FAILED;
    enrollError = "Timeout waiting for finger";
    return false;
  }

  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    enrollState = ENROLL_FAILED;
    enrollError = "Failed to process first image";
    return false;
  }
  
  // Check if fingerprint already exists!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    enrollState = ENROLL_FAILED;
    // Store the ID that was found
    enrollDuplicateID = finger.fingerID; 
    enrollError = "Fingerprint already enrolled (ID #" + String(finger.fingerID) + ")";
    Serial.println("Error: Fingerprint already exists!");
    return false;
  }
  
  // Step 2: Wait for finger removal
  enrollState = ENROLL_WAITING_REMOVE;
  Serial.println("Remove finger...");
  delay(2000);

  timeout = millis() + 10000;
  while (finger.getImage() != FINGERPRINT_NOFINGER && millis() < timeout) {
    delay(50);
  }

  // Step 3: Wait for second finger
  enrollState = ENROLL_WAITING_SECOND;
  Serial.println("Place same finger again (2nd scan)...");
  
  timeout = millis() + 30000;
  while (finger.getImage() != FINGERPRINT_OK && millis() < timeout) {
    delay(50);
  }

  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    enrollState = ENROLL_FAILED;
    enrollError = "Failed to process second image";
    return false;
  }
  
  // Step 4: Create and store model
  enrollState = ENROLL_PROCESSING;
  Serial.println("Creating model...");
  
  if (finger.createModel() != FINGERPRINT_OK) {
    enrollState = ENROLL_FAILED;
    enrollError = "Fingerprints did not match";
    return false;
  }
  
  if (finger.storeModel(id) != FINGERPRINT_OK) {
    enrollState = ENROLL_FAILED;
    enrollError = "Failed to store model";
    return false;
  }

  enrollState = ENROLL_SUCCESS;
  Serial.println("Enrollment successful!");
  return true;
}

int verifyFingerprint() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerSearch();
  if (p == FINGERPRINT_NOTFOUND) return -2;
  if (p != FINGERPRINT_OK) return -1;

  return finger.fingerID;
}

bool deleteFingerprint(uint16_t id) {
  return finger.deleteModel(id) == FINGERPRINT_OK;
}

#endif
