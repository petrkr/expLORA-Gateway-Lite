#include "SensorManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <HTTPClient.h>

// Konstruktor
SensorManager::SensorManager(Logger &log, const char *file)
    : sensorCount(0), logger(log), sensorsFile(file)
{
}

// Destruktor
SensorManager::~SensorManager()
{
    // Nic speciálního není potřeba uvolňovat, všechny objekty se uvolní samy
}

// Inicializace správce senzorů
bool SensorManager::init()
{
    logger.info("Initializing sensor manager");
    return loadSensors();
}

// Přidání nového senzoru
int SensorManager::addSensor(SensorType deviceType, uint32_t serialNumber, uint32_t deviceKey, const String &name)
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    // Kontrola, zda senzor již existuje
    int existingIndex = findSensorBySN(serialNumber);
    if (existingIndex >= 0)
    {
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
    if (sensorCount >= MAX_SENSORS)
    {
        logger.error("Failed to add sensor: maximum number of sensors reached");
        return -1;
    }

    // Najít první volné místo
    int newIndex = -1;
    for (size_t i = 0; i < MAX_SENSORS; i++)
    {
        if (!sensors[i].configured)
        {
            newIndex = i;
            break;
        }
    }

    // Pokud není volné místo, použijeme poslední index
    if (newIndex == -1)
    {
        newIndex = sensorCount;
        sensorCount++;
    }
    else if (newIndex >= sensorCount)
    {
        sensorCount = newIndex + 1;
    }

    // Inicializace nového senzoru
    sensors[newIndex].deviceType = deviceType;
    sensors[newIndex].serialNumber = serialNumber;
    sensors[newIndex].deviceKey = deviceKey;
    sensors[newIndex].name = name;
    sensors[newIndex].customUrl = "";
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
int SensorManager::findSensorBySN(uint32_t serialNumber)
{
    for (size_t i = 0; i < sensorCount; i++)
    {
        if (sensors[i].configured && sensors[i].serialNumber == serialNumber)
        {
            return i;
        }
    }
    return -1;
}

// Aktualizace dat senzoru
bool SensorManager::updateSensor(int index, const SensorData &data)
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
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
                                     float ppm, float lux, float batteryVoltage, int rssi,
                                     float windSpeed, uint16_t windDirection,
                                     float rainAmount, float rainRate)
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        logger.warning("Attempt to update non-existent sensor at index " + String(index));
        return false;
    }

    // Store original values for logging
    float originalTemp = temperature;
    float originalHum = humidity;
    float originalPress = pressure;
    float originalPPM = ppm;
    float originalLux = lux;
    float originalWindSpeed = windSpeed;
    uint16_t originalWindDir = windDirection;
    float originalRainAmount = rainAmount;
    float originalRainRate = rainRate;

    // Apply corrections to input values before updating
    temperature += sensors[index].temperatureCorrection;
    humidity += sensors[index].humidityCorrection;
    pressure += sensors[index].pressureCorrection;
    ppm += sensors[index].ppmCorrection;
    lux += sensors[index].luxCorrection;
    windSpeed *= sensors[index].windSpeedCorrection;

    // For wind direction, ensure value stays in 0-359 range
    windDirection = (windDirection + sensors[index].windDirectionCorrection) % 360;

    rainAmount *= sensors[index].rainAmountCorrection;
    rainRate *= sensors[index].rainRateCorrection;

    // Log if corrections were applied
    bool correctionsApplied = false;
    String correctionLog = "Corrections applied to " + sensors[index].name + ": ";

    if (sensors[index].hasTemperature() && sensors[index].temperatureCorrection != 0)
    {
        correctionLog += "Temp " + String(originalTemp, 2) + "→" + String(temperature, 2) + "°C, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasHumidity() && sensors[index].humidityCorrection != 0)
    {
        correctionLog += "Hum " + String(originalHum, 2) + "→" + String(humidity, 2) + "%, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasPressure() && sensors[index].pressureCorrection != 0)
    {
        correctionLog += "Press " + String(originalPress, 2) + "→" + String(pressure, 2) + "hPa, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasPPM() && sensors[index].ppmCorrection != 0)
    {
        correctionLog += "CO2 " + String(originalPPM, 0) + "→" + String(ppm, 0) + "ppm, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasLux() && sensors[index].luxCorrection != 0)
    {
        correctionLog += "Lux " + String(originalLux, 1) + "→" + String(lux, 1) + "lx, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasWindSpeed() && sensors[index].windSpeedCorrection != 1.0f)
    {
        correctionLog += "Wind " + String(originalWindSpeed, 1) + "→" + String(windSpeed, 1) + "m/s, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasWindDirection() && sensors[index].windDirectionCorrection != 0)
    {
        correctionLog += "Dir " + String(originalWindDir) + "→" + String(windDirection) + "°, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasRainAmount() && sensors[index].rainAmountCorrection != 1.0f)
    {
        correctionLog += "Rain " + String(originalRainAmount, 1) + "→" + String(rainAmount, 1) + "mm, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasRainRate() && sensors[index].rainRateCorrection != 1.0f)
    {
        correctionLog += "Rate " + String(originalRainRate, 1) + "→" + String(rainRate, 1) + "mm/h, ";
        correctionsApplied = true;
    }

    // Log corrections if any were applied
    if (correctionsApplied)
    {
        // Remove trailing comma and space
        correctionLog = correctionLog.substring(0, correctionLog.length() - 2);
        logger.debug(correctionLog);
    }

    // Aktualizujeme pouze relevantní hodnoty podle typu senzoru
    if (sensors[index].hasTemperature())
    {
        sensors[index].temperature = temperature;
    }

    if (sensors[index].hasHumidity())
    {
        sensors[index].humidity = humidity;
    }

    if (sensors[index].hasPressure())
    {
        sensors[index].pressure = pressure;
    }

    if (sensors[index].hasPPM())
    {
        sensors[index].ppm = ppm;
    }

    if (sensors[index].hasLux())
    {
        sensors[index].lux = lux;
    }

    // Meteorologická data
    if (sensors[index].hasWindSpeed())
    {
        sensors[index].windSpeed = windSpeed;
    }

    if (sensors[index].hasWindDirection())
    {
        sensors[index].windDirection = windDirection;
    }

    if (sensors[index].hasRainAmount())
    {
        sensors[index].rainAmount = rainAmount;

        // Kontrola, zda je potřeba resetovat denní úhrn (nový den)
        if (Logger::isTimeInitialized())
        { // Použití metody z Logger
            // Získáme aktuální čas
            struct tm timeinfo;
            time_t now;
            time(&now);

            if (getLocalTime(&timeinfo))
            {
                // Vytvoříme časové značky pro porovnání datumů
                // Datum posledního resetu
                struct tm lastResetTime;
                localtime_r((time_t *)&sensors[index].lastRainReset, &lastResetTime);

                // Pokud poslední reset byl v jiný den než dnes, resetujeme počítadlo
                if (sensors[index].lastRainReset == 0 ||
                    lastResetTime.tm_mday != timeinfo.tm_mday ||
                    lastResetTime.tm_mon != timeinfo.tm_mon ||
                    lastResetTime.tm_year != timeinfo.tm_year)
                {

                    logger.info("Resetting daily rain total for sensor: " + sensors[index].name);
                    sensors[index].dailyRainTotal = 0.0f;
                    sensors[index].lastRainReset = now;
                }
            }
        }

        // Přidáme aktuální množství srážek k dennímu úhrnu
        sensors[index].dailyRainTotal += rainAmount;
    }

    if (sensors[index].hasRainRate())
    {
        sensors[index].rainRate = rainRate;
    }

    // Obecná data aktualizujeme vždy
    sensors[index].batteryVoltage = batteryVoltage;
    sensors[index].rssi = rssi;
    sensors[index].lastSeen = millis();

    if (WiFi.status() == WL_CONNECTED)
    {
        forwardSensorData(index);
    }
    else
    {
        logger.debug("Not forwarding data - WiFi not connected");
    }

    return true;
}

bool SensorManager::forwardSensorData(int index)
{
    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        logger.warning("Attempt to forward data for non-existent sensor at index " + String(index));
        return false;
    }

    // Check if the sensor has a custom endpoint configured
    if (sensors[index].customUrl.length() == 0)
    {
        // No endpoint configured, nothing to do
        return true;
    }

    // Create a WiFi client for HTTP request
    WiFiClient client;
    HTTPClient http;

    // Format the custom URL by replacing placeholders
    String url = sensors[index].customUrl;

    // Skip if no URL is configured
    if (url.length() == 0)
    {
        return true;
    }

    // Check if HTTP or HTTPS
    bool isHttps = url.startsWith("https://");

    // Process the URL - replace placeholders with actual values
    if (sensors[index].hasTemperature())
    {
        url.replace("*TEMP*", String(sensors[index].temperature, 2));
    }
    if (sensors[index].hasHumidity())
    {
        url.replace("*HUM*", String(sensors[index].humidity, 2));
    }
    if (sensors[index].hasPressure())
    {
        url.replace("*PRESS*", String(sensors[index].pressure, 2));
    }
    if (sensors[index].hasPPM())
    {
        url.replace("*PPM*", String(sensors[index].ppm, 0));
    }
    if (sensors[index].hasLux())
    {
        url.replace("*LUX*", String(sensors[index].lux, 1));
    }

    // Přidání placeholderů pro METEO data
    if (sensors[index].hasWindSpeed())
    {
        url.replace("*WIND_SPEED*", String(sensors[index].windSpeed, 1));
    }
    if (sensors[index].hasWindDirection())
    {
        url.replace("*WIND_DIR*", String(sensors[index].windDirection));
    }
    if (sensors[index].hasRainAmount())
    {
        url.replace("*RAIN*", String(sensors[index].rainAmount, 1));
        url.replace("*DAILY_RAIN*", String(sensors[index].dailyRainTotal, 1)); // Nový placeholder
    }
    if (sensors[index].hasRainRate())
    {
        url.replace("*RAIN_RATE*", String(sensors[index].rainRate, 1));
    }

    // Replace other common placeholders
    url.replace("*BAT*", String(sensors[index].batteryVoltage, 2));
    url.replace("*RSSI*", String(sensors[index].rssi));
    url.replace("*SN*", String(sensors[index].serialNumber, HEX));
    url.replace("*TYPE*", String(static_cast<uint8_t>(sensors[index].deviceType)));

    logger.debug("Forwarding data for sensor " + sensors[index].name + " to URL: " + url);

    // For HTTPS, use a secure client but with insecure flag
    if (isHttps)
    {
        WiFiClientSecure *secureClient = new WiFiClientSecure();
        secureClient->setInsecure(); // Skip certificate validation
        http.begin(*secureClient, url);
    }
    else
    {
        http.begin(client, url);
    }

    // Send the request
    int httpCode = http.GET();

    // Check result
    if (httpCode > 0)
    {
        logger.debug("HTTP request sent, response code: " + String(httpCode));
        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            logger.debug("Response: " + payload.substring(0, 100)); // Only log first 100 chars
        }
    }
    else
    {
        logger.warning("HTTP request failed: " + http.errorToString(httpCode));
    }

    http.end();

    return (httpCode == HTTP_CODE_OK);
}

// Aktualizace konfigurace senzoru
// Update sensor configuration
bool SensorManager::updateSensorConfig(int index, const String &name, SensorType deviceType,
                                       uint32_t serialNumber, uint32_t deviceKey,
                                       const String &customUrl, int altitude,
                                       float tempCorr, float humCorr, float pressCorr,
                                       float ppmCorr, float luxCorr,
                                       float windSpeedCorr, int windDirCorr,
                                       float rainAmountCorr, float rainRateCorr)
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        logger.warning("Attempt to update non-existent sensor at index " + String(index));
        return false;
    }

    // Check if serial number is already used by another sensor
    int existingIndex = findSensorBySN(serialNumber);
    if (existingIndex >= 0 && existingIndex != index)
    {
        logger.warning("Cannot update sensor config: Serial number " +
                       String(serialNumber, HEX) + " already used by sensor " +
                       sensors[existingIndex].name);
        return false;
    }

    // Update basic configuration
    sensors[index].name = name;
    sensors[index].deviceType = deviceType;
    sensors[index].serialNumber = serialNumber;
    sensors[index].deviceKey = deviceKey;
    sensors[index].customUrl = customUrl;
    sensors[index].altitude = altitude;

    // Update correction values
    sensors[index].temperatureCorrection = tempCorr;
    sensors[index].humidityCorrection = humCorr;
    sensors[index].pressureCorrection = pressCorr;
    sensors[index].ppmCorrection = ppmCorr;
    sensors[index].luxCorrection = luxCorr;
    sensors[index].windSpeedCorrection = windSpeedCorr;
    sensors[index].windDirectionCorrection = windDirCorr;
    sensors[index].rainAmountCorrection = rainAmountCorr;
    sensors[index].rainRateCorrection = rainRateCorr;

    logger.info("Updated configuration for sensor: " + name + " (SN: " + String(serialNumber, HEX) + ")");
    saveSensors(false);
    return true;
}

// Smazání senzoru
bool SensorManager::deleteSensor(int index)
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
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
size_t SensorManager::getSensorCount() const
{
    return sensorCount;
}

// Získání senzoru podle indexu (const verze)
const SensorData *SensorManager::getSensor(int index) const
{
    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        return nullptr;
    }
    return &sensors[index];
}

// Získání senzoru podle indexu (non-const verze)
SensorData *SensorManager::getSensor(int index)
{
    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        return nullptr;
    }
    return &sensors[index];
}

// Získání seznamu všech senzorů
std::vector<SensorData> SensorManager::getAllSensors() const
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    std::vector<SensorData> result;
    for (size_t i = 0; i < sensorCount; i++)
    {
        result.push_back(sensors[i]);
    }
    return result;
}

// Získání seznamu všech aktivních senzorů (nakonfigurovaných)
std::vector<SensorData> SensorManager::getActiveSensors() const
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    std::vector<SensorData> result;
    for (size_t i = 0; i < sensorCount; i++)
    {
        if (sensors[i].configured)
        {
            result.push_back(sensors[i]);
        }
    }
    return result;
}

// Uložení konfigurace senzorů do souboru
bool SensorManager::saveSensors(bool lockMutex)
{
    if (lockMutex)
    {
        std::lock_guard<std::mutex> lock(sensorMutex);
    }

    // Otevření souboru pro zápis
    File file = LittleFS.open(sensorsFile, "w");
    if (!file)
    {
        logger.info("Failed to open sensors file for writing: " + String(sensorsFile));
        return false;
    }

    // Vytvoření JSON dokumentu
    DynamicJsonDocument doc(4096); // Dostatečně velký buffer
    JsonArray sensorArray = doc.createNestedArray("sensors");

    // Přidání senzorů do JSON
    for (size_t i = 0; i < sensorCount; i++)
    {
        if (sensors[i].configured)
        {
            JsonObject sensor = sensorArray.createNestedObject();
            sensor["deviceType"] = static_cast<uint8_t>(sensors[i].deviceType);
            sensor["serialNumber"] = sensors[i].serialNumber;
            sensor["deviceKey"] = sensors[i].deviceKey;
            sensor["name"] = sensors[i].name;
            sensor["customUrl"] = sensors[i].customUrl;
            sensor["altitude"] = sensors[i].altitude;

            // Uložíme i denní úhrn srážek a čas posledního resetu
            if (sensors[i].hasRainAmount())
            {
                sensor["dailyRainTotal"] = sensors[i].dailyRainTotal;
                sensor["lastRainReset"] = sensors[i].lastRainReset;
            }

            sensor["temperatureCorrection"] = sensors[i].temperatureCorrection;
            sensor["humidityCorrection"] = sensors[i].humidityCorrection;
            sensor["pressureCorrection"] = sensors[i].pressureCorrection;
            sensor["ppmCorrection"] = sensors[i].ppmCorrection;
            sensor["luxCorrection"] = sensors[i].luxCorrection;
            sensor["windSpeedCorrection"] = sensors[i].windSpeedCorrection;
            sensor["windDirectionCorrection"] = sensors[i].windDirectionCorrection;
            sensor["rainAmountCorrection"] = sensors[i].rainAmountCorrection;
            sensor["rainRateCorrection"] = sensors[i].rainRateCorrection;
        }
    }
    logger.info("Serializing sensors to JSON");
    // Serializace JSON do souboru
    if (serializeJson(doc, file) == 0)
    {
        logger.info("Failed to write sensors to file");
        file.close();
        return false;
    }

    file.close();
    logger.info("Saved " + String(sensorArray.size()) + " sensors to " + String(sensorsFile));
    return true;
}

// Načtení konfigurace senzorů ze souboru
bool SensorManager::loadSensors()
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    // Reset počtu senzorů
    sensorCount = 0;
    for (size_t i = 0; i < MAX_SENSORS; i++)
    {
        sensors[i].configured = false;
    }

    // Kontrola, zda soubor existuje
    if (!LittleFS.exists(sensorsFile))
    {
        logger.info("Sensors file not found: " + String(sensorsFile) + ", starting with empty configuration");
        return true; // Není to chyba, prostě začneme s prázdnou konfigurací
    }

    // Otevření souboru pro čtení
    File file = LittleFS.open(sensorsFile, "r");
    if (!file)
    {
        logger.error("Failed to open sensors file for reading: " + String(sensorsFile));
        return false;
    }

    // Vytvoření JSON dokumentu
    DynamicJsonDocument doc(4096); // Dostatečně velký buffer

    // Deserializace JSON ze souboru
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        logger.error("Failed to parse sensors file: " + String(error.c_str()));
        return false;
    }

    // Načtení senzorů z JSON
    JsonArray sensorArray = doc["sensors"].as<JsonArray>();
    for (JsonObject sensorObj : sensorArray)
    {
        if (sensorCount < MAX_SENSORS)
        {
            uint8_t typeValue = sensorObj["deviceType"];
            SensorType deviceType = static_cast<SensorType>(typeValue);
            uint32_t serialNumber = sensorObj["serialNumber"];
            uint32_t deviceKey = sensorObj["deviceKey"];
            String name = sensorObj["name"].as<String>();
            String customUrl = sensorObj["customUrl"].as<String>();

            // Vytvoření senzoru
            sensors[sensorCount].deviceType = deviceType;
            sensors[sensorCount].serialNumber = serialNumber;
            sensors[sensorCount].deviceKey = deviceKey;
            sensors[sensorCount].name = name;
            sensors[sensorCount].customUrl = customUrl;
            sensors[sensorCount].lastSeen = 0;
            sensors[sensorCount].temperature = 0.0f;
            sensors[sensorCount].humidity = 0.0f;
            sensors[sensorCount].pressure = 0.0f;
            sensors[sensorCount].ppm = 0.0f;
            sensors[sensorCount].lux = 0.0f;
            sensors[sensorCount].batteryVoltage = 0.0f;
            sensors[sensorCount].rssi = 0;
            sensors[sensorCount].configured = true;

            // Načtení denního úhrnu srážek, pokud existuje
            if (sensorObj.containsKey("dailyRainTotal"))
            {
                sensors[sensorCount].dailyRainTotal = sensorObj["dailyRainTotal"].as<float>();
            }
            else
            {
                sensors[sensorCount].dailyRainTotal = 0.0f;
            }

            // Načtení času posledního resetu, pokud existuje
            if (sensorObj.containsKey("lastRainReset"))
            {
                sensors[sensorCount].lastRainReset = sensorObj["lastRainReset"].as<unsigned long>();
            }
            else
            {
                sensors[sensorCount].lastRainReset = 0;
            }

            if (sensorObj.containsKey("altitude"))
            {
                sensors[sensorCount].altitude = sensorObj["altitude"].as<int>();
            }
            else
            {
                sensors[sensorCount].altitude = 0;
            }

            if (sensorObj.containsKey("temperatureCorrection"))
            {
                sensors[sensorCount].temperatureCorrection = sensorObj["temperatureCorrection"].as<float>();
            }
            if (sensorObj.containsKey("humidityCorrection"))
            {
                sensors[sensorCount].humidityCorrection = sensorObj["humidityCorrection"].as<float>();
            }
            if (sensorObj.containsKey("pressureCorrection"))
            {
                sensors[sensorCount].pressureCorrection = sensorObj["pressureCorrection"].as<float>();
            }
            if (sensorObj.containsKey("ppmCorrection"))
            {
                sensors[sensorCount].ppmCorrection = sensorObj["ppmCorrection"].as<float>();
            }
            if (sensorObj.containsKey("luxCorrection"))
            {
                sensors[sensorCount].luxCorrection = sensorObj["luxCorrection"].as<float>();
            }
            if (sensorObj.containsKey("windSpeedCorrection"))
            {
                sensors[sensorCount].windSpeedCorrection = sensorObj["windSpeedCorrection"].as<float>();
            }
            if (sensorObj.containsKey("windDirectionCorrection"))
            {
                sensors[sensorCount].windDirectionCorrection = sensorObj["windDirectionCorrection"].as<int>();
            }
            if (sensorObj.containsKey("rainAmountCorrection"))
            {
                sensors[sensorCount].rainAmountCorrection = sensorObj["rainAmountCorrection"].as<float>();
            }
            if (sensorObj.containsKey("rainRateCorrection"))
            {
                sensors[sensorCount].rainRateCorrection = sensorObj["rainRateCorrection"].as<float>();
            }
            sensorCount++;
        }
        else
        {
            logger.warning("Too many sensors in configuration file, ignoring some");
            break;
        }
    }

    logger.info("Loaded " + String(sensorCount) + " sensors from configuration");
    return true;
}