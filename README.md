# Rellotge_Sensoriat - ESP8266 (Huzzah)
A WiFi-synced clock with temperature and humidity display, built for the ESP8266 NodeMCU v3 (LoLin), developed in C++ using the Arduino framework with PlatformIO.

## Features
* NTP time synchronization with automatic timezone handling (CET/CEST Spain).
* Automatic cycle display: time, day of the week, date, temperature and humidity.
* Manual mode toggle between full auto-cycle and time-only display.
* Nightly brightness reduction (22:00–08:00).
* Dual WiFi network support with automatic fallback.
* Daily re-sync at midnight to keep time accurate.
* Physical button control (mode and power) plus serial control for debugging.
* Power on/off with star animation and vertical line transition effects.

## Project Structure
* **src/main.cpp**: Core application logic. Handles NTP sync, DHT22 sensor reading, display cycle management, button debouncing, animations and serial control.
* **include/secrets.h**: WiFi credentials (not tracked by Git — see setup instructions below).
* **platformio.ini**: PlatformIO build configuration. Defines the target board NodeMCU v3 (LoLin), framework and library dependencies.

## Hardware
* **Board**: Adafruit Huzzah ESP8266
* **Display**: 8x32 LED matrix (4× MAX7219, FC16 hardware type)
* **Sensor**: DHT22 (temperature and humidity)
* **Buttons**: 2× physical push buttons (mode and power)

| Component    | GPIO | NodeMCU pin |
|--------------|------|-------------|
| Matrix CLK   | 14   | D5          |
| Matrix DATA  | 13   | D7          |
| Matrix CS    | 15   | D8          |
| DHT22        | 2    | D4          |
| Button Mode  | 0    | D3          |
| Button Power | 5    | D1          |

## Setup

### 1. WiFi credentials
Create the file `include/secrets.h` (already listed in `.gitignore`) with your credentials:
```cpp
#pragma once
#define WIFI_SSID1 "your_network_1"
#define WIFI_PASS1 "your_password_1"
#define WIFI_SSID2 "your_network_2"
#define WIFI_PASS2 "your_password_2"
```

### 2. Dependencies
All libraries are managed automatically by PlatformIO:
* MD_MAX72XX
* MD_Parola
* DHT sensor library (Adafruit)
* Adafruit Unified Sensor

### 3. Build and flash
1. Open the project in VS Code with the PlatformIO extension installed.
2. Connect the Huzzah via USB.
3. Click **Build** then **Upload** from the PlatformIO toolbar.
4. Open the Serial Monitor at 9600 baud to verify the WiFi sync and debug output.

## Serial Controls
While connected via USB, you can send commands through the Serial Monitor:
* **`1`** — Toggle between auto-cycle mode and time-only mode.
* **`2`** — Toggle the display on/off.