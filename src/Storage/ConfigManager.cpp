#include "ConfigManager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Konstruktor
ConfigManager::ConfigManager(Logger& log, const char* file)
    : logger(log), preferencesInitialized(false), configFile(file) {
    
    // Výchozí hodnoty
    resetToDefaults();
}

// Destruktor
ConfigManager::~ConfigManager() {
    // Ukončení Preferences, pokud byly inicializovány
    if (preferencesInitialized) {
        preferences.end();
    }
}

// Inicializace
bool ConfigManager::init() {
    // Inicializace Preferences
    preferencesInitialized = preferences.begin("sverio", false);
    
    if (!preferencesInitialized) {
        logger.error("Failed to initialize Preferences");
    } else {
        logger.debug("Preferences initialized successfully");
    }
    
    // Načtení konfigurace
    return load();
}

// Načtení konfigurace z obou zdrojů
bool ConfigManager::load() {
    bool fsSuccess = loadFromFS();
    bool prefSuccess = loadFromPreferences();
    
    if (fsSuccess) {
        logger.info("Configuration loaded from file system");
    }
    
    if (prefSuccess) {
        logger.info("Persistent configuration loaded from Preferences");
    }
    
    return fsSuccess || prefSuccess;
}

// Uložení konfigurace do obou zdrojů
bool ConfigManager::save() {
    bool fsSuccess = saveToFS();
    bool prefSuccess = saveToPreferences();
    
    if (fsSuccess) {
        logger.info("Configuration saved to file system");
    } else {
        logger.error("Failed to save configuration to file system");
    }
    
    if (prefSuccess) {
        logger.info("Persistent configuration saved to Preferences");
    } else {
        logger.error("Failed to save configuration to Preferences");
    }
    
    return fsSuccess && prefSuccess;
}

// Načítání z LittleFS
bool ConfigManager::loadFromFS() {
    // Check if file exists
    if (!LittleFS.exists(configFile)) {
        logger.warning("Config file not found: " + String(configFile));
        return false;
    }
    
    // Open file
    File file = LittleFS.open(configFile, "r");
    if (!file) {
        logger.error("Failed to open config file for reading: " + String(configFile));
        return false;
    }
    
    // Log the raw content for debugging
    String fileContent = file.readString();
    logger.debug("Raw config file content: " + fileContent);
    file.close();
    
    // Re-open the file for parsing
    file = LittleFS.open(configFile, "r");
    
    // Create JSON document
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        logger.error("Failed to parse config file: " + String(error.c_str()));
        return false;
    }
    
    // Load values and log them
    wifiSSID = doc["ssid"].as<String>();
    wifiPassword = doc["password"].as<String>();
    configMode = doc["configMode"] | true;
    timezone = doc["timezone"] | DEFAULT_TIMEZONE;

    // MQTT configuration
    mqttHost = doc["mqttHost"] | MQTT_DEFAULT_HOST;
    mqttPort = doc["mqttPort"] | MQTT_DEFAULT_PORT;
    mqttUser = doc["mqttUser"] | MQTT_DEFAULT_USER;
    mqttPassword = doc["mqttPassword"] | MQTT_DEFAULT_PASS;
    mqttEnabled = doc["mqttEnabled"] | MQTT_DEFAULT_ENABLED;
    
    logger.debug("Loaded config - SSID: " + wifiSSID + 
                ", Password length: " + String(wifiPassword.length()) + 
                ", configMode: " + String(configMode ? "true" : "false"));
    
    // Set log level if available
    if (doc.containsKey("logLevel")) {
        String logLevelStr = doc["logLevel"].as<String>();
        logLevel = logger.levelFromString(logLevelStr);
    }
    
    return true;
}

// Ukládání do LittleFS
bool ConfigManager::saveToFS() {
    // Otevření souboru pro zápis
    File file = LittleFS.open(configFile, "w");
    if (!file) {
        logger.error("Failed to open config file for writing: " + String(configFile));
        return false;
    }
    
    // Vytvoření JSON dokumentu
    DynamicJsonDocument doc(512);
    doc["ssid"] = wifiSSID;
    doc["password"] = wifiPassword;
    doc["configMode"] = configMode;
    doc["logLevel"] = logger.levelToString(logLevel);
    doc["timezone"] = timezone;
    doc["mqttHost"] = mqttHost;
    doc["mqttPort"] = mqttPort;
    doc["mqttUser"] = mqttUser;
    doc["mqttPassword"] = mqttPassword;
    doc["mqttEnabled"] = mqttEnabled;
    
    // Serializace do souboru
    if (serializeJson(doc, file) == 0) {
        logger.error("Failed to write config to file");
        file.close();
        return false;
    }
    
    file.close();
    return true;
}

// Načítání z Preferences
bool ConfigManager::loadFromPreferences() {
    if (!preferencesInitialized) {
        return false;
    }
    
    // Načtení základní konfigurace
    // Poznámka: Necháme Preferences načíst jen logLevel, 
    // který skutečně potřebujeme zachovat při aktualizaci firmwaru
    
    if (preferences.isKey("logLevel")) {
        String logLevelStr = preferences.getString("logLevel", "INFO");
        logLevel = logger.levelFromString(logLevelStr);
    }
    
    if (preferences.isKey("timezone")) {
        timezone = preferences.getString("timezone", DEFAULT_TIMEZONE);
    }
    
    // MQTT configuration from preferences
    if (preferences.isKey("mqttHost")) {
        mqttHost = preferences.getString("mqttHost", MQTT_DEFAULT_HOST);
    }
    if (preferences.isKey("mqttPort")) {
        mqttPort = preferences.getInt("mqttPort", MQTT_DEFAULT_PORT);
    }
    if (preferences.isKey("mqttUser")) {
        mqttUser = preferences.getString("mqttUser", MQTT_DEFAULT_USER);
    }
    if (preferences.isKey("mqttPassword")) {
        mqttPassword = preferences.getString("mqttPassword", MQTT_DEFAULT_PASS);
    }
    mqttEnabled = preferences.getBool("mqttEnabled", MQTT_DEFAULT_ENABLED);

    // Ostatní hodnoty by měly být načteny z LittleFS, ale pokud je potřeba,
    // můžeme je načíst z Preferences jako záložní řešení
    if (wifiSSID.length() == 0 && preferences.isKey("ssid")) {
        wifiSSID = preferences.getString("ssid", "");
        wifiPassword = preferences.getString("password", "");
        configMode = preferences.getBool("configMode", true);
    }
    
    return true;
}

// Ukládání do Preferences
bool ConfigManager::saveToPreferences() {
    if (!preferencesInitialized) {
        return false;
    }
    
    // Uložení úrovně logování
    String logLevelStr = logger.levelToString(logLevel);
    preferences.putString("logLevel", logLevelStr);
    preferences.putString("timezone", timezone);

    // Save MQTT configuration
    preferences.putString("mqttHost", mqttHost);
    preferences.putInt("mqttPort", mqttPort);
    preferences.putString("mqttUser", mqttUser);
    preferences.putString("mqttPassword", mqttPassword);
    preferences.putBool("mqttEnabled", mqttEnabled);
    
    // Uložení WiFi konfigurace (jako zálohu)
    preferences.putString("ssid", wifiSSID);
    preferences.putString("password", wifiPassword);
    preferences.putBool("configMode", configMode);
    
    return true;
}

// Add a new method to set MQTT configuration
bool ConfigManager::setMqttConfig(const String& host, int port, const String& user, 
                                 const String& password, bool enabled, bool saveConfig) {
    mqttHost = host;
    mqttPort = port;
    mqttUser = user;
    mqttPassword = password;
    mqttEnabled = enabled;
    
    return saveConfig ? save() : true;
}

// Add a new method to set the timezone
bool ConfigManager::setTimezone(const String& newTimezone, bool saveConfig) {
    timezone = newTimezone;
    
    // Configure the time with new timezone
    configTime(0, 0, NTP_SERVER); // First set to UTC
    setenv("TZ", timezone.c_str(), 1); // Set the TZ environment variable
    tzset(); // Apply the time zone
    
    if (saveConfig) {
        return save();
    }
    
    return true;
}

// Resetování konfigurace na výchozí hodnoty
void ConfigManager::resetToDefaults() {
   wifiSSID = "";
    wifiPassword = "";
    configMode = true;
    lastWifiAttempt = 0;
    logLevel = LogLevel::INFO;
    timezone = DEFAULT_TIMEZONE;
    mqttHost = MQTT_DEFAULT_HOST;
    mqttPort = MQTT_DEFAULT_PORT;
    mqttUser = MQTT_DEFAULT_USER;
    mqttPassword = MQTT_DEFAULT_PASS;
    mqttEnabled = MQTT_DEFAULT_ENABLED;
}

// Získání verze firmwaru
String ConfigManager::getFirmwareVersion() const {
    return FIRMWARE_VERSION;
}

// Získání MAC adresy zařízení
String ConfigManager::getMacAddress() const {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    return String(macStr);
}

// Získání názvu zařízení pro AP mód
String ConfigManager::getDeviceName() const {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    
    String deviceName = "expLORA-GW-";
    for (int i = 0; i < 6; i++) {
        if (mac[i] < 16) deviceName += "0";
        deviceName += String(mac[i], HEX);
    }
    
    String result = deviceName;
    result.toUpperCase();
    return result;
}

// Nastavení WiFi konfigurace
bool ConfigManager::setWiFiConfig(const String& ssid, const String& password, bool saveConfig) {
    wifiSSID = ssid;
    wifiPassword = password;
    configMode = false;
    
    if (saveConfig) {
        return save();
    }
    
    return true;
}

// Přepnutí do režimu konfigurace (AP)
void ConfigManager::enableConfigMode(bool enable, bool saveConfig) {
    configMode = enable;
    
    if (saveConfig) {
        save();
    }
}

// Nastavení úrovně logování
void ConfigManager::setLogLevel(LogLevel level, bool saveConfig) {
    logLevel = level;
    
    // Aktualizace úrovně v loggeru
    logger.setLogLevel(level);
    
    if (saveConfig) {
        save();
    }
}