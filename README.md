# IoT-Based Numerical DC Protection Relay (V1.0 Official)

An industrial-grade, IoT-enabled numerical protection relay for DC systems. This project implements **IDMT (Inverse Definite Minimum Time)** characteristics to protect DC loads (like motors) from overcurrent faults while providing a remote control and monitoring dashboard.

## Features
- **Inverse-Time Protection**: Sophisticated IDMT curve logic (Standard Inverse) ensures sensitive yet stable protection.
- **Safety Interlock**: Physical-software synchronization prevents remote resets while hardware faults are active.
- **Dual-Core Architecture**: Protection logic runs on ESP32 Core 0 independently of WiFi/Network tasks for maximum reliability.
- **Real-time Dashboard**: Professional monitoring via Node-RED with live gauges and fault history.
- **Bi-directional Control**: Remote reset capability with authenticated feedback loops.

---

## Hardware Connections

| Component | ESP32 Pin | Connection Type | Description |
| :--- | :--- | :--- | :--- |
| **MOSFET Gate** | GPIO 5 | Output | Controls the IRL540N MOSFET (via 220 Ohm Resistor) |
| **Fault Switch** | GPIO 4 | Input (Pull-down) | Toggles the simulated fault condition |
| **INA219 SDA** | GPIO 21 | I2C Data | Current/Voltage sensor data |
| **INA219 SCL** | GPIO 22 | I2C Clock | Current/Voltage sensor clock |
| **INA219 VCC** | 3.3V | Power | Power for the sensor |
| **INA219 GND** | GND | Ground | Common ground |

---

## Bill of Materials (BOM)

The following components were used in the construction of this numerical relay:

1. Microcontroller: ESP32 Development Board (30-pin NodeMCU-32S).
2. Current Sensor: INA219 I2C Current/Voltage Sensor (Bi-directional).
3. Switching Element: IRL540N N-Channel MOSFET (Logic-Level).
4. Primary Load: BO Motor (Geared DC Motor).
5. Interactive Hardware: SPST Toggle Switch (Manual Fault Trigger).
6. Passive Components: 
   - 10k Ohm Resistor (Pull-down for toggle switch).
   - 220 Ohm Resistor (Gate protection for MOSFET).
   - 1N4007 Diode (Flyback protection for motor).
7. Infrastructure: 2 x 9V Battery Source, LM2596 Buck Converter (Step-down to 5V), Jumper Wires, and Breadboard.

---

## Installation and Setup

### 1. Prerequisites
- Arduino IDE: For flashing the ESP32.
- Docker Desktop: To run the backend MQTT broker and Node-RED.

### 2. ESP32 Firmware
- Open `SGP_PE.ino` in the Arduino IDE.
- Install the following libraries:
  - `Adafruit INA219`
  - `PubSubClient`
- Update lines 11-13 with your local WiFi SSID, Password, and the IP address of your Mac/PC running Docker.
- Upload to your ESP32.

### 3. Backend (Docker)
This project uses a dockerized stack for Mosquitto (MQTT) and Node-RED. Launch the stack in the background using:
```bash
docker compose up -d
```

### 3. Dashboard Setup
- Access Node-RED at `http://localhost:1880`.
- Import the `node-red-flow.json` file.
- Configure the MQTT Broker node with:
  - **Server**: `mosquitto`
  - **Port**: `1883`
  - **User**: `admin`
  - **Password**: `admin123`
- Access the dashboard at `http://localhost:1880/ui`.

---

## Safety Protocols
- **Auto-Trip**: The system will trip if current exceeds **0.55A** based on the IDMT delay.
- **Interlock**: Remote reset is **denied** if the physical "Fault Switch" is still in the active (HIGH) position.

## License
This project was developed for the **Switchgear and Protection (SGP)** course. All rights reserved.
