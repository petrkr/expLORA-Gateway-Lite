#pragma once

#include <Arduino.h>
#include "../config.h"

/**
 * Definice typů senzorů a jejich vlastností
 * 
 * Tento soubor obsahuje definice všech podporovaných typů senzorů včetně jejich
 * specifických vlastností a schopností. Při přidání nového typu senzoru stačí
 * definovat jeho vlastnosti zde a přidat implementaci jeho zpracování.
 */

// Enumerace pro typy senzorů - shodné s definicemi v config.h
enum class SensorType : uint8_t {
    UNKNOWN = SENSOR_TYPE_UNKNOWN,
    BME280 = SENSOR_TYPE_BME280,     // Teplota, vlhkost, tlak
    SCD40 = SENSOR_TYPE_SCD40,       // Teplota, vlhkost, CO2
    VEML7700 = SENSOR_TYPE_VEML7700, // Světelný senzor (LUX)
    METEO = SENSOR_TYPE_METEO,       // Meteorologická stanice
    // Přidejte další typy senzorů zde...
};

// Struktura pro informace o typu senzoru
struct SensorTypeInfo {
    SensorType type;               // Typ senzoru (enum)
    const char* name;              // Název typu senzoru
    uint8_t expectedDataLength;    // Očekávaná délka dat v paketu (bez hlavičky a checksum)
    uint8_t packetDataOffset;      // Offset, kde začínají data senzoru v paketu
    bool hasTemperature;           // Zda senzor poskytuje teplotu
    bool hasHumidity;              // Zda senzor poskytuje vlhkost
    bool hasPressure;              // Zda senzor poskytuje tlak
    bool hasPPM;                   // Zda senzor poskytuje PPM (CO2)
    bool hasLux;                   // Zda senzor poskytuje osvětlení (lux)
    bool hasWindSpeed;             // Zda senzor poskytuje rychlost větru
    bool hasWindDirection;         // Zda senzor poskytuje směr větru
    bool hasRainAmount;            // Zda senzor poskytuje množství srážek
    bool hasRainRate;              // Zda senzor poskytuje intenzitu srážek
    // Možnost přidat další parametry podle potřeby
};

// Tabulka definic typů senzorů - pro snadné vyhledávání a přidávání nových typů
const SensorTypeInfo SENSOR_TYPE_DEFINITIONS[] = {
    // type,                 name,       dataLen, offset, temp,  hum,   press, ppm,   lux,   windS, windD, rain,  rainR
    {SensorType::UNKNOWN,   "Unknown",      0,     7,    false, false, false, false, false, false, false, false, false},
    {SensorType::BME280,    "CLIMA",       6,     7,    true,  true,  true,  false, false, false, false, false, false},
    {SensorType::SCD40,     "CARBON",        6,     7,    true,  true,  false, true,  false, false, false, false, false},
    {SensorType::METEO,     "METEO",       14,     7,    true,  true,  true,  false, false, true,  true,  true,  true},
    //{SensorType::VEML7700,  "VEML7700",     4,     7,    false, false, false, false, true,  false, false, false, false},
    // Přidejte další typy senzorů zde v budoucnosti...
};

// Pomocná funkce pro získání informací o typu senzoru podle enum hodnoty
inline const SensorTypeInfo& getSensorTypeInfo(SensorType type) {
    for (const auto& info : SENSOR_TYPE_DEFINITIONS) {
        if (info.type == type) {
            return info;
        }
    }
    return SENSOR_TYPE_DEFINITIONS[0]; // Vrátí UNKNOWN, pokud typ není nalezen
}

// Pomocná funkce pro získání informací o typu senzoru podle uint8_t hodnoty
inline const SensorTypeInfo& getSensorTypeInfo(uint8_t typeValue) {
    return getSensorTypeInfo(static_cast<SensorType>(typeValue));
}

// Pomocná funkce pro získání enum SensorType z uint8_t hodnoty
inline SensorType sensorTypeFromValue(uint8_t value) {
    return static_cast<SensorType>(value);
}

// Převod SensorType na string pro debugging a logování
inline String sensorTypeToString(SensorType type) {
    const SensorTypeInfo& info = getSensorTypeInfo(type);
    return String(info.name);
}