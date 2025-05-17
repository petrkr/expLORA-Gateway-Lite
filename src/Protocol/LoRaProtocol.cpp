#include "LoRaProtocol.h"

// Konstanty pro přepočet tlaku
#define G 9.80665          // tíhové zrychlení [m/s^2]
#define M 0.0289644        // molární hmotnost vzduchu [kg/mol]
#define R 8.3144598        // univerzální plynová konstanta [J/(mol·K)]
#define L 0.0065           // teplotní gradient [K/m]

// Přepočet relativního tlaku na absolutní
double LoRaProtocol::relativeToAbsolutePressure(double p_rel_hpa, int altitude_m, double temp_c) {
    if (altitude_m == 0) {
        return p_rel_hpa;
    }
    double T = temp_c + 273.15;
    double exponent = (G * M) / (R * L);
    return p_rel_hpa / pow(1 - (L * altitude_m) / T, exponent);
}

// Konstruktor
LoRaProtocol::LoRaProtocol(LoRaModule& module, SensorManager& manager, Logger& log)
    : loraModule(module), sensorManager(manager), logger(log), lastProcessedSensorIndex(-1) {
}

// Destruktor
LoRaProtocol::~LoRaProtocol() {
    // Nic specifického neuvolňujeme
}

// Zpracování přijatého paketu
bool LoRaProtocol::processReceivedPacket() {
    yield();
    // Kontrola, zda byla přijata data
    if (!loraModule.hasInterrupt()) {
        return false;
    }
    
    // Reset příznaku přerušení
    loraModule.clearInterrupt();
    
    // Pokus o přijetí paketu
    uint8_t length = 0;
    if (!loraModule.receivePacket(packetBuffer, &length)) {
        logger.warning("Failed to receive packet from LoRa module");
        return false;
    }
    
    // Logování přijatého paketu v hexadecimálním formátu
    String hexData = "Received data (HEX): ";
    for (uint8_t i = 0; i < length; i++) {
        if (packetBuffer[i] < 0x10) hexData += "0";
        hexData += String(packetBuffer[i], HEX) + " ";
    }
    logger.debug(hexData);
    
    // Získání RSSI pro diagnostiku
    int rssi = loraModule.getRSSI();
    logger.debug("RSSI: " + String(rssi) + " dBm, SNR: " + String(loraModule.getSNR()) + " dB");
    
    // Pokus o dešifrování paketu
    int sensorIndex = tryDecryptWithAllKeys(packetBuffer, length, decryptedBuffer);
        
    // Pokud není nalezen známý senzor
    if (sensorIndex < 0) {
        logger.debug("Unknown sensor detected - cannot process packet");
        return false;
    }

    // Logování dešifrovaných dat
    hexData = "Decrypted data (HEX): ";
    for (uint8_t i = 0; i < length; i++) {
        if (decryptedBuffer[i] < 0x10) hexData += "0";
        hexData += String(decryptedBuffer[i], HEX) + " ";
    }
    logger.debug(hexData);
    
    // Kontrola kontrolního součtu
    if (!validateChecksum(decryptedBuffer, length)) {
        logger.warning("Invalid checksum in received packet - data corrupted");
        return false;
    }
    
    // Kontrola platnosti paketu
    if (!isValidPacket(decryptedBuffer, length)) {
        logger.warning("Received packet has invalid format");
        return false;
    }
    
    // Získání typu senzoru z dešifrovaných dat
    SensorType deviceType = static_cast<SensorType>(decryptedBuffer[1]);
    
    lastProcessedSensorIndex = sensorIndex;

    // Zpracování paketu podle typu senzoru
    return processPacketByType(deviceType, decryptedBuffer, length, sensorIndex, rssi);
}

bool LoRaProtocol::processPacketByType(SensorType type, uint8_t *data, uint8_t len, int sensorIndex, int rssi) {
    switch (type) {
        case SensorType::BME280:
            return processBME280Packet(data, len, sensorIndex, rssi);
            
        case SensorType::SCD40:
            return processSCD40Packet(data, len, sensorIndex, rssi);
            
        case SensorType::VEML7700:
            return processVEML7700Packet(data, len, sensorIndex, rssi);
            
        case SensorType::METEO:
            return processMeteoPacket(data, len, sensorIndex, rssi);
            
        default:
            logger.warning("Unknown device type: 0x" + String(static_cast<uint8_t>(type), HEX));
            return false;
    }
}

// Zpracování paketu z BME280 senzoru
bool LoRaProtocol::processBME280Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi) {
    // Kontrola minimální délky pro BME280 paket (hlavička + data + checksum)
    const SensorTypeInfo& typeInfo = getSensorTypeInfo(SensorType::BME280);
    if (len < typeInfo.packetDataOffset + typeInfo.expectedDataLength + 1) {
        logger.warning("Packet too short for BME280");
        return false;
    }
    
    // Extrakce společných údajů
    uint32_t serialNumber = ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 8) | data[4];
    uint16_t batteryRaw = ((uint16_t)data[5] << 8) | data[6];
    float voltage = batteryRaw / 1000.0;
    
    // Extrakce dat specifických pro BME280
    int16_t tempRaw = (int16_t)(((uint16_t)data[8] << 8) | data[9]);
    float temp = tempRaw / 100.0;
    
    uint16_t pressRaw = ((uint16_t)data[10] << 8) | data[11];
    float press = pressRaw / 10.0;
    
    uint16_t humRaw = ((uint16_t)data[12] << 8) | data[13];
    float hum = humRaw / 100.0;
    
    // Aktualizace dat senzoru
    SensorData* sensor = sensorManager.getSensor(sensorIndex);
    if (!sensor) {
        logger.error("Error accessing sensor data at index " + String(sensorIndex));
        return false;
    }
    
    float adjustedPressure = press;
    if (sensor->altitude > 0) {
        adjustedPressure = relativeToAbsolutePressure(press, sensor->altitude, temp);
        logger.debug("Adjusted pressure from " + String(press, 2) + " hPa to " + 
                    String(adjustedPressure, 2) + " hPa at altitude " + 
                    String(sensor->altitude) + " m");  // Odstraněn formátovací parametr
    }
    
    // Aktualizace dat s upraveným tlakem
    bool result = sensorManager.updateSensorData(sensorIndex, temp, hum, adjustedPressure, 
                                               0.0f, 0.0f, voltage, rssi);
    
    if (result) {
        logger.info(sensor->name + " data updated - Temp: " + String(temp, 2) + "°C, Hum: " + 
                   String(hum, 2) + "%, Press: " + String(adjustedPressure, 2) + " hPa" +
                   (sensor->altitude > 0 ? " (adjusted from " + String(press, 2) + " hPa)" : "") +
                   ", Batt: " + String(voltage, 2) + "V");
    }
    
    return result;
}

// Zpracování paketu z SCD40 senzoru
bool LoRaProtocol::processSCD40Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi) {
    // Kontrola minimální délky pro SCD40 paket (hlavička + data + checksum)
    const SensorTypeInfo& typeInfo = getSensorTypeInfo(SensorType::SCD40);
    if (len < typeInfo.packetDataOffset + typeInfo.expectedDataLength + 1) {
        logger.warning("Packet too short for SCD40");
        return false;
    }
    
    // Extrakce společných údajů
    uint32_t serialNumber = ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 8) | data[4];
    uint16_t batteryRaw = ((uint16_t)data[5] << 8) | data[6];
    float voltage = batteryRaw / 1000.0;
    
    // Extrakce dat specifických pro SCD40
    int16_t tempRaw = (int16_t)(((uint16_t)data[8] << 8) | data[9]);
    float temp = tempRaw / 100.0;
    
    uint16_t ppmRaw = ((uint16_t)data[10] << 8) | data[11];
    float ppm = ppmRaw;
    
    uint16_t humRaw = ((uint16_t)data[12] << 8) | data[13];
    float hum = humRaw / 100.0;
    
    // Aktualizace dat senzoru
    SensorData* sensor = sensorManager.getSensor(sensorIndex);
    if (!sensor) {
        logger.error("Error accessing sensor data at index " + String(sensorIndex));
        return false;
    }
    
    // Aktualizace dat
    bool result = sensorManager.updateSensorData(sensorIndex, temp, hum, 0.0f, ppm, 0.0f, voltage, rssi);
    
    if (result) {
        logger.info(sensor->name + " data updated - Temp: " + String(temp, 2) + "°C, Hum: " + 
                    String(hum, 2) + "%, CO2: " + String(ppm, 0) + " ppm, Batt: " + 
                    String(voltage, 2) + "V");
    }
    
    return result;
}

// Zpracování paketu z VEML7700 senzoru
bool LoRaProtocol::processVEML7700Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi) {
    // Kontrola minimální délky pro VEML7700 paket (hlavička + data + checksum)
    const SensorTypeInfo& typeInfo = getSensorTypeInfo(SensorType::VEML7700);
    if (len < typeInfo.packetDataOffset + typeInfo.expectedDataLength + 1) {
        logger.warning("Packet too short for VEML7700");
        return false;
    }
    
    // Extrakce společných údajů
    uint32_t serialNumber = ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 8) | data[4];
    uint16_t batteryRaw = ((uint16_t)data[5] << 8) | data[6];
    float voltage = batteryRaw / 1000.0;
    
    // Extrakce dat specifických pro VEML7700
    uint32_t luxRaw = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) | 
                     ((uint32_t)data[10] << 8) | data[11];
    float lux = luxRaw / 100.0;  // Předpokládáme, že hodnota lux je v setinách
    
    // Aktualizace dat senzoru
    SensorData* sensor = sensorManager.getSensor(sensorIndex);
    if (!sensor) {
        logger.error("Error accessing sensor data at index " + String(sensorIndex));
        return false;
    }
    
    // Aktualizace dat
    bool result = sensorManager.updateSensorData(sensorIndex, 0.0f, 0.0f, 0.0f, 0.0f, lux, voltage, rssi);
    
    if (result) {
        logger.info(sensor->name + " data updated - Light: " + String(lux, 1) + " lux, Batt: " + 
                   String(voltage, 2) + "V");
    }
    
    return result;
}

bool LoRaProtocol::processMeteoPacket(uint8_t *data, uint8_t len, int sensorIndex, int rssi) {
    // Kontrola minimální délky pro METEO paket
    if (len < 21) {  // 8 (hlavička) + 12 (6 hodnot * 2 bajty) + 1 (checksum)
        logger.warning("Packet too short for METEO: " + String(len) + " bytes");
        return false;
    }
    
    // Extrakce společných údajů
    uint32_t serialNumber = ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 8) | data[4];
    uint16_t batteryRaw = ((uint16_t)data[5] << 8) | data[6];
    float voltage = batteryRaw / 1000.0;  // mV to V
    
    // Výpis pro ladění
    logger.debug("METEO packet: SN=" + String(serialNumber, HEX) + 
                ", battery=" + String(voltage) + "V, values=" + String(data[7]));
    
    // Extrakce dat specifických pro METEO
    int16_t tempRaw = (int16_t)(((uint16_t)data[8] << 8) | data[9]);
    float temp = tempRaw / 100.0;  // setiny °C na °C
    
    uint16_t pressRaw = ((uint16_t)data[10] << 8) | data[11];
    float press = pressRaw / 10.0;  // desetiny hPa na hPa
    
    uint16_t humRaw = ((uint16_t)data[12] << 8) | data[13];
    float hum = humRaw / 100.0;  // setiny % na %
    
    // Extrakce meteorologických dat
    uint16_t windSpeedRaw = ((uint16_t)data[14] << 8) | data[15];
    float windSpeed = windSpeedRaw / 10.0;  // setiny m/s na m/s
    
    uint16_t windDirection = ((uint16_t)data[16] << 8) | data[17];
    
    uint16_t rainAmountRaw = ((uint16_t)data[18] << 8) | data[19];
    float rainAmount = rainAmountRaw / 1000.0;  // tisíciny na mm
    
    float rainRate = 0.0f;
    // Pokud máme rozšířený paket, přečteme i intenzitu srážek
    if (len >= 23) {
        uint16_t rainRateRaw = ((uint16_t)data[20] << 8) | data[21];
        rainRate = rainRateRaw / 100.0;  // setiny mm/h na mm/h
    }
    
    // Debug log všech hodnot
    logger.debug("METEO values: temp=" + String(temp) + "°C, press=" + String(press) + 
               "hPa, hum=" + String(hum) + "%, wind=" + String(windSpeed) + 
               "m/s at " + String(windDirection) + "°, rain=" + String(rainAmount) + 
               "mm, rate=" + String(rainRate) + "mm/h");
    
    // Aktualizace dat senzoru
    SensorData* sensor = sensorManager.getSensor(sensorIndex);
    if (!sensor) {
        logger.error("Error accessing sensor data at index " + String(sensorIndex));
        return false;
    }
    
    // Aktualizace základních dat
    bool result = sensorManager.updateSensorData(sensorIndex, temp, hum, press, 0.0f, 0.0f, voltage, rssi,
                                              windSpeed, windDirection, rainAmount, rainRate);
    
    if (result) {
        logger.info(sensor->name + " data updated - Temp: " + String(temp, 2) + "°C, Hum: " + 
                   String(hum, 2) + "%, Press: " + String(press, 2) + " hPa, Wind: " +
                   String(windSpeed, 1) + " m/s at " + String(windDirection) + "°, Rain: " +
                   String(rainAmount, 1) + " mm (rate: " + String(rainRate, 1) + " mm/h), Batt: " +
                   String(voltage, 2) + "V");
    }
    
    return result;
}

// Dešifrování dat s klíčem
void LoRaProtocol::decryptData(uint8_t *data, uint8_t data_len, uint32_t key) {
    uint8_t *key_bytes = (uint8_t *)&key;
    uint8_t prev_byte = 0; // Začínáme od nuly

    for (uint8_t i = 0; i < data_len; i++) {
        uint8_t key_byte = key_bytes[i & 0x03];
        // Aktuální zašifrovaný bajt
        uint8_t current_encrypted = data[i];
        // Rozšifrujeme (XOR stejné hodnoty jako v encryptu)
        data[i] = current_encrypted ^ key_byte ^ (prev_byte >> 1);
        // Posuneme "rolovací" bajt pro další iteraci
        prev_byte = current_encrypted;
    }
}

// Zkouška dešifrování se všemi známými klíči
int LoRaProtocol::tryDecryptWithAllKeys(uint8_t *encData, uint8_t len, uint8_t *decData) {
    // Získání seznamu všech aktivních senzorů
    std::vector<SensorData> activeSensors = sensorManager.getActiveSensors();
    
    // Průchod všemi senzory a pokus o dešifrování
    for (size_t i = 0; i < activeSensors.size(); i++) {
        // Kopírování zašifrovaných dat
        memcpy(decData, encData, len);
        
        // Pokus o dešifrování s klíčem tohoto senzoru
        decryptData(decData, len, activeSensors[i].deviceKey);
        
        // Ověření kontrolního součtu
        if (validateChecksum(decData, len)) {
            // Ověření, že sériové číslo v dešifrovaných datech odpovídá senzoru
            uint32_t packetSN = ((uint32_t)decData[2] << 16) | ((uint32_t)decData[3] << 8) | decData[4];
            
            if (packetSN == activeSensors[i].serialNumber) {
                // Našli jsme shodu, vracíme index senzoru
                logger.debug("Packet successfully decrypted with key from sensor " + 
                           activeSensors[i].name + " (SN: " + String(activeSensors[i].serialNumber, HEX) + ")");
                
                // Vracíme index senzoru v globálním poli
                return sensorManager.findSensorBySN(activeSensors[i].serialNumber);
            }
        }
    }
    
    // Pokud jsme nenašli shodu, zkusíme dešifrovat libovolným klíčem pro detekci neznámých senzorů
    if (!activeSensors.empty()) {
        // Použijeme klíč prvního senzoru
        memcpy(decData, encData, len);
        decryptData(decData, len, activeSensors[0].deviceKey);
    } else {
        // Nemáme žádné klíče, pouze kopírujeme data
        memcpy(decData, encData, len);
    }
    
    return -1;  // Žádný senzor nenalezen
}

// Validace kontrolního součtu
bool LoRaProtocol::validateChecksum(uint8_t *buf, uint8_t len) {
    // Kontrola minimální délky paketu
    if (len < 2) {
        return false;
    }
    
    // Poslední byte je kontrolní součet všech předchozích bytů
    uint8_t receivedChecksum = buf[len - 1];
    
    // Vypočet kontrolního součtu dat (bez kontrolního součtu)
    uint8_t calculatedChecksum = calculateChecksum(buf, len - 1);
    
    // Vrácení true, pokud se kontrolní součty shodují
    return (receivedChecksum == calculatedChecksum);
}

// Výpočet kontrolního součtu (jednoduchý XOR všech bytů)
uint8_t LoRaProtocol::calculateChecksum(const uint8_t *data, uint8_t length) {
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// Kontrola platnosti paketu
bool LoRaProtocol::isValidPacket(uint8_t *buf, uint8_t len) {
    // Check packet length (minimum 7 bytes for header without checksum)
    if (len < 9) {  // 8 + 1 for checksum
        return false;
    }

    // Get device type and number of values from packet
    uint8_t deviceType = buf[1];
    uint8_t numValues = buf[7];

    // Validate packet length based on device type and number of values
    if (deviceType == SENSOR_TYPE_METEO) {
        // METEO paket má 6 hodnot s možným rozšířením na 7 hodnot
        // (v případě, že obsahuje i intenzitu srážek)
        
        // Ověříme, zda délka je 23 (pro 7 hodnot) nebo 21 (pro 6 hodnot)
        if (len != 23 && len != 21) {
            logger.warning("Invalid METEO packet length: " + String(len) + 
                          ", expected: 21 or 23 bytes");
            return false;
        }
        
        // Pokud délka je 23, ale numValues je 6, upravíme očekávaný počet hodnot
        if (len == 23 && numValues == 6) {
            logger.info("Detected extended METEO packet with 7 values (including rain rate)");
            // Poznámka: Neměníme numValues v paketu, protože by to změnilo checksum
        }
    } else {
        // Standardní ověření pro ostatní typy paketů
        if (len != 8 + (numValues * 2) + 1) {
            logger.warning("Invalid packet length: " + String(len) + 
                          ", expected: " + String(8 + (numValues * 2) + 1) + 
                          " for " + String(numValues) + " values");
            return false;
        }
    }

    // Check if number of values is reasonable (e.g., not more than 10)
    if (numValues > 10) {
        logger.warning("Invalid number of values: " + String(numValues));
        return false;
    }

    // Check if deviceType makes sense (between 1-10)
    if (deviceType == 0 || deviceType > 10) {
        logger.warning("Invalid device type: " + String(deviceType));
        return false;
    }

    // Basic range checks for typical values - specific to device type
    if (deviceType == SENSOR_TYPE_METEO && numValues >= 6) {
        // Temperature check (similar for all sensors)
        int16_t tempSigned = (int16_t)(((uint16_t)buf[8] << 8) | buf[9]);
        if (tempSigned < -5000 || tempSigned > 6000) {
            logger.warning("Invalid temperature: " + String(tempSigned));
            return false;
        }

        // Pressure check (for METEO)
        uint16_t press = ((uint16_t)buf[10] << 8) | buf[11];
        if (press < 8500 || press > 11000) {
            logger.warning("Invalid pressure: " + String(press));
            return false;
        }

        // Humidity check (for METEO)
        uint16_t hum = ((uint16_t)buf[12] << 8) | buf[13];
        if (hum > 10000) {  // 0-100% with 2 decimal places
            logger.warning("Invalid humidity: " + String(hum));
            return false;
        }

        // Wind speed check (0-60 m/s in hundredths of m/s)
        uint16_t windSpeed = ((uint16_t)buf[14] << 8) | buf[15];
        if (windSpeed > 6000) {  // Max 60 m/s = 6000 (hundredths)
            logger.warning("Invalid wind speed: " + String(windSpeed));
            return false;
        }

        // Wind direction check (0-359 degrees)
        uint16_t windDir = ((uint16_t)buf[16] << 8) | buf[17];
        if (windDir > 359) {
            logger.warning("Invalid wind direction: " + String(windDir));
            return false;
        }

        // Rain amount and rain rate checks are less strict as they can vary greatly
    }
    else if (numValues >= 3) {
        // Standard checks for other sensor types
        // Temperature check
        int16_t tempSigned = (int16_t)(((uint16_t)buf[8] << 8) | buf[9]);
        if (tempSigned < -5000 || tempSigned > 6000) {
            logger.warning("Invalid temperature: " + String(tempSigned));
            return false;
        }

        // For BME280 check pressure, for SCD40 check PPM
        if (deviceType == SENSOR_TYPE_BME280) {
            // Pressure: 85-110 kPa (850-1100 hPa after division by 10)
            uint16_t press = ((uint16_t)buf[10] << 8) | buf[11];
            if (press < 8500 || press > 11000) {
                logger.warning("Invalid pressure: " + String(press));
                return false;
            }
        } 
        else if (deviceType == SENSOR_TYPE_SCD40) {
            // CO2 PPM: 0-10000 ppm (reasonable range for indoor air)
            uint16_t ppm = ((uint16_t)buf[10] << 8) | buf[11];
            if (ppm > 10000) {
                logger.warning("Invalid CO2 PPM: " + String(ppm));
                return false;
            }
        }

        // Humidity check
        uint16_t hum = ((uint16_t)buf[12] << 8) | buf[13];
        if (hum > 10000) {  // 0-100% with 2 decimal places
            logger.warning("Invalid humidity: " + String(hum));
            return false;
        }
    }

    return true;
}