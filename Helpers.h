#ifndef HELPERS_H
#define HELPERS_H

#include "Config.h"

// ============================================
// CORS Helper
// ============================================

void addCorsHeaders(AsyncWebServerResponse *response) {
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

// ============================================
// LED Status Helper
// ============================================

void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(delayMs);
    digitalWrite(STATUS_LED, LOW);
    delay(delayMs);
  }
}

// ============================================
// HTTP POST Helper for Laravel Backend
// ============================================

/**
 * POST JSON data to a URL and return the response body.
 * Returns empty string on failure. Sets httpCode by reference.
 */
String httpPostJson(const char* url, String jsonPayload, int &httpCode) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  httpCode = http.POST(jsonPayload);
  String response = "";

  if (httpCode > 0) {
    response = http.getString();
  }

  http.end();
  return response;
}

#endif
