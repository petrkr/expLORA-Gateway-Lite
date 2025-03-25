#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>

// Konfigurační hlavičky
#include "config.h"

// Data
#include "Data/SensorTypes.h"
#include "Data/SensorData.h"
#include "Data/Logging.h"
#include "Data/SensorManager.h"

// Hardware
#include "Hardware/LoRa_Module.h"
#include "Hardware/PSRAM_Manager.h"
#include "Hardware/SPI_Manager.h"

// Storage
#include "Storage/ConfigManager.h"

// Protokol
#include "Protocol/LoRaProtocol.h"

// Web rozhraní
#include "Web/WebServer.h"
#include "Web/HTMLGenerator.h"

// Globální instance objektů
Logger logger;                // Systém logování
ConfigManager* configManager; // Správce konfigurace
SPIManager* spiManager;       // Správce SPI komunikace
LoRaModule* loraModule;       // LoRa modul
SensorManager* sensorManager; // Správce senzorů
LoRaProtocol* loraProtocol;   // LoRa protokol
WebPortal* webPortal;         // Webové rozhraní

// Inicializace souborového systému
bool initFileSystem() {
    if (!LittleFS.begin(true)) {
        Serial.println("ERROR: Failed to mount LittleFS");
        return false;
    }
    Serial.println("LittleFS mounted successfully");
    return true;
}

// Setup - inicializace zařízení
void setup() {
    // Inicializace sériového portu
    Serial.begin(115200);
    delay(1000);  // Počkáme na stabilizaci
    
    Serial.println("\n\nSVERIO ESP32-S3 + RFM95W LoRa Gateway");
    Serial.println("------------------------------------------------------------");
    
    // Explicitní inicializace PSRAM
    #ifdef BOARD_HAS_PSRAM
    if (psramInit()) {
        Serial.println("PSRAM initialized manually!");
    }
    #endif
    
    // Diagnostika dostupné paměti
    Serial.printf("Total heap: %d bytes\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    
    // Inicializace PSRAM
    #ifdef BOARD_HAS_PSRAM
    bool psramInitialized = false;
    
    // Zkusíme různé metody detekce PSRAM
    if (esp_spiram_is_initialized()) {
        psramInitialized = true;
        Serial.println("PSRAM initialized via esp_spiram_is_initialized()");
    } 
    else if (ESP.getPsramSize() > 0) {
        psramInitialized = true;
        Serial.println("PSRAM detected via ESP.getPsramSize()");
    }
    
    if (psramInitialized) {
        Serial.printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
        Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    } else {
        Serial.println("PSRAM initialization failed or not available");
    }
    #endif
    
    // Inicializace souborového systému
    if (!initFileSystem()) {
        Serial.println("ERROR: File system initialization failed");
        return;
    }
    
    // Inicializace systému logování
    if (!logger.init(LOG_BUFFER_SIZE)) {
        Serial.println("ERROR: Logger initialization failed");
        return;
    }
    
    // Základní log po inicializaci
    logger.info("SVERIO Gateway starting up - Firmware v" + String(FIRMWARE_VERSION));

    // Inicializace HTML generátoru
    if (!HTMLGenerator::init(true, WEB_BUFFER_SIZE)) {
        logger.error("Failed to initialize HTML generator");
        return;
    }
    
    // Inicializace správce konfigurace
    configManager = new ConfigManager(logger);
    if (!configManager->init()) {
        logger.error("Failed to initialize configuration manager");
        return;
    }
    
    // Nastavení úrovně logování podle konfigurace
    logger.setLogLevel(configManager->logLevel);
    
    // Inicializace SPI správce
    spiManager = new SPIManager(logger);
    if (!spiManager->init()) {
        logger.error("Failed to initialize SPI manager");
        return;
    }
    
    // Inicializace správce senzorů
    sensorManager = new SensorManager(logger);
    if (!sensorManager->init()) {
        logger.error("Failed to initialize sensor manager");
    } else {
        logger.info("Sensor manager initialized with " + 
                    String(sensorManager->getSensorCount()) + " sensors");
    }
    
   // Initialization WiFi (based on configuration)
// In the setup() function of main.cpp
// Replace the existing WiFi initialization block with this:

    // Initialization WiFi (based on configuration)
    logger.info("Configuring WiFi. ConfigMode: " + String(configManager->configMode ? "true" : "false") + 
                ", SSID length: " + String(configManager->wifiSSID.length()));

    if (configManager->configMode || configManager->wifiSSID.length() == 0) {
        logger.info("Starting in AP mode");
        configManager->enableConfigMode(true);
    } else {
        logger.info("Attempting to connect to WiFi: " + configManager->wifiSSID);
        
        WiFi.mode(WIFI_STA);
        WiFi.begin(configManager->wifiSSID.c_str(), configManager->wifiPassword.c_str());
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            logger.info("WiFi connected! IP: " + WiFi.localIP().toString());
            configManager->enableConfigMode(false);
            
            // Initialize NTP
            configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
            logger.info("NTP time set");
            logger.setTimeInitialized(true);
        } else {
            logger.warning("Failed to connect to WiFi after " + String(attempts) + " attempts. SSID: " + 
                        configManager->wifiSSID + ", Starting in AP mode");
            configManager->enableConfigMode(true);
        }
    }
    
    // Inicializace LoRa modulu
    loraModule = new LoRaModule(logger, spiManager);
    if (!loraModule->init()) {
        logger.error("Failed to initialize LoRa module");
    } else {
        logger.info("LoRa module initialized successfully");
    }
    
    // Inicializace LoRa protokolu
    loraProtocol = new LoRaProtocol(*loraModule, *sensorManager, logger);
    
    // Inicializace webového rozhraní
    webPortal = new WebPortal(*sensorManager, logger, 
                             configManager->wifiSSID, configManager->wifiPassword, 
                             configManager->configMode);
    
    if (!webPortal->init()) {
        logger.error("Failed to initialize web portal");
    } else {
        logger.info("Web portal initialized successfully");
    }

    // Initialize task watchdog
    esp_task_wdt_init(WDT_TIMEOUT, true); // Enable panic on timeout
    esp_task_wdt_add(NULL); // Add current task to watchdog
    logger.info("Task watchdog initialized with timeout of " + String(WDT_TIMEOUT) + " seconds");
    
    logger.info("System initialization complete");
}

// Loop - hlavní smyčka
void loop() {

    esp_task_wdt_reset();


    // Zpracování LoRa paketů
    if (loraModule && loraProtocol) {
        if (LoRaModule::hasInterrupt()) {
            logger.debug("LoRa interrupt detected");
            loraProtocol->processReceivedPacket();
        }
    }
    
    // Pokud jsme v AP módu, zpracujeme DNS captive portal:
    if (webPortal && webPortal->isInAPMode()) {
        webPortal->processDNS();  // Tato metoda uvnitř volá dnsServer.processNextRequest();
    }

    // Obsluha webového rozhraní
    if (webPortal) {
        webPortal->handleClient();
    }
    
    // Kontrola WiFi připojení a případný reconnect
    if (!configManager->configMode && WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - configManager->lastWifiAttempt > WIFI_RECONNECT_INTERVAL) {
            logger.info("Attempting to reconnect to WiFi...");
            configManager->lastWifiAttempt = now;
            WiFi.begin(configManager->wifiSSID.c_str(), configManager->wifiPassword.c_str());
            
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 10) {
                delay(500);
                Serial.print(".");
                attempts++;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                logger.info("WiFi reconnected! IP: " + WiFi.localIP().toString());
                
                // Aktualizace NTP času
                configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
            } else {
                logger.warning("Failed to reconnect to WiFi");
            }
        }
    }
    
    // Krátké zpoždění pro stabilitu
    delay(5);
    
    // Diagnostika paměti každých 5 minut
    static unsigned long lastMemCheck = 0;
    if (millis() - lastMemCheck > 600000) {  // 10 minut
        lastMemCheck = millis();
        
        logger.info("Memory status - Free heap: " + String(ESP.getFreeHeap()) + 
                    " bytes, Largest block: " + String(ESP.getMaxAllocHeap()) + " bytes");
                    
        #ifdef BOARD_HAS_PSRAM
        if (esp_spiram_is_initialized()) {
            logger.debug("PSRAM status - Free: " + String(ESP.getFreePsram()) + 
                       " bytes, Largest block: " + String(ESP.getMaxAllocPsram()) + " bytes");
        }
        #endif
    }
}