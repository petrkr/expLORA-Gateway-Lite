#include "SensorManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <HTTPClient.h>

// Konstruktor
SensorManager::SensorManager(Logger& log, const char* file)
    : sensorCount(0), logger(log), sensorsFile(file) {
}

// Destruktor
SensorManager::~SensorManager() {
    // Nic speciálního není potřeba uvolňovat, všechny objekty se uvolní samy
}

// Inicializace správce senzorů
bool SensorManager::init() {
    logger.info("Initializing sensor manager");
    return loadSensors();
}

// Přidání nového senzoru
int SensorManager::addSensor(SensorType deviceType, uint32_t serialNumber, uint32_t deviceKey, const String& name) {
    std::lock_guard<std::mutex> lock(sensorMutex);
    
    // Kontrola, zda senzor již existuje
    int existingIndex = findSensorBySN(serialNumber);
    if (existingIndex >= 0) {
        // Aktualizace existujícího senzoru
        sensors[existingIndex].deviceType = deviceType;
        sensors[existingIndex].deviceKey = deviceKey;
        sensors[existingIndex].name = name;
        sensors[existingIndex].configured = true;
        
        logger.info("Updated existing sensor: " + name + " (SN: " + String(serialNumber, HEX) + ")");
        saveSensors(false);
        return existingIndex;
    }
    
    // Kontrola, zda máme ještě místo
    if (sensorCount >= MAX_SENSORS) {
        logger.error("Failed to add sensor: maximum number of sensors reached");
        return -1;
    }
    
    // Najít první volné místo
    int newIndex = -1;
    for (size_t i = 0; i < MAX_SENSORS; i++) {
        if (!sensors[i].configured) {
            newIndex = i;
            break;
        }
    }
    
    // Pokud není volné místo, použijeme poslední index
    if (newIndex == -1) {
        newIndex = sensorCount;
        sensorCount++;
    } else if (newIndex >= sensorCount) {
        sensorCount = newIndex + 1;
    }
    
    // Inicializace nového senzoru
    sensors[newIndex].deviceType = deviceType;
    sensors[newIndex].serialNumber = serialNumber;
    sensors[newIndex].deviceKey = deviceKey;
    sensors[newIndex].name = name;
    sensors[newIndex].endpoint = "";
    sensors[newIndex].customParams = "";
    sensors[newIndex].lastSeen = 0;
    sensors[newIndex].temperature = 0.0f;
    sensors[newIndex].humidity = 0.0f;
    sensors[newIndex].pressure = 0.0f;
    sensors[newIndex].ppm = 0.0f;
    sensors[newIndex].lux = 0.0f;
    sensors[newIndex].batteryVoltage = 0.0f;
    sensors[newIndex].rssi = 0;
    sensors[newIndex].configured = true;
    
    logger.info("Added new sensor: " + name + " (SN: " + String(serialNumber, HEX) + ")");
    saveSensors(false);
    return newIndex;
}

// Nalezení senzoru podle sériového čísla
int SensorManager::findSensorBySN(uint32_t serialNumber) {
    for (size_t i = 0; i < sensorCount; i++) {
        if (sensors[i].configured && sensors[i].serialNumber == serialNumber) {
            return i;
        }
    }
    return -1;
}

// Aktualizace dat senzoru
bool SensorManager::updateSensor(int index, const SensorData& data) {
    std::lock_guard<std::mutex> lock(sensorMutex);
    
    if (index < 0 || index >= sensorCount || !sensors[index].configured) {
        logger.warning("Attempt to update non-existent sensor at index " + String(index));
        return false;
    }
    
    // Aktualizace dat při zachování konfigurace
    sensors[index].deviceType = data.deviceType;
    sensors[index].temperature = data.temperature;
    sensors[index].humidity = data.humidity;
    sensors[index].pressure = data.pressure;
    sensors[index].ppm = data.ppm;
    sensors[index].lux = data.lux;
    sensors[index].batteryVoltage = data.batteryVoltage;
    sensors[index].rssi = data.rssi;
    sensors[index].lastSeen = millis();
    
    return true;
}

// Aktualizace dat senzoru podle typu
bool SensorManager::updateSensorData(int index, float temperature, float humidity, float pressure, 
                                    float ppm, float lux, float batteryVoltage, int rssi) {
    std::lock_guard<std::mutex> lock(sensorMutex);
    
    if (index < 0 || index >= sensorCount || !sensors[index].configured) {
        logger.warning("Attempt to update non-existent sensor at index " + String(index));
        return false;
    }
    
    // Aktualizujeme pouze relevantní hodnoty podle typu senzoru
    if (sensors[index].hasTemperature()) {
        sensors[index].temperature = temperature;
    }
    
    if (sensors[index].hasHumidity()) {
        sensors[index].humidity = humidity;
    }
    
    if (sensors[index].hasPressure()) {
        sensors[index].pressure = pressure;
    }
    
    if (sensors[index].hasPPM()) {
        sensors[index].ppm = ppm;
    }
    
    if (sensors[index].hasLux()) {
        sensors[index].lux = lux;
    }
    
    // Obecná data aktualizujeme vždy
    sensors[index].batteryVoltage = batteryVoltage;
    sensors[index].rssi = rssi;
    sensors[index].lastSeen = millis();

    if (WiFi.status() == WL_CONNECTED) {
        forwardSensorData(index);
    } else {
        logger.debug("Not forwarding data - WiFi not connected");
    }
    
    return true;
}

bool SensorManager::forwardSensorData(int index) {
    if (index < 0 || index >= sensorCount || !sensors[index].configured) {
        logger.warning("Attempt to forward data for non-existent sensor at index " + String(index));
        return false;
    }
    
    // Check if the sensor has a custom endpoint configured
    if (sensors[index].endpoint.length() == 0) {
        // No endpoint configured, nothing to do
        return true;
    }
        
    // Create a WiFi client for HTTP request
    WiFiClient client;
    HTTPClient http;
    
    // Format URL with custom parameters if available
    String url = sensors[index].endpoint;
    
    // If there are custom parameters, process them
    if (sensors[index].customParams.length() > 0) {
        // Add ? or & depending on whether the URL already has parameters
        if (url.indexOf('?') == -1) {
            url += "?";
        } else if (url.charAt(url.length() - 1) != '&' && url.charAt(url.length() - 1) != '?') {
            url += "&";
        }
        
        // Process the custom parameters template
        String params = sensors[index].customParams;
        
        // Replace placeholders with actual values
        if (sensors[index].hasTemperature()) {
            params.replace("*TEMP*", String(sensors[index].temperature, 2));
        }
        if (sensors[index].hasHumidity()) {
            params.replace("*HUM*", String(sensors[index].humidity, 2));
        }
        if (sensors[index].hasPressure()) {
            params.replace("*PRESS*", String(sensors[index].pressure, 2));
        }
        if (sensors[index].hasPPM()) {
            params.replace("*PPM*", String(sensors[index].ppm, 0));
        }
        if (sensors[index].hasLux()) {
            params.replace("*LUX*", String(sensors[index].lux, 1));
        }
        
        // Replace other common placeholders
        params.replace("*BAT*", String(sensors[index].batteryVoltage, 2));
        params.replace("*RSSI*", String(sensors[index].rssi));
        params.replace("*SN*", String(sensors[index].serialNumber, HEX));
        params.replace("*TYPE*", String(static_cast<uint8_t>(sensors[index].deviceType)));
        
        url += params;
    }
    
    logger.debug("Forwarding data for sensor " + sensors[index].name + " to: " + url);

    // Begin HTTP request
    http.begin(client, url);
    
    // Send the request
    int httpCode = http.GET();
    
    // Check result
    if (httpCode > 0) {
        logger.debug("HTTP request sent, response code: " + String(httpCode));
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            logger.debug("Response: " + payload);
        }
    } else {
        logger.warning("HTTP request failed: " + http.errorToString(httpCode));
    }
    
    http.end();
    
    return (httpCode == HTTP_CODE_OK);
}

// Aktualizace konfigurace senzoru
bool SensorManager::updateSensorConfig(int index, const String& name, SensorType deviceType, 
                                      uint32_t serialNumber, uint32_t deviceKey, 
                                      const String& endpoint, const String& customParams) {
    std::lock_guard<std::mutex> lock(sensorMutex);
    
    if (index < 0 || index >= sensorCount || !sensors[index].configured) {
        logger.warning("Attempt to update non-existent sensor at index " + String(index));
        return false;
    }
    
    // Kontrola, zda sériové číslo není již použito jiným senzorem
    int existingIndex = findSensorBySN(serialNumber);
    if (existingIndex >= 0 && existingIndex != index) {
        logger.warning("Cannot update sensor config: Serial number " + 
                       String(serialNumber, HEX) + " already used by sensor " + 
                       sensors[existingIndex].name);
        return false;
    }
    
    // Aktualizace konfigurace
    sensors[index].name = name;
    sensors[index].deviceType = deviceType;
    sensors[index].serialNumber = serialNumber;
    sensors[index].deviceKey = deviceKey;
    sensors[index].endpoint = endpoint;
    sensors[index].customParams = customParams;
    
    logger.info("Updated configuration for sensor: " + name + " (SN: " + String(serialNumber, HEX) + ")");
    saveSensors(false);
    return true;
}

// Smazání senzoru
bool SensorManager::deleteSensor(int index) {
    std::lock_guard<std::mutex> lock(sensorMutex);
    
    if (index < 0 || index >= sensorCount || !sensors[index].configured) {
        logger.warning("Attempt to delete non-existent sensor at index " + String(index));
        return false;
    }
    
    String name = sensors[index].name;
    uint32_t serialNumber = sensors[index].serialNumber;
    
    // Označíme jako nekonfigurovaný namísto fyzického odstranění
    sensors[index].configured = false;
    
    logger.info("Deleted sensor: " + name + " (SN: " + String(serialNumber, HEX) + ")");
    saveSensors(false);
    return true;
}

// Získání počtu senzorů
size_t SensorManager::getSensorCount() const {
    return sensorCount;
}

// Získání senzoru podle indexu (const verze)
const SensorData* SensorManager::getSensor(int index) const {
    if (index < 0 || index >= sensorCount || !sensors[index].configured) {
        return nullptr;
    }
    return &sensors[index];
}

// Získání senzoru podle indexu (non-const verze)
SensorData* SensorManager::getSensor(int index) {
    if (index < 0 || index >= sensorCount || !sensors[index].configured) {
        return nullptr;
    }
    return &sensors[index];
}

// Získání seznamu všech senzorů
std::vector<SensorData> SensorManager::getAllSensors() const {
    std::lock_guard<std::mutex> lock(sensorMutex);
    
    std::vector<SensorData> result;
    for (size_t i = 0; i < sensorCount; i++) {
        result.push_back(sensors[i]);
    }
    return result;
}

// Získání seznamu všech aktivních senzorů (nakonfigurovaných)
std::vector<SensorData> SensorManager::getActiveSensors() const {
    std::lock_guard<std::mutex> lock(sensorMutex);
    
    std::vector<SensorData> result;
    for (size_t i = 0; i < sensorCount; i++) {
        if (sensors[i].configured) {
            result.push_back(sensors[i]);
        }
    }
    return result;
}

// Uložení konfigurace senzorů do souboru
bool SensorManager::saveSensors(bool lockMutex) {
    if (lockMutex) {
        std::lock_guard<std::mutex> lock(sensorMutex);
    }
    
    // Otevření souboru pro zápis
    File file = LittleFS.open(sensorsFile, "w");
    if (!file) {
        logger.info("Failed to open sensors file for writing: " + String(sensorsFile));
        return false;
    }
    
    // Vytvoření JSON dokumentu
    DynamicJsonDocument doc(4096); // Dostatečně velký buffer
    JsonArray sensorArray = doc.createNestedArray("sensors");
    
    // Přidání senzorů do JSON
    for (size_t i = 0; i < sensorCount; i++) {
        if (sensors[i].configured) {
            JsonObject sensor = sensorArray.createNestedObject();
            sensor["deviceType"] = static_cast<uint8_t>(sensors[i].deviceType);
            sensor["serialNumber"] = sensors[i].serialNumber;
            sensor["deviceKey"] = sensors[i].deviceKey;
            sensor["name"] = sensors[i].name;
            sensor["endpoint"] = sensors[i].endpoint;
            sensor["customParams"] = sensors[i].customParams;
        }
    }
    logger.info("Serializing sensors to JSON");
    // Serializace JSON do souboru
    if (serializeJson(doc, file) == 0) {
        logger.info("Failed to write sensors to file");
        file.close();
        return false;
    }
    
    file.close();
    logger.info("Saved " + String(sensorArray.size()) + " sensors to " + String(sensorsFile));
    return true;
}

// Načtení konfigurace senzorů ze souboru
bool SensorManager::loadSensors() {
    std::lock_guard<std::mutex> lock(sensorMutex);
    
    // Reset počtu senzorů
    sensorCount = 0;
    for (size_t i = 0; i < MAX_SENSORS; i++) {
        sensors[i].configured = false;
    }
    
    // Kontrola, zda soubor existuje
    if (!LittleFS.exists(sensorsFile)) {
        logger.info("Sensors file not found: " + String(sensorsFile) + ", starting with empty configuration");
        return true; // Není to chyba, prostě začneme s prázdnou konfigurací
    }
    
    // Otevření souboru pro čtení
    File file = LittleFS.open(sensorsFile, "r");
    if (!file) {
        logger.error("Failed to open sensors file for reading: " + String(sensorsFile));
        return false;
    }
    
    // Vytvoření JSON dokumentu
    DynamicJsonDocument doc(4096); // Dostatečně velký buffer
    
    // Deserializace JSON ze souboru
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        logger.error("Failed to parse sensors file: " + String(error.c_str()));
        return false;
    }
    
    // Načtení senzorů z JSON
    JsonArray sensorArray = doc["sensors"].as<JsonArray>();
    for (JsonObject sensorObj : sensorArray) {
        if (sensorCount < MAX_SENSORS) {
            uint8_t typeValue = sensorObj["deviceType"];
            SensorType deviceType = static_cast<SensorType>(typeValue);
            uint32_t serialNumber = sensorObj["serialNumber"];
            uint32_t deviceKey = sensorObj["deviceKey"];
            String name = sensorObj["name"].as<String>();
            String endpoint = sensorObj["endpoint"].as<String>();
            String customParams = sensorObj["customParams"].as<String>();
            
            // Vytvoření senzoru
            sensors[sensorCount].deviceType = deviceType;
            sensors[sensorCount].serialNumber = serialNumber;
            sensors[sensorCount].deviceKey = deviceKey;
            sensors[sensorCount].name = name;
            sensors[sensorCount].endpoint = endpoint;
            sensors[sensorCount].customParams = customParams;
            sensors[sensorCount].lastSeen = 0;
            sensors[sensorCount].temperature = 0.0f;
            sensors[sensorCount].humidity = 0.0f;
            sensors[sensorCount].pressure = 0.0f;
            sensors[sensorCount].ppm = 0.0f;
            sensors[sensorCount].lux = 0.0f;
            sensors[sensorCount].batteryVoltage = 0.0f;
            sensors[sensorCount].rssi = 0;
            sensors[sensorCount].configured = true;
            
            sensorCount++;
        } else {
            logger.warning("Too many sensors in configuration file, ignoring some");
            break;
        }
    }
    
    logger.info("Loaded " + String(sensorCount) + " sensors from configuration");
    return true;
}