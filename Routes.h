#ifndef ROUTES_H
#define ROUTES_H

#include "Config.h"
#include "Helpers.h"
#include "Fingerprint.h"
#include "Display.h"
#include "RTC.h"

// ============================================
// ESP32 Web Server Routes
// Kept for remote management from admin frontend
// ============================================

void setupRoutes() {

  // ---- CORS Preflight Handlers ----
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");

  server.onNotFound([](AsyncWebServerRequest *request){
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404, "application/json", "{\"error\":\"not found\"}");
    }
  });

  // ---- Status Endpoint ----
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    bool sensorLive = sensorConnected ? finger.verifyPassword() : false;
    finger.getTemplateCount();

    String json = "{";
    json += "\"status\":\"online\"";
    json += ",\"sensor\":" + String(sensorLive ? "true" : "false");
    json += ",\"rtc\":" + String(rtcConnected ? "true" : "false");
    json += ",\"sd_card\":" + String(sdCardReady ? "true" : "false");
    json += ",\"backend\":" + String(backendConnected ? "true" : "false");
    json += ",\"offline_mode\":" + String(offlineMode ? "true" : "false");
    json += ",\"fingerprint_count\":" + String(finger.templateCount);
    json += ",\"fingerprint_capacity\":" + String(finger.capacity);
    json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
    json += ",\"ssid\":\"" + WiFi.SSID() + "\"";
    json += ",\"rssi\":" + String(WiFi.RSSI());
    json += ",\"datetime\":\"" + getRTCDateTime() + "\"";
    json += ",\"backend_url\":\"" + String(backendBaseURL) + "\"";
    json += "}";

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    addCorsHeaders(response);
    request->send(response);
  });

  // ---- WiFi Reset ----
  server.on("/wifi/reset", HTTP_POST, [](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json",
      "{\"status\":\"resetting\",\"message\":\"WiFi settings cleared. Device will restart in AP mode.\"}");
    addCorsHeaders(response);
    request->send(response);

    delay(1000);
    wifiManager.resetSettings();
    ESP.restart();
  });

  // ---- Enrollment Status ----
  server.on("/enroll/status", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";

    switch (enrollState) {
      case ENROLL_IDLE:
        json += "\"status\":\"idle\"";
        break;
      case ENROLL_WAITING_FIRST:
        json += "\"status\":\"waiting_first_scan\",\"step\":1";
        break;
      case ENROLL_WAITING_REMOVE:
        json += "\"status\":\"remove_finger\",\"step\":2";
        break;
      case ENROLL_WAITING_SECOND:
        json += "\"status\":\"waiting_second_scan\",\"step\":3";
        break;
      case ENROLL_PROCESSING:
        json += "\"status\":\"processing\",\"step\":4";
        break;
      case ENROLL_SUCCESS:
        json += "\"status\":\"success\",\"id\":" + String(enrollID);
        enrollState = ENROLL_IDLE;
        break;
      case ENROLL_FAILED:
        json += "\"status\":\"failed\",\"error\":\"" + enrollError + "\"";
        if (enrollDuplicateID != -1) {
           json += ",\"duplicate_id\":" + String(enrollDuplicateID);
        }
        enrollState = ENROLL_IDLE;
        break;
    }

    json += "}";

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    addCorsHeaders(response);
    request->send(response);
  });

  // ---- Enroll Fingerprint ----
  server.on("/enroll", HTTP_ANY, [](AsyncWebServerRequest *request){
    Serial.println("\n📝 Enroll request received");

    if (!request->hasParam("id", false) && !request->hasParam("id")) {
      AsyncWebServerResponse *response = request->beginResponse(400, "application/json",
        "{\"error\":\"missing id\"}");
      addCorsHeaders(response);
      request->send(response);
      return;
    }

    // Try POST param first, then GET param
    if (request->hasParam("id", false)) {
      enrollID = request->getParam("id", false)->value().toInt();
    } else {
      enrollID = request->getParam("id")->value().toInt();
    }

    Serial.println("Starting enrollment for ID: " + String(enrollID));
    showEnrollStep(enrollID, "Place finger...");

    enrollRequested = true;
    enrollState = ENROLL_IDLE;
    enrollError = "";
    enrollDuplicateID = -1;

    AsyncWebServerResponse *response = request->beginResponse(202, "application/json",
      "{\"status\":\"enrollment_started\",\"id\":" + String(enrollID) + "}");
    addCorsHeaders(response);
    request->send(response);
  });

  // ---- Manual Verify (for testing) ----
  server.on("/verify/result", HTTP_GET, [](AsyncWebServerRequest *request){
    String json;

    if (lastVerifiedID == -1) {
      json = "{\"status\":\"waiting\"}";
    } else if (lastVerifiedID == -2) {
      json = "{\"status\":\"not_found\"}";
      lastVerifiedID = -1;
    } else {
      json = "{\"status\":\"ok\",\"fingerprint_id\":" + String(lastVerifiedID) + "}";
      lastVerifiedID = -1;
    }

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    addCorsHeaders(response);
    request->send(response);
  });

  server.on("/verify", HTTP_GET, [](AsyncWebServerRequest *request){
    verifyRequested = true;
    lastVerifiedID = -1;

    AsyncWebServerResponse *response = request->beginResponse(202, "application/json",
      "{\"status\":\"waiting_for_fingerprint\"}");
    addCorsHeaders(response);
    request->send(response);
  });

  // ---- Empty Fingerprint Database ----
  server.on("/empty", HTTP_POST, [](AsyncWebServerRequest *request){
    finger.emptyDatabase();
    Serial.println("🗑️ Sensor database cleared!");

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json",
      "{\"status\":\"cleared\",\"message\":\"All fingerprints removed from sensor\"}");
    addCorsHeaders(response);
    request->send(response);
  });

  // ---- Delete Single Fingerprint ----
  server.on("/delete", HTTP_ANY, [](AsyncWebServerRequest *request){
    if (!request->hasParam("id", false) && !request->hasParam("id")) {
      AsyncWebServerResponse *response = request->beginResponse(400, "application/json",
        "{\"error\":\"missing id\"}");
      addCorsHeaders(response);
      request->send(response);
      return;
    }

    uint16_t id;
    if (request->hasParam("id", false)) {
      id = request->getParam("id", false)->value().toInt();
    } else {
      id = request->getParam("id")->value().toInt();
    }

    bool ok = deleteFingerprint(id);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json",
      ok ? "{\"status\":\"deleted\",\"id\":" + String(id) + "}" : "{\"status\":\"fail\"}");
    addCorsHeaders(response);
    request->send(response);
  });

  // ---- Fingerprint Count ----
  server.on("/count", HTTP_GET, [](AsyncWebServerRequest *request){
    finger.getTemplateCount();
    String json = "{\"count\":" + String(finger.templateCount) + ",\"capacity\":" + String(finger.capacity) + "}";

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    addCorsHeaders(response);
    request->send(response);
  });

  // ---- Get/Set Backend Config ----
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"server_ip\":\"" + String(serverIP) + "\"";
    json += ",\"backend_url\":\"" + String(backendBaseURL) + "\"";
    json += ",\"heartbeat_url\":\"" + String(heartbeatURL) + "\"";
    json += ",\"checkin_url\":\"" + String(checkInURL) + "\"";
    json += ",\"sync_url\":\"" + String(syncURL) + "\"";
    json += "}";

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    addCorsHeaders(response);
    request->send(response);
  });

  // ---- Set RTC Time ----
  server.on("/rtc/set", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!request->hasParam("datetime", false)) {
      AsyncWebServerResponse *response = request->beginResponse(400, "application/json",
        "{\"error\":\"missing datetime param (ISO 8601)\"}");
      addCorsHeaders(response);
      request->send(response);
      return;
    }

    String dt = request->getParam("datetime", false)->value();
    // Parse ISO 8601: "2026-02-22T08:30:00"
    int year = dt.substring(0, 4).toInt();
    int month = dt.substring(5, 7).toInt();
    int day = dt.substring(8, 10).toInt();
    int hour = dt.substring(11, 13).toInt();
    int minute = dt.substring(14, 16).toInt();
    int second = dt.substring(17, 19).toInt();

    setRTCTime(year, month, day, hour, minute, second);

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json",
      "{\"status\":\"ok\",\"datetime\":\"" + getRTCDateTime() + "\"}");
    addCorsHeaders(response);
    request->send(response);
  });

  // ---- Get SD Card Queue Info ----
  server.on("/queue", HTTP_GET, [](AsyncWebServerRequest *request){
    int count = sdCardReady ? getQueueCount() : 0;
    String json = "{\"sd_ready\":" + String(sdCardReady ? "true" : "false");
    json += ",\"queued\":" + String(count) + "}";

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    addCorsHeaders(response);
    request->send(response);
  });

  // ---- Force Sync ----
  server.on("/sync", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!sdCardReady) {
      AsyncWebServerResponse *response = request->beginResponse(400, "application/json",
        "{\"error\":\"SD card not ready\"}");
      addCorsHeaders(response);
      request->send(response);
      return;
    }
    if (!backendConnected) {
      AsyncWebServerResponse *response = request->beginResponse(400, "application/json",
        "{\"error\":\"Backend offline\"}");
      addCorsHeaders(response);
      request->send(response);
      return;
    }

    int synced = syncQueuedRecords();
    String json = "{\"status\":\"ok\",\"synced\":" + String(synced) + "}";

    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    addCorsHeaders(response);
    request->send(response);
  });

  server.begin();
  Serial.println("🌐 HTTP server started on port 80");
}

#endif
