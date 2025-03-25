#pragma once

#include <Arduino.h>
#include <mutex>
#include <vector>
#include "SensorData.h"
#include "Logging.h"

/**
 * Třída pro správu kolekce senzorů
 * 
 * Zajišťuje přidávání, úpravu a mazání senzorů, jejich vyhledávání
 * a správu jejich dat. Ukládá konfiguraci senzorů do souborového systému.
 */
class SensorManager {
private:
    SensorData sensors[MAX_SENSORS];  // Pole senzorů - omezeno na MAX_SENSORS
    size_t sensorCount;               // Aktuální počet senzorů
    mutable std::mutex sensorMutex;   // Mutex pro bezpečný vícevláknový přístup
    Logger& logger;                   // Reference na logger
    
    // Název souboru pro ukládání konfigurace senzorů
    const char* sensorsFile;
    
public:
    // Konstruktor
    SensorManager(Logger& log, const char* file = SENSORS_FILE);
    
    // Destruktor
    ~SensorManager();
    
    // Inicializace správce senzorů
    bool init();
    
    // Přidání nového senzoru
    int addSensor(SensorType deviceType, uint32_t serialNumber, uint32_t deviceKey, const String& name);
    
    // Nalezení senzoru podle sériového čísla
    int findSensorBySN(uint32_t serialNumber);
    
    // Aktualizace dat senzoru
    bool updateSensor(int index, const SensorData& data);
    
    // Aktualizace dat senzoru podle typu (přetížená metoda pro jednodušší použití)
    bool updateSensorData(int index, float temperature, float humidity, float pressure, 
                         float ppm, float lux, float batteryVoltage, int rssi);
    
    // Aktualizace konfigurace senzoru
    bool updateSensorConfig(int index, const String& name, SensorType deviceType, 
                           uint32_t serialNumber, uint32_t deviceKey, 
                           const String& endpoint, const String& customParams);
    
    // Smazání senzoru
    bool deleteSensor(int index);
    
    // Získání počtu senzorů
    size_t getSensorCount() const;
    
    // Získání senzoru podle indexu
    const SensorData* getSensor(int index) const;
    SensorData* getSensor(int index);
    
    // Získání seznamu všech senzorů
    std::vector<SensorData> getAllSensors() const;
    
    // Získání seznamu všech aktivních senzorů (nakonfigurovaných)
    std::vector<SensorData> getActiveSensors() const;
    
    // Uložení konfigurace senzorů do souboru
    bool saveSensors(bool lockMutex);
    
    // Načtení konfigurace senzorů ze souboru
    bool loadSensors();

    bool forwardSensorData(int index);
};