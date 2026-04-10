#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <MD_Parola.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <DHT.h>
#include "secrets.h"

// ============================================
// CONFIGURACIÓ HORÀRIA (ESPANYA)
// ============================================
// CET-1CEST: Central European Time +1, Daylight Saving +2
// M3.5.0: Canvi a l'estiu l'últim (5) diumenge (0) de març (3)
// M10.5.0: Canvi a l'hivern l'últim (5) diumenge (0) d'octubre (10)
#define MY_TZ "CET-1CEST,M3.5.0,M10.5.0/3"

// ============================================
// DEFINICIONS DE HARDWARE
// ============================================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN 14
#define DATA_PIN 13
#define CS_PIN 15

// DHT22
#define DHT_PIN 2       // GPIO2 - D4
#define DHT_TYPE DHT22

// DEFINICIONS BOTONS FÍSICS
#define BOTON_MODE_PIN 0     // GPIO0 - D3
#define BOTON_POWER_PIN 5    // GPIO5 - D1

// ============================================
// INSTÀNCIES
// ============================================
MD_Parola matrix = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
DHT dht(DHT_PIN, DHT_TYPE);

// ============================================
// CONFIGURACIÓ WIFI - DUES XARXES
// ============================================
const char* ssid1     = WIFI_SSID1;
const char* password1 = WIFI_PASS1;
const char* ssid2     = WIFI_SSID2;
const char* password2 = WIFI_PASS2;

// ============================================
// PROTOTIPS
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
// VARIABLES DE TEMPS
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
bool wifiNeedSync = false; // Flag per sincronitzar a mitjanit

// ============================================
// VARIABLES DE MODOS
// ============================================
uint8_t displayMode = 0;  // 0=BUCLE, 1=HORA
bool autoCycleModes = true;

// Duracions de cada modo (ms)
const unsigned long MODE_DURATION = 5000;

// Control del cicle automàtic
uint8_t currentCycleMode = 1;
unsigned long cycleStartTime = 0;

// ============================================
// VARIABLES SENSORS
// ============================================
float temperatura = 0.0;
float humitat = 0.0;

// ============================================
// VARIABLES DE CONTROL
// ============================================
uint8_t lastDisplayedMinutes = 255;
bool lastDotsState = true;
uint8_t lastDisplayMode = 255;
bool lastSystemState = true;

// ============================================
// Variables per debounce
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
    
    // Inicialitzar DHT22
    dht.begin();
    
    // Inicialitzar matriu
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
    
    // Connexió WiFi i sincronització de temps per primer cop
    syncTimeNTP();
    
    // Llegir sensor al inici
    leerSensor();
    
    // Inicialitzar variables de temps
    lastSecond = millis();
    cycleStartTime = millis();
    
    // Comenzar en modo BUCLE - HORA
    displayMode = 0;
    currentCycleMode = 1;
    autoCycleModes = true;

    // Configurar pins dels botons
    pinMode(BOTON_MODE_PIN, INPUT_PULLUP);
    pinMode(BOTON_POWER_PIN, INPUT_PULLUP);
    
    updateDisplay();
}

// ============================================
// SINCRONITZACIÓ DE TEMPS NTP I WIFI
// ============================================
void syncTimeNTP() {
    Serial.println("\n--- Sincronitzant hora (NTP) ---");
    WiFi.mode(WIFI_STA);
    matrix.displayClear();
    matrix.setTextAlignment(PA_CENTER);
    matrix.print("WIFI");
    delay(800);
    
    // Intentar primera xarxa
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
    
    // Si no es connecta, intentar segona xarxa
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
    
    // Comprovar resultat final i configurar hora
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi OK. Obtenint hora...");
        matrix.displayClear();
        matrix.setTextAlignment(PA_CENTER);
        
        // Configurar la zona horària i servidors NTP
        configTime(MY_TZ, "pool.ntp.org", "time.google.com");
        
        // Esperem que arribi la dada vàlida del temps
        time_t now = time(nullptr);
        attempts = 0;
        while (now < 8 * 3600 * 2 && attempts < 10) {
            delay(500);
            now = time(nullptr);
            attempts++;
        }
        
        matrix.print("OK");
        delay(800);
        updateTimeVariables(); // Forçar una actualització immediata de les variables
    } else {
        Serial.println("Error de WiFi.");
        matrix.displayClear();
        matrix.setTextAlignment(PA_CENTER);
        matrix.print("ERR");
        delay(1200);
    }
    
    // Desconectar WiFi per estalviar consum
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi apagat.");
}

// ============================================
// ACTUALITZAR VARIABLES DE TEMPS
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
    
    // Si són exactament les 00:00:00 i encara no hem activat el flag, l'activem
    if (hours == 0 && minutes == 0 && seconds == 0) {
        wifiNeedSync = true;
    }
}

// ============================================
// LECTURA SENSOR DHT22
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
// Funció per llegir botons físics
// ============================================
void checkPhysicalButtons() {
    static unsigned long lastPress1 = 0;
    static unsigned long lastPress2 = 0;
    
    int currentState1 = digitalRead(BOTON_MODE_PIN);
    int currentState2 = digitalRead(BOTON_POWER_PIN);
    
    // Botó Mode - sense debounce complex
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
    
    // Botó Power - sense debounce complex  
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
// ANIMACIÓ ESTRELLES COMPLETA
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
// TRANSICIÓ LÍNIA VERTICAL (DRETA → ESQUERRA)
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
// AJUSTAR INTENSITAT NOCTURNA
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
// LOOP PRINCIPAL
// ============================================
void loop() {
    // Lectura de botons per serial
    checkSerialButtons();
    checkPhysicalButtons();
    
    // Si el sistema està apagat
    if (!systemPoweredOn) {
        delay(100);
        return;
    }
    
    // Ajustar intensitat segons hora (MODE NIT)
    ajustarIntensitatNocturna();
    
    // Lectura sensor cada 1 minut
    static unsigned long lastSensorRead = 0;
    if (millis() - lastSensorRead > 60000UL) {
        lastSensorRead = millis();
        leerSensor();
    }
    
    // Animar display (important per al scroll del dia)
    matrix.displayAnimate();
    
    // ============================================
    // ACTUALITZACIÓ CADA SEGON
    // ============================================
    if (millis() - lastSecond >= 1000UL) {
        lastSecond = millis();
        
        // Actualitzem les variables d'hora internament des de l'ESP
        updateTimeVariables();
        
        // Si és mitjanit (00:00:00), ens connectem al WiFi per sincronitzar
        if (wifiNeedSync) {
            syncTimeNTP();
            wifiNeedSync = false; // Reset del flag per no fer-ho diverses vegades
        }
        
        // Parpadeo dels punts
        dotsOn = !dotsOn;
        
        // ============================================
        // BUCLE AUTOMÁTICO (MODO 0)
        // ============================================
        if (autoCycleModes && displayMode == 0) {
            unsigned long duration = MODE_DURATION;
    
            switch(currentCycleMode) {
                case 1: duration = 8000; break;  // HORA 
                case 2: duration = 3000; break;  // DIA 
                case 3: duration = 3000; break;  // DATA
                case 4: duration = 3000; break;  // TEMPERATURA
                case 5: duration = 3000; break;  // HUMITAT
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
        
        // Actualitzar pantalla si és necessari
        updateDisplayIfNeeded();
    }
}

// ============================================
// LECTURA DE BOTONS PER SERIAL
// ============================================
void checkSerialButtons() {
    if (Serial.available() > 0) {
        char command = Serial.read();
        
        // BOTÓ 1 - CANVI DE MODE (BUCLE/HORA)
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
        
        // BOTÓ 2 - ON/OFF DEL SISTEMA
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
// ACTUALITZAR PANTALLA SI NECESSARI
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
// ACTUALITZAR PANTALLA
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
// MOSTRA HORA
// ============================================
void displayTime() {
    char timeStr[12];
    sprintf(timeStr, dotsOn ? "%02d:%02d" : "%02d %02d", hours, minutes);
    matrix.setTextAlignment(PA_CENTER);
    matrix.print(timeStr);
}

// ============================================
// MOSTRA DIA DE LA SETMANA (SCROLL)
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
// MOSTRA DATA (ESTÀTICA)
// ============================================
void displayDate() {
    char dateStr[12];
    sprintf(dateStr, "%02d.%02d", day, month + 1);
    matrix.setTextAlignment(PA_CENTER);
    matrix.print(dateStr);
}

// ============================================
// MOSTRA TEMPERATURA
// ============================================
void displayTemperatura() {
    char tempStr[12];
    sprintf(tempStr, "%.1fC", temperatura);
    matrix.setTextAlignment(PA_CENTER);
    matrix.print(tempStr);
}

// ============================================
// MOSTRA HUMITAT
// ============================================
void displayHumitat() {
    char humStr[12];
    sprintf(humStr, "%.0f%%", humitat);
    matrix.setTextAlignment(PA_CENTER);
    matrix.print(humStr);
}