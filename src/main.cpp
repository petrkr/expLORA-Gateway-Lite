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
// MQTT
#include "Protocol/MQTTManager.h"

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
MQTTManager* mqttManager;     // MQTT Manager
WebPortal* webPortal;         // Webové rozhraní

// Časovač pro vypnutí AP režimu
unsigned long apStartTime = 0;
bool temporaryAPMode = false;

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
    
    Serial.println("\n\nexpLORA Gateway Lite");
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
    logger.info("expLORA Gateway Lite starting up - Firmware v" + String(FIRMWARE_VERSION));

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
    
    // Initialization WiFi - UPRAVENÁ ČÁST KÓDU
    logger.info("Configuring WiFi. ConfigMode: " + String(configManager->configMode ? "true" : "false") + 
                ", SSID length: " + String(configManager->wifiSSID.length()));

    // Nastavení časovače pro dočasný AP režim
    apStartTime = millis();
    temporaryAPMode = true;
    
    if (configManager->configMode || configManager->wifiSSID.length() == 0) {
        // Jsme v konfiguračním režimu nebo nemáme credentials - pouze AP režim
        logger.info("Starting in AP mode only");
        WiFi.mode(WIFI_AP);
        
        // Získáme MAC adresu zařízení a vytvoříme unikátní SSID
        String macAddress = WiFi.macAddress();
        macAddress.replace(":", ""); // Odstranění dvojteček
        String uniqueSSID = "expLORA-GW-" + macAddress.substring(6); // Použijeme posledních 6 znaků MAC adresy
        
        // Konfigurace AP s unikátním SSID
        WiFi.softAP(uniqueSSID.c_str());
        logger.info("AP started with SSID: " + uniqueSSID + ", IP: " + WiFi.softAPIP().toString());
        
        configManager->enableConfigMode(true);
        webPortal = new WebPortal(*sensorManager, logger,
            configManager->wifiSSID, configManager->wifiPassword,
            configManager->configMode, *configManager, configManager->timezone);
    } else {
        // Máme credentials - spustíme AP+STA režim
        logger.info("Starting in AP+STA mode (dual mode)");
        WiFi.mode(WIFI_AP_STA);
        
        // Získáme MAC adresu zařízení a vytvoříme unikátní SSID
        String macAddress = WiFi.macAddress();
        macAddress.replace(":", ""); // Odstranění dvojteček
        String uniqueSSID = "expLORA-GW-" + macAddress.substring(6); // Použijeme posledních 6 znaků MAC adresy
        
        // Konfigurace AP části s unikátním SSID
        WiFi.softAP(uniqueSSID.c_str());
        logger.info("Temporary AP started with SSID: " + uniqueSSID + 
                   " (will be active for 5 minutes). IP: " + WiFi.softAPIP().toString());
        
        // Konfigurace STA části (klient)
        logger.info("Attempting to connect to WiFi: " + configManager->wifiSSID);
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
            configTime(0, 0, NTP_SERVER); // First set to UTC
            setenv("TZ", configManager->timezone.c_str(), 1); // Set the TZ environment variable
            tzset(); // Apply the time zone
            logger.info("NTP time set");
            logger.setTimeInitialized(true);
        } else {
            logger.warning("Failed to connect to WiFi after " + String(attempts) + " attempts. SSID: " + 
                        configManager->wifiSSID + ", Continuing in AP mode only");
            // Přepneme se pouze na AP režim
            WiFi.mode(WIFI_AP);
        }
        
        // Inicializace webového rozhraní - bude dostupné přes AP i klienta (pokud je připojen)
        webPortal = new WebPortal(*sensorManager, logger, 
            configManager->wifiSSID, configManager->wifiPassword,
            configManager->configMode, *configManager, configManager->timezone);
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
    
    // Inicializace webového portálu, pokud ještě není inicializován
    if (!webPortal) { 
        webPortal = new WebPortal(*sensorManager, logger, configManager->wifiSSID, configManager->wifiPassword, configManager->configMode, *configManager,
        configManager->timezone);
    }

    // Initialize MQTT Manager if WiFi is connected
    mqttManager = new MQTTManager(*sensorManager, *configManager, logger);
    if (!mqttManager->init()) {
        logger.debug("MQTT Manager initialization skipped (disabled in config)");
    }

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

    // Kontrola časovače pro dočasný AP režim
    if (temporaryAPMode && !configManager->configMode && configManager->wifiSSID.length() > 0) {
        if (millis() - apStartTime > AP_TIMEOUT) {
            // Čas vypršel, přepneme do klientského režimu, pokud jsme úspěšně připojeni
            if (WiFi.status() == WL_CONNECTED) {
                logger.info("Temporary AP timeout reached. Switching to client mode only.");
                WiFi.mode(WIFI_STA);
                temporaryAPMode = false;
            } else {
                // Nejsme připojeni jako klient, necháme AP běžet
                logger.info("Temporary AP timeout reached but WiFi client not connected. Keeping AP mode active.");
                temporaryAPMode = false; // Zastavíme časovač, ale AP zůstane aktivní
            }
        }
    }

    // Zpracování LoRa paketů
    //if (loraModule && loraProtocol) {
    //    if (LoRaModule::hasInterrupt()) {
    //        logger.debug("LoRa interrupt detected");
    //        loraProtocol->processReceivedPacket();
    //    }
    //}
    
    // Pokud jsme v AP módu, zpracujeme DNS captive portal:
    if (webPortal && webPortal->isInAPMode()) {
        webPortal->processDNS();  // Tato metoda uvnitř volá dnsServer.processNextRequest();
    }

    // Obsluha webového rozhraní
    if (webPortal) {
        webPortal->handleClient();
    }
    
    // Process MQTT communication
    if (mqttManager && WiFi.status() == WL_CONNECTED) {
        mqttManager->process();
    }
    
    // Add a check for LoRa packet processing to publish to MQTT
    static unsigned int lastProcessedIndex = -1;
    if (loraModule && loraProtocol && LoRaModule::hasInterrupt()) {
        if (loraProtocol->processReceivedPacket()) {
            // If we have a mqttManager and it's connected, publish the latest sensor data
            if (mqttManager && WiFi.status() == WL_CONNECTED) {
                mqttManager->publishSensorData(loraProtocol->getLastProcessedSensorIndex());
            }
        }
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
                configTime(0, 0, NTP_SERVER); // First set to UTC
                setenv("TZ", configManager->timezone.c_str(), 1); // Set the TZ environment variable
                tzset(); // Apply the time zone
            } else {
                logger.warning("Failed to reconnect to WiFi");
            }
        }
    }
    
    // Krátké zpoždění pro stabilitu
    delay(5);
    
    // Diagnostika paměti každých 10 minut
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

