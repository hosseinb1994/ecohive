# Ecohive IoT Project

This project is a **sample IoT system** demonstrating environmental data collection, local monitoring, and cloud integration using **Bare-Metal C** and **FreeRTOS** on an STM32 microcontroller.

---

## 🧩 Overview
The system reads multiple sensors (gas, temperature, humidity) and sends their data to an **ESP32** via **SPI**.  
The ESP32 then transmits the collected data to the **AWS IoT Cloud** using **Wi-Fi (MQTT)** protocol.  
A UART terminal provides local monitoring, and a heartbeat LED indicates system health.

---

## ⚙️ Hardware Requirements
| Component | Description |
|------------|-------------|
| **STM32 Nucleo-F401RE** | Main controller running Bare-Metal + FreeRTOS firmware |
| **ESP32** | Transfers data to AWS IoT via Wi-Fi and MQTT |
| **MQ9 Sensor** | Gas detection (CO, LPG, CH4) |
| **AM2302 Sensor (DHT22)** | Temperature and humidity measurement |
| **AWS IoT Platform** | Cloud monitoring and control interface |

---

## 🧠 System Details

### **Sensor & Peripheral Connections**
| Function | Pin | Description |
|-----------|-----|-------------|
| MQ9 Sensor | `PA0 (ADC1_IN0)` | Analog input for gas concentration |
| Internal Temperature Sensor | `ADC1_IN16`, `ADC1_IN17 (VREFINT)`, `ADC1_IN18` | Measures MCU internal temperature and voltage reference |
| AM2302 Sensor | Custom driver implemented | Measures ambient temperature and humidity |
| Onboard LED | `PA5` | Blinks periodically (heartbeat indicator) |

---

### **Communication Interfaces**
| Interface | Pins | Description |
|------------|------|-------------|
| UART1 | `PA9 (TX)`, `PA10 (RX)` | Sends system data and heartbeat to PC terminal |
| SPI2 | `PC2 (MISO)`, `PC3 (MOSI)` | Transfers data from STM32F401 → ESP32 |
| Wi-Fi + MQTT | — | ESP32 → AWS IoT communication |

**Data Flow:**  
🧭 `Sensors → STM32F401 (Bare-Metal + FreeRTOS) → SPI → ESP32 → Wi-Fi (MQTT) → AWS IoT`

---

## 🧾 Features
- Developed entirely in **Bare-Metal C (register-level programming)**  
- **FreeRTOS** handles multitasking and peripheral management  
- Each peripheral implemented in its own `.c` / `.h` driver file  
- **Heartbeat LED (PA5)** and UART log ensure active operation  
- Data securely sent to AWS IoT via **ESP32 using MQTT over Wi-Fi**

---

## 🧱 Software Stack
- **Bare-Metal C** for STM32 hardware drivers  
- **FreeRTOS** for task scheduling and timing  
- **MQTT** protocol for cloud communication  
- **AWS IoT Core** as cloud backend  

---

## 🧑‍💻 Author
**Hossein Baghaei**

---

## 🪄 Future Improvements
- Add OTA firmware updates via ESP32  
- Build AWS dashboard for real-time visualization  
- Implement SD card data logging for offline analysis
