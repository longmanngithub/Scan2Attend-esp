# ESP32 Fingerprint Attendance Device

The IoT firmware for **Scan2Attend** — an ESP32-based classroom attendance terminal that records attendance via fingerprint scanning and syncs records to the Laravel backend.

## Overview

The device runs a **24/7 continuous scan loop**: when a student places their finger on the sensor it is identified, the attendance record is POST-ed to the backend over Wi-Fi, and the result is shown on the LCD with an audible buzzer tone. If the backend is unreachable, records are queued to a local SD card and automatically synced when connectivity is restored.

## Hardware

| Component | Model | Purpose |
|---|---|---|
| Microcontroller | ESP32 (30-pin) | Main controller |
| Fingerprint sensor | AS608 | Biometric identification |
| Real-time clock | DS3231 | Accurate timestamps |
| Display | LCD 16×2 I2C | Status and scan feedback |
| Storage | MicroSD card (SPI) | Offline attendance queue |
| Buzzer | Passive piezo | Audible scan feedback |
| Status LED | Built-in (GPIO 2) | Visual connection indicator |

### Wiring

```
AS608 Fingerprint Sensor
  TX  → GPIO 16 (UART2 RX)
  RX  → GPIO 17 (UART2 TX)
  VCC → 3.3 V
  GND → GND

DS3231 RTC  (I2C bus)
  SDA → GPIO 21
  SCL → GPIO 22
  VCC → 3.3 V
  GND → GND

LCD 16×2 I2C  (shared I2C bus, address 0x27)
  SDA → GPIO 21
  SCL → GPIO 22
  VCC → 5 V
  GND → GND

MicroSD Card Module  (SPI)
  CS   → GPIO 5
  MOSI → GPIO 23
  MISO → GPIO 19
  SCK  → GPIO 18
  VCC  → 3.3 V
  GND  → GND

Passive Buzzer
  Signal → GPIO 25
  GND    → GND
```

## File Structure

| File | Description |
|---|---|
| `ESP32_Fingerprint_Attendance.ino` | Main sketch — `setup()` and `loop()` |
| `Config.h` | Pin definitions, global objects, shared state variables, URL builders |
| `WiFiControl.h` | Wi-Fi Manager setup, backend heartbeat / connectivity check |
| `Fingerprint.h` | Fingerprint enrollment and verification logic |
| `Display.h` | All LCD 16×2 display helper functions |
| `RTC.h` | DS3231 initialisation and datetime formatting |
| `Buzzer.h` | LEDC PWM tone generation and named sound patterns |
| `SDStorage.h` | Offline queue read/write/sync using JSONL on SD card |
| `Routes.h` | ESPAsyncWebServer HTTP route handlers |
| `Helpers.h` | Shared utilities — HTTP POST, CORS headers, LED blink |

## Required Libraries

Install the following libraries via the **Arduino Library Manager** or PlatformIO:

| Library | Author |
|---|---|
| WiFiManager | tzapu / tablatronix |
| Adafruit Fingerprint Sensor Library | Adafruit |
| ESPAsyncWebServer | me-no-dev |
| AsyncTCP | me-no-dev |
| RTClib | Adafruit |
| LiquidCrystal I2C | Frank de Brabander |
| ArduinoJson | Benoît Blanchon |
| SD (built-in) | Arduino |

Board: **ESP32 Dev Module** (esp32 by Espressif, v2.x or v3.x).

## Configuration

### Compile-time

Edit `Config.h` to change the default server IP used when no saved value exists:

```cpp
#define DEFAULT_SERVER_IP "192.168.1.1"
```

### Runtime (via Wi-Fi Manager captive portal)

On first boot (or after a Wi-Fi reset) the device starts an access point named **ScanToAttend**. Connect to it and open the captive portal to:

- Enter your Wi-Fi SSID and password.
- Enter the backend server IP address.

The server IP is persisted to ESP32 NVS flash (Preferences) and rebuilt into the backend URLs on every boot:

```
http://<server_ip>/api/heartbeat
http://<server_ip>/api/student/check-in
http://<server_ip>/api/attendance/sync
```

### RTC Time

Set the RTC clock over HTTP after flashing (see [Web API](#web-api) below), or the firmware falls back to the compile-time timestamp on first power-up if the RTC has lost power.

## Boot Sequence

1. LCD shows "ScanToAttend / Starting..."
2. Initialise LCD → Buzzer → RTC → AS608 sensor (5 retries)
3. Connect to Wi-Fi via Wi-Fi Manager
4. Initialise SD card and count queued offline records
5. Start `ESPAsyncWebServer` on port **80**
6. Play startup tone and print status to serial

If the fingerprint sensor is not detected the device halts with an error message and blinks the status LED indefinitely.

## Main Loop

The loop runs five tasks on every iteration:

1. **Backend heartbeat** — polls `/api/heartbeat` every 5 s and updates `backendConnected`.
2. **SD health check** — verifies SD accessibility every 10 s.
3. **Auto-sync** — when online, flushes queued offline records to the backend every 30 s.
4. **LCD refresh** — updates the idle clock display every 1 s.
5. **Fingerprint scan** — reads from AS608 continuously; on a match:
   - Posts `{ fingerprint_id, scanned_at }` to `/api/student/check-in`.
   - Handles backend responses: `200 OK` (success), `400` (already checked in / no active session), or any error (queue to SD).
   - A 4-second per-finger cooldown prevents duplicate records.

## Offline Mode

When the backend is unreachable, each attendance record is appended to `/attendance_queue.jsonl` on the SD card as a JSON line. On the next successful backend connection the file is replayed via `/api/attendance/sync` and successfully submitted lines are removed.

## Buzzer Sounds

| Event | Sound |
|---|---|
| Startup | Three-note ascending sweep |
| Successful check-in | C6 → E6 → G6 ascending chord |
| Error / unknown finger | Three descending notes |
| Duplicate (already checked in) | Two-note descending pattern |
| No active session | Double short beep |
| Offline record saved | Rising two-tone beep |

## Web API

The device exposes an HTTP server on port **80**. All endpoints return JSON and include CORS headers.

| Method | Path | Description |
|---|---|---|
| `GET` | `/status` | Device health: sensor, RTC, SD, backend, Wi-Fi, fingerprint count |
| `GET` | `/count` | Fingerprint template count and sensor capacity |
| `GET` | `/config` | Current backend URL configuration |
| `POST` | `/enroll?id=<n>` | Begin fingerprint enrollment for template ID `n` |
| `GET` | `/enroll/status` | Poll enrollment state machine (`idle` → `waiting_first_scan` → `remove_finger` → `waiting_second_scan` → `processing` → `success`/`failed`) |
| `GET` | `/verify` | Trigger a one-shot manual fingerprint verification |
| `GET` | `/verify/result` | Poll the result of the last verify request |
| `POST` | `/delete?id=<n>` | Delete fingerprint template ID `n` from the sensor |
| `POST` | `/empty` | Erase all fingerprint templates from the sensor |
| `POST` | `/rtc/set?datetime=<ISO8601>` | Set RTC time (e.g. `2026-03-14T08:30:00`) |
| `POST` | `/wifi/reset` | Clear saved Wi-Fi credentials and restart in AP mode |

### Example: enroll a student

```bash
# 1. Start enrollment for fingerprint ID 42
curl -X POST "http://<esp-ip>/enroll?id=42"

# 2. Poll until complete
curl "http://<esp-ip>/enroll/status"
```

### Example: check device status

```bash
curl "http://<esp-ip>/status"
```

```json
{
  "status": "online",
  "sensor": true,
  "rtc": true,
  "sd_card": true,
  "backend": true,
  "offline_mode": false,
  "fingerprint_count": 15,
  "fingerprint_capacity": 127,
  "ip": "192.168.1.50",
  "ssid": "MyNetwork",
  "rssi": -62,
  "datetime": "2026-03-14T08:30:00",
  "backend_url": "http://192.168.1.1/api"
}
```

## Serial Monitor

Connect at **115200 baud** to see real-time logs including scan results, sync events, Wi-Fi status, and error messages.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| "No Sensor!" on LCD | AS608 not wired or wrong baud | Check GPIO 16/17 wiring; ensure 3.3 V supply |
| "RTC Failed" on LCD | DS3231 not found on I2C | Check GPIO 21/22 and 3.3 V supply |
| Device stuck in AP mode | No saved Wi-Fi credentials | Connect to **ScanToAttend** AP and configure |
| Scans queue offline but never sync | Wrong server IP saved | POST to `/wifi/reset`, reconfigure server IP |
| Duplicate scans recorded | Cooldown too short | Increase `SCAN_COOLDOWN` in `Config.h` (default 4000 ms) |
| RTC shows wrong time | Battery dead or first boot | POST to `/rtc/set` with current ISO 8601 datetime |
