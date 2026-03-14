#ifndef WIFICONTROL_H
#define WIFICONTROL_H

#include "Config.h"
#include "Helpers.h"
#include "Display.h"

// ============================================
// WiFi Manager + Backend Heartbeat
// ============================================

// -------- WiFi Manager Callbacks --------

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("=================================");
  Serial.println("Entered WiFi Configuration Mode");
  Serial.println("=================================");
  Serial.print("Connect to AP: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("Then open: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("=================================");

  showWiFiConnecting();
  blinkLED(5, 100);
}

void saveConfigCallback() {
  Serial.println("WiFi configuration saved!");
  blinkLED(3, 200);
  shouldSaveConfig = true;
}

// -------- Setup WiFi with Manager --------

WiFiManagerParameter* serverIPParam;

void setupWiFi() {
  // Set LED as output
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  // Load saved Server IP from Preferences
  preferences.begin("config", true); // read-only
  String savedIP = preferences.getString("serverIP", DEFAULT_SERVER_IP);
  preferences.end();

  savedIP.toCharArray(serverIP, sizeof(serverIP));
  Serial.print("Loaded Server IP: ");
  Serial.println(serverIP);

  // Build all backend URLs
  buildBackendURLs();
  Serial.print("Backend Base URL: ");
  Serial.println(backendBaseURL);

  // Create custom parameter for Server IP input
  serverIPParam = new WiFiManagerParameter("server_ip", "Backend Server IP (e.g. 192.168.1.100)", serverIP, 40);
  wifiManager.addParameter(serverIPParam);

  // WiFi Manager configuration
  wifiManager.setDebugOutput(true);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(300);
  wifiManager.setMinimumSignalQuality(20);

  Serial.println("Starting WiFi Manager...");

  // Try to connect to saved WiFi, if fails, start config portal
  if (!wifiManager.autoConnect("ScanToAttend-Setup")) {
    Serial.println("Failed to connect and hit timeout");
    Serial.println("Restarting in 3 seconds...");
    showError("WiFi Failed");
    delay(3000);
    ESP.restart();
  }

  // If configuration was updated, save custom parameter
  if (shouldSaveConfig) {
    String newIP = serverIPParam->getValue();
    newIP.trim();

    if (newIP.length() > 0) {
      newIP.toCharArray(serverIP, sizeof(serverIP));

      // Save to Preferences
      preferences.begin("config", false);
      preferences.putString("serverIP", newIP);
      preferences.end();

      Serial.print("Saved new Server IP: ");
      Serial.println(serverIP);

      // Rebuild all URLs
      buildBackendURLs();
    }

    Serial.println("Configuration saved. Restarting...\n");
    delay(1000);
    ESP.restart();
  }

  // Connected!
  Serial.println("\n=================================");
  Serial.println("WiFi Connected Successfully!");
  Serial.println("=================================");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Signal Strength: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.print("Backend: ");
  Serial.println(backendBaseURL);
  Serial.println("=================================\n");

  showWiFiConnected(WiFi.localIP().toString());
  digitalWrite(STATUS_LED, HIGH);
}

// -------- Backend Heartbeat --------

// Periodic status print so the serial monitor always shows current state
unsigned long lastStatusPrint = 0;
const unsigned long STATUS_PRINT_INTERVAL = 30000;  // Print full status every 30 seconds

void printBackendStatus() {
  Serial.println("─────────────────────────────────");
  Serial.print("📡 WiFi: ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected (");
    Serial.print(WiFi.SSID());
    Serial.print(", ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm)");
  } else {
    Serial.println("DISCONNECTED");
  }
  Serial.print("🖥️  Backend: ");
  Serial.print(backendConnected ? "ONLINE" : "OFFLINE");
  Serial.print(" (");
  Serial.print(backendBaseURL);
  Serial.println(")");
  Serial.print("💾 SD Card: ");
  Serial.println(sdCardReady ? "Ready" : "NOT AVAILABLE");
  if (sdCardReady) {
    int q = getQueueCount();
    if (q > 0) {
      Serial.print("📦 Queued records: ");
      Serial.println(q);
    }
  }
  Serial.print("🔒 Mode: ");
  Serial.println(backendConnected ? "Online (live check-in)" : "Offline (queueing to SD)");
  Serial.println("─────────────────────────────────");
}

void checkBackendStatus() {
  if (millis() - heartbeatTimer > HEARTBEAT_INTERVAL) {
    heartbeatTimer = millis();

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(String(heartbeatURL));
      http.setTimeout(2000);

      int httpCode = http.GET();

      if (httpCode == 200) {
        if (!backendConnected) {
          Serial.println("\n✅ Backend is now ONLINE! Switching to live mode.");
          backendConnected = true;
          printBackendStatus();
        }
      } else {
        if (backendConnected) {
          Serial.println("\n❌ Backend went OFFLINE! code: " + String(httpCode));
          Serial.println("   Scanning continues — records will be saved to SD card.");
          backendConnected = false;
          printBackendStatus();
        }
      }
      http.end();
    } else {
      // WiFi itself is down
      if (backendConnected) {
        Serial.println("\n⚠️ WiFi disconnected — backend unreachable.");
        Serial.println("   Scanning continues — records will be saved to SD card.");
        backendConnected = false;
        printBackendStatus();
      }
    }
  }

  // Periodic status print regardless of state changes
  if (millis() - lastStatusPrint > STATUS_PRINT_INTERVAL) {
    lastStatusPrint = millis();
    printBackendStatus();
  }

  // Update offline mode flag
  offlineMode = !backendConnected;

  if (!backendConnected) {
    // Slow blink warning if backend offline
    showOfflineLED();
  }
}

#endif
