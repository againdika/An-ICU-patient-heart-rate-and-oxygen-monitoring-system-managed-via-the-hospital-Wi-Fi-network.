# 🏥 ICU Patient Heart Rate & Oxygen Monitoring System

An IoT-based, real-time vital signs monitoring solution designed for Intensive Care Units (ICUs), managed wirelessly over a hospital Wi-Fi network.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32%20%2F%20ESP8266-blue.svg)](https://www.espressif.com/)
[![Category: Biomedical IoT](https://img.shields.io/badge/Category-Biomedical%20IoT-green.svg)]()

---

## 📌 Project Overview

In Intensive Care Units (ICUs), continuous and reliable monitoring of patient vital signs is critical for immediate clinical intervention. This project provides a low-cost, high-reliability telemetry monitoring system that measures a patient's **Heart Rate (BPM)** and **Blood Oxygen Saturation ($SpO_2$)**.

The acquired telemetry data is transmitted wirelessly over the internal hospital Wi-Fi network to a central nursing station dashboard, enabling real-time remote monitoring, threshold alerts, and historical data logging.

---

## ✨ Key Features

- **Continuous Vital Measurement:** Real-time pulse rate and $SpO_2$ acquisition via optical photoplethysmography (PPG).
- **Wi-Fi Telemetry:** Transmits live patient data across local hospital networks using lightweight IoT protocols (MQTT / WebSockets / HTTP REST).
- **Central Nursing Station Dashboard:** Visualizes multi-patient vitals at a centralized control station.
- **Automated Alarm System:** Immediate audible and visual alerts triggered when patient vitals breach safe thresholds (e.g., Hypoxia $SpO_2 < 90\%$, Bradycardia, or Tachycardia).
- **Bedside Display:** Local 0.96" OLED display for immediate bedside reading by healthcare staff.

---

## 🛠️ Hardware Requirements

| Component | Recommended Specification | Quantity |
| :--- | :--- | :---: |
| **Microcontroller** | ESP32-WROOM-32 or NodeMCU ESP8266 (Wi-Fi Enabled) | 1 |
| **Pulse Oximeter & HR Sensor** | MAX30102 / MAX30100 Pulse Oximeter Module | 1 |
| **Bedside Display** | 0.96" I2C OLED Display (SSD1306, 128x64) | 1 |
| **Alert Indicators** | Active Buzzer & LED Indicators (Red/Green) | 1 set |
| **Power Supply** | Medical-grade 5V DC Power Adapter / Battery Backup | 1 |
| **Wiring & Case** | Jumper Wires, Breadboard / Enclosure Case | - |

---

## 💻 Software & Tech Stack

- **Firmware:** C++ / Arduino Framework / ESP-IDF
- **Protocols:** MQTT (PubSubClient), HTTP/REST, WebSockets
- **Libraries:**
  - `SparkFun MAX3010x Pulse and Proximity Sensor Library`
  - `Adafruit_SSD1306` & `Adafruit_GFX`
  - `WiFi.h` / `ESP8266WiFi.h`
- **Dashboard / Server Options:** Node-RED, ThingsBoard, Grafana, or Web Application (Node.js / Python Flask).

---

## 📐 Circuit Connection Matrix

### MAX30102 Sensor Wiring

| MAX30102 Pin | ESP32 Pin | ESP8266 Pin | Description |
| :--- | :--- | :--- | :--- |
| **VCC** | 3.3V | 3.3V | 3.3V Power Supply |
| **GND** | GND | GND | Ground |
| **SDA** | GPIO 21 | GPIO 4 (D2) | I2C Data Line |
| **SCL** | GPIO 22 | GPIO 5 (D1) | I2C Clock Line |
| **INT** | GPIO 19 | GPIO 12 (D6) | Interrupt (Optional) |

### OLED Display Wiring

| OLED Pin | ESP32 Pin | Description |
| :--- | :--- | :--- |
| **VCC** | 3.3V / 5V | Power Supply |
| **GND** | GND | Ground |
| **SDA** | GPIO 21 | Shared I2C SDA Line |
| **SCL** | GPIO 22 | Shared I2C SCL Line |

---

## 🚀 Installation & Setup

### 1. Prerequisites
1. Download and install the [Arduino IDE](https://www.arduino.cc/en/software).
2. Add ESP32 / ESP8266 support to Arduino IDE:
   - Go to `File` -> `Preferences` -> `Additional Boards Manager URLs` and paste:
     ```
     [https://dl.espressif.com/dl/package_esp32_index.json](https://dl.espressif.com/dl/package_esp32_index.json)
     ```
3. Install required libraries via Arduino Library Manager (`Ctrl+Shift+I`):
   - `SparkFun MAX3010x Pulse and Proximity Sensor Library`
   - `Adafruit SSD1306`
   - `PubSubClient`

### 2. Configuration
Open the `.ino` firmware file and configure your local network credentials and broker address:

```cpp
// Wi-Fi Credentials
const char* ssid     = "HOSPITAL_WIFI_SSID";
const char* password = "HOSPITAL_WIFI_PASSWORD";

// MQTT Server Configuration
const char* mqtt_server = "192.168.x.x";
const int   mqtt_port   = 1883;
const char* patient_id  = "ICU_BED_01";
---
## ✍️ Author
    Author: Aruna Indika

## 📊 System Architecture

+--------------------------+       Wi-Fi (WPA2)       +--------------------------+
|      ICU Patient Bed     | -----------------------> |  Hospital Local Server   |
|  - MAX30102 PPG Sensor   |   MQTT / HTTP JSON       |  - Centralized Dashboard |
|  - ESP32 Microcontroller |                          |  - Nursing Station Alert |
|  - Bedside OLED Display  |                          |  - Data Archive / EMR    |
+--------------------------+                          +--------------------------+

##⚠️ Medical Disclaimer

    Note: This project is developed for educational, academic, and experimental IoT research purposes. Before any real-world clinical deployment, biomedical equipment requires strict compliance, calibration, and safety certification under medical device regulations.
