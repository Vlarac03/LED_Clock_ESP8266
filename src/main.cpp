#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <MD_Parola.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <DHT.h>
#include "secrets.h"

// ============================================
// TIMEZONE CONFIGURATION (SPAIN)
// ============================================
// CET-1CEST: Central European Time +1, Daylight Saving +2
// M3.5.0: Switch to summer on last (5) Sunday (0) of March (3)
// M10.5.0: Switch to winter on last (5) Sunday (0) of October (10)
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"

// ============================================
// HARDWARE DEFINITIONS
// ============================================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN 14
#define DATA_PIN 13
#define CS_PIN 15

// DHT22
#define DHT_PIN 2       // GPIO2 - D4
#define DHT_TYPE DHT22

// PHYSICAL BUTTON DEFINITIONS
#define BOTON_MODE_PIN 0     // GPIO0 - D3
#define BOTON_POWER_PIN 5    // GPIO5 - D1

// ============================================
// INSTANCES
// ============================================
MD_Parola matrix = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
DHT dht(DHT_PIN, DHT_TYPE);

// ============================================
// WIFI CONFIGURATION - TWO NETWORKS
// ============================================
const char* ssid1     = WIFI_SSID1;
const char* password1 = WIFI_PASS1;
const char* ssid2     = WIFI_SSID2;
const char* password2 = WIFI_PASS2;

// ============================================
// FUNCTION PROTOTYPES
// ============================================
void syncTimeNTP();
void updateTimeVariables();
void leerSensor();
void checkPhysicalButtons();
void checkSerialButtons();
void updateDisplayIfNeeded();
void updateDisplay();
void displayTime();
void displayDay();
void displayDate();
void displayTemperatura();
void displayHumitat();
void animacioEstrellesCompleta();
void transicioLiniaVertical();
void ajustarIntensitatNocturna();

// ============================================
// TIME VARIABLES
// ============================================
uint8_t hours = 0;
uint8_t minutes = 0;
uint8_t seconds = 0;
uint8_t day = 1;
uint8_t month = 1;
uint16_t year = 2025;
uint8_t dayOfWeek = 1;

unsigned long lastSecond = 0;
bool dotsOn = true;
bool timeSynchronized = false;
bool systemPoweredOn = true;
bool wifiNeedSync = false; // Flag to trigger midnight re-sync

// ============================================
// DISPLAY MODE VARIABLES
// ============================================
uint8_t displayMode = 0;  // 0=BUCLE, 1=HORA
bool autoCycleModes = true;

// Duration of each mode (ms)
const unsigned long MODE_DURATION = 5000;

// Auto-cycle control
uint8_t currentCycleMode = 1;
unsigned long cycleStartTime = 0;

// ============================================
// SENSOR VARIABLES
// ============================================
float temperatura = 0.0;
float humitat = 0.0;

// ============================================
// CONTROL VARIABLES
// ============================================
uint8_t lastDisplayedMinutes = 255;
bool lastDotsState = true;
uint8_t lastDisplayMode = 255;
bool lastSystemState = true;

// ============================================
// DEBOUNCE VARIABLES
// ============================================
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
const unsigned long debounceDelay = 50;
int lastButtonState1 = HIGH;
int lastButtonState2 = HIGH;

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(9600);
    delay(200);
    
    Serial.println("Iniciant rellotge...");
    Serial.println("Controls per serial:");
    Serial.println(" - Prem '1' per canviar mode (BUCLE/HORA)");
    Serial.println(" - Prem '2' per ON/OFF del sistema");
    
    // Initialize DHT22
    dht.begin();
    
    // Initialize matrix
    matrix.begin();
    mx.begin();
    
    matrix.setZoneEffect(0, true, PA_FLIP_UD);
    matrix.setZoneEffect(0, true, PA_FLIP_LR);
    
    matrix.setIntensity(5);
    mx.control(MD_MAX72XX::INTENSITY, 5);
    matrix.displayClear();
    mx.clear();
    
    matrix.setPause(1000);
    matrix.setSpeed(30);
    
    // Connect to WiFi and sync time for the first time
    syncTimeNTP();
    
    // Read sensor on startup
    leerSensor();
    
    // Initialize time tracking variables
    lastSecond = millis();
    cycleStartTime = millis();
    
    // Start in CYCLE mode - TIME
    displayMode = 0;
    currentCycleMode = 1;
    autoCycleModes = true;

    // Configure button pins
    pinMode(BOTON_MODE_PIN, INPUT_PULLUP);
    pinMode(BOTON_POWER_PIN, INPUT_PULLUP);
    
    updateDisplay();
}

// ============================================
// NTP TIME SYNC
// ============================================
void syncTimeNTP() {
    Serial.println("\n--- Sincronitzant hora (NTP) ---");
    WiFi.mode(WIFI_STA);
    matrix.displayClear();
    matrix.setTextAlignment(PA_CENTER);
    matrix.print("WIFI");
    delay(800);
    
    // Try first network
    WiFi.begin(ssid1, password1);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 15) {
        delay(500);
        attempts++;
        
        matrix.displayClear();
        matrix.setTextAlignment(PA_CENTER);
        switch(attempts % 4) {
            case 0: matrix.print("W"); break;
            case 1: matrix.print("WI"); break;
            case 2: matrix.print("WIF"); break;
            case 3: matrix.print("WIFI"); break;
        }
    }
    
    // If not connected, try second network
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(ssid2, password2);
        attempts = 0;
        
        while (WiFi.status() != WL_CONNECTED && attempts < 15) {
            delay(500);
            attempts++;
            
            matrix.displayClear();
            matrix.setTextAlignment(PA_CENTER);
            switch(attempts % 4) {
                case 0: matrix.print("W"); break;
                case 1: matrix.print("WI"); break;
                case 2: matrix.print("WIF"); break;
                case 3: matrix.print("WIFI"); break;
            }
        }
    }
    
    // Check final result and configure time
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi OK. Obtenint hora...");
        matrix.displayClear();
        matrix.setTextAlignment(PA_CENTER);
        
        // Configure timezone and NTP servers
        configTime(MY_TZ, "pool.ntp.org", "time.google.com");
        
        // Wait for valid time data
        time_t now = time(nullptr);
        attempts = 0;
        while (now < 8 * 3600 * 2 && attempts < 10) {
            delay(500);
            now = time(nullptr);
            attempts++;
        }
        
        matrix.print("OK");
        delay(800);
        updateTimeVariables(); // Force immediate update of time variables
    } else {
        Serial.println("Error de WiFi.");
        matrix.displayClear();
        matrix.setTextAlignment(PA_CENTER);
        matrix.print("ERR");
        delay(1200);
    }
    
    // Disconnect WiFi to save power
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi apagat.");
}

// ============================================
// UPDATE TIME VARIABLES
// ============================================
void updateTimeVariables() {
    time_t now = time(nullptr);
    struct tm *ptm = localtime(&now);
    
    hours = ptm->tm_hour;
    minutes = ptm->tm_min;
    seconds = ptm->tm_sec;
    day = ptm->tm_mday;
    month = ptm->tm_mon;
    dayOfWeek = ptm->tm_wday;
    year = ptm->tm_year + 1900;
    
    // If exactly 00:00:00, activate the midnight re-sync flag
    if (hours == 0 && minutes == 0 && seconds == 0) {
        wifiNeedSync = true;
    }
}

// ============================================
// DHT22 SENSOR READING
// ============================================
void leerSensor() {
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    
    if (!isnan(temp)) {
        temperatura = temp;
    } else {
        temperatura = 0.0;
    }
    
    if (!isnan(hum)) {
        humitat = hum;
    } else {
        humitat = 0.0;
    }
}

// ============================================
// PHYSICAL BUTTON HANDLER
// ============================================
void checkPhysicalButtons() {
    static unsigned long lastPress1 = 0;
    static unsigned long lastPress2 = 0;
    
    int currentState1 = digitalRead(BOTON_MODE_PIN);
    int currentState2 = digitalRead(BOTON_POWER_PIN);
    
    // Mode button
    if (currentState1 == LOW && (millis() - lastPress1 > 500)) {
        Serial.println("BOTO MODE ACTIVAT!");
        lastPress1 = millis();
        
        if (systemPoweredOn) {
            transicioLiniaVertical();
            if (displayMode == 0) {
                displayMode = 1;
                autoCycleModes = false;
                Serial.println("Mode: HORA SOLA");
            } else {
                displayMode = 0;
                autoCycleModes = true;
                currentCycleMode = 1;
                cycleStartTime = millis();
                Serial.println("Mode: BUCLE AUTOMATIC");
            }
            updateDisplay();
        }
    }
    
    // Power button
    if (currentState2 == LOW && (millis() - lastPress2 > 500)) {
        Serial.println("BOTO POWER ACTIVAT!");
        lastPress2 = millis();
        
        if (systemPoweredOn) {
            animacioEstrellesCompleta();
            systemPoweredOn = false;
            matrix.displayClear();
            mx.clear();
            Serial.println("Sistema: APAGAT");
        } else {
            animacioEstrellesCompleta();
            systemPoweredOn = true;
            displayMode = 0;
            autoCycleModes = true;
            currentCycleMode = 1;
            cycleStartTime = millis();
            transicioLiniaVertical();
            updateDisplay();
            Serial.println("Sistema: ENCES (Mode BUCLE)");
        }
    }
}

// ============================================
// STAR ANIMATION
// ============================================
void animacioEstrellesCompleta() {
    byte estrelles[][8] = {
        {0x00, 0x42, 0x00, 0x18, 0x18, 0x00, 0x42, 0x00},
        {0x81, 0x00, 0x18, 0x24, 0x24, 0x18, 0x00, 0x81},
        {0x00, 0x24, 0x42, 0x00, 0x00, 0x42, 0x24, 0x00}
    };
    
    for(int frame = 0; frame < 3; frame++) {
        for(int dev = 0; dev < MAX_DEVICES; dev++) {
            for(int row = 0; row < 8; row++) {
                mx.setRow(dev, row, estrelles[frame][row]);
            }
        }
        delay(400);
    }
}

// ============================================
// VERTICAL LINE TRANSITION (RIGHT TO LEFT)
// ============================================
void transicioLiniaVertical() {
    for(int step = 0; step < 32; step++) {
        mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
        mx.clear();
        
        for(int dev = 0; dev < MAX_DEVICES; dev++) {
            int columnaGlobal = dev * 8;
            for(int col = 0; col < 8; col++) {
                if(columnaGlobal + col == (31 - step)) {
                    mx.setColumn(dev, col, 0xFF);
                }
            }
        }
        
        mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
        delay(15);
    }
    
    mx.clear();
}

// ============================================
// NIGHT BRIGHTNESS ADJUSTMENT
// ============================================
void ajustarIntensitatNocturna() {
    if((hours >= 22 || hours < 8)) {
        matrix.setIntensity(2);
        mx.control(MD_MAX72XX::INTENSITY, 2);
    } else {
        matrix.setIntensity(5);
        mx.control(MD_MAX72XX::INTENSITY, 5);
    }
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    // Read serial and physical buttons
    checkSerialButtons();
    checkPhysicalButtons();
    
    // If system is off, do nothing
    if (!systemPoweredOn) {
        delay(100);
        return;
    }
    
    // Adjust brightness based on time (NIGHT MODE)
    ajustarIntensitatNocturna();
    
    // Read sensor every 1 minute
    static unsigned long lastSensorRead = 0;
    if (millis() - lastSensorRead > 60000UL) {
        lastSensorRead = millis();
        leerSensor();
    }
    
    // Animate display (required for day-of-week scroll)
    matrix.displayAnimate();
    
    // ============================================
    // UPDATE EVERY SECOND
    // ============================================
    if (millis() - lastSecond >= 1000UL) {
        lastSecond = millis();
        
        // Update time variables from ESP internal clock
        updateTimeVariables();
        
        // If midnight, re-sync time via WiFi
        if (wifiNeedSync) {
            syncTimeNTP();
            wifiNeedSync = false; // Reset flag
        }
        
        // Toggle blinking dots
        dotsOn = !dotsOn;
        
        // ============================================
        // AUTO CYCLE (MODE 0)
        // ============================================
        if (autoCycleModes && displayMode == 0) {
            unsigned long duration = MODE_DURATION;
    
            switch(currentCycleMode) {
                case 1: duration = 8000; break;  // TIME
                case 2: duration = 3000; break;  // DAY
                case 3: duration = 3000; break;  // DATE
                case 4: duration = 3000; break;  // TEMPERATURE
                case 5: duration = 3000; break;  // HUMIDITY
            }
    
            if (millis() - cycleStartTime > duration) {
                currentCycleMode++;
        
                if (currentCycleMode > 5) {
                    currentCycleMode = 1;
                }
        
                cycleStartTime = millis();
                updateDisplay();
            }
        }
        
        // Update display if needed
        updateDisplayIfNeeded();
    }
}

// ============================================
// SERIAL BUTTON HANDLER
// ============================================
void checkSerialButtons() {
    if (Serial.available() > 0) {
        char command = Serial.read();
        
        // BUTTON 1 - TOGGLE MODE (CYCLE/TIME ONLY)
        if (command == '1') {
            if (systemPoweredOn) {
                transicioLiniaVertical();
                
                if (displayMode == 0) {
                    displayMode = 1;
                    autoCycleModes = false;
                    Serial.println("Mode: HORA SOLA");
                } else {
                    displayMode = 0;
                    autoCycleModes = true;
                    currentCycleMode = 1;
                    cycleStartTime = millis();
                    Serial.println("Mode: BUCLE AUTOMATIC");
                }
                
                updateDisplay();
            }
        }
        
        // BUTTON 2 - SYSTEM ON/OFF
        if (command == '2') {
            if (systemPoweredOn) {
                animacioEstrellesCompleta();
                systemPoweredOn = false;
                matrix.displayClear();
                mx.clear();
                Serial.println("Sistema: APAGAT");
            } else {
                animacioEstrellesCompleta();
                systemPoweredOn = true;
                displayMode = 0;
                autoCycleModes = true;
                currentCycleMode = 1;
                cycleStartTime = millis();
                transicioLiniaVertical();
                updateDisplay();
                Serial.println("Sistema: ENCES (Mode BUCLE)");
            }
        }
    }
}

// ============================================
// UPDATE DISPLAY IF NEEDED
// ============================================
void updateDisplayIfNeeded() {
    bool needsUpdate = false;
    
    if (displayMode != lastDisplayMode || systemPoweredOn != lastSystemState) {
        needsUpdate = true;
        lastDisplayMode = displayMode;
        lastSystemState = systemPoweredOn;
    }
    
    if ((displayMode == 0 && currentCycleMode == 1) || displayMode == 1) {
        if (minutes != lastDisplayedMinutes || dotsOn != lastDotsState) {
            needsUpdate = true;
            lastDisplayedMinutes = minutes;
            lastDotsState = dotsOn;
        }
    }
    
    if (needsUpdate && systemPoweredOn) {
        updateDisplay();
    }
}

// ============================================
// UPDATE DISPLAY
// ============================================
void updateDisplay() {
    if (!systemPoweredOn) return;
    
    matrix.displayClear();
    mx.clear();
    
    uint8_t modeToShow = displayMode;
    if (displayMode == 0) {
        modeToShow = currentCycleMode;
    }
    
    switch(modeToShow) {
        case 1: displayTime(); break;
        case 2: displayDay(); break;
        case 3: displayDate(); break;
        case 4: displayTemperatura(); break;
        case 5: displayHumitat(); break;
        default: displayTime(); break;
    }
    
    lastDisplayMode = displayMode;
}

// ============================================
// DISPLAY TIME
// ============================================
void displayTime() {
    char timeStr[12];
    sprintf(timeStr, dotsOn ? "%02d:%02d" : "%02d %02d", hours, minutes);
    matrix.setTextAlignment(PA_CENTER);
    matrix.print(timeStr);
}

// ============================================
// DISPLAY DAY OF WEEK (SCROLL)
// ============================================
void displayDay() {
    const char* days[] = {
        "DIUMENGE", "DILLUNS", "DIMARTS", "DIMECRES", 
        "DIJOUS", "DIVENDRES", "DISSABTE"
    };
    const char* text = days[dayOfWeek % 7];
    
    matrix.displayText(text, PA_LEFT, 45, 0, PA_SCROLL_RIGHT, PA_SCROLL_RIGHT);
}

// ============================================
// DISPLAY DATE (STATIC)
// ============================================
void displayDate() {
    char dateStr[12];
    sprintf(dateStr, "%02d.%02d", day, month + 1);
    matrix.setTextAlignment(PA_CENTER);
    matrix.print(dateStr);
}

// ============================================
// DISPLAY TEMPERATURE
// ============================================
void displayTemperatura() {
    char tempStr[12];
    sprintf(tempStr, "%.1fC", temperatura);
    matrix.setTextAlignment(PA_CENTER);
    matrix.print(tempStr);
}

// ============================================
// DISPLAY HUMIDITY
// ============================================
void displayHumitat() {
    char humStr[12];
    sprintf(humStr, "%.0f%%", humitat);
    matrix.setTextAlignment(PA_CENTER);
    matrix.print(humStr);
}