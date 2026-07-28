// ╔══════════════════════════════════════════════════════════════════════╗
// ║  ICU Bedside Vitals Monitor   v1.0                                   ║
// ║  ESP32-C6 + MAX30102 (HR/SpO2) + LCD + MQTT (TLS)                    ║
// ║                                                                        ║
// ║  Reads heart rate + SpO2 every 60s, classifies into SAFE/YELLOW/RED   ║
// ║  zone, shows on LCD, publishes to MQTT, fires an alert once on entry  ║
// ║  into RED zone. Light-sleeps between cycles to save power while      ║
// ║  keeping WiFi/MQTT alive for continuous alert coverage.               ║
// ║                                                                        ║
// ║  PRIVACY NOTE: this device only ever transmits BED_ID — never a       ║
// ║  patient name. The bed → patient mapping lives only in the hospital   ║
// ║  database, not on the device, to minimize PII exposure at the edge.   ║
// ╚══════════════════════════════════════════════════════════════════════╝
//
//  Libraries (Arduino Library Manager):
//    - SparkFun MAX3010x Pulse and Proximity Sensor Library
//    - PubSubClient           by Nick O'Leary
//    - LiquidCrystal I2C      by Frank de Brabander
//    - WiFiClientSecure       built-in for ESP32 (TLS)


//  ── WIRING (ESP32) ──────────────────────────────────────────────────
//  Bus 1 (default Wire) — LCD
//    SDA → GPIO 21
//    SCL → GPIO 22

//  Bus 2 (Wire1) — MAX30102:
//    SDA → GPIO 33
//    SCL → GPIO 32

//  ── MQTT TOPICS ──────────────────────────────────────────────────────
//  hospital/icu/<BED_ID>/vitals   → routine reading every 60s (QoS 0)
//  hospital/icu/<BED_ID>/alert    → fires ONCE on entering RED zone (QoS 1)
//  hospital/icu/<BED_ID>/status   → fires ONCE on leaving RED zone (QoS 1)

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include "esp_sleep.h"
#include "time.h"

// ── VARIABLE DECLERATION ────────────────────────────────────────
const char* WIFI_SSID   = "ArunaPhn";
const char* WIFI_PASS   = "112345678990";

const char* MQTT_BROKER = "192.168.43.110";    
const int   MQTT_PORT   = 8883;              // TLS
const char* MQTT_USER   = "esp32user";
const char* MQTT_PASS   = "pass@pass123";


const char* BED_ID      = "BED-01";

// broker's CA cert

const char* ROOT_CA = R"EOF(-----BEGIN CERTIFICATE-----
AGMIIC8jCCAdqgAwIBAgIUJZk8gz9Qmqw/DnqbK4wuscQ2OxUwDQYJKoZIhvcNAQEL
BQAwGTEXMBUGA1UEAwwOSG9zcGl0YWxJQ1UtQ0EwHhcNMjYwNzE2MDMyNzU4WhcN
MzYwNzEzMDMyNzU4WjAZMRcwFQYDVQQDDA5Ib3NwaXRhbElDVS1DQTCCASIwDQYJ
KoZIhvcNAQEBBQADggEPADCCAQoCggEBAIgIpAGUZZH3tWrMS0i9ISyAFWuUhSv5
cg+qbfiwhqR04pKKCy3Ilk4POArCD0VvYj0dZOgmw1elTBIandvPeysAijlhqiO2
vgtjzHy+bBefewfVmcCau3w8siY9IAaRgPjPFp5E7TDSi7QhjL6cxT7d5/P2bkZT
o2HUBipPprfhiV/BL7Sj1SOfDcjZnmh6AgqVWK1bi1xoNj23rcylukRFHkIjDNAj
feEEL24FJB9X3m4akkV7RvGltiCrmAylfXyaRdqO7HO1c1faROwWU1NoOe6kbt8t
66kZucGM1moAtgevgWyRHiLLLOz0nY1Rl3+MKWPRyvBC8feyrTs6kgUCAwEAAaMy
MDAwHQYDVR0OBBYEFM2nbLBFyFhBUEaYOxOK2YOaW0CNMA8GA1UdEwEB/wQFMAMB
Af8wDQYJKoZIhvcNAQELBQADggEBAFOvrd/SgJZnJQfsEJxWBHPq7Bqn/RIpEw1y
Aj877RjlUbxX/Xy69VEpanY03FTeI7YcxPYrFR0MpSe+hbHioR3mlJYKjInAii+A
cq1AvjITZNybXHxqwV4drEBnWlYHjaEVOPjDco+5rgiGkMZGwcPlB4VdvlYA80h8
BI+wbwBjG8at2jJtwuHZTtLMVgG5Xq+uvSpUKHP6pEbDkJUJIwH3lZE2RjyUVWSN
FfOQxklo2gbVwUZ/qZy0pjbCdB915dLCdMR/XDwe8ZuQW8zdjyQ5BIEMErK0llK2
xYR/l95CGXBsWJIP2Sj05VfOv4RlBCDPhUNULeqYXHfwamu2IGE=
-----END CERTIFICATE-----)EOF";
// ─────────────────────────────────────────────────────────────────────

// ── CLINICAL THRESHOLDS──────
#define HR_SAFE_LOW     60
#define HR_SAFE_HIGH    100
#define HR_YELLOW_LOW   50
#define HR_YELLOW_HIGH  120
// outside YELLOW range → RED

#define SPO2_SAFE_MIN   95
#define SPO2_YELLOW_MIN 90
// below YELLOW_MIN → RED
// ─────────────────────────────────────────────────────────────────────

#define READ_CYCLE_S    60      // full cycle: read + publish + sleep
#define BUFFER_SIZE     100     // samples collected per reading

#define NTP_SERVER      "pool.ntp.org"
#define GMT_OFFSET_SEC  19800   // UTC+05:30 Sri Lanka
#define DST_OFFSET_SEC  0

MAX30105 particleSensor;
WiFiClientSecure secureClient;
PubSubClient     mqtt;
LiquidCrystal_I2C lcd(0x27, 16, 2);

uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

int32_t spo2;
int8_t  validSPO2;
int32_t heartRate;
int8_t  validHeartRate;

enum Zone { ZONE_SAFE, ZONE_YELLOW, ZONE_RED, ZONE_NO_SIGNAL };
Zone currentZone   = ZONE_NO_SIGNAL;
bool redAlertActive = false;

unsigned long mqttBackoffMs = 1000;
const unsigned long MQTT_BACKOFF_MAX = 30000;
unsigned long lastReconnectAttempt = 0;

bool ntpReady() { time_t t; time(&t); return t > 100000; }

void syncTime() {
    Serial.print("[NTP] Syncing time");
    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
    for (int i = 0; !ntpReady() && i < 20; i++) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    if (ntpReady()) {
        Serial.println("[NTP] Time synced — TLS cert validation will work correctly");
    } else {
        Serial.println("[NTP] WARNING: time sync failed — TLS connections will likely fail");
    }
}

// ══════════════════════════════════════════════════════════════════════
void connectWiFi() {
    WiFi.disconnect(true);   // force clean state before reconnecting — prevents
    delay(100);              // "cannot set config" collisions after light sleep
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);    // disable WiFi's own power-save mode; it conflicts
                              // with our manual light-sleep duty cycling
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[WLAN] Connecting");
    for (int i = 0; WiFi.status() != WL_CONNECTED && i < 40; i++) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WLAN] Connected, IP: " + WiFi.localIP().toString());
    } else {
        Serial.println("[WLAN] FAILED — will retry in loop()");
    }
}

bool connectMQTTOnce() {
    Serial.print("[MQTT] Attempting connect...");
    bool ok = mqtt.connect(BED_ID, MQTT_USER, MQTT_PASS);
    if (ok) {
        Serial.println(" connected.");
        mqttBackoffMs = 1000;
    } else {
        Serial.printf(" failed, rc=%d. Next retry in %lums\n", mqtt.state(), mqttBackoffMs);
        mqttBackoffMs = min(mqttBackoffMs * 2, MQTT_BACKOFF_MAX);
    }
    return ok;
}

void maintainMQTT() {
    if (mqtt.connected()) return;
    unsigned long now = millis();
    if (now - lastReconnectAttempt >= mqttBackoffMs) {
        lastReconnectAttempt = now;
        connectMQTTOnce();
    }
}

// ──────────────────────────────────────────────────────────────────────
void lcdWrite(const String& r0, const String& r1) {
    auto pad = [](String s) -> String {
        while ((int)s.length() < 16) s += ' ';
        return s.substring(0, 16);
    };
    lcd.setCursor(0, 0); lcd.print(pad(r0));
    lcd.setCursor(0, 1); lcd.print(pad(r1));
}

// ══════════════════════════════════════════════════════════════════════
// Zone classification — overall zone is the WORSE of HR zone and SpO2 zone
Zone classifyHR(int32_t hr) {
    if (hr >= HR_SAFE_LOW && hr <= HR_SAFE_HIGH) return ZONE_SAFE;
    if (hr >= HR_YELLOW_LOW && hr <= HR_YELLOW_HIGH) return ZONE_YELLOW;
    return ZONE_RED;
}

Zone classifySpO2(int32_t sp) {
    if (sp >= SPO2_SAFE_MIN) return ZONE_SAFE;
    if (sp >= SPO2_YELLOW_MIN) return ZONE_YELLOW;
    return ZONE_RED;
}

Zone worseZone(Zone a, Zone b) {
    if (a == ZONE_RED || b == ZONE_RED) return ZONE_RED;
    if (a == ZONE_YELLOW || b == ZONE_YELLOW) return ZONE_YELLOW;
    return ZONE_SAFE;
}

const char* zoneName(Zone z) {
    switch (z) {
        case ZONE_SAFE:      return "SAFE";
        case ZONE_YELLOW:    return "YELLOW";
        case ZONE_RED:       return "RED";
        default:             return "NO_SIGNAL";
    }
}

// ══════════════════════════════════════════════════════════════════════
// Collect BUFFER_SIZE samples and run the Maxim HR/SpO2 algorithm
void takeReading() {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        while (!particleSensor.available()) particleSensor.check();
        redBuffer[i] = particleSensor.getRed();
        irBuffer[i]  = particleSensor.getIR();
        particleSensor.nextSample();
    }

    maxim_heart_rate_and_oxygen_saturation(
        irBuffer, BUFFER_SIZE, redBuffer,
        &spo2, &validSPO2, &heartRate, &validHeartRate
    );

    // No finger / weak signal detection — low IR reading means nothing's on the sensor
    long irMean = 0;
    for (int i = 0; i < BUFFER_SIZE; i++) irMean += irBuffer[i];
    irMean /= BUFFER_SIZE;

    if (irMean < 50000 || !validHeartRate || !validSPO2) {
        currentZone = ZONE_NO_SIGNAL;
        Serial.println("[SENSOR] No finger detected / invalid reading");
        lcdWrite("No signal", "Check sensor");
        return;
    }

    Zone hrZone   = classifyHR(heartRate);
    Zone spo2Zone = classifySpO2(spo2);
    currentZone   = worseZone(hrZone, spo2Zone);

    Serial.printf("[VITALS] HR=%ld bpm  SpO2=%ld%%  Zone=%s\n",
                  heartRate, spo2, zoneName(currentZone));

    lcdWrite(
        "HR:" + String(heartRate) + " O2:" + String(spo2) + "%",
        "Zone: " + String(zoneName(currentZone))
    );
}

// ══════════════════════════════════════════════════════════════════════
void publishJSON(const char* topicSuffix, const String& json, int qos) {
    String topic = "hospital/icu/" + String(BED_ID) + "/" + topicSuffix;
    mqtt.publish(topic.c_str(), json.c_str());
    Serial.printf("[MQTT] -> %s (QoS%d) : %s\n", topic.c_str(), qos, json.c_str());
}

void publishVitals() {
    String json = "{";
    json += "\"bed_id\":\"" + String(BED_ID) + "\",";
    json += "\"hr\":" + String(heartRate) + ",";
    json += "\"spo2\":" + String(spo2) + ",";
    json += "\"zone\":\"" + String(zoneName(currentZone)) + "\"";
    json += "}";
    publishJSON("vitals", json, 0);
}

void fireRedAlert() {
    redAlertActive = true;
    String json = "{";
    json += "\"bed_id\":\"" + String(BED_ID) + "\",";
    json += "\"level\":\"RED\",";
    json += "\"hr\":" + String(heartRate) + ",";
    json += "\"spo2\":" + String(spo2) + ",";
    json += "\"message\":\"Patient vitals in critical zone\"";
    json += "}";
    publishJSON("alert", json, 1);
}

void clearRedAlert() {
    redAlertActive = false;
    String json = "{";
    json += "\"bed_id\":\"" + String(BED_ID) + "\",";
    json += "\"level\":\"CLEARED\",";
    json += "\"hr\":" + String(heartRate) + ",";
    json += "\"spo2\":" + String(spo2);
    json += "}";
    publishJSON("status", json, 1);
}

void evaluateAlert() {
    if (currentZone == ZONE_RED && !redAlertActive) {
        fireRedAlert();
    } else if (currentZone != ZONE_RED && redAlertActive) {
        clearRedAlert();
    }
}

// ══════════════════════════════════════════════════════════════════════
// Light sleep for the remainder of the 60s cycle — keeps WiFi/MQTT state
// alive (unlike deep sleep) so alert coverage stays continuous.
void sleepRemainder(unsigned long elapsedMs) {
    long remainingS = READ_CYCLE_S - (elapsedMs / 1000);
    if (remainingS <= 0) return;
    Serial.printf("[SLEEP] Light-sleeping for %lds\n", remainingS);
    WiFi.disconnect(true);   // clean teardown before sleep — avoids a corrupted
                              // radio state on wake that caused reconnect failures
    esp_sleep_enable_timer_wakeup((uint64_t)remainingS * 1000000ULL);
    esp_light_sleep_start();
    Serial.println("[SLEEP] Woke up");
}

// ══════════════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    Serial.printf("\n=== ICU Bedside Monitor v1.0 | %s ===\n", BED_ID);

    Wire.begin(21, 22);    // Bus 1: LCD — SDA=GPIO21, SCL=GPIO22 (ESP32 default, matches your existing LCD wiring)
    Wire1.begin(33, 32);   // Bus 2: MAX30102 — SDA=GPIO33, SCL=GPIO32
    lcd.init();
    lcd.backlight();
    lcdWrite("ICU Monitor", String(BED_ID));

    if (!particleSensor.begin(Wire1, I2C_SPEED_FAST)) {
        Serial.println("[SENSOR] MAX30102 not found — check wiring");
        lcdWrite("Sensor ERROR", "Check wiring");
        while (1) delay(1000);
    }
    particleSensor.setup();   // default settings: 100Hz, 4 LED pulse width, etc.

    connectWiFi();
    syncTime();   // MUST happen before TLS — cert validation checks dates against device clock

    secureClient.setCACert(ROOT_CA);
    mqtt.setClient(secureClient);
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    connectMQTTOnce();
}

// ══════════════════════════════════════════════════════════════════════
void loop() {
    unsigned long cycleStart = millis();

    if (WiFi.status() != WL_CONNECTED) connectWiFi();
    maintainMQTT();
    mqtt.loop();

    lcdWrite("Reading...", "Hold still");
    takeReading();

    if (currentZone != ZONE_NO_SIGNAL) {
        publishVitals();
        evaluateAlert();
    }

    unsigned long elapsed = millis() - cycleStart;

    // Skip sleep while a RED alert is active — poll faster to catch recovery sooner
    if (!redAlertActive) {
        sleepRemainder(elapsed);
    } else {
        delay(5000);
    }
}
