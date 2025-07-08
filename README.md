# ESP32 Smart Home System

A modular smart home system built with ESP32 microcontrollers. It enables:

- Remote control of lighting and devices (e.g., blinds, thermostat),
- Environmental monitoring (temperature, humidity, brightness),
- Communication with a central server via HTTP and data exchange in JSON format.

---

## Project Structure
```
ESP32-SmartHome/
│
├── Salon.ino # Living room: sensors, blinds, thermostat
├── Sypialnia.ino # Bedroom: sensors, blinds, lights, thermostat
├── Przedpokoj.ino # Controls lights in living room, bathroom, and hallway
├── Serwer.ino # Central server handling data and state synchronization
├── README.md # This file
```

## 🌐 Requirements

- ESP32 microcontrollers  
- HTTP server (e.g., ESP32 as a hub or local Node.js/Flask server)  
- WiFi network  
- Arduino libraries:
  - `WiFi.h`  
  - `HTTPClient.h`    
  - `DHT.h`  
  - `DallasTemperature.h`  
  - `OneWire.h`  
  - `Stepper.h`  
  - `LightDependentResistor.h`

---

## Modules and Features

### `salon.ino`

**Readings:**
- Temperature (DS18B20)  
- Humidity (DHT11)  
- Brightness (LDR)  

**Control:** 
- Thermostat (stepper motor)  
- Blinds (DC motor)  

**Communication:**
- Sends data to the server every 1 second  
- Fetches device states from `/status`

---

### `sypialnia.ino`

**Readings:**
- Temperature (DS18B20)  
- Humidity (DHT11)  
- Brightness (LDR)  

**Control:**
- 2 lights  (relays)
- Thermostat (stepper motor)  
- Blinds  (DC motor)

**Communication:**
- Sends data in JSON format  
- Fetches device states from `/status`

---

### `przedpokoj.ino`

**Control:**
- Lights: living room (2), bathroom (2), hallway (1)

**Communication:**
- Receives device states from `/status` and applies them directly to GPIO

---

## Server Communication

### JSON Payload (POST to `/status`)

Used by ESP32 modules to send data or retrieve state updates.

**Example:**
```json
{
  "status": {
    "living_room": {
      "temperature": 23.5,
      "humidity": 40.2,
      "brightness": 120.3
    },
    "bedroom": {
      "temperature": 22.1,
      "humidity": 50.0,
      "brightness": 90.0
    }
  }
}
